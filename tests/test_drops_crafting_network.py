"""Independent protocol-47 checks for crafting, item transfers and crash recovery."""
import gzip
from pathlib import Path
import struct
import tempfile
import time
import unittest

from test_inventory_network import (InventoryPeer, inventory_payload, metadata, parse_slot,
                                    player_file, wire_slot)
from test_multiplayer import position, read_vint, running_server


def decode_nbt(data):
    def text(offset):
        size = struct.unpack_from(">H", data, offset)[0]
        start = offset + 2
        return data[start:start + size].decode("utf-8"), start + size

    def payload(kind, offset, depth=0):
        assert depth <= 64
        scalar = {1: ">b", 2: ">h", 3: ">i", 4: ">q", 5: ">f", 6: ">d"}
        if kind in scalar:
            fmt = scalar[kind]
            return struct.unpack_from(fmt, data, offset)[0], offset + struct.calcsize(fmt)
        if kind == 8:
            return text(offset)
        if kind == 9:
            child, count = struct.unpack_from(">Bi", data, offset)
            assert 0 <= count <= len(data)
            values, offset = [], offset + 5
            for _ in range(count):
                value, offset = payload(child, offset, depth + 1)
                values.append(value)
            return values, offset
        if kind == 10:
            values = {}
            while data[offset]:
                child = data[offset]
                key, offset = text(offset + 1)
                value, offset = payload(child, offset, depth + 1)
                values[key] = value
            return values, offset + 1
        if kind in (7, 11):
            count = struct.unpack_from(">i", data, offset)[0]
            assert 0 <= count <= len(data)
            offset += 4
            if kind == 7:
                return data[offset:offset + count], offset + count
            values = list(struct.unpack_from(">%di" % count, data, offset))
            return values, offset + 4 * count
        raise AssertionError("Unexpected target-version tag %d" % kind)

    assert data[0] == 10
    _, offset = text(1)
    value, end = payload(10, offset)
    assert end == len(data)
    return value


def item_snapshot(world):
    return decode_nbt(gzip.decompress(Path(str(world) + ".items.dat").read_bytes()))["Entities"]


def move(peer, x, y, z, pitch=90):
    peer.send(6, struct.pack(">dddffB", x, y, z, 0, pitch, 0))


def item_packet(payload):
    eid, offset = read_vint(payload)
    assert payload[offset] == 0xaa
    item, end = parse_slot(payload, offset + 1)
    assert payload[end:] == b"\x7f"
    return eid, item


class DropCraftingTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="c919-transfer-network-")
        self.world = Path(self.directory.name) / "world.c919"
        self.peers = []

    def tearDown(self):
        for peer in self.peers:
            peer.close()
        self.directory.cleanup()

    def peer(self, port, name):
        peer = InventoryPeer(port)
        self.peers.append(peer)
        return peer.login(name)

    def test_full_nbt_q_drop_has_delay_and_another_player_collects(self):
        with running_server(self.world) as port:
            owner = self.peer(port, "DropOwner")
            other = self.peer(port, "DropPicker")
            x, y, z = owner.spawn[:3]
            move(other, x + 8, y, z)
            move(owner, x, y, z)
            owner.creative(36, 276, 1, 7, metadata())
            started = time.monotonic()
            owner.send(7, b"\x04" + position(0, 0, 0) + b"\0")
            eid, item = item_packet(owner.wait(0x1c))
            self.assertEqual(item, (276, 1, 7, metadata()))
            spawn = next(p for kind, p in owner.observed if kind == 0x0e and read_vint(p)[0] == eid)
            offset = read_vint(spawn)[1]
            self.assertEqual(spawn[offset], 2)
            self.assertEqual(struct.unpack_from(">iii", spawn, offset + 1),
                             (int(x * 32), int((y + 1.32) * 32), int(z * 32)))
            slots = inventory_payload(owner.wait(0x30))
            self.assertEqual(slots[36][0], -1)
            move(owner, x + 8, y, z)
            move(other, x, y - 1, z)
            picked = None
            deadline = time.monotonic() + 6
            while time.monotonic() < deadline:
                kind, packet = other.packet()
                if kind == 0:
                    other.send(0, packet)
                if kind == 0x30:
                    picked = inventory_payload(packet)
                if kind == 0x0d and read_vint(packet)[0] == eid:
                    self.assertEqual(read_vint(packet, read_vint(packet)[1])[0], other.entity)
                    break
            else:
                self.fail("item was not collected")
            self.assertGreaterEqual(time.monotonic() - started, 1.6)
            self.assertIsNotNone(picked)
            self.assertEqual(sum(s[1] for s in picked if s[0] == 276 and s[2] == 7 and s[3] == metadata()), 1)
            other.wait(0x13, lambda p: eid == read_vint(p, read_vint(p)[1])[0])
            self.assertEqual(item_snapshot(self.world), [])

    def test_close_drops_cursor_and_inputs_durably_without_result_duplication(self):
        with running_server(self.world) as port:
            owner = self.peer(port, "CloseOwner")
            owner.creative(1, 17, 2)
            owner.creative(2, 264, 3)
            owner.creative(9, 276, 1, 7, metadata())
            owner.click(9, 0, 1, 0, wire_slot(276, 1, 7, metadata()))
            owner.send(0x0d, b"\0")
            slots = inventory_payload(owner.wait(0x30))
            cursor = parse_slot(owner.wait(0x2f, lambda p: p[0] == 255), 3)[0]
            self.assertEqual(cursor[0], -1)
            self.assertTrue(all(s[0] == -1 for s in slots[:5]))
            self.assertIn((276, 1, 7, metadata()), owner.dropped_slots())
            self.assertIn((17, 2, 0, b"\0"), owner.dropped_slots())
            self.assertIn((264, 3, 0, b"\0"), owner.dropped_slots())
            entities = item_snapshot(self.world)
            self.assertEqual(len(entities), 3)
            self.assertTrue(all(e["PickupDelay"] == 40 for e in entities))
            self.assertTrue(all("Thrower" not in e for e in entities))
        with running_server(self.world) as port:
            restored = self.peer(port, "CloseOwner")
            slots = inventory_payload(next(p for kind, p in restored.initial if kind == 0x30))
            self.assertTrue(all(s[0] == -1 for s in slots[:5]))
            self.assertEqual(sum(e["Item"]["Count"] for e in item_snapshot(self.world)), 6)
            self.assertEqual(len({e["C919EntityId"] for e in item_snapshot(self.world)}), 3)
            self.assertNotIn(restored.entity, {e["C919EntityId"] for e in item_snapshot(self.world)})

    def test_crafting_result_click_shift_number_clone_and_drop(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "CraftOutput")
            peer.creative(44)
            peer.creative(1, 17, 3)
            preview = peer.wait(0x2f, lambda p: p[0] == 0 and struct.unpack_from(">h", p, 1)[0] == 0)
            self.assertEqual(parse_slot(preview, 3)[0][:3], (5, 4, 0))
            slots, cursor = peer.click(0, 1, 1, 0, wire_slot(5, 4))
            self.assertEqual(cursor[:3], (5, 4, 0))
            self.assertEqual(slots[1][:3], (17, 2, 0))
            peer.click(35, 0, 2, 0)
            slots, _ = peer.click(0, 0, 3, 1, wire_slot(5, 4))
            self.assertEqual(slots[35][:3], (5, 12, 0))
            self.assertEqual(slots[44][0], -1, "existing stacks merge before empty destinations")
            self.assertEqual(slots[1][0], -1)
            peer.creative(1, 17, 1)
            slots, _ = peer.click(0, 0, 4, 4)
            self.assertEqual(slots[1][0], -1)
            self.assertIn((5, 4, 0, b"\0"), peer.dropped_slots())
            peer.creative(36)
            peer.creative(1, 17, 1)
            slots, _ = peer.click(0, 0, 5, 2)
            self.assertEqual(slots[36][:3], (5, 4, 0))
            self.assertEqual(slots[1][0], -1)
            peer.creative(1, 17, 1)
            slots, cursor = peer.click(0, 2, 6, 3)
            self.assertEqual(cursor[:3], (5, 64, 0))
            self.assertEqual(slots[1][:3], (17, 1, 0))
            peer.send(0x0d, b"\0")
            slots = inventory_payload(peer.wait(0x30))
            self.assertTrue(all(s[0] == -1 for s in slots[:5]))
            counts = [(e["Item"]["id"], e["Item"]["Count"]) for e in item_snapshot(self.world)]
            self.assertIn(("minecraft:planks", 64), counts)
            self.assertIn(("minecraft:log", 1), counts)

    def test_full_creative_inventory_consumes_pickup_as_target_sink(self):
        with running_server(self.world) as port:
            owner = self.peer(port, "SinkDropper")
            collector = self.peer(port, "SinkPicker")
            x, y, z = owner.spawn[:3]
            move(collector, x + 8, y, z)
            for index in range(9, 45):
                owner.creative(index, 3, 64)
                collector.creative(index, 3, 64)
            owner.creative(36, 264, 1)
            move(owner, x, y, z)
            owner.send(7, b"\x03" + position(0, 0, 0) + b"\0")
            eid, item = item_packet(owner.wait(0x1c))
            self.assertEqual(item[:3], (264, 1, 0))
            owner.creative(36, 3, 64)
            move(owner, x + 8, y, z)
            move(collector, x, y - 1, z)
            collector.wait(0x0d, lambda p: read_vint(p)[0] == eid)
            snapshots = [inventory_payload(p) for kind, p in collector.observed if kind == 0x30]
            self.assertTrue(all(s[:3] == (3, 64, 0) for s in snapshots[-1][9:45]))
            self.assertEqual(item_snapshot(self.world), [], "Creative discards a full-inventory remainder in 1.8")

    def test_outside_click_drops_one_then_all_of_cursor(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "CursorDrops")
            peer.creative(9, 264, 7, 0, metadata())
            peer.click(9, 0, 1, 0, wire_slot(264, 7, 0, metadata()))
            _, cursor = peer.click(-999, 1, 2, 0)
            self.assertEqual(cursor, (264, 6, 0, metadata()))
            self.assertIn((264, 1, 0, metadata()), peer.dropped_slots())
            _, cursor = peer.click(-999, 0, 3, 0)
            self.assertEqual(cursor[0], -1)
            self.assertIn((264, 6, 0, metadata()), peer.dropped_slots())
            self.assertEqual(sum(e["Item"]["Count"] for e in item_snapshot(self.world)), 7)

    def test_world_merge_ignores_damage_only_for_items_without_subtypes(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "MergeVariants")
            x, y, z = peer.spawn[:3]
            move(peer, x, y, z)
            spawned = []
            for item, damage in ((266, 7), (266, 8), (35, 7), (35, 8)):
                peer.send(0x10, struct.pack(">h", -1) + wire_slot(item, 1, damage, metadata()))
                eid, actual = item_packet(peer.wait(0x1c))
                self.assertEqual(actual, (item, 1, damage, metadata()))
                spawned.append(eid)
            move(peer, x + 8, y, z)
            deadline = time.monotonic() + 4
            while time.monotonic() < deadline:
                entities = item_snapshot(self.world)
                gold = [e for e in entities if e["Item"]["id"] == "minecraft:gold_ingot"]
                if len(gold) == 1 and gold[0]["Item"]["Count"] == 2:
                    break
                time.sleep(0.05)
            else:
                self.fail("same-NBT gold drops did not merge")
            wool = [e for e in entities if e["Item"]["id"] == "minecraft:wool"]
            self.assertEqual(sorted((e["Item"]["Damage"], e["Item"]["Count"]) for e in wool),
                             [(7, 1), (8, 1)])
            self.assertEqual(len(entities), 3)
            merged_id = gold[0]["C919EntityId"]
            peer.wait(0x1c, lambda p: item_packet(p)[0] == merged_id and item_packet(p)[1][1] == 2)
            removed_id = next(eid for eid in spawned[:2] if eid != merged_id)
            destroyed = lambda p: removed_id == read_vint(p, read_vint(p)[1])[0]
            if not any(kind == 0x13 and destroyed(p) for kind, p in peer.observed):
                peer.wait(0x13, destroyed)

    def test_invalid_existing_item_snapshot_refuses_startup_without_replacing_file(self):
        with running_server(self.world):
            pass
        path = Path(str(self.world) + ".items.dat")
        original = gzip.compress(b"\x0a\0\0\x09\0\x08Entities\x0a\xff\xff\xff\xff\0")
        path.write_bytes(original)
        with self.assertRaisesRegex(AssertionError, "existing data was preserved"):
            with running_server(self.world):
                pass
        self.assertEqual(path.read_bytes(), original)

    def test_preparation_failure_preserves_held_item_and_no_entity_appears(self):
        with running_server(self.world) as port:
            owner = self.peer(port, "AbortTransfer")
            owner.creative(36, 276, 1, 7, metadata())
            blocked = Path(str(self.world) + ".pending-player.dat")
            blocked.unlink()
            blocked.mkdir()
            owner.send(7, b"\x04" + position(0, 0, 0) + b"\0")
            owner.wait(0x40)
            player = decode_nbt(gzip.decompress(player_file(self.world, "AbortTransfer").read_bytes()))
            held = next(e for e in player["Inventory"] if e["Slot"] == 0)
            self.assertEqual((held["id"], held["Count"], held["Damage"]), ("minecraft:diamond_sword", 1, 7))
            self.assertEqual(item_snapshot(self.world), [])
            self.assertFalse(Path(str(self.world) + ".transfer.dat").exists())
            blocked.rmdir()

    def test_checkpoint_failure_recovers_committed_drop_exactly_once(self):
        with running_server(self.world) as port:
            owner = self.peer(port, "RecoverTransfer")
            owner.creative(36, 276, 1, 7, metadata())
            blocked = Path(str(self.world) + ".items.dat")
            blocked.unlink()
            blocked.mkdir()
            owner.send(7, b"\x04" + position(0, 0, 0) + b"\0")
            owner.wait(0x40)
            self.assertTrue(Path(str(self.world) + ".transfer.dat").is_file())
            blocked.rmdir()
        with running_server(self.world) as port:
            restored = self.peer(port, "RecoverTransfer")
            slots = inventory_payload(next(p for kind, p in restored.initial if kind == 0x30))
            self.assertEqual(slots[36][0], -1)
            entities = item_snapshot(self.world)
            self.assertEqual(len(entities), 1)
            self.assertEqual((entities[0]["Item"]["id"], entities[0]["Item"]["Count"]), ("minecraft:diamond_sword", 1))
            self.assertEqual(entities[0]["Item"]["tag"]["display"]["Name"], "日本語の剣")
            self.assertFalse(Path(str(self.world) + ".transfer.dat").exists())
        with running_server(self.world):
            self.assertEqual(len(item_snapshot(self.world)), 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
