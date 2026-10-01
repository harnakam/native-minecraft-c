"""Independent protocol-47 checks for crafting, item transfers and crash recovery."""
import gzip
from pathlib import Path
import struct
import tempfile
import time
import unittest

from test_inventory_network import (InventoryPeer, inventory_payload, metadata, parse_slot,
                                    player_file, wire_slot, nbt_values, item_metadata,
                                    SlotAssertions, typed_nbt, named, compound)
from test_multiplayer import position, read_vint, running_server, vint


def decode_nbt(data):
    return nbt_values(data)


def item_snapshot(world):
    return decode_nbt(gzip.decompress(Path(str(world) + ".items.dat").read_bytes()))["Entities"]


def move(peer, x, y, z, pitch=90):
    peer.send(6, struct.pack(">dddffB", x, y, z, 0, pitch, 0))


def item_packet(payload):
    result = item_metadata(payload)
    assert result is not None, "no item entry in entity metadata"
    return result


class DropCraftingTests(SlotAssertions, unittest.TestCase):
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

    def test_drop_uses_source_standing_and_sneaking_eye_height_and_ignores_dig_face(self):
        def float32(value):
            return struct.unpack(">f", struct.pack(">f", value))[0]

        with running_server(self.world) as port:
            peer = self.peer(port, "DropEyes")
            x, y, z = peer.spawn[:3]
            entities = []
            for action, height in ((0, float32(float32(1.62) - float32(0.08))), (1, float32(1.62))):
                peer.send(0x0b, vint(peer.entity) + vint(action) + vint(0))
                peer.creative(36, 264, 1)
                # Source EnumFacing.getFront masks the signed face byte and
                # DROP_ITEM ignores it. 255 must be accepted by the real codec.
                peer.send(7, vint(4) + position(0, 0, 0) + b"\xff")
                eid, item = item_packet(peer.wait(0x1c, lambda p: item_metadata(p) is not None and
                                                  item_metadata(p)[0] not in entities))
                self.assertEqual(item[:3], (264, 1, 0))
                spawn = next(payload for kind, payload in peer.observed if kind == 0x0e and read_vint(payload)[0] == eid)
                offset = read_vint(spawn)[1]
                expected_y = y - 0.30000001192092896 + height
                self.assertEqual(struct.unpack_from(">iii", spawn, offset + 1),
                                 (int(x * 32), int(expected_y * 32), int(z * 32)))
                entities.append(eid)

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
            eid, item = item_packet(owner.wait(0x1c, lambda p: item_metadata(p) is not None))
            self.assertSlotEqual(item, (276, 1, 7, metadata()))
            spawn = next(p for kind, p in owner.observed if kind == 0x0e and read_vint(p)[0] == eid)
            offset = read_vint(spawn)[1]
            self.assertEqual(spawn[offset], 2)
            self.assertEqual(struct.unpack_from(">iii", spawn, offset + 1),
                             (int(x * 32), int((y + 1.32) * 32), int(z * 32)))
            slots, _ = owner.snapshot()
            self.assertEqual(slots[36][0], -1)
            move(owner, x + 8, y, z)
            move(other, x, y - 1, z)
            deadline = time.monotonic() + 6
            while time.monotonic() < deadline:
                kind, packet = other.packet()
                if kind == 0:
                    other.send(0, packet)
                if kind == 0x0d and read_vint(packet)[0] == eid:
                    self.assertEqual(read_vint(packet, read_vint(packet)[1])[0], other.entity)
                    break
            else:
                self.fail("item was not collected")
            self.assertGreaterEqual(time.monotonic() - started, 1.6)
            picked, _ = other.snapshot()
            self.assertEqual(sum(s[1] for s in picked if s[0] == 276 and s[2] == 7 and
                                 typed_nbt(s[3]) == typed_nbt(metadata())), 1)
            destroyed = lambda p: eid == read_vint(p, read_vint(p)[1])[0]
            if not any(kind == 0x13 and destroyed(p) for kind, p in other.observed):
                other.wait(0x13, destroyed)
        self.assertEqual(item_snapshot(self.world), [])

    def test_close_drops_cursor_and_inputs_durably_without_result_duplication(self):
        # Crash after a committed observation. Real periodic graph ticks may
        # have advanced Age/delay since creation; their sum verifies the source
        # 40-tick creation delay without claiming an atomic TCP/read barrier.
        with running_server(self.world, crash=True) as port:
            owner = self.peer(port, "CloseOwner")
            owner.creative(1, 17, 2)
            owner.creative(2, 264, 3)
            owner.creative(9, 276, 1, 7, metadata())
            owner.click(9, 0, 1, 0, wire_slot(276, 1, 7, metadata()))
            owner.send(0x0d, b"\0")
            slots, cursor = owner.snapshot()
            self.assertEqual(cursor[0], -1)
            self.assertTrue(all(s[0] == -1 for s in slots[:5]))
            self.assertSlotIn((276, 1, 7, metadata()), owner.dropped_slots())
            self.assertIn((17, 2, 0, b"\0"), owner.dropped_slots())
            self.assertIn((264, 3, 0, b"\0"), owner.dropped_slots())
        entities = item_snapshot(self.world)
        self.assertEqual(len(entities), 3)
        self.assertTrue(all(e["Age"] >= 0 and e["PickupDelay"] == max(40 - e["Age"], 0)
                            for e in entities))
        self.assertTrue(all("Thrower" not in e for e in entities))
        with running_server(self.world) as port:
            restored = self.peer(port, "CloseOwner")
            slots = inventory_payload(next(p for kind, p in restored.initial if kind == 0x30))
            self.assertTrue(all(s[0] == -1 for s in slots[:5]))
        entities = item_snapshot(self.world)
        self.assertEqual(sum(e["Item"]["Count"] for e in entities), 6)
        self.assertEqual(len({e["C919EntityId"] for e in entities}), 3)
        self.assertNotIn(restored.entity, {e["C919EntityId"] for e in entities})

    def test_crafting_result_click_shift_number_clone_and_drop(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "CraftOutput")
            peer.creative(44)
            peer.creative(1, 17, 3)
            slots, _ = peer.snapshot()
            self.assertEqual(slots[0][:3], (5, 4, 0))
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
            slots, _ = peer.snapshot()
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
                # Batch setup; equal C10 overwrites need no S2F in the source.
                owner.send(0x10, struct.pack(">h", index) + wire_slot(3, 64))
                collector.send(0x10, struct.pack(">h", index) + wire_slot(3, 64))
            for player in (owner, collector):
                slots, _ = player.snapshot()
                self.assertTrue(all(slot[:3] == (3, 64, 0) for slot in slots[9:45]))
            owner.creative(36, 264, 1)
            move(owner, x, y, z)
            owner.send(7, b"\x03" + position(0, 0, 0) + b"\0")
            eid, item = item_packet(owner.wait(0x1c, lambda p: item_metadata(p) is not None))
            self.assertEqual(item[:3], (264, 1, 0))
            owner.creative(36, 3, 64)
            move(owner, x + 8, y, z)
            move(collector, x, y - 1, z)
            collector.wait(0x0d, lambda p: read_vint(p)[0] == eid)
            slots, _ = collector.snapshot()
            self.assertTrue(all(s[:3] == (3, 64, 0) for s in slots[9:45]))
        self.assertEqual(item_snapshot(self.world), [], "Creative discards a full-inventory remainder in 1.8")

    def test_outside_click_drops_one_then_all_of_cursor(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "CursorDrops")
            peer.creative(9, 264, 7, 0, metadata())
            peer.click(9, 0, 1, 0, wire_slot(264, 7, 0, metadata()))
            _, cursor = peer.click(-999, 1, 2, 0)
            self.assertSlotEqual(cursor, (264, 6, 0, metadata()))
            self.assertSlotIn((264, 1, 0, metadata()), peer.dropped_slots())
            _, cursor = peer.click(-999, 0, 3, 0)
            self.assertEqual(cursor[0], -1)
            self.assertSlotIn((264, 6, 0, metadata()), peer.dropped_slots())
        self.assertEqual(sum(e["Item"]["Count"] for e in item_snapshot(self.world)), 7)

    def test_world_merge_ignores_damage_only_for_items_without_subtypes(self):
        with running_server(self.world) as port:
            peer = self.peer(port, "MergeVariants")
            x, y, z = peer.spawn[:3]
            move(peer, x, y, z)
            spawned = []
            for item, damage in ((266, 7), (266, 8), (35, 7), (35, 8)):
                peer.send(0x10, struct.pack(">h", -1) + wire_slot(item, 1, damage, metadata()))
                def new_item(payload):
                    entry = item_metadata(payload)
                    return entry is not None and entry[0] not in spawned and entry[1][:3] == (item, 1, damage)
                eid, actual = item_packet(peer.wait(0x1c, new_item))
                self.assertSlotEqual(actual, (item, 1, damage, metadata()))
                spawned.append(eid)
            move(peer, x + 8, y, z)
            merged = peer.wait(0x1c, lambda p: item_metadata(p) is not None and item_packet(p)[1][:2] == (266, 2), seconds=4)
            merged_id = item_packet(merged)[0]
            removed_id = next(eid for eid in spawned[:2] if eid != merged_id)
            destroyed = lambda p: removed_id == read_vint(p, read_vint(p)[1])[0]
            if not any(kind == 0x13 and destroyed(p) for kind, p in peer.observed):
                peer.wait(0x13, destroyed)
        # The packets follow durable commitment. Stop the checkpoint writer
        # before opening its Windows target name; the observer must not make a
        # valid server transfer fail with MoveFileEx sharing/access contention.
        entities = item_snapshot(self.world)
        gold = [e for e in entities if e["Item"]["id"] == "minecraft:gold_ingot"]
        self.assertEqual(len(gold), 1)
        self.assertEqual(gold[0]["Item"]["Count"], 2)
        self.assertEqual(gold[0]["C919EntityId"], merged_id)
        wool = [e for e in entities if e["Item"]["id"] == "minecraft:wool"]
        self.assertEqual(sorted((e["Item"]["Damage"], e["Item"]["Count"]) for e in wool),
                         [(7, 1), (8, 1)])
        self.assertEqual(len(entities), 3)

    def test_invalid_existing_item_snapshot_refuses_startup_without_replacing_file(self):
        # Seed the invalid target before the first startup. Terminating a
        # periodic tick first could leave a legitimate journal, whose recovery
        # must take precedence over a subsequently edited checkpoint target.
        path = Path(str(self.world) + ".items.dat")
        original = gzip.compress(b"\x0a\0\0\x09\0\x08Entities\x0a\xff\xff\xff\xff\0")
        path.write_bytes(original)
        with self.assertRaisesRegex(AssertionError, "existing data.*preserved"):
            with running_server(self.world):
                pass
        self.assertEqual(path.read_bytes(), original)

    def test_finite_large_saved_item_position_refuses_startup_and_preserves_checkpoint(self):
        def vector(label, values):
            return named(9,label,b"\x06"+struct.pack(">i3d",3,*values))

        item=compound(named(8,"id",b"\0\x11minecraft:diamond"),named(1,"Count",b"\x01"),
                      named(2,"Damage",b"\0\0"))
        entity=compound(named(8,"id",b"\0\x04Item"),named(3,"C919EntityId",struct.pack(">i",7)),
                        vector("Pos",(1.0,1e20,1.0)),vector("Motion",(0.0,0.0,0.0)),
                        named(2,"Health",struct.pack(">h",5)),named(2,"Age",b"\0\0"),
                        named(2,"PickupDelay",struct.pack(">h",40)),named(10,"Item",item))
        root=named(10,"",compound(named(3,"Version",struct.pack(">i",1)),
                                  named(9,"Entities",b"\x0a"+struct.pack(">i",1)+entity)))
        self.assertEqual(decode_nbt(root)["Entities"][0]["Pos"], [1.0,1e20,1.0])
        path=Path(str(self.world)+".items.dat")
        original=gzip.compress(root)
        path.write_bytes(original)
        with self.assertRaisesRegex(AssertionError,"existing data.*preserved"):
            with running_server(self.world):
                pass
        self.assertEqual(path.read_bytes(),original)
        self.assertFalse(Path(str(self.world)+".transfer.dat").exists())

    def test_preparation_failure_preserves_held_item_and_no_entity_appears(self):
        with running_server(self.world) as port:
            owner = self.peer(port, "AbortTransfer")
            owner.creative(36, 276, 1, 7, metadata())
            blocked = Path(str(self.world) + ".pending-player." + player_file(self.world, "AbortTransfer").stem + ".dat")
            blocked.unlink(missing_ok=True)
            blocked.mkdir()
            owner.send(7, b"\x04" + position(0, 0, 0) + b"\0")
            owner.wait(0x40)
            blocked.rmdir()
        player = decode_nbt(gzip.decompress(player_file(self.world, "AbortTransfer").read_bytes()))
        held = next(e for e in player["Inventory"] if e["Slot"] == 0)
        self.assertEqual((held["id"], held["Count"], held["Damage"]), ("minecraft:diamond_sword", 1, 7))
        self.assertEqual(item_snapshot(self.world), [])
        self.assertFalse(Path(str(self.world) + ".transfer.dat").exists())

    def test_checkpoint_failure_recovers_committed_graph_without_loss_or_duplication(self):
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
            self.assertFalse(Path(str(self.world) + ".transfer.dat").exists())
        entities = item_snapshot(self.world)
        # A real periodic graph tick can hit the externally blocked target
        # before C07 reaches the server. Recovery must retain whichever whole
        # graph reached the journal commit point, never invent a packet order.
        inventory_swords = [slot for slot in slots if slot[0] == 276]
        dropped_swords = [entry["Item"] for entry in entities if entry["Item"]["id"] == "minecraft:diamond_sword"]
        self.assertEqual(sum(slot[1] for slot in inventory_swords) +
                         sum(item["Count"] for item in dropped_swords), 1)
        for slot in inventory_swords:
            self.assertSlotEqual(slot, (276, 1, 7, metadata()))
        for item in dropped_swords:
            self.assertEqual((item["Count"], item["Damage"]), (1, 7))
            self.assertEqual(item["tag"], decode_nbt(metadata()))
        with running_server(self.world) as port:
            restored = self.peer(port, "RecoverTransfer")
            again = inventory_payload(next(p for kind, p in restored.initial if kind == 0x30))
            self.assertEqual([slot[:3] for slot in again], [slot[:3] for slot in slots])
        self.assertEqual(item_snapshot(self.world), entities)


if __name__ == "__main__":
    unittest.main(verbosity=2)
