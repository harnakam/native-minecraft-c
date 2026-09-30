"""Independent full Slot/NBT and window-0 transaction interoperability tests."""
import gzip
import hashlib
from pathlib import Path
import struct
import tempfile
import unittest
import uuid

from test_multiplayer import Peer, SERVER, running_server, read_vint, position, string, read_string


def nbt_string(text):
    data = text.encode("utf-8")
    return struct.pack(">H", len(data)) + data


def named(kind, name, data):
    return bytes([kind]) + nbt_string(name) + data


def compound(*fields):
    return b"".join(fields) + b"\0"


def metadata():
    display = named(10, "display", compound(named(8, "Name", nbt_string("日本語の剣")),
                                          named(9, "Lore", b"\x08" + struct.pack(">i", 2) +
                                                nbt_string("第一行") + nbt_string("第二行"))))
    ench = named(9, "ench", b"\x0a" + struct.pack(">i", 1) +
                 compound(named(2, "id", struct.pack(">h", 16)), named(2, "lvl", struct.pack(">h", 5))))
    custom = named(11, "C919Ints", struct.pack(">i3i", 3, 1, -2, 3))
    return named(10, "", compound(display, ench, custom, named(3, "RepairCost", struct.pack(">i", 4))))


def wire_slot(item=-1, count=0, damage=0, tag=b"\0"):
    if item < 0:
        return struct.pack(">h", -1)
    return struct.pack(">hBh", item, count, damage) + tag


def skip_payload(data, pos, kind, depth=0):
    if depth > 64:
        raise ValueError("NBT depth")
    widths = {1: 1, 2: 2, 3: 4, 4: 8, 5: 4, 6: 8}
    if kind in widths:
        return pos + widths[kind]
    if kind == 8:
        return pos + 2 + struct.unpack_from(">H", data, pos)[0]
    if kind in (7, 11, 12):
        length = struct.unpack_from(">i", data, pos)[0]
        if length < 0:
            raise ValueError("negative array")
        return pos + 4 + length * {7: 1, 11: 4, 12: 8}[kind]
    if kind == 9:
        child, length = struct.unpack_from(">Bi", data, pos)
        pos += 5
        for _ in range(length):
            pos = skip_payload(data, pos, child, depth + 1)
        return pos
    if kind == 10:
        while data[pos]:
            child = data[pos]
            pos += 1
            length = struct.unpack_from(">H", data, pos)[0]
            pos += 2 + length
            pos = skip_payload(data, pos, child, depth + 1)
        return pos + 1
    raise ValueError(f"unknown NBT type {kind}")


def parse_slot(data, pos=0):
    item = struct.unpack_from(">h", data, pos)[0]
    pos += 2
    if item < 0:
        return (-1, 0, 0, b"\0"), pos
    count, damage = struct.unpack_from(">Bh", data, pos)
    pos += 3
    start = pos
    kind = data[pos]
    pos += 1
    if kind:
        length = struct.unpack_from(">H", data, pos)[0]
        pos += 2 + length
        pos = skip_payload(data, pos, kind)
    return (item, count, damage, data[start:pos]), pos


def inventory_payload(payload):
    window, count = struct.unpack_from(">Bh", payload)
    assert window == 0 and count == 45
    result, pos = [], 3
    for _ in range(count):
        item, pos = parse_slot(payload, pos)
        result.append(item)
    assert pos == len(payload)
    return result


def player_file(world, name):
    digest = bytearray(hashlib.md5(("OfflinePlayer:" + name).encode()).digest())
    digest[6] = (digest[6] & 15) | 48
    digest[8] = (digest[8] & 63) | 128
    return Path(str(world) + ".players") / (str(uuid.UUID(bytes=bytes(digest))) + ".dat")


class InventoryPeer(Peer):
    def packet(self):
        result = super().packet()
        if not hasattr(self, "observed"):
            self.observed = []
        self.observed.append(result)
        return result

    def dropped_slots(self):
        result = []
        for kind, payload in getattr(self, "observed", []):
            if kind == 0x1c:
                _, offset = read_vint(payload)
                if payload[offset] == 0xaa:
                    item, end = parse_slot(payload, offset + 1)
                    assert payload[end:] == b"\x7f"
                    result.append(item)
        return result

    def creative(self, index, item=-1, count=0, damage=0, tag=b"\0"):
        self.send(0x10, struct.pack(">h", index) + wire_slot(item, count, damage, tag))
        payload = self.wait(0x2F, lambda p: p[0] == 0 and struct.unpack_from(">h", p, 1)[0] == index)
        return parse_slot(payload, 3)[0]

    def click(self, index, button, action, mode, expected=b"\xff\xff", accepted=True):
        self.send(0x0E, struct.pack(">BhBhB", 0, index, button, action, mode) + expected)
        confirm = self.wait(0x32)
        fields = struct.unpack(">BhB", confirm)
        assert fields == (0, action, int(accepted)), fields
        snapshot = inventory_payload(self.wait(0x30))
        cursor = parse_slot(self.wait(0x2F, lambda p: p[0] == 255), 3)[0]
        return snapshot, cursor


class InventoryNetworkTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(SERVER.is_file(), f"server not built: {SERVER}")
        self.directory = tempfile.TemporaryDirectory(prefix="c919-inventory-test-")
        self.world = Path(self.directory.name) / "world.c919"
        self.peers = []

    def tearDown(self):
        for peer in self.peers:
            peer.close()
        self.directory.cleanup()

    def peer(self, port, name):
        peer = InventoryPeer(port)
        self.peers.append(peer)
        peer.login(name)
        return peer

    def test_full_creative_nbt_metadata_equipment_and_reconnect(self):
        tag = metadata()
        with running_server(self.world) as port:
            owner = self.peer(port, "NBTBuilder")
            observer = self.peer(port, "NBTObserver")
            observer.wait(0x0C) if not any(kind == 0x0C for kind, _ in observer.initial) else None
            result = owner.creative(36, 276, 1, 7, tag)
            self.assertEqual(result, (276, 1, 7, tag))
            equipment = observer.wait(4, lambda p: read_vint(p)[0] == owner.entity)
            _, offset = read_vint(equipment)
            self.assertEqual(parse_slot(equipment, offset + 2)[0], (276, 1, 7, tag))
            self.assertEqual(owner.creative(9, 5, 31, 2, tag), (5, 31, 2, tag))
            owner.close()
            observer.wait(0x13)
            restored = self.peer(port, "NBTBuilder")
            slots = inventory_payload(next(payload for kind, payload in restored.initial if kind == 0x30))
            self.assertEqual(slots[36], (276, 1, 7, tag))
            self.assertEqual(slots[9], (5, 31, 2, tag))
        sidecar = player_file(self.world, "NBTBuilder")
        data = gzip.decompress(sidecar.read_bytes())
        self.assertIn(b"Inventory", data)
        self.assertIn("日本語の剣".encode(), data)
        self.assertIn(b"ench", data)
        self.assertIn(b"minecraft:diamond_sword", data)

    def test_right_click_shift_hotbar_swap_and_rejection_ack(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "Clicks")
            peer.creative(10, 3, 9)
            slots, cursor = peer.click(10, 1, 1, 0, wire_slot(3, 9))
            self.assertEqual(slots[10][:3], (3, 4, 0))
            self.assertEqual(cursor[:3], (3, 5, 0))
            slots, cursor = peer.click(11, 1, 2, 0)
            self.assertEqual(slots[11][:3], (3, 1, 0))
            self.assertEqual(cursor[:3], (3, 4, 0))
            slots, cursor = peer.click(11, 3, 3, 2)
            self.assertEqual(slots[39][:3], (3, 1, 0))
            self.assertEqual(slots[11][0], 4)
            slots, cursor = peer.click(12, 0, 4, 0)
            self.assertEqual(slots[12][:3], (3, 4, 0))
            self.assertEqual(cursor[0], -1)
            slots, cursor = peer.click(10, 0, 5, 4)
            self.assertEqual(slots[10][:3], (3, 3, 0))
            self.assertIn((3, 1, 0, b"\0"), peer.dropped_slots())
            peer.creative(38)
            peer.creative(9, 276, 1, 3, metadata())
            slots, _ = peer.click(9, 0, 6, 1, wire_slot(276, 1, 3, metadata()))
            self.assertEqual(slots[9][0], -1)
            self.assertEqual(slots[38][:3], (276, 1, 3))
            # Vanilla commits a legitimate action before detecting a false
            # return stack, then rejects the confirmation and resynchronizes.
            slots, _ = peer.click(38, 0, 7, 0, wire_slot(1, 64), accepted=False)
            self.assertEqual(slots[38][0], -1)

    def test_armor_and_malformed_nbt_preserve_items_with_creative_drops(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "Armor")
            # Vanilla creative overwrite bypasses normal armor and stack
            # restrictions; normal click handling still enforces them.
            self.assertEqual(peer.creative(5, 310, 2, 20, metadata())[:3], (310, 2, 20))
            self.assertEqual(peer.creative(6, 310, 1)[:3], (310, 1, 0))
            peer.creative(9, 311, 1)
            peer.click(9, 0, 1, 0, wire_slot(311, 1))
            slots, cursor = peer.click(5, 0, 2, 0, wire_slot(310, 2, 20, metadata()))
            self.assertEqual(slots[5][:3], (310, 2, 20))
            self.assertEqual(cursor[:3], (311, 1, 0))
            slots, cursor = peer.click(6, 0, 3, 0, wire_slot(310, 1))
            self.assertEqual(slots[6][:3], (311, 1, 0))
            self.assertEqual(cursor[:3], (310, 1, 0))
            peer.send(0x10, struct.pack(">h", -1) + wire_slot(264, 7))
            payload = peer.wait(0x1c, lambda p: p[read_vint(p)[1]] == 0xaa)
            self.assertEqual(parse_slot(payload, read_vint(payload)[1] + 1)[0][:3], (264, 7, 0))
            slots, _ = peer.click(35, 0, 4, 0)
            self.assertEqual(slots[5][:3], (310, 2, 20))
            malformed = self.peer(port, "MalformedNBT")
            malformed.send(0x10, struct.pack(">h", 9) + wire_slot(1, 1, 0, b"\x0a\x00\x00\x09\x00\x01x\x01\xff\xff\xff\xff\x00"))
            self.assertIn("Malformed", read_string(malformed.wait(0x40))[0])

    def test_aggregate_nbt_limit_rejects_only_mutation(self):
        large = named(10, "", compound(named(7, "C919Blob", struct.pack(">i", 1_100_000) + b"a" * 1_100_000)))
        with running_server(self.world) as port:
            peer = self.peer(port, "LargeMetadata")
            self.assertEqual(peer.creative(9, 1, 1, 0, large)[3], large)
            peer.send(0x10, struct.pack(">h", 10) + wire_slot(1, 1, 0, large))
            slots = inventory_payload(peer.wait(0x30))
            self.assertEqual(slots[9][3], large)
            self.assertEqual(slots[10][0], -1)
            peer.send(1, string("still connected"))
            self.assertIn(b"still connected", peer.wait(2))

    def test_nbt_budget_includes_all_slot_headers_before_creative_mutation(self):
        # The tag fits the old raw-NBT budget but exceeds the shared container
        # budget once the 45 Slot headers and cursor are accounted for.
        blob_size = 2 * 1024 * 1024 - 8192 - 200
        large = named(10, "", compound(named(7, "C919Blob", struct.pack(">i", blob_size) + b"a" * blob_size)))
        with running_server(self.world) as port:
            peer = self.peer(port, "SlotHeaderBudget")
            peer.send(0x10, struct.pack(">h", 9) + wire_slot(1, 1, 0, large))
            slots = inventory_payload(peer.wait(0x30))
            self.assertEqual(slots[9][0], -1)
            self.assertEqual(peer.creative(10, 260, 5)[:3], (260, 5, 0))

    def test_cursor_drops_and_unknown_player_fields_survive_reconnect(self):
        with running_server(self.world) as port:
            owner = self.peer(port, "ForeignFields")
            observer = self.peer(port, "Witness")
            owner.creative(9, 3, 9, 0, metadata())
            _, cursor = owner.click(9, 1, 1, 0, wire_slot(3, 9, 0, metadata()))
            self.assertEqual(cursor[:3], (3, 5, 0))
            owner.close()
            observer.wait(0x13)
            self.assertIn(cursor, observer.dropped_slots())
            path = player_file(self.world, "ForeignFields")
            data = gzip.decompress(path.read_bytes())
            foreign = named(10, "ForeignCompound", compound(named(8, "Note", nbt_string("残すべき情報")),
                                                         named(4, "OpaqueTick", struct.pack(">q", 123456789))))
            path.write_bytes(gzip.compress(data[:-1] + foreign + b"\0"))
            restored = self.peer(port, "ForeignFields")
            cursor_payload = next(payload for kind, payload in restored.initial if kind == 0x2F and payload[0] == 255)
            self.assertEqual(parse_slot(cursor_payload, 3)[0], (-1, 0, 0, b"\0"))
            restored.creative(10, 260, 5)
            self.assertIn(foreign, gzip.decompress(path.read_bytes()))

    def test_resource_string_and_legacy_short_player_item_ids(self):
        with running_server(self.world) as port:
            owner = self.peer(port, "SavedFormats")
            witness = self.peer(port, "FormatWitness")
            owner.close()
            witness.wait(0x13)
            sword = compound(named(1, "Slot", b"\x00"), named(8, "id", nbt_string("minecraft:diamond_sword")),
                             named(1, "Count", b"\x01"), named(2, "Damage", struct.pack(">h", 12)),
                             named(10, "tag", metadata()[3:]))
            wool = compound(named(1, "Slot", b"\x09"), named(2, "id", struct.pack(">h", 35)),
                            named(1, "Count", b"\x11"), named(2, "Damage", struct.pack(">h", 11)))
            opaque = named(8, "ForeignNote", nbt_string("preserve external player data"))
            root = named(10, "Player", compound(named(9, "Inventory", b"\x0a" + struct.pack(">i", 2) + sword + wool),
                                              named(3, "SelectedItemSlot", struct.pack(">i", 2)), opaque))
            path = player_file(self.world, "SavedFormats")
            path.write_bytes(gzip.compress(root))
            loaded = self.peer(port, "SavedFormats")
            slots = inventory_payload(next(payload for kind, payload in loaded.initial if kind == 0x30))
            self.assertEqual(slots[36], (276, 1, 12, metadata()))
            self.assertEqual(slots[9][:3], (35, 17, 11))
            loaded.creative(10, 260, 5)
            saved = gzip.decompress(path.read_bytes())
            self.assertIn(b"minecraft:diamond_sword", saved)
            self.assertIn(b"minecraft:wool", saved)
            self.assertIn(opaque, saved)
            self.assertTrue(saved.startswith(b"\x0a\x00\x06Player"))

    def test_held_metadata_drives_authoritative_block_state(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "WoolMetadata")
            tag = metadata()
            peer.creative(36, 35, 31, 11, tag)
            ground = int(peer.spawn[1]) - 2
            peer.send(8, position(8, ground, 8) + b"\x01" + wire_slot(35, 31, 11, tag) + bytes([8, 16, 8]))
            target = position(8, ground + 1, 8)
            change = peer.wait(0x23, lambda payload: payload[:8] == target)
            self.assertEqual(read_vint(change, 8)[0], (35 << 4) | 11)

    def test_near_packet_limit_foreign_player_data_rejected_before_login(self):
        with running_server(self.world) as port:
            tag = named(10, "", compound(named(7, "x", struct.pack(">i", 2 * 1024 * 1024 - 90) +
                                              b"x" * (2 * 1024 * 1024 - 90))))
            entry = compound(named(1, "Slot", b"\x09"), named(2, "id", struct.pack(">h", 1)),
                             named(1, "Count", b"\x01"), named(2, "Damage", b"\0\0"), named(10, "tag", tag[3:]))
            root = named(10, "", compound(named(9, "Inventory", b"\x0a" + struct.pack(">i", 1) + entry)))
            self.assertEqual(len(root), 2 * 1024 * 1024 - 18)
            path = player_file(self.world, "NearCap")
            path.parent.mkdir(exist_ok=True)
            committed = gzip.compress(root)
            path.write_bytes(committed)
            peer = InventoryPeer(port)
            self.peers.append(peer)
            peer.handshake(2)
            peer.send(0, string("NearCap"))
            kind, payload = peer.packet()
            self.assertEqual(kind, 0, "reject before LoginSuccess instead of truncating WindowItems")
            reason = read_string(payload)[0].lower()
            self.assertIn("inventory", reason)
            self.assertIn("size", reason)
            self.assertEqual(path.read_bytes(), committed)
            status = InventoryPeer(port)
            self.peers.append(status)
            status.handshake(1)
            status.send(0)
            self.assertIn(b'"protocol":47', status.wait(0))


if __name__ == "__main__":
    unittest.main()
