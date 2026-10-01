"""Independent protocol-47 workbench, map and crash-recovery contracts."""
import gzip
import json
import math
from pathlib import Path
import struct
import tempfile
import time
import unittest

from test_drops_crafting_network import decode_nbt, item_snapshot, move
from test_inventory_network import (InventoryPeer, parse_slot, player_file, wire_slot,
                                    compound, named, nbt_string)
from test_multiplayer import position, read_string, read_vint, running_server, vint


def window_items(payload, window):
    actual, count = struct.unpack_from(">Bh", payload)
    assert actual == window and count == (46 if window else 45), (actual, count)
    slots, offset = [], 3
    for _ in range(count):
        slot, offset = parse_slot(payload, offset)
        slots.append(slot)
    assert offset == len(payload)
    return slots


class WorkbenchPeer(InventoryPeer):
    window = 0

    def open_table(self, table):
        self.send(8, position(*table) + b"\x01" + wire_slot() + b"\x08\x08\x08")
        payload = self.wait(0x2d)
        self.window = payload[0]
        kind, offset = read_string(payload, 1)
        title, offset = read_string(payload, offset)
        assert 1 <= self.window <= 100 and kind == "minecraft:crafting_table"
        assert json.loads(title) == {"translate": "tile.workbench.name"}
        assert payload[offset:] == b"\0"
        slots = window_items(self.wait(0x30, lambda p: p[0] == self.window), self.window)
        cursor = parse_slot(self.wait(0x2f, lambda p: p[0] == 255), 3)[0]
        return slots, cursor

    def table_click(self, index, button, action, mode=0, expected=b"\xff\xff", accepted=True):
        self.send(0x0e, struct.pack(">BhBhB", self.window, index, button, action, mode) + expected)
        fields = struct.unpack(">BhB", self.wait(0x32, lambda p: struct.unpack(">BhB", p)[:2] == (self.window, action)))
        assert fields == (self.window, action, int(accepted)), fields
        if accepted:
            assert mode != 5, "observe only after completing the source drag sequence"
            payload, cursor = self.window_snapshot(self.window)
            return window_items(payload, self.window), cursor
        slots = window_items(self.wait(0x30, lambda p: p[0] == self.window), self.window)
        cursor = parse_slot(self.wait(0x2f, lambda p: p[0] == 255), 3)[0]
        return slots, cursor

    def close_table(self, payload_id=255):
        self.send(0x0d, bytes([payload_id]))
        self.window = 0
        return self.snapshot()


class WorkbenchTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="c919-workbench-")
        self.world = Path(self.directory.name) / "world.c919"
        self.peers = []

    def tearDown(self):
        for peer in self.peers:
            peer.close()
        self.directory.cleanup()

    def peer(self, port, name):
        peer = WorkbenchPeer(port).login(name)
        self.peers.append(peer)
        return peer

    def place_table(self, peer):
        x, z = math.floor(peer.spawn[0]) + 2, math.floor(peer.spawn[2])
        table = (x, peer.surface(x, z) + 1, z)
        move(peer, x - 0.5, table[1] + 1, z + 0.5)
        peer.creative(36, 58, 1)
        below = (table[0], table[1] - 1, table[2])
        peer.send(8, position(*below) + b"\x01" + wire_slot(58, 1) + b"\x08\x10\x08")
        update = peer.wait(0x23, lambda p: p[:8] == position(*table))
        self.assertEqual(update[8:], vint(58 << 4))
        peer.creative(36)
        return table

    def test_empty_hand_3x3_chest_and_id_agnostic_close(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "TableCraft")
            table = self.place_table(peer)
            peer.creative(9, 5, 8)
            slots, cursor = peer.open_table(table)
            self.assertEqual(slots[10][:3], (5, 8, 0))
            self.assertEqual(cursor[0], -1)
            _, cursor = peer.table_click(10, 0, 1, expected=wire_slot(5, 8))
            self.assertEqual(cursor[:3], (5, 8, 0))
            for action, index in enumerate((1, 2, 3, 4, 6, 7, 8, 9), 2):
                slots, cursor = peer.table_click(index, 1, action)
            self.assertEqual(slots[0][:3], (54, 1, 0))
            slots, cursor = peer.table_click(0, 0, 10, expected=wire_slot(54, 1))
            self.assertEqual(cursor[:3], (54, 1, 0))
            self.assertTrue(all(s[0] == -1 for s in slots[1:10]))
            slots, cursor = peer.close_table(255)
            self.assertEqual(cursor[0], -1)
            self.assertEqual(sum(e["Item"]["Count"] for e in item_snapshot(self.world)
                                 if e["Item"]["id"] == "minecraft:chest"), 1)
            first = peer.window
            peer.open_table(table)
            self.assertGreater(peer.window, first)
            peer.close_table()

    def test_rejected_click_commits_and_false_ack_unlocks(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "TableAck")
            table = self.place_table(peer)
            peer.creative(9, 17, 2)
            peer.open_table(table)
            peer.table_click(10, 0, 1, expected=wire_slot(17, 2))
            peer.table_click(1, 0, 2)
            slots, cursor = peer.table_click(0, 0, 3, accepted=False)
            self.assertEqual(slots[1][:3], (17, 1, 0))
            self.assertEqual(cursor[:3], (5, 4, 0))
            # Locked clicks and clicks for another window are ignored.
            peer.send(0x0e, struct.pack(">BhBhB", peer.window, 0, 0, 4, 0) + wire_slot(5, 4))
            peer.send(0x0e, struct.pack(">BhBhB", peer.window + 1, 0, 0, 5, 0) + wire_slot(5, 4))
            peer.send(0x0f, struct.pack(">BhB", peer.window, 3, 0))
            slots, cursor = peer.table_click(0, 0, 6, expected=wire_slot(5, 4))
            self.assertEqual(slots[1][0], -1)
            self.assertEqual(cursor[:3], (5, 8, 0))
            peer.close_table()

    def test_range_closes_and_persists_input_drop(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "TableRange")
            table = self.place_table(peer)
            peer.creative(9, 264, 3)
            peer.open_table(table)
            peer.table_click(10, 0, 1, expected=wire_slot(264, 3))
            peer.table_click(9, 0, 2)
            move(peer, peer.spawn[0] + 12, peer.spawn[1], peer.spawn[2])
            self.assertEqual(peer.wait(0x2e), bytes([peer.window]))
            peer.window = 0
            slots, _ = peer.snapshot()
            self.assertEqual(slots[9][0], -1)
            self.assertEqual(sum(e["Item"]["Count"] for e in item_snapshot(self.world)
                                 if e["Item"]["id"] == "minecraft:diamond"), 3)

    def test_cake_remainders_survive_mapped_server_close(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "TableCake")
            table = self.place_table(peer)
            ingredients = (335, 335, 335, 353, 344, 353, 296, 296, 296)
            for i, item in enumerate(ingredients):
                peer.creative(9 + i, item, 1)
            peer.open_table(table)
            for i, item in enumerate(ingredients):
                peer.table_click(10 + i, 0, i * 2 + 1, expected=wire_slot(item, 1))
                slots, _ = peer.table_click(1 + i, 0, i * 2 + 2)
            self.assertEqual(slots[0][:3], (354, 1, 0))
            slots, cursor = peer.table_click(0, 1, 19, expected=wire_slot(354, 1))
            self.assertEqual(cursor[:3], (354, 1, 0))
            self.assertEqual([s[:3] for s in slots[1:4]], [(325, 1, 0)] * 3)
            peer.close_table()
            entities = item_snapshot(self.world)
            totals = {}
            for entry in entities:
                item = entry["Item"]
                totals[item["id"]] = totals.get(item["id"], 0) + item["Count"]
            self.assertEqual(totals, {"minecraft:bucket": 3, "minecraft:cake": 1})

    def test_another_player_breaking_table_closes_current_window(self):
        with running_server(self.world) as port:
            owner = self.peer(port, "TableOwner")
            other = self.peer(port, "TableBreaker")
            table = self.place_table(owner)
            owner.creative(9, 264, 3)
            owner.open_table(table)
            owner.table_click(10, 0, 1, expected=wire_slot(264, 3))
            owner.table_click(9, 0, 2)
            move(other, table[0] - 0.5, table[1] + 1, table[2] + 0.5)
            other.send(7, vint(0) + position(*table) + b"\x01")
            self.assertEqual(owner.wait(0x2e), bytes([owner.window]))
            owner.window = 0
            owner.snapshot()
            self.assertEqual(sum(e["Item"]["Count"] for e in item_snapshot(self.world)
                                 if e["Item"]["id"] == "minecraft:diamond"), 3)

    def test_crash_recovers_workbench_inputs_once(self):
        with running_server(self.world, crash=True) as port:
            peer = self.peer(port, "TableCrash")
            table = self.place_table(peer)
            peer.creative(9, 264, 3)
            peer.open_table(table)
            peer.table_click(10, 0, 1, expected=wire_slot(264, 3))
            peer.table_click(9, 0, 2)
            saved = decode_nbt(gzip.decompress(player_file(self.world, "TableCrash").read_bytes()))
            self.assertEqual(saved["C919Workbench"][0]["Count"], 3)
        # An ungraceful stop leaves the journaled grid for restart recovery.
        with running_server(self.world) as port:
            peer = self.peer(port, "TableCrash")
            initial = window_items(next(p for kind, p in peer.initial if kind == 0x30), 0)
            self.assertEqual(initial[9][0], -1)
            self.assertEqual(sum(e["Item"]["Count"] for e in item_snapshot(self.world)
                                 if e["Item"]["id"] == "minecraft:diamond"), 3)
            saved = decode_nbt(gzip.decompress(player_file(self.world, "TableCrash").read_bytes()))
            self.assertNotIn("C919Workbench", saved)

    def test_empty_map_surveys_and_scaling_commits_new_map_with_player(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "TableMaps")
            table = self.place_table(peer)
            peer.creative(36, 395, 1)
            before = len(peer.observed)
            peer.send(8, position(-1, -1, -1) + b"\xff" + wire_slot(395, 1) + b"\0\0\0")
            slots, _ = peer.snapshot()
            self.assertEqual(slots[36][:3], (358, 1, 0))
            maps = [payload for kind, payload in peer.observed[before:] if kind == 0x34]
            packet = maps[0] if maps else peer.wait(0x34)
            map_id, offset = read_vint(packet)
            self.assertEqual(map_id, 0)
            self.assertEqual(packet[offset], 0)
            icons, offset = read_vint(packet, offset + 1)
            offset += icons * 3
            width, height, x, z = packet[offset:offset + 4]
            length, offset = read_vint(packet, offset + 4)
            self.assertEqual((width, height, x, z, length), (128, 128, 0, 0, 16384))
            self.assertEqual(len(packet[offset:]), 16384)
            self.assertTrue(any(packet[offset:]), "held map must survey real terrain")
            peer.creative(9, 339, 8)
            peer.open_table(table)
            peer.table_click(37, 0, 1, expected=wire_slot(358, 1, 0))
            peer.table_click(5, 0, 2)
            peer.table_click(10, 0, 3, expected=wire_slot(339, 8))
            for action, index in enumerate((1, 2, 3, 4, 6, 7, 8, 9), 4):
                slots, _ = peer.table_click(index, 1, action)
            preview = slots[0]
            self.assertEqual(preview[:3], (358, 1, 0))
            self.assertEqual(decode_nbt(preview[3])["map_is_scaling"], 1)
            slots, cursor = peer.table_click(0, 0, 12, expected=wire_slot(*preview))
            self.assertEqual(cursor[:3], (358, 1, 1))
            self.assertEqual(decode_nbt(cursor[3])["map_is_scaling"], 1)
            self.assertTrue(all(s[0] == -1 for s in slots[1:10]))
            saved = decode_nbt(gzip.decompress(Path(str(self.world) + ".maps.dat").read_bytes()))
            self.assertEqual(saved["NextId"], 2)
            maps = {entry["Id"]: entry["Data"] for entry in saved["Maps"]}
            self.assertEqual(maps[1]["scale"], 1)
            self.assertEqual((maps[1]["xCenter"], maps[1]["zCenter"]), (64, 64))
            self.assertEqual(maps[1]["colors"], bytes(16384))
            saved_player = decode_nbt(gzip.decompress(player_file(self.world, "TableMaps").read_bytes()))
            self.assertEqual(saved_player["C919Cursor"]["Damage"], 1)
            peer.table_click(37, 0, 13)
            peer.close_table()
        with running_server(self.world) as port:
            restored = self.peer(port, "TableMaps")
            slots = window_items(next(p for kind, p in restored.initial if kind == 0x30), 0)
            self.assertEqual(slots[36][:3], (358, 1, 1))
            saved = decode_nbt(gzip.decompress(Path(str(self.world) + ".maps.dat").read_bytes()))
            self.assertEqual(saved["NextId"], 2)

    def test_map_player_marker_negative_edge_uses_signed_byte_wrap(self):
        with running_server(self.world) as port:
            self.peer(port, "MapMarker")
        body = compound(named(1, "dimension", b"\0"), named(3, "xCenter", struct.pack(">i", 1024)),
                        named(3, "zCenter", struct.pack(">i", 0)), named(1, "scale", b"\x04"),
                        named(2, "width", struct.pack(">h", 128)), named(2, "height", struct.pack(">h", 128)),
                        named(7, "colors", struct.pack(">i", 16384) + bytes(16384)))
        entry = compound(named(3, "Id", struct.pack(">i", 0)), named(10, "Data", body))
        data = named(10, "", compound(named(3, "Version", struct.pack(">i", 1)),
                                     named(3, "NextId", struct.pack(">i", 1)),
                                     named(9, "Maps", b"\x0a" + struct.pack(">i", 1) + entry)))
        Path(str(self.world) + ".maps.dat").write_bytes(gzip.compress(data))
        with running_server(self.world) as port:
            peer = self.peer(port, "MapMarker")
            # Marker rounding is tested at a deliberate player position; the
            # SourceMP constructor's random admission column is independent.
            move(peer, 8.5, peer.spawn[1], 8.5)
            peer.creative(36, 358, 1)
            packet = peer.wait(0x34)
            _, offset = read_vint(packet)
            icons, offset = read_vint(packet, offset + 1)
            self.assertEqual(icons, 1)
            self.assertEqual(struct.unpack_from(">Bbb", packet, offset), (0x60, -128, 1))

    def test_unselected_main_inventory_map_resolves_and_uses_mapinfo_packets(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "MapInStorage")
            peer.creative(9, 358, 1, 7)
            packet = peer.wait(0x34)
            map_id, offset = read_vint(packet)
            self.assertEqual((map_id, packet[offset]), (0, 3))
            icons, offset = read_vint(packet, offset + 1)
            self.assertEqual(icons, 1)
            offset += icons * 3
            self.assertEqual(tuple(packet[offset:offset + 4]), (128, 128, 0, 0))
            length, offset = read_vint(packet, offset + 4)
            self.assertEqual((length, packet[offset:]), (16384, bytes(16384)),
                             "unselected maps resolve and track players without surveying terrain")
            packet = peer.wait(0x34)
            _, offset = read_vint(packet)
            icons, offset = read_vint(packet, offset + 1)
            offset += icons * 3
            self.assertEqual(packet[offset:], b"\0", "unchanged MapInfo sends icons only")
            # Move the resolved map onto selected hotbar index 0. The very next
            # real survey must use its retained viewer state, not another full map.
            peer.send(9, struct.pack(">h", 0))
            before = len(peer.observed)
            peer.click(9, 0, 1, 2)
            maps = [payload for kind, payload in peer.observed[before:] if kind == 0x34]
            for _ in range(40):
                packet = maps.pop(0) if maps else peer.wait(0x34)
                _, offset = read_vint(packet)
                icons, offset = read_vint(packet, offset + 1)
                offset += icons * 3
                if packet[offset]:
                    break
            else:
                self.fail("selected map produced no dirty update")
            width, height, x, z = packet[offset:offset + 4]
            length, offset = read_vint(packet, offset + 4)
            self.assertEqual(length, width * height)
            self.assertTrue(any(packet[offset:]))
            self.assertLess(width * height, 16384, "initial full MapInfo state must survive unselected ticks")

    def test_armor_only_map_resolves_without_main_inventory_onupdate_or_packet(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "ArmorMap")
            start = len(peer.observed)
            peer.creative(5, 358, 1, 77)
            time.sleep(0.15)
            slots, _ = peer.snapshot()
            self.assertEqual(slots[5][:3], (358, 1, 0), "armor packet lookup still resolves missing MapData")
            self.assertFalse(any(kind == 0x34 for kind, _ in peer.observed[start:]),
                             "InventoryPlayer.onUpdate covers main36; armor alone must not register MapInfo")
        saved = decode_nbt(gzip.decompress(Path(str(self.world) + ".maps.dat").read_bytes()))
        self.assertEqual(saved["NextId"], 1)
        self.assertEqual(saved["Maps"][0]["Data"]["scale"], 3)

    def test_all_main_map_decorations_update_before_first_map_packet(self):
        # Prepare both source main-inventory stacks before the first tick so
        # packet observation cannot be confused with separate C10/tick order.
        def decoration(label, kind, x):
            element = compound(named(8, "id", nbt_string(label)), named(1, "type", bytes([kind])),
                               named(6, "x", struct.pack(">d", x)), named(6, "z", struct.pack(">d", 0)),
                               named(6, "rot", struct.pack(">d", 0)))
            return compound(named(9, "Decorations", b"\x0a" + struct.pack(">i", 1) + element))

        entries = []
        for index, label, kind, x in ((0, "first", 3, 16), (9, "second", 4, 32)):
            entries.append(compound(named(1, "Slot", bytes([index])),
                                    named(8, "id", nbt_string("minecraft:filled_map")), named(1, "Count", b"\x01"),
                                    named(2, "Damage", b"\0\0"), named(10, "tag", decoration(label, kind, x))))
        player = named(10, "", compound(named(9, "Inventory", b"\x0a" + struct.pack(">i", 2) + b"".join(entries)),
                                         named(3, "SelectedItemSlot", struct.pack(">i", 0))))
        path = player_file(self.world, "MapOrder")
        path.parent.mkdir()
        path.write_bytes(gzip.compress(player))
        map_body = compound(named(1, "dimension", b"\0"), named(3, "xCenter", struct.pack(">i", 0)),
                            named(3, "zCenter", struct.pack(">i", 0)), named(1, "scale", b"\x04"),
                            named(2, "width", struct.pack(">h", 128)), named(2, "height", struct.pack(">h", 128)),
                            named(7, "colors", struct.pack(">i", 16384) + bytes(16384)))
        map_entry = compound(named(3, "Id", struct.pack(">i", 0)), named(10, "Data", map_body))
        maps = named(10, "", compound(named(3, "Version", struct.pack(">i", 1)),
                                      named(3, "NextId", struct.pack(">i", 1)),
                                      named(9, "Maps", b"\x0a" + struct.pack(">i", 1) + map_entry)))
        Path(str(self.world) + ".maps.dat").write_bytes(gzip.compress(maps))
        with running_server(self.world) as port:
            peer = self.peer(port, "MapOrder")
            observed = [payload for kind, payload in peer.observed if kind == 0x34]
            packet = observed[0] if observed else peer.wait(0x34)
            map_id, offset = read_vint(packet)
            self.assertEqual((map_id, packet[offset]), (0, 4))
            count, offset = read_vint(packet, offset + 1)
            self.assertEqual(count, 3, "all main36 onUpdate calls precede all main+armor40 packet lookups")
            icons = [struct.unpack_from(">Bbb", packet, offset + i * 3) for i in range(count)]
            self.assertEqual(icons[1:], [(0x30, 2, 0), (0x40, 4, 0)])


if __name__ == "__main__":
    unittest.main()
