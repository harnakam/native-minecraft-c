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
    # DataOutput.writeUTF encodes UTF-16 code units, including NUL and lone
    # surrogates. Ordinary network String packets use a different codec.
    units = struct.unpack(">%dH" % (len(text.encode("utf-16-be", "surrogatepass")) // 2),
                          text.encode("utf-16-be", "surrogatepass"))
    data = bytearray()
    for unit in units:
        if 0 < unit < 128:
            data.append(unit)
        elif unit < 2048:
            data.extend((0xc0 | (unit >> 6), 0x80 | (unit & 63)))
        else:
            data.extend((0xe0 | (unit >> 12), 0x80 | ((unit >> 6) & 63), 0x80 | (unit & 63)))
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
    return struct.pack(">hbh", item, count, damage) + tag


def nbt_text(data, pos):
    if pos + 2 > len(data):
        raise ValueError("truncated NBT string")
    length = struct.unpack_from(">H", data, pos)[0]
    pos, end = pos + 2, pos + 2 + length
    if end > len(data):
        raise ValueError("truncated NBT string")
    units = []
    while pos < end:
        first = data[pos]
        pos += 1
        if first < 128:
            units.append(first)
        elif first & 0xe0 == 0xc0:
            if pos >= end or data[pos] & 0xc0 != 0x80:
                raise ValueError("invalid modified UTF-8")
            units.append(((first & 31) << 6) | (data[pos] & 63))
            pos += 1
        elif first & 0xf0 == 0xe0:
            if pos + 2 > end or any(byte & 0xc0 != 0x80 for byte in data[pos:pos + 2]):
                raise ValueError("invalid modified UTF-8")
            units.append(((first & 15) << 12) | ((data[pos] & 63) << 6) | (data[pos + 1] & 63))
            pos += 2
        else:
            raise ValueError("invalid modified UTF-8")
    return tuple(units), end


def nbt_payload(data, pos, kind, depth=0):
    """Typed independent NBT parser; only Compound member order is ignored."""
    if depth > 64:
        raise ValueError("NBT depth")
    widths = {1: 1, 2: 2, 3: 4, 4: 8, 5: 4, 6: 8}
    if kind in widths:
        end = pos + widths[kind]
        if end > len(data):
            raise ValueError("truncated NBT scalar")
        # Retain exact floating-point bits (NaN payload and signed zero too).
        value = data[pos:end] if kind in (5, 6) else int.from_bytes(data[pos:end], "big", signed=True)
        return (kind, value), end
    if kind == 8:
        value, end = nbt_text(data, pos)
        return (kind, value), end
    if kind in (7, 11):
        if pos + 4 > len(data):
            raise ValueError("truncated NBT array")
        length = struct.unpack_from(">i", data, pos)[0]
        width = 1 if kind == 7 else 4
        end = pos + 4 + length * width
        if length < 0 or end > len(data):
            raise ValueError("invalid NBT array")
        raw = data[pos + 4:end]
        value = raw if kind == 7 else tuple(struct.unpack(">%di" % length, raw))
        return (kind, value), end
    if kind == 9:
        if pos + 5 > len(data):
            raise ValueError("truncated NBT list")
        child, length = struct.unpack_from(">Bi", data, pos)
        if length < 0 or length > len(data) or child > 11 or (child == 0 and length):
            raise ValueError("invalid NBT list")
        values, pos = [], pos + 5
        for _ in range(length):
            value, pos = nbt_payload(data, pos, child, depth + 1)
            values.append(value)
        return (kind, (child, tuple(values))), pos
    if kind == 10:
        values = {}
        while True:
            if pos >= len(data):
                raise ValueError("truncated NBT compound")
            child, pos = data[pos], pos + 1
            if child == 0:
                return (kind, values), pos
            key, pos = nbt_text(data, pos)
            value, pos = nbt_payload(data, pos, child, depth + 1)
            # NBTTagCompound.read replaces an earlier equal name.
            values[key] = value
    raise ValueError(f"unknown target-version NBT type {kind}")


def parse_nbt(data, pos=0):
    if pos >= len(data):
        raise ValueError("truncated NBT root")
    kind, pos = data[pos], pos + 1
    if kind == 0:
        return (0, (), None), pos
    name, pos = nbt_text(data, pos)
    value, pos = nbt_payload(data, pos, kind)
    return (kind, name, value), pos


def typed_nbt(data):
    value, end = parse_nbt(data)
    if end != len(data):
        raise ValueError("trailing NBT bytes")
    return value


def nbt_values(data):
    """Convenient save-file view, while typed_nbt is used for wire equality."""
    def text(units):
        return struct.pack(">%dH" % len(units), *units).decode("utf-16-be", "surrogatepass")

    def value(tag):
        kind, content = tag
        if kind == 10:
            return {text(key): value(child) for key, child in content.items()}
        if kind == 9:
            return [value(child) for child in content[1]]
        if kind == 8:
            return text(content)
        if kind in (5, 6):
            return struct.unpack(">f" if kind == 5 else ">d", content)[0]
        if kind == 11:
            return list(content)
        return content

    root = typed_nbt(data)
    if root[0] != 10:
        raise ValueError("save root must be Compound")
    return value(root[2])


def skip_payload(data, pos, kind, depth=0):
    return nbt_payload(data, pos, kind, depth)[1]


def canonical_slot(slot):
    return slot[:3] + (typed_nbt(slot[3]),)


class SlotAssertions:
    def assertSlotEqual(self, actual, expected):
        self.assertEqual(canonical_slot(actual), canonical_slot(expected))

    def assertSlotIn(self, expected, actual):
        self.assertIn(canonical_slot(expected), [canonical_slot(slot) for slot in actual])


class InventoryFixtureTests(unittest.TestCase):
    def test_nbt_comparison_ignores_only_compound_member_order(self):
        first = named(3, "a", struct.pack(">i", -3))
        second = named(8, "b", nbt_string("\0\ud800\U0001f642"))
        left = named(10, "", compound(first, second))
        right = named(10, "", compound(second, first))
        self.assertNotEqual(left, right)
        self.assertEqual(typed_nbt(left), typed_nbt(right))
        self.assertEqual(nbt_values(left)["b"], "\0\ud800\U0001f642")
        wrong_type = named(10, "", compound(named(2, "a", struct.pack(">h", -3)), second))
        self.assertNotEqual(typed_nbt(left), typed_nbt(wrong_type))
        lists = [named(10, "", compound(named(9, "x", b"\x03" + struct.pack(">i2i", 2, *values))))
                 for values in ((1, -2), (-2, 1))]
        self.assertNotEqual(typed_nbt(lists[0]), typed_nbt(lists[1]))
        floats = [named(10, "", compound(named(5, "x", struct.pack(">I", bits))))
                  for bits in (0, 0x80000000, 0x7fc00001, 0x7fc00002)]
        self.assertEqual(len({repr(typed_nbt(tag)) for tag in floats}), 4)
        for invalid in (left[:-1], left + b"x", b"\x0a\0\0\x09\0\x01x\x01\xff\xff\xff\xff\0"):
            with self.assertRaises(ValueError):
                typed_nbt(invalid)

    def test_slots_keep_signed_count_and_nonnull_zero(self):
        for count in (-128, -1, 0, 127):
            encoded = wire_slot(387, count, 0, metadata())
            parsed, end = parse_slot(encoded)
            self.assertEqual(end, len(encoded))
            self.assertEqual(parsed[:3], (387, count, 0))
        self.assertNotEqual(canonical_slot(parse_slot(wire_slot(387, 0))[0]),
                            canonical_slot(parse_slot(wire_slot())[0]))

    def test_metadata_item_entry_can_follow_all_inherited_entries(self):
        entries = (b"\0\x01" + b"\x21\x01\x2c" + b"\x82" + string("label") +
                   b"\x03\0\x04\x01" + b"\xaa" + wire_slot(264, -1, 7, metadata()))
        eid, values = entity_metadata(b"\x01" + entries + b"\x7f")
        self.assertEqual(eid, 1)
        self.assertEqual(set(values), {0, 1, 2, 3, 4, 10})
        self.assertEqual(item_metadata(b"\x01" + entries + b"\x7f")[1][:3], (264, -1, 7))
        self.assertIsNone(item_metadata(b"\x01\0\x01\x7f"))
        for invalid in (b"\x01" + entries, b"\x01\0\x01\0\x02\x7f", b"\x01\x7fx"):
            with self.assertRaises(ValueError):
                entity_metadata(invalid)


def parse_slot(data, pos=0):
    item = struct.unpack_from(">h", data, pos)[0]
    pos += 2
    if item < 0:
        return (-1, 0, 0, b"\0"), pos
    count, damage = struct.unpack_from(">bh", data, pos)
    pos += 3
    start = pos
    _, pos = parse_nbt(data, pos)
    return (item, count, damage, data[start:pos]), pos


def entity_metadata(payload):
    """Read every protocol-47 DataWatcher entry, including inherited entries."""
    eid, offset = read_vint(payload)
    values = {}
    while True:
        if offset >= len(payload):
            raise ValueError("truncated metadata")
        header, offset = payload[offset], offset + 1
        if header == 0x7f:
            if offset != len(payload):
                raise ValueError("trailing metadata")
            return eid, values
        kind, index = header >> 5, header & 31
        if index in values:
            raise ValueError("duplicate metadata index")
        scalar = {0: ">b", 1: ">h", 2: ">i", 3: ">f", 6: ">iii", 7: ">fff"}
        if kind in scalar:
            fmt = scalar[kind]
            value = struct.unpack_from(fmt, payload, offset)
            offset += struct.calcsize(fmt)
            if kind < 4:
                value = value[0]
        elif kind == 4:
            value, offset = read_string(payload, offset)
        elif kind == 5:
            value, offset = parse_slot(payload, offset)
        else:
            raise ValueError("unknown metadata type")
        values[index] = (kind, value)


def item_metadata(payload):
    eid, values = entity_metadata(payload)
    item = values.get(10)
    return (eid, item[1]) if item is not None and item[0] == 5 else None


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
    def __init__(self, port):
        super().__init__(port)
        self._probe_action = -1

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
                item = item_metadata(payload)
                if item is not None:
                    result.append(item[1])
        return result

    def creative(self, index, item=-1, count=0, damage=0, tag=b"\0"):
        self.send(0x10, struct.pack(">h", index) + wire_slot(item, count, damage, tag))
        # Source detectAndSendChanges sends nothing for an equal overwrite and
        # may send a derived crafting result before the inventory slot itself.
        slots, _ = self.snapshot()
        return slots[index]

    def window_snapshot(self, window=0):
        """Observe completed actions using the source rejected no-op branch.

        Mode 7 returns NULL without changing slots/cursor. A non-NULL expected
        result therefore asks the original handler for S30/cursor and locks
        this window; C0F then restores it. Do not call during a drag sequence:
        any non-drag click resets the source Container's in-progress drag.
        """
        action = self._probe_action
        self._probe_action = -1 if action == -32768 else action - 1
        self.send(0x0e, struct.pack(">BhBhB", window, -999, 0, action, 7) + wire_slot(1, 1))
        confirm = self.wait(0x32, lambda p: struct.unpack(">BhB", p)[:2] == (window, action))
        assert struct.unpack(">BhB", confirm) == (window, action, 0)
        snapshot = self.wait(0x30, lambda p: p[0] == window)
        cursor = parse_slot(self.wait(0x2f, lambda p: p[0] == 255), 3)[0]
        self.send(0x0f, struct.pack(">BhB", window, action, 0))
        return snapshot, cursor

    def snapshot(self):
        payload, cursor = self.window_snapshot(0)
        return inventory_payload(payload), cursor

    def click(self, index, button, action, mode, expected=b"\xff\xff", accepted=True):
        self.send(0x0E, struct.pack(">BhBhB", 0, index, button, action, mode) + expected)
        confirm = self.wait(0x32, lambda p: struct.unpack(">BhB", p)[:2] == (0, action))
        fields = struct.unpack(">BhB", confirm)
        assert fields == (0, action, int(accepted)), fields
        if accepted:
            assert mode != 5, "observe only after completing the source drag sequence"
            return self.snapshot()
        # An intentionally rejected click stays locked for tests of C0F.
        snapshot = inventory_payload(self.wait(0x30))
        cursor = parse_slot(self.wait(0x2F, lambda p: p[0] == 255), 3)[0]
        return snapshot, cursor


class InventoryNetworkTests(SlotAssertions, unittest.TestCase):
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

    def test_rejected_noop_snapshot_preserves_state_and_ack_restores_next_click(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "SnapshotProbe")
            peer.creative(1, 17, 2)
            peer.creative(9, 276, 1, 7, metadata())
            slots, cursor = peer.click(9, 0, 1, 0, wire_slot(276, 1, 7, metadata()))
            before = ([canonical_slot(slot) for slot in slots], canonical_slot(cursor))
            start = len(peer.observed)
            again, held = peer.snapshot()
            self.assertEqual(([canonical_slot(slot) for slot in again], canonical_slot(held)), before)
            self.assertFalse(any(kind in (0x0e, 0x0d, 0x13) for kind, _ in peer.observed[start:]),
                             "the observation probe must not create, collect, or destroy items")
            after, held = peer.click(35, 0, 2, 0)
            self.assertSlotEqual(after[35], cursor)
            self.assertEqual(held[0], -1)
            self.assertEqual(after[1][:3], (17, 2, 0))
            self.assertEqual(after[0][:3], (5, 4, 0))

    def test_full_creative_nbt_metadata_equipment_and_reconnect(self):
        tag = metadata()
        with running_server(self.world) as port:
            owner = self.peer(port, "NBTBuilder")
            observer = self.peer(port, "NBTObserver")
            observer.wait(0x0C) if not any(kind == 0x0C for kind, _ in observer.initial) else None
            result = owner.creative(36, 276, 1, 7, tag)
            self.assertSlotEqual(result, (276, 1, 7, tag))
            equipment = observer.wait(4, lambda p: read_vint(p)[0] == owner.entity)
            _, offset = read_vint(equipment)
            self.assertSlotEqual(parse_slot(equipment, offset + 2)[0], (276, 1, 7, tag))
            self.assertSlotEqual(owner.creative(9, 5, 31, 2, tag), (5, 31, 2, tag))
            owner.close()
            observer.wait(0x13)
            restored = self.peer(port, "NBTBuilder")
            slots = inventory_payload(next(payload for kind, payload in restored.initial if kind == 0x30))
            self.assertSlotEqual(slots[36], (276, 1, 7, tag))
            self.assertSlotEqual(slots[9], (5, 31, 2, tag))
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
            payload = peer.wait(0x1c, lambda p: item_metadata(p) is not None)
            self.assertEqual(item_metadata(payload)[1][:3], (264, 7, 0))
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
        # Derive the native aggregate boundary from independent Source packet
        # serialization: S30 id/window/count, all45 slots, then S2F cursor.
        with running_server(self.world) as port:
            peer = self.peer(port, "SlotHeaderBudget")
            slots, cursor = peer.snapshot()
            empty_blob = named(10, "", compound(named(7, "C919Blob", struct.pack(">i", 0))))
            slots[9] = (1, 1, 0, empty_blob)
            headers_and_tag = 4 + sum(len(wire_slot(*slot)) for slot in slots) + 4 + len(wire_slot(*cursor))
            cap = 2 * 1024 * 1024 - 8192
            blob_size = cap - headers_and_tag + 1
            large = named(10, "", compound(named(7, "C919Blob", struct.pack(">i", blob_size) + b"a" * blob_size)))
            slots[9] = (1, 1, 0, large)
            self.assertEqual(4 + sum(len(wire_slot(*slot)) for slot in slots) + 4 + len(wire_slot(*cursor)), cap + 1)
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
            self.assertSlotIn(cursor, observer.dropped_slots())
            path = player_file(self.world, "ForeignFields")
            data = gzip.decompress(path.read_bytes())
            foreign = named(10, "ForeignCompound", compound(named(8, "Note", nbt_string("残すべき情報")),
                                                         named(4, "OpaqueTick", struct.pack(">q", 123456789))))
            path.write_bytes(gzip.compress(data[:-1] + foreign + b"\0"))
            restored = self.peer(port, "ForeignFields")
            cursor_payload = next(payload for kind, payload in restored.initial if kind == 0x2F and payload[0] == 255)
            self.assertEqual(parse_slot(cursor_payload, 3)[0], (-1, 0, 0, b"\0"))
            restored.creative(10, 260, 5)
            saved = nbt_values(gzip.decompress(path.read_bytes()))
            self.assertEqual(saved["ForeignCompound"], {"Note": "残すべき情報", "OpaqueTick": 123456789})

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
            self.assertSlotEqual(slots[36], (276, 1, 12, metadata()))
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
            self.assertTrue(reason)
            self.assertEqual(path.read_bytes(), committed)
            status = InventoryPeer(port)
            self.peers.append(status)
            status.handshake(1)
            status.send(0)
            self.assertIn(b'"protocol":47', status.wait(0))


if __name__ == "__main__":
    unittest.main()
