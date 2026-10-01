"""Independent TCP observations of the real client's source inventory ticks."""
import contextlib
import queue
import socket
import struct
import subprocess
import threading
import unittest
from test_client import CLIENT, read_frame, send_frame
from test_multiplayer import string, vint


@contextlib.contextmanager
def animation_peer(kind):
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(6)
    errors, packets = queue.Queue(), []

    def serve():
        try:
            sock, _ = listener.accept()
            with sock:
                sock.settimeout(4)
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                assert read_frame(sock)[0] == 0
                assert read_frame(sock)[0] == 0
                send_frame(sock, 2, string("00000000-0000-3000-8000-000000000001") + string("Animation"))
                send_frame(sock, 1, struct.pack(">iBbBB", 1, 1, 0, 0, 8) + string("default") + b"\0")
                send_frame(sock, 8, struct.pack(">dddffB", 8.5, 7, 8.5, 0, 0, 0))
                data = bytes(8192 + 2048) + b"\xff" * 2048 + b"\1" * 256
                send_frame(sock, 0x21, struct.pack(">iiBH", 0, 0, 1, 1) + vint(len(data)) + data)
                send_frame(sock, 0x30, b"\0" + struct.pack(">h", 45) + b"\xff\xff" * 45)
                values = [(36, 1, 1, 0)]
                if kind == "signed":
                    values = [(36, 276, 0, -7), (37, 1, -128, 0)]
                elif kind == "map":
                    values = [(36, 358, 1, 123)]
                elif kind == "unknown":
                    values = [(36, 32767, -1, -3)]
                for index, item, count, damage in values:
                    send_frame(sock, 0x2f, struct.pack(">bhhbhB", 0, index, item, count, damage, 0))
                while True:
                    try:
                        packets.append(read_frame(sock))
                    except (EOFError, ConnectionResetError):
                        break
        except Exception as exc:
            errors.put(exc)

    worker = threading.Thread(target=serve, daemon=True)
    worker.start()
    try:
        yield listener.getsockname()[1], packets
    finally:
        worker.join(6)
        listener.close()
        if worker.is_alive():
            raise AssertionError("animation peer did not finish")
        if not errors.empty():
            raise errors.get()


class ClientAnimationTests(unittest.TestCase):
    def run_client(self, port, *args):
        return subprocess.run([str(CLIENT), "--host", "127.0.0.1", "--port", str(port),
                               "--name", "Animation", "--headless", "--run-seconds", "0.7", *args],
                              capture_output=True, text=True, timeout=9)

    def test_real_set_slot_animation_reaches_zero_on_source_ticks(self):
        with animation_peer("ordinary") as (port, _):
            result = self.run_client(port)
        output = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, output)
        self.assertRegex(output, r"CLIENT_SLOT index=36 id=1 count=1 damage=0 .*animations=0")

    def test_zero_negative_stacks_tick_while_inventory_screen_is_open(self):
        with animation_peer("signed") as (port, packets):
            result = self.run_client(port, "--inventory-actions", "9:0:0")
        output = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, output)
        self.assertRegex(output, r"CLIENT_SLOT index=36 id=276 count=0 damage=0 .*animations=0")
        self.assertRegex(output, r"CLIENT_SLOT index=37 id=1 count=-128 damage=0 .*animations=0")
        self.assertTrue(any(kind == 0x0e for kind, _ in packets))
        self.assertIn("CLIENT_WINDOW id=0 ready=1 slots=45", output)
        self.assertFalse(any(kind == 0x0d for kind, _ in packets))

    def test_remote_map_ticks_do_not_allocate_missing_map_data_or_send_effects(self):
        with animation_peer("map") as (port, packets):
            result = self.run_client(port)
        output = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, output)
        self.assertRegex(output, r"CLIENT_SLOT index=36 id=358 count=1 damage=123 .*animations=0")
        self.assertNotIn("CLIENT_MAP ", output)
        self.assertFalse(any(kind in (0x0e, 0x08, 0x10) for kind, _ in packets))

    def test_unknown_nonnull_main_stack_fails_original_tick_instead_of_being_skipped(self):
        with animation_peer("unknown") as (port, _):
            result = self.run_client(port)
        output = result.stdout + result.stderr
        self.assertNotEqual(result.returncode, 0, output)
        self.assertIn("Source inventory animation update failed", output)
        self.assertNotIn("CLIENT_CURSOR id=-1", output)  # Failed graph is never borrowed for diagnostics.


if __name__ == "__main__":
    unittest.main()
