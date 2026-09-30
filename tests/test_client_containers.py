"""Independent compressed protocol47 workbench peer and real server integration."""
import contextlib
import gzip
import math
from pathlib import Path
import tempfile
import zlib
import queue
import socket
import struct
import subprocess
import threading
import unittest

from test_client import CLIENT, read_frame, send_frame
from test_client_inventory import EMPTY, TOOL, TOOL_NBT
from test_inventory_network import wire_slot, player_file, compound, named, nbt_string, parse_slot
from test_workbench_network import WorkbenchPeer
from test_multiplayer import running_server, position, read_vint
from test_multiplayer import string, vint


def open_window(window=7, kind="minecraft:crafting_table", count=0, title='{"text":"日本の作業台"}'):
    return bytes([window])+string(kind)+string(title)+bytes([count])


@contextlib.contextmanager
def container_peer(kind="snapshot", slots=None, cursor=EMPTY, window=7, bad_open=None):
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(6)
    observed, errors = [], queue.Queue()
    provided = [EMPTY]*46 if slots is None else slots.copy()
    if kind == "map_use_table":
        provided[37] = wire_slot(395, 2)

    def serve():
        try:
            sock, _ = listener.accept()
            with sock:
                sock.settimeout(4)
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                assert read_frame(sock)[0] == 0
                assert read_frame(sock)[0] == 0
                send_frame(sock, 3, vint(64))
                send_frame(sock, 2, string("00000000-0000-3000-8000-000000000001")+string("ContainerTest"), True)
                send_frame(sock, 1, struct.pack(">iBbBB", 1, 0 if kind == "use" else 3 if kind == "map_use_spectator" else 1, 0, 0, 8)+string("default")+b"\0", True)
                send_frame(sock, 8, struct.pack(">dddffB", 8.5, 7, 8.5, 0, 0, 0), True)
                states = bytearray(8192)
                if kind in ["use", "map_use_ground", "map_use_table"]:
                    for z in range(16):
                        for x in range(16):
                            struct.pack_into("<H", states, ((6<<8)|(z<<4)|x)*2, 16)
                    if kind != "map_use_ground":
                        struct.pack_into("<H", states, ((6<<8)|(10<<4)|8)*2, 58<<4)
                data = bytes(states)+bytes(2048)+b"\xff"*2048+b"\1"*256
                send_frame(sock, 0x21, struct.pack(">iiBH", 0, 0, 1, 1)+vint(len(data))+data, True)
                initial = [EMPTY]*45
                if kind.startswith("map_use"):
                    initial[36] = wire_slot(395, 2)
                send_frame(sock, 0x30, b"\0"+struct.pack(">h", 45)+b"".join(initial), True)
                if kind == "use" or kind.startswith("map_use"):
                    pass
                elif kind == "horse":
                    send_frame(sock, 0x2d, open_window(window, "EntityHorse", 2)+struct.pack(">i", -2147483648), True)
                else:
                    send_frame(sock, 0x2d, open_window(window), True)
                    # Seed the global cursor before a potentially large window
                    # snapshot enables scripted clicks. This avoids a late
                    # initial cursor packet overwriting the tested prediction.
                    send_frame(sock, 0x2f, b"\xff\xff\xff"+cursor, True)
                    send_frame(sock, 0x30, bytes([window])+struct.pack(">h", 46)+b"".join(provided), True)
                    if kind == "snapshot":
                        send_frame(sock, 0x2f, struct.pack(">bh", 0, 36)+wire_slot(1, 11), True)
                        send_frame(sock, 0x2f, struct.pack(">Bh", window, 45)+TOOL, True)
                    elif kind == "truncated":
                        send_frame(sock, 0x30, bytes([window])+struct.pack(">h", 46)+EMPTY*45+TOOL[:-2], True)
                    elif kind == "bad_open":
                        send_frame(sock, 0x2d, (open_window(8)+b"\0" if bad_open is None else bad_open), True)
                    elif kind == "stale":
                        send_frame(sock, 0x2d, open_window(8), True)
                        send_frame(sock, 0x30, b"\10"+struct.pack(">h", 46)+EMPTY*46, True)
                        send_frame(sock, 0x2f, struct.pack(">Bh", window, 1)+TOOL, True)
                    elif kind in ["map", "map_bad"]:
                        pixels = bytes([4, 5, 6, 7])
                        send_frame(sock, 0x34, vint(23)+b"\2"+vint(1)+b"\x07\0\0"+b"\2\2\3\4"+vint(4)+pixels, True)
                        if kind == "map_bad":
                            send_frame(sock, 0x34, vint(23)+b"\3"+vint(0)+b"\2\2\3\4"+vint(4)+pixels[:-1], True)
                    elif kind == "aggregate":
                        large = named(10, "", named(7, "foreign", struct.pack(">i", 1100000)+bytes(1100000))+b"\0")
                        item = wire_slot(387, 1, tag=large)
                        send_frame(sock, 0x2f, struct.pack(">Bh", window, 1)+item, True)
                        send_frame(sock, 0x2f, struct.pack(">Bh", window, 2)+item, True)
                    elif kind == "forced":
                        send_frame(sock, 0x2e, b"\xff", True)
                while True:
                    try:
                        packet_id, packet = read_frame(sock, True)
                    except (EOFError, ConnectionResetError):
                        break
                    observed.append((packet_id, packet))
                    if packet_id == 0x08 and (kind == "use" or kind == "map_use_table"):
                        send_frame(sock, 0x2d, open_window(window), True)
                        send_frame(sock, 0x30, bytes([window])+struct.pack(">h", 46)+b"".join(provided), True)
                        send_frame(sock, 0x2f, b"\xff\xff\xff"+cursor, True)
                    if packet_id == 0x08 and kind.startswith("map_use") and packet[8] == 255:
                        send_frame(sock, 0x2f, b"\0"+struct.pack(">h", 36)+wire_slot(358, 1, 23), True)
                        send_frame(sock, 0x34, vint(23)+b"\0"+vint(0)+b"\1\1\0\0"+vint(1)+b"\5", True)
                    if packet_id == 0x0e:
                        action = struct.unpack_from(">h", packet, 4)[0]
                        if kind.startswith("backup_"):
                            if kind != "backup_valid":
                                send_frame(sock, 0x2f, b"\xff\xff\xff"+EMPTY, True)
                            initial = [EMPTY]*45
                            if kind != "backup_valid":
                                initial[36] = provided[1]
                            if kind == "backup_slot":
                                send_frame(sock, 0x2f, b"\0"+struct.pack(">h", 36)+provided[1], True)
                            else:
                                send_frame(sock, 0x30, b"\0"+struct.pack(">h", 45)+b"".join(initial), True)
                            send_frame(sock, 0x32, struct.pack(">BhB", window, action, kind == "backup_valid"), True)
                            continue
                        if kind == "reuse" and action == 1:
                            send_frame(sock, 0x2d, open_window(window), True)
                            send_frame(sock, 0x30, bytes([window])+struct.pack(">h", 46)+b"".join(provided), True)
                            send_frame(sock, 0x2f, b"\xff\xff\xff"+cursor, True)
                            send_frame(sock, 0x32, struct.pack(">BhB", window, action, 1), True)
                            continue
                        if kind.startswith("reject"):
                            send_frame(sock, 0x32, struct.pack(">BhB", window, action, 0), True)
                            if kind == "reject_cursor_first":
                                send_frame(sock, 0x2f, b"\xff\xff\xff"+cursor, True)
                            if kind != "reject_cursor_only":
                                send_frame(sock, 0x30, bytes([window])+struct.pack(">h", 46)+b"".join(provided), True)
                            if kind not in ["reject_cursor_first", "reject_slots_only"]:
                                send_frame(sock, 0x2f, b"\xff\xff\xff"+cursor, True)
                        else:
                            send_frame(sock, 0x32, struct.pack(">BhB", window, action, 1), True)
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
            raise AssertionError("container peer did not finish")
        if not errors.empty():
            raise errors.get()


class ClientContainerTests(unittest.TestCase):
    def client(self, port, *options):
        result = subprocess.run([str(CLIENT), "--host", "127.0.0.1", "--port", str(port),
                                 "--headless", "--run-seconds", "0.7", *options],
                                capture_output=True, text=True, timeout=8)
        return result, result.stdout+result.stderr

    def test_workbench_snapshot_maps_shared_player_and_hotbar(self):
        slots = [EMPTY]*46
        slots[1], slots[10], slots[37] = wire_slot(5, 2), TOOL, wire_slot(266, 3)
        with container_peer(slots=slots) as (port, observed):
            result, output = self.client(port)
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("CLIENT_WINDOW id=7 ready=1 slots=46", output)
        self.assertIn("CLIENT_CONTAINER_SLOT index=1 id=5 count=2", output)
        self.assertIn("CLIENT_SLOT index=9 id=276 count=1", output)
        self.assertIn("CLIENT_SLOT index=36 id=1 count=11", output)
        self.assertIn("CLIENT_SLOT index=44 id=276 count=1", output)
        self.assertFalse(any(kind == 0x0d for kind, _ in observed))

    def test_click_and_close_use_current_window_and_global_cursor(self):
        slots = [EMPTY]*46
        slots[10] = TOOL
        with container_peer("click", slots=slots) as (port, observed):
            result, output = self.client(port, "--inventory-actions", "10:0:0", "--close-inventory")
        self.assertEqual(result.returncode, 0, output)
        clicks = [data for kind, data in observed if kind == 0x0e]
        self.assertEqual(len(clicks), 1)
        self.assertEqual(clicks[0][0], 7)
        self.assertEqual(clicks[0][7:], TOOL)
        self.assertEqual([data for kind, data in observed if kind == 0x0d], [b"\7"])
        self.assertIn("CLIENT_CURSOR id=-1 count=0", output)

    def test_rejection_ack_targets_external_window(self):
        slots = [EMPTY]*46
        slots[10] = TOOL
        with container_peer("reject", slots=slots) as (port, observed):
            result, output = self.client(port, "--inventory-actions", "10:0:0")
        self.assertEqual(result.returncode, 0, output)
        self.assertEqual([data for kind, data in observed if kind == 0x0f], [struct.pack(">BhB", 7, 1, 1)])
        self.assertIn("inventory_rejections=1 inventory_pending=0", output)
        self.assertIn("CLIENT_SLOT index=9 id=276 count=1", output)
        self.assertIn("CLIENT_CURSOR id=-1 count=0", output)

    def test_atomic_snapshot_and_open_window_parse(self):
        slots = [EMPTY]*46
        slots[1] = TOOL
        for kind in ["truncated", "bad_open"]:
            with container_peer(kind, slots=slots) as (port, _):
                result, output = self.client(port)
            self.assertNotEqual(result.returncode, 0, output)
            self.assertIn("CLIENT_WINDOW id=7 ready=1 slots=46", output)
            self.assertIn("CLIENT_CONTAINER_SLOT index=1 id=276 count=1", output)

    def test_stale_window_update_and_horse_tail_and_server_forced_close(self):
        with container_peer("stale") as (port, _):
            result, output = self.client(port)
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("CLIENT_WINDOW id=8 ready=1 slots=46", output)
        self.assertNotIn("CLIENT_CONTAINER_SLOT index=1 ", output)
        with container_peer("horse") as (port, observed):
            result, output = self.client(port)
        self.assertEqual(result.returncode, 0, output)
        self.assertEqual([data for kind, data in observed if kind == 0x0d], [b"\7"])
        with container_peer("forced", cursor=TOOL) as (port, observed):
            result, output = self.client(port)
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("CLIENT_WINDOW id=0", output)
        self.assertIn("CLIENT_CURSOR id=-1 count=0", output)
        self.assertFalse(any(kind == 0x0d for kind, _ in observed))

    def test_all_workbench_cells_drag_real_packets_and_conserve_items(self):
        actions = "-999:0:5,"+",".join(f"{i}:1:5" for i in range(1, 46))+",-999:2:5"
        with container_peer("click", cursor=wire_slot(1, 90)) as (port, observed):
            result, output = self.client(port, "--run-seconds", "3.2", "--inventory-actions", actions)
        self.assertEqual(result.returncode, 0, output)
        clicks = [data for kind, data in observed if kind == 0x0e]
        self.assertEqual(len(clicks), 47)
        self.assertEqual([struct.unpack_from(">h", c, 1)[0] for c in clicks], [-999, *range(1, 46), -999])
        self.assertTrue(all(c[0] == 7 and c[6] == 5 and c[7:] == EMPTY for c in clicks))
        self.assertIn("CLIENT_CURSOR id=-1 count=0", output)
        self.assertEqual(output.count("id=1 count=2"), 45)

    def test_workbench_output_crafts_three_by_three_without_snapshot(self):
        slots = [EMPTY]*46
        slots[0] = wire_slot(54, 1)
        for i in [1, 2, 3, 4, 6, 7, 8, 9]:
            slots[i] = wire_slot(5, 1)
        with container_peer("click", slots=slots) as (port, observed):
            result, output = self.client(port, "--inventory-actions", "0:0:0")
        self.assertEqual(result.returncode, 0, output)
        self.assertEqual(next(c for k, c in observed if k == 0x0e)[7:], wire_slot(54, 1))
        self.assertIn("CLIENT_CURSOR id=54 count=1", output)
        self.assertNotIn("CLIENT_CONTAINER_SLOT index=", output)

    def test_workbench_number_key_relocates_hotbar_item_and_keeps_overstack(self):
        slots = [EMPTY]*46
        slots[1], slots[37], slots[38] = wire_slot(1, 5), wire_slot(5, 10), wire_slot(5, 30)
        with container_peer("click", slots=slots) as (port, observed):
            result, output = self.client(port, "--inventory-actions", "1:0:2")
        self.assertEqual(result.returncode, 0, output)
        self.assertEqual(next(c for k, c in observed if k == 0x0e)[7:], EMPTY)
        self.assertNotIn("CLIENT_CONTAINER_SLOT index=1 ", output)
        self.assertIn("CLIENT_SLOT index=36 id=1 count=5", output)
        self.assertIn("CLIENT_SLOT index=37 id=5 count=40", output)
        slots = [EMPTY]*46
        slots[37] = wire_slot(1, 127)
        with container_peer("click", slots=slots) as (port, _):
            result, output = self.client(port, "--inventory-actions", "1:0:2")
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("CLIENT_CONTAINER_SLOT index=1 id=1 count=127", output)
        self.assertNotIn("CLIENT_SLOT index=36 ", output)

    def test_unknown_map_preview_survives_and_crafts_without_allocating_map(self):
        marker = named(10, "", named(1, "map_is_scaling", b"\1")+b"\0")
        slots = [wire_slot(358, 1, 23, marker)]+[wire_slot(339, 1)]*9+[EMPTY]*36
        slots[5] = wire_slot(358, 1, 23)
        with container_peer("map", slots=slots) as (port, observed):
            result, output = self.client(port, "--inventory-actions", "0:0:0")
        self.assertEqual(result.returncode, 0, output)
        self.assertEqual(next(c for k, c in observed if k == 0x0e)[7:], slots[0])
        self.assertIn("CLIENT_CURSOR id=358 count=1 damage=23", output)
        self.assertNotIn("CLIENT_CONTAINER_SLOT index=", output)
        colors = bytearray(16384)
        colors[3+4*128:5+4*128] = b"\4\5"
        colors[3+5*128:5+5*128] = b"\6\7"
        self.assertIn(f"CLIENT_MAP id=23 scale=2 icons=1 metadata_known=0 pixels_crc={zlib.crc32(colors):08x}", output)

    def test_malformed_map_and_aggregate_nbt_updates_are_atomic(self):
        with container_peer("map_bad") as (port, _):
            result, output = self.client(port)
        self.assertNotEqual(result.returncode, 0, output)
        self.assertIn("CLIENT_MAP id=23 scale=2 icons=1", output)
        with container_peer("aggregate") as (port, _):
            result, output = self.client(port)
        self.assertNotEqual(result.returncode, 0, output)
        self.assertIn("CLIENT_CONTAINER_SLOT index=1 id=387 count=1", output)
        self.assertNotIn("CLIENT_CONTAINER_SLOT index=2 ", output)

    def test_pending_authoritative_nbt_budget_is_atomic_for_slot_and_snapshot(self):
        large = named(10, "", named(7, "foreign", struct.pack(">i", 1100000)+bytes(1100000))+b"\0")
        slots = [EMPTY]*46
        slots[1] = wire_slot(387, 1, tag=large)
        for kind in ["backup_slot", "backup_snapshot"]:
            with container_peer(kind, slots=slots) as (port, _):
                result, output = self.client(port, "--inventory-actions", "1:0:0")
            self.assertNotEqual(result.returncode, 0, output)
            self.assertNotIn("CLIENT_SLOT index=36 ", output)
            self.assertIn("CLIENT_CURSOR id=-1 count=0", output)
        # Window0 supplies no cursor and must preserve the distinct backup
        # cursor when the current workbench prediction owns the large item.
        with container_peer("backup_valid", slots=slots) as (port, _):
            result, output = self.client(port, "--inventory-actions", "1:0:0")
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("CLIENT_CURSOR id=387 count=1", output)
        self.assertIn("inventory_pending=0", output)
        self.assertNotIn("CLIENT_CONTAINER_SLOT index=1 ", output)

    def test_rejection_requires_both_window_and_cursor_snapshot_in_either_order(self):
        slots = [EMPTY]*46
        slots[10] = TOOL
        for kind, sync in [("reject_cursor_first", 0), ("reject_cursor_only", 1), ("reject_slots_only", 1)]:
            with container_peer(kind, slots=slots) as (port, observed):
                result, output = self.client(port, "--inventory-actions", "10:0:0,10:0:0")
            self.assertEqual(result.returncode, 0, output)
            self.assertIn(f"sync={sync}", output)
            self.assertEqual(sum(k == 0x0e for k, _ in observed), 2 if sync == 0 else 1)
            self.assertTrue(all(c[0] == 7 for k, c in observed if k == 0x0f))

    def test_window_id_reuse_and_high_unsigned_id(self):
        slots = [EMPTY]*46
        slots[10] = TOOL
        with container_peer("reuse", slots=slots) as (port, observed):
            result, output = self.client(port, "--inventory-actions", "10:0:0,10:0:0")
        self.assertEqual(result.returncode, 0, output)
        clicks = [c for k, c in observed if k == 0x0e]
        self.assertEqual([struct.unpack_from(">h", c, 4)[0] for c in clicks], [1, 2])
        self.assertIn("inventory_pending=0", output)
        self.assertIn("CLIENT_CURSOR id=276 count=1", output)
        with container_peer("snapshot", window=200, slots=slots) as (port, _):
            result, output = self.client(port)
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("CLIENT_WINDOW id=200 ready=1 slots=46", output)
        self.assertIn("CLIENT_SLOT index=44 id=276 count=1", output)

    def test_open_window_malformed_json_horse_tail_and_type_length_rollback(self):
        slots = [EMPTY]*46
        slots[1] = TOOL
        cases = [open_window(8, title='{'), open_window(8, title='["x",]'),
                 open_window(8, title='["'+'x'*10+'"'),
                 open_window(8, title='['*33+'0'+']'*33),
                 open_window(8, kind="x"*33), open_window(8, "EntityHorse", 2),
                 open_window(8, "EntityHorse", 2)+b"\0\0\0"]
        for bad in cases:
            with container_peer("bad_open", slots=slots, bad_open=bad) as (port, _):
                result, output = self.client(port)
            self.assertNotEqual(result.returncode, 0, output)
            self.assertIn("CLIENT_WINDOW id=7 ready=1 slots=46", output)
            self.assertIn("CLIENT_CONTAINER_SLOT index=1 id=276 count=1", output)

    def test_survival_empty_hand_use_sends_actual_raycast_before_inventory_click(self):
        with container_peer("use") as (port, observed):
            result, output = self.client(port, "--use-block", "8,6,10", "--inventory-actions", "10:0:0")
        self.assertEqual(result.returncode, 0, output)
        use = next(c for k, c in observed if k == 0x08)
        self.assertEqual(use[:8], position(8, 6, 10))
        self.assertIn(use[8], range(6))
        self.assertEqual(use[9:11], EMPTY)
        first_use = next(i for i, (k, _) in enumerate(observed) if k == 0x08)
        first_click = next(i for i, (k, _) in enumerate(observed) if k == 0x0e)
        self.assertLess(first_use, first_click)
        self.assertIn("CLIENT_WINDOW id=7 ready=1 slots=46", output)

    def test_native_empty_map_use_air_ground_fallback_and_workbench_activation(self):
        for kind, target, faces in [("map_use_air", "8,8,10", [255]),
                                    ("map_use_ground", "8,6,10", [1, 255]),
                                    ("map_use_table", "8,6,10", [1]),
                                    ("map_use_spectator", "8,8,10", [])]:
            with container_peer(kind) as (port, observed):
                result, output = self.client(port, "--use-block", target)
            self.assertEqual(result.returncode, 0, output)
            uses = [p for k, p in observed if k == 0x08]
            self.assertEqual([p[8] for p in uses], faces)
            self.assertTrue(all(p[9:-3] == wire_slot(395, 2) for p in uses))
            if kind == "map_use_spectator":
                self.assertIn("CLIENT_SLOT index=36 id=395 count=2", output)
                self.assertNotIn("CLIENT_MAP id=", output)
            elif kind == "map_use_table":
                self.assertIn("CLIENT_WINDOW id=7 ready=1 slots=46", output)
                self.assertIn("CLIENT_SLOT index=36 id=395 count=2", output)
            else:
                self.assertEqual(uses[-1][:8], b"\xff"*8)
                self.assertEqual(uses[-1][-3:], bytes(3))
                self.assertIn("CLIENT_SLOT index=36 id=358 count=1 damage=23", output)
                self.assertIn("CLIENT_MAP id=23 scale=0", output)

    def test_real_server_native_empty_map_create_receives_held_pixels(self):
        with tempfile.TemporaryDirectory(prefix="c919-client-map-") as directory:
            world = Path(directory)/"world.c919"
            saved = player_file(world, "NativeMap")
            saved.parent.mkdir()
            blank = compound(named(1, "Slot", b"\0"), named(8, "id", nbt_string("minecraft:map")),
                             named(1, "Count", b"\1"), named(2, "Damage", bytes(2)))
            saved.write_bytes(gzip.compress(named(10, "", compound(named(9, "Inventory", b"\x0a"+struct.pack(">i", 1)+blank)))))
            with running_server(world) as port:
                result, output = self.client(port, "--name", "NativeMap", "--use-block", "8,20,10", "--run-seconds", "1.1")
            self.assertEqual(result.returncode, 0, output)
            self.assertIn("CLIENT_SLOT index=36 id=358 count=1 damage=0", output)
            self.assertIn("CLIENT_MAP id=0 scale=0", output)
            self.assertIn("metadata_known=0", output)

    def test_real_server_empty_hand_use_three_by_three_craft_and_close_drop(self):
        with tempfile.TemporaryDirectory(prefix="c919-client-table-") as directory:
            world = Path(directory)/"world.c919"
            saved = player_file(world, "NativeTable")
            saved.parent.mkdir()
            wood = compound(named(1, "Slot", b"\x09"), named(8, "id", nbt_string("minecraft:planks")),
                            named(1, "Count", b"\x08"), named(2, "Damage", bytes(2)))
            saved.write_bytes(gzip.compress(named(10, "", compound(named(9, "Inventory", b"\x0a"+struct.pack(">i", 1)+wood)))))
            with running_server(world) as port:
                witness = WorkbenchPeer(port).login("TableWitness")
                try:
                    table = (8, math.floor(witness.spawn[1])-1, 10)
                    witness.creative(36, 58, 1)
                    witness.send(8, position(table[0], table[1]-1, table[2])+b"\1"+wire_slot(58, 1)+b"\x08\x10\x08")
                    witness.wait(0x23, lambda p: p[:8] == position(*table))
                    witness.creative(36)
                    script = "10:0:0,"+",".join(f"{i}:1:0" for i in [1, 2, 3, 4, 6, 7, 8, 9])+",0:0:0"
                    result, output = self.client(port, "--name", "NativeTable", "--use-block", ",".join(map(str, table)),
                                                 "--inventory-actions", script, "--close-inventory", "--run-seconds", "1.5")
                    self.assertEqual(result.returncode, 0, output)
                    self.assertIn("inventory_rejections=0 inventory_pending=0", output)
                    self.assertIn("CLIENT_WINDOW id=0", output)
                    self.assertIn("CLIENT_CURSOR id=-1 count=0", output)
                    self.assertNotIn("CLIENT_SLOT index=9 id=5", output)
                    while True:
                        packet = witness.wait(0x1c)
                        _, offset = read_vint(packet)
                        if packet[offset] == 0xaa and parse_slot(packet, offset+1)[0][:3] == (54, 1, 0):
                            break
                finally:
                    witness.close()

    def test_use_block_cli_rejects_invalid_targets_before_network(self):
        for value in ["1,2", "1,2,3,4", "1,256,3", "1,-1,3", "30000000,1,3", "1,2,nan", "1,2,3x"]:
            result = subprocess.run([str(CLIENT), "--headless", "--use-block", value], capture_output=True, text=True, timeout=3)
            self.assertEqual(result.returncode, 2, result.stdout+result.stderr)
            self.assertIn("Invalid block-use target", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
