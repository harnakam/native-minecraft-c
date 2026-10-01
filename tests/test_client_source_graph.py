"""Independent TCP contracts for source-owned client prediction and signed stacks."""
import contextlib
import queue
import socket
import struct
import subprocess
import threading
import unittest

from test_client import CLIENT, read_frame, send_frame
from test_client_inventory import EMPTY, TOOL, TOOL_NBT
from test_multiplayer import string, vint


@contextlib.contextmanager
def source_peer(kind):
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
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                assert read_frame(sock)[0] == 0
                assert read_frame(sock)[0] == 0
                send_frame(sock, 3, vint(64))
                send_frame(sock, 2, string("00000000-0000-3000-8000-000000000001") + string("SourceClient"), True)
                send_frame(sock, 1, struct.pack(">iBbBB", 1, 1, 0, 0, 8) + string("default") + b"\0", True)
                send_frame(sock, 8, struct.pack(">dddffB", 8.5, 7, 8.5, 0, 0, 0), True)
                data = bytes(8192 + 2048) + b"\xff" * 2048 + b"\1" * 256
                send_frame(sock, 0x21, struct.pack(">iiBH", 0, 0, 1, 1) + vint(len(data)) + data, True)
                slots = [EMPTY] * 45
                slots[9] = TOOL
                slots[36] = TOOL
                if kind == "signed":
                    slots[36] = struct.pack(">hbhB", 276, 0, -7, 0)
                    slots[37] = struct.pack(">hbhB", 1, -128, 0, 0)
                    slots[38] = struct.pack(">hbhB", 32767, -1, -3, 0)
                send_frame(sock, 0x30, b"\0" + struct.pack(">h", 45) + b"".join(slots), True)
                if kind == "prefix":
                    send_frame(sock, 0x30, b"\0" + struct.pack(">h", 10) + EMPTY * 10, True)
                if kind == "cursor_index":
                    send_frame(sock, 0x2f, struct.pack(">bh", -1, 123) + TOOL, True)
                clicks = 0
                while True:
                    try:
                        packet_id, packet = read_frame(sock, True)
                    except (EOFError, ConnectionResetError):
                        break
                    observed.append((packet_id, packet))
                    if packet_id == 0x0e:
                        window, slot, button, action, mode = struct.unpack_from(">BhBhB", packet)
                        assert (window, button, mode) == (0, 0, 0)
                        assert slot == (9 if clicks == 0 else 10)
                        assert packet[7:] == (TOOL if clicks == 0 else EMPTY)
                        clicks += 1
                        # No confirmation is sent for the first action. The
                        # source client must execute the second prediction.
                        if clicks == 2:
                            send_frame(sock, 0x32, struct.pack(">BhB", 0, 900, 0), True)
                    if kind == "close" and packet_id == 0x0d:
                        assert packet == b"\0"
        except Exception as exc:
            errors.put(exc)

    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    try:
        yield listener.getsockname()[1], observed
    finally:
        worker.join(6)
        listener.close()
        if worker.is_alive():
            raise AssertionError("source client peer did not finish")
        if not errors.empty():
            raise errors.get()


class SourceGraphClientTests(unittest.TestCase):
    def run_client(self, port, *args):
        result = subprocess.run([str(CLIENT), "--host", "127.0.0.1", "--port", str(port),
                                 "--name", "SourceClient", "--headless", "--run-seconds", "1",
                                 *args], capture_output=True, text=True, timeout=9)
        output = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, output)
        return output

    def test_clicks_do_not_wait_for_ack_and_rejection_does_not_roll_back(self):
        with source_peer("clicks") as (port, packets):
            output = self.run_client(port, "--inventory-actions", "9:0:0,10:0:0")
        self.assertEqual(sum(k == 0x0e for k, _ in packets), 2)
        self.assertIn((0x0f, struct.pack(">BhB", 0, 900, 1)), packets)
        self.assertIn("CLIENT_SLOT index=10 id=276 count=1 damage=7", output)
        self.assertNotIn("CLIENT_SLOT index=9 ", output)
        self.assertIn("CLIENT_CURSOR id=-1 count=0", output)

    def test_close_screen_sends_close_without_waiting_for_click_confirmation(self):
        with source_peer("close") as (port, packets):
            output = self.run_client(port, "--inventory-actions", "9:0:0", "--close-inventory")
        self.assertTrue(any(k == 0x0e for k, _ in packets))
        self.assertIn((0x0d, b"\0"), packets)
        self.assertIn("CLIENT_CURSOR id=-1 count=0", output)

    def test_sp_drop_only_sends_packet_until_server_slot_update(self):
        with source_peer("drop") as (port, packets):
            output = self.run_client(port, "--drop-actions", "one,all")
        drops = [p for k, p in packets if k == 7]
        self.assertEqual(drops, [vint(4) + bytes(9), vint(3) + bytes(9)])
        self.assertIn("CLIENT_SLOT index=36 id=276 count=1 damage=7", output)

    def test_nonnull_zero_negative_and_unknown_item_stacks_remain_source_objects(self):
        with source_peer("signed") as (port, _):
            output = self.run_client(port)
        self.assertIn("CLIENT_SLOT index=36 id=276 count=0 damage=0", output)
        self.assertIn("CLIENT_SLOT index=37 id=1 count=-128 damage=0", output)
        # Original Item.getIdFromItem(NULL) is 0; the printed slot occurrence
        # still distinguishes this nonnull unknown-Item stack from NULL.
        self.assertIn("CLIENT_SLOT index=38 id=0 count=-1 damage=0", output)

    def test_short_window_snapshot_updates_its_prefix_only(self):
        with source_peer("prefix") as (port, _):
            output = self.run_client(port)
        self.assertNotIn("CLIENT_SLOT index=9 ", output)
        self.assertIn("CLIENT_SLOT index=36 id=276 count=1 damage=7", output)

    def test_minus_one_window_sets_cursor_for_every_slot_index(self):
        with source_peer("cursor_index") as (port, _):
            output = self.run_client(port)
        self.assertIn("CLIENT_CURSOR id=276 count=1 damage=7", output)
        self.assertIn("nbt_size=%d" % len(TOOL_NBT), output)


if __name__ == "__main__":
    unittest.main()
