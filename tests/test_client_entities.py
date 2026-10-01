"""Independent compressed protocol47 peer exercises owned item entities and inputs."""
import contextlib
from pathlib import Path
import queue
import socket
import struct
import subprocess
import threading
import tempfile
import time
import unittest
import zlib

from test_client import CLIENT, read_frame, send_frame
from test_client_inventory import EMPTY, TOOL, TOOL_NBT
from test_multiplayer import string, vint
from test_multiplayer import running_server, read_vint
from test_inventory_network import InventoryPeer, compound, named, nbt_string, player_file, wire_slot, item_metadata


def spawn(eid, x=8, y=7, z=8, pitch=0, yaw=0, data=1, velocity=(0, 0, 0)):
    payload = vint(eid) + struct.pack(">BiiiBBi", 2, x*32, y*32, z*32, pitch, yaw, data)
    return payload + (struct.pack(">hhh", *velocity) if data > 0 else b"")


@contextlib.contextmanager
def entity_peer(kind="lifecycle", inventory=None, cursor=EMPTY, ack_snapshot=None, forced_window=0):
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(6)
    errors, observed = queue.Queue(), []

    def serve():
        try:
            sock, _ = listener.accept()
            with sock:
                sock.settimeout(4)
                assert read_frame(sock)[0] == 0
                assert read_frame(sock)[0] == 0
                send_frame(sock, 3, vint(64))
                send_frame(sock, 2, string("00000000-0000-3000-8000-000000000001") + string("ClientTest"), True)
                send_frame(sock, 1, struct.pack(">iBbBB", 1, 1, 0, 0, 8) + string("default") + b"\0", True)
                send_frame(sock, 8, struct.pack(">dddffB", 8.5, 7, 8.5, 0, 0, 0), True)
                data = bytes(8192 + 2048) + b"\xff"*2048 + b"\1"*256
                send_frame(sock, 0x21, struct.pack(">iiBH", 0, 0, 1, 1) + vint(len(data)) + data, True)
                slots = [EMPTY]*45 if inventory is None else inventory.copy()
                send_frame(sock, 0x30, b"\0"+struct.pack(">h", 45)+b"".join(slots), True)
                send_frame(sock, 0x2f, b"\xff\xff\xff"+cursor, True)
                for eid in ([0, -1, -2147483648, 2147483647] if kind == "signed_ids" else [20, 21, 22]):
                    send_frame(sock, 0x0e, spawn(eid), True)
                    send_frame(sock, 0x1c, vint(eid)+b"\xaa"+TOOL+b"\x7f", True)
                if kind == "signed_ids":
                    for eid in [0, -1, -2147483648, 2147483647]:
                        send_frame(sock, 0x12, vint(eid)+struct.pack(">hhh", 800, 0, 0), True)
                        send_frame(sock, 0x14, vint(eid)+b"\1", True)
                        send_frame(sock, 0x18, vint(eid)+struct.pack(">iiiBBB", -1, 224, 256, 0, 0, 0), True)
                        send_frame(sock, 0x15, vint(eid)+struct.pack(">bbbB", 2, 0, -1, 0), True)
                        send_frame(sock, 0x16, vint(eid)+b"\0\0\1", True)
                        send_frame(sock, 0x17, vint(eid)+b"\1\0\0\0\0\1", True)
                    send_frame(sock, 0x0d, vint(0)+vint(-2147483648), True)
                    send_frame(sock, 0x0d, vint(-1)+vint(0), True)
                    send_frame(sock, 0x13, vint(1)+vint(-2147483648), True)
                elif kind == "lifecycle":
                    send_frame(sock, 0x12, vint(20)+struct.pack(">hhh", 800, 0, 0), True)
                    send_frame(sock, 0x18, vint(20)+struct.pack(">iiiBBB", -1, 224, 256, 0, 0, 0), True)
                    send_frame(sock, 0x15, vint(20)+struct.pack(">bbbB", 2, 0, -1, 0), True)
                    send_frame(sock, 0x0d, vint(21)+vint(1), True)
                    send_frame(sock, 0x13, vint(2)+vint(22)+vint(21), True)
                elif kind == "malformed":
                    send_frame(sock, 0x1c, vint(20)+b"\xaa"+EMPTY+b"\xa5"+TOOL[:-2], True)
                elif kind == "nullable":
                    send_frame(sock, 0x1c, vint(20)+b"\xaa"+EMPTY+b"\x7f", True)
                    send_frame(sock, 0x0e, spawn(21), True)
                elif kind == "spawn_angles":
                    cases = [(20, 128, 255, 0), (21, 255, 127, -1), (22, 127, 128, 1)]
                    for eid, pitch, yaw, object_data in cases:
                        send_frame(sock, 0x0e, spawn(eid, y=20, pitch=pitch, yaw=yaw,
                                                   data=object_data, velocity=(800, -1600, 2400)), True)
                        send_frame(sock, 0x1c, vint(eid)+b"\xaa"+TOOL+b"\x7f", True)
                elif kind == "all_metadata":
                    extra = (b"\x00\1\x21"+struct.pack(">h", 7)+b"\x42"+struct.pack(">i", 99)+
                             b"\x63"+struct.pack(">f", 0.5)+b"\x84"+string("extra")+
                             b"\xa5"+TOOL+b"\xc6"+struct.pack(">iii", 1, 2, 3)+
                             b"\xe7"+struct.pack(">fff", 1, 2, 3)+b"\xaa"+TOOL+b"\x7f")
                    send_frame(sock, 0x1c, vint(20)+extra, True)
                elif kind == "malformed_destroy":
                    send_frame(sock, 0x13, vint(2)+vint(20), True)
                elif kind == "outer_position":
                    send_frame(sock, 0x18, vint(20)+struct.pack(">iiiBBB", 40000000*32, 224, 256, 0, 0, 0), True)
                elif kind == "unsupported_spawn":
                    send_frame(sock, 0x0e, spawn(99, x=67108833), True)
                elif kind == "unsupported_teleport":
                    send_frame(sock, 0x18, vint(20)+struct.pack(">iiiBBB", 67108833*32, 224, 256, 0, 0, 0), True)
                elif kind == "aggregate":
                    large = named(10, "", named(7, "unknown", struct.pack(">i", 1100000)+bytes(1100000))+b"\0")
                    slot = wire_slot(387, 1, tag=large)
                    send_frame(sock, 0x1c, vint(20)+b"\xaa"+slot+b"\x7f", True)
                    send_frame(sock, 0x1c, vint(21)+b"\xaa"+slot+b"\x7f", True)
                elif kind == "forced_close":
                    send_frame(sock, 0x2e, bytes([forced_window]), True)
                elif kind == "unload":
                    send_frame(sock, 0x21, struct.pack(">iiBH", 0, 0, 1, 0)+vint(0), True)
                elif kind == "respawn":
                    send_frame(sock, 7, struct.pack(">iBB", 0, 0, 1)+string("default"), True)
                    send_frame(sock, 8, struct.pack(">dddffB", 8.5, 7, 8.5, 0, 0, 0), True)
                    send_frame(sock, 0x21, struct.pack(">iiBH", 0, 0, 1, 1)+vint(len(data))+data, True)
                while True:
                    try:
                        packet = read_frame(sock, True)
                    except EOFError:
                        break
                    observed.append(packet)
                    if packet[0] == 0x0e:
                        action = struct.unpack_from(">h", packet[1], 4)[0]
                        if kind == "forced_pending":
                            send_frame(sock, 0x2e, bytes([forced_window]), True)
                            send_frame(sock, 0x32, struct.pack(">BhB", 0, action, 1), True)
                            continue
                        if ack_snapshot is not None:
                            send_frame(sock, 0x30, b"\0"+struct.pack(">h", 45)+b"".join(ack_snapshot), True)
                        send_frame(sock, 0x32, struct.pack(">BhB", 0, action, 1), True)
        except Exception as exc:
            errors.put(exc)

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    try:
        yield listener.getsockname()[1], observed
    finally:
        thread.join(6)
        listener.close()
        if thread.is_alive():
            raise AssertionError("entity peer did not finish")
        if not errors.empty():
            raise errors.get()


class ClientEntityTests(unittest.TestCase):
    def client(self, port, *options):
        result = subprocess.run([str(CLIENT), "--host", "127.0.0.1", "--port", str(port),
                                 "--headless", "--run-seconds", "0.6", *options],
                                capture_output=True, text=True, timeout=8)
        return result, result.stdout+result.stderr

    def test_spawn_full_nbt_velocity_movement_collect_destroy(self):
        with entity_peer() as (port, _):
            result, output = self.client(port)
        self.assertEqual(result.returncode, 0, output)
        self.assertRegex(output, r"item_entities=1")
        self.assertRegex(output, r"CLIENT_ITEM eid=20 ready=1 id=276 count=1 damage=7 nbt_size=%d nbt_crc=%08x" %
                         (len(TOOL_NBT), zlib.crc32(TOOL_NBT)))
        self.assertIn("server_position=0.031,7.000,7.969", output)
        self.assertRegex(output, r"item_spawns=3 item_metadata=3 item_collects=1")

    def test_malformed_metadata_rolls_back_existing_item(self):
        with entity_peer("malformed") as (port, _):
            result, output = self.client(port)
        self.assertNotEqual(result.returncode, 0, output)
        self.assertIn("Malformed", output)
        self.assertRegex(output, r"CLIENT_ITEM eid=20 ready=1 id=276 count=1 damage=7 nbt_size=%d" % len(TOOL_NBT))

    def test_signed_spawn_angles_and_absent_velocity(self):
        with entity_peer("spawn_angles") as (port, _):
            result, output = self.client(port)
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("item_entities=3 item_spawns=6 item_metadata=6", output)
        for eid, angles in [(20, "-180.000000,-1.406250"),
                            (21, "-1.406250,178.593750"),
                            (22, "178.593750,-180.000000")]:
            self.assertRegex(output, r"CLIENT_ITEM eid=%d [^\n]*rotation=%s" % (eid, angles))

    def test_signed_entity_ids_include_zero_and_int32_boundaries(self):
        with entity_peer("signed_ids") as (port, _):
            result, output = self.client(port)
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("item_entities=1", output)
        self.assertIn("item_spawns=4 item_metadata=4 item_collects=2", output)
        self.assertRegex(output, r"CLIENT_ITEM eid=2147483647 ready=1 id=276 count=1 damage=7 nbt_size=%d nbt_crc=%08x" %
                         (len(TOOL_NBT), zlib.crc32(TOOL_NBT)))
        self.assertIn("server_position=0.062,7.000,7.969", output)

    def test_empty_slot_and_duplicate_spawn_free_metadata(self):
        with entity_peer("nullable") as (port, _):
            result, output = self.client(port)
        self.assertEqual(result.returncode, 0, output)
        self.assertRegex(output, r"CLIENT_ITEM eid=20 ready=0 id=-1 count=0")
        self.assertRegex(output, r"CLIENT_ITEM eid=21 ready=0 id=-1 count=0")

    def test_all_metadata_types_atomic_destroy_and_cumulative_large_tags(self):
        with entity_peer("all_metadata") as (port, _):
            result, output = self.client(port)
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("item_metadata=4", output)
        with entity_peer("malformed_destroy") as (port, _):
            result, output = self.client(port)
        self.assertNotEqual(result.returncode, 0, output)
        self.assertIn("item_entities=3", output)
        with entity_peer("aggregate") as (port, _):
            result, output = self.client(port)
        self.assertEqual(result.returncode, 0, output)
        self.assertRegex(output, r"CLIENT_ITEM eid=20 ready=1 id=387 count=1 damage=0 nbt_size=1100018")
        self.assertRegex(output, r"CLIENT_ITEM eid=21 ready=1 id=387 count=1 damage=0 nbt_size=1100018")

    def test_chunk_unload_and_respawn_remove_entities(self):
        for kind in ["unload", "respawn"]:
            with entity_peer(kind) as (port, _):
                result, output = self.client(port)
            # Unloading the only chunk also makes the existing headless world check fail.
            if kind == "respawn":
                self.assertEqual(result.returncode, 0, output)
            self.assertIn("item_entities=0", output)

    def test_native_item_coordinate_envelope_is_consistent_and_atomic(self):
        with entity_peer("outer_position") as (port, _):
            result, output = self.client(port)
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("server_position=40000000.000,7.000,8.000", output)
        for kind in ["unsupported_spawn", "unsupported_teleport"]:
            with entity_peer(kind) as (port, _):
                result, output = self.client(port)
            self.assertNotEqual(result.returncode, 0, output)
            self.assertIn("Malformed", output)
            self.assertIn("item_entities=3 item_spawns=3", output)
            self.assertIn("server_position=8.000,7.000,8.000", output)
            self.assertNotIn("CLIENT_ITEM eid=99", output)

    def test_q_and_control_q_emit_real_digging_drop_actions(self):
        slots = [EMPTY]*45
        slots[36] = struct.pack(">hBhB", 1, 3, 0, 0)
        with entity_peer(inventory=slots) as (port, observed):
            result, output = self.client(port, "--drop-actions", "one,all")
        self.assertEqual(result.returncode, 0, output)
        drops = [packet for kind, packet in observed if kind == 7]
        self.assertEqual(drops, [b"\4"+bytes(9), b"\3"+bytes(9)])
        # Original EntityPlayerSP.dropOneItem only sends C07. Inventory changes
        # arrive from the server, and this independent peer sends none.
        self.assertIn("CLIENT_SLOT index=36 id=1 count=3", output)

    def test_invalid_drop_and_signed_click_scripts_fail_before_connect(self):
        for options in [["--drop-actions", "one,"], ["--drop-actions", "many"],
                        ["--inventory-actions", "-998:0:0"], ["--inventory-actions", "-99999999999:0:0"]]:
            result = subprocess.run([str(CLIENT), "--headless", *options], capture_output=True, text=True, timeout=3)
            self.assertEqual(result.returncode, 2, result.stdout+result.stderr)

    def test_craft_output_normal_shift_number_clone_and_drop(self):
        slots = [EMPTY]*45
        slots[1] = wire_slot(41, 1)
        slots[0] = wire_slot(266, 9)
        cases = [("0:0:0", "CLIENT_CURSOR id=266 count=9", wire_slot(266, 9)),
                 ("0:0:1", "CLIENT_SLOT index=44 id=266 count=9", wire_slot(266, 9)),
                 ("0:0:2", "CLIENT_SLOT index=36 id=266 count=9", EMPTY),
                 ("0:2:3", "CLIENT_CURSOR id=266 count=64", EMPTY),
                 ("0:0:4", "CLIENT_CURSOR id=-1 count=0", EMPTY)]
        for actions, final, returned in cases:
            with entity_peer(inventory=slots) as (port, observed):
                result, output = self.client(port, "--inventory-actions", actions)
            self.assertEqual(result.returncode, 0, output)
            self.assertIn(final, output)
            clicks = [packet for kind, packet in observed if kind == 0x0e]
            self.assertEqual(len(clicks), 1)
            self.assertEqual(clicks[0][7:], returned)
            if actions != "0:2:3":
                self.assertNotIn("CLIENT_SLOT index=1 ", output)
                self.assertNotIn("CLIENT_SLOT index=0 ", output)

    def test_slots_snapshot_before_accept_preserves_predicted_craft_cursor(self):
        slots = [EMPTY]*45
        slots[1], slots[0] = wire_slot(41, 1), wire_slot(266, 9)
        with entity_peer(inventory=slots, ack_snapshot=[EMPTY]*45) as (port, _):
            result, output = self.client(port, "--inventory-actions", "0:0:0")
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("CLIENT_CURSOR id=266 count=9", output)

    def test_close_with_cursor_and_grid_sends_close_and_clears_derived_output(self):
        slots = [EMPTY]*45
        slots[1], slots[0] = wire_slot(41, 1), wire_slot(266, 9)
        with entity_peer(inventory=slots, cursor=TOOL) as (port, observed):
            result, output = self.client(port, "--close-inventory")
        self.assertEqual(result.returncode, 0, output)
        self.assertEqual([packet for kind, packet in observed if kind == 0x0d], [b"\0"])
        self.assertIn("CLIENT_CURSOR id=-1 count=0", output)
        self.assertNotIn("CLIENT_SLOT index=1 ", output)
        self.assertNotIn("CLIENT_SLOT index=0 ", output)
        for window in [0, 7, 255]:
            with entity_peer("forced_close", inventory=slots, cursor=TOOL, forced_window=window) as (port, observed):
                result, output = self.client(port)
            self.assertEqual(result.returncode, 0, output)
            self.assertEqual([packet for kind, packet in observed if kind == 0x0d], [])
            self.assertIn("CLIENT_CURSOR id=-1 count=0", output)
            # No inventory GUI is open: only the global cursor is cleared.
            self.assertIn("CLIENT_SLOT index=1 id=41 count=1", output)
            self.assertIn("CLIENT_SLOT index=0 id=266 count=9", output)

    def test_server_forced_close_cancels_pending_and_cleans_open_player_grid(self):
        slots = [EMPTY]*45
        slots[1], slots[0] = wire_slot(41, 1), wire_slot(266, 9)
        for window in [0, 7, 255]:
            with entity_peer("forced_pending", inventory=slots, cursor=TOOL, forced_window=window) as (port, observed):
                result, output = self.client(port, "--inventory-actions", "9:0:0")
            self.assertEqual(result.returncode, 0, output)
            self.assertEqual([packet for kind, packet in observed if kind == 0x0d], [])
            self.assertIn("inventory_pending=0", output)
            self.assertIn("CLIENT_CURSOR id=-1 count=0", output)
            self.assertIn("CLIENT_SLOT index=9 id=276 count=1", output)
            self.assertNotIn("CLIENT_SLOT index=1 ", output)
            self.assertNotIn("CLIENT_SLOT index=0 ", output)

    def test_outside_cursor_drop_and_normal_inventory_drop(self):
        with entity_peer(cursor=TOOL) as (port, observed):
            result, output = self.client(port, "--inventory-actions", "-999:1:0")
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("CLIENT_CURSOR id=-1 count=0", output)
        click = next(packet for kind, packet in observed if kind == 0x0e)
        self.assertEqual(struct.unpack_from(">h", click, 1)[0], -999)
        self.assertEqual(click[7:], EMPTY)
        slots = [EMPTY]*45
        slots[9] = TOOL
        with entity_peer(inventory=slots) as (port, observed):
            result, output = self.client(port, "--inventory-actions", "9:0:4")
        self.assertEqual(result.returncode, 0, output)
        self.assertNotIn("CLIENT_SLOT index=9 ", output)
        self.assertEqual(next(packet for kind, packet in observed if kind == 0x0e)[7:], EMPTY)

    def test_left_right_creative_drag_sequences_preserve_counts(self):
        for base, placed, remaining in [(0, 4, 0), (4, 1, 6), (8, 64, 0)]:
            script = f"-999:{base}:5,9:{base+1}:5,10:{base+1}:5,-999:{base+2}:5"
            with entity_peer(cursor=wire_slot(1, 8)) as (port, observed):
                result, output = self.client(port, "--inventory-actions", script)
            self.assertEqual(result.returncode, 0, output)
            self.assertIn(f"CLIENT_SLOT index=9 id=1 count={placed}", output)
            self.assertIn(f"CLIENT_SLOT index=10 id=1 count={placed}", output)
            self.assertIn(f"CLIENT_CURSOR id={1 if remaining else -1} count={remaining}", output)
            clicks = [packet for kind, packet in observed if kind == 0x0e]
            self.assertEqual([packet[3] for packet in clicks], [base, base+1, base+1, base+2])
            self.assertTrue(all(packet[6] == 5 and packet[7:] == EMPTY for packet in clicks))

    def test_real_server_craft_close_q_transfers_items_into_world(self):
        import gzip
        with tempfile.TemporaryDirectory(prefix="c919-client-item-") as directory:
            world = Path(directory)/"world.c919"
            path = player_file(world, "ClientCraft")
            path.parent.mkdir()
            def stored(index, resource, count):
                return compound(named(1, "Slot", bytes([index])), named(8, "id", nbt_string(resource)),
                                named(1, "Count", bytes([count])), named(2, "Damage", bytes(2)))
            data = named(10, "", compound(
                named(9, "Inventory", b"\x0a"+struct.pack(">i", 1)+stored(0, "minecraft:stone", 3)),
                named(9, "C919Crafting", b"\x0a"+struct.pack(">i", 1)+stored(1, "minecraft:gold_block", 1)),
                named(3, "SelectedItemSlot", bytes(4))))
            path.write_bytes(gzip.compress(data))
            with running_server(world) as port:
                witness = InventoryPeer(port)
                try:
                    witness.login("ItemWitness")
                    result, output = self.client(port, "--name", "ClientCraft", "--inventory-actions", "0:0:0",
                                                 "--close-inventory", "--drop-actions", "one,all")
                    self.assertEqual(result.returncode, 0, output)
                    self.assertIn("CLIENT_CURSOR id=-1 count=0", output)
                    self.assertNotIn("CLIENT_SLOT index=36 ", output)
                    expected = {(266, 9, 0), (1, 1, 0), (1, 2, 0)}
                    while expected:
                        packet = witness.wait(0x1c)
                        item = item_metadata(packet)
                        if item is not None:
                            expected.discard(item[1][:3])
                finally:
                    witness.close()

    def test_real_server_delay_then_pickup_updates_client_inventory_and_collects(self):
        import gzip
        with tempfile.TemporaryDirectory(prefix="c919-client-pickup-") as directory:
            world = Path(directory)/"world.c919"
            path = player_file(world, "ClientPickup")
            path.parent.mkdir()
            item = compound(named(1, "Slot", b"\0"), named(8, "id", nbt_string("minecraft:stone")),
                            named(1, "Count", b"\3"), named(2, "Damage", bytes(2)))
            data = named(10, "", compound(named(9, "Inventory", b"\x0a"+struct.pack(">i", 1)+item)))
            path.write_bytes(gzip.compress(data))
            with running_server(world) as port:
                witness = InventoryPeer(port)
                results = []
                try:
                    witness.login("PickupWitness")
                    worker = threading.Thread(target=lambda: results.append(self.client(port, "--name", "ClientPickup",
                                                                                       "--drop-actions", "all", "--run-seconds", "3.3")))
                    worker.start()
                    target, collected = None, False
                    deadline = time.monotonic()+4
                    while time.monotonic()<deadline and not collected:
                        kind, packet = witness.packet()
                        if kind == 0:
                            witness.send(0, packet)
                        elif kind == 0x0e:
                            eid, offset = read_vint(packet)
                            if packet[offset] == 2:
                                target = eid
                        elif kind == 0x18:
                            eid, offset = read_vint(packet)
                            if eid == target:
                                x, y, z = (value/32 for value in struct.unpack_from(">iii", packet, offset))
                                witness.send(6, struct.pack(">dddffB", x, y, z, 0, 0, 0))
                        elif kind == 0x0d:
                            eid, offset = read_vint(packet)
                            collector, _ = read_vint(packet, offset)
                            if eid == target:
                                self.assertEqual(collector, witness.entity)
                                collected = True
                    self.assertTrue(collected, "walking witness did not collect the delayed drop")
                    inventory, _ = witness.snapshot()
                    self.assertTrue(any(slot[:3] == (1, 3, 0) for slot in inventory))
                    worker.join(8)
                    self.assertFalse(worker.is_alive())
                    result, output = results[0]
                finally:
                    witness.close()
            self.assertEqual(result.returncode, 0, output)
            self.assertNotIn("CLIENT_SLOT index=36 ", output)
            self.assertIn("item_entities=0", output)
            # Initial getAllWatched does not clear dirty flags; the following
            # getChanged emission is a second actual S1C for the same entity.
            self.assertRegex(output, r"item_spawns=1 item_metadata=2 item_collects=1")


if __name__ == "__main__":
    unittest.main()
