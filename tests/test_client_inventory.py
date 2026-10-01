"""Independent protocol-47 peer verifies native inventory/NBT preservation."""
import contextlib
import queue
import socket
import struct
import subprocess
import threading
import time
import unittest
import zlib

from test_client import CLIENT, read_frame, send_frame
from test_multiplayer import string, vint


def named(kind, name, payload):
    encoded = name.encode("ascii")
    return bytes([kind]) + struct.pack(">H", len(encoded)) + encoded + payload


TOOL_NBT = named(10, "", named(10, "display", named(8, "Name", struct.pack(">H", 12) + b"Named sword!") + b"\0")
                 + named(11, "unknown", struct.pack(">iii", 2, -2147483648, 2147483647)) + b"\0")
TOOL = struct.pack(">hBh", 276, 1, 7) + TOOL_NBT
OVERSTACK = struct.pack(">hBhB", 1, 127, 2, 0)
EMPTY = b"\xff\xff"


@contextlib.contextmanager
def inventory_peer(malformed=False, reject=False, direct=False, delayed=False, cursor_first=False, survival=False):
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(6)
    errors = queue.Queue()
    observed = []

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
                data = bytes(8192 + 2048) + b"\xff" * 2048 + b"\1" * 256
                send_frame(sock, 0x21, struct.pack(">iiBH", 0, 0, 1, 1) + vint(len(data)) + data, True)
                slots = [EMPTY] * 45
                slots[9] = TOOL
                slots[36] = OVERSTACK
                payload = b"\0" + struct.pack(">h", 45) + b"".join(slots)
                if malformed:
                    send_frame(sock, 0x30, payload, True)
                    replacement = slots.copy()
                    replacement[9] = EMPTY
                    replacement[36] = struct.pack(">hBhB", 1, 2, 5, 0)
                    payload = (b"\0" + struct.pack(">h", 45) + b"".join(replacement))[:-1]
                send_frame(sock, 0x30, payload, True)
                send_frame(sock, 0x2f, b"\xff\xff\xff" + (EMPTY if delayed else TOOL), True)
                if direct:
                    send_frame(sock, 0x2f, b"\xfe\0\0" + TOOL, True)
                if survival:
                    send_frame(sock, 0x2b, struct.pack(">Bf", 3, 0), True)
                clicks = 0
                while True:
                    try:
                        kind, packet = read_frame(sock, True)
                    except EOFError:
                        break
                    observed.append((kind, packet))
                    if kind == 0x0e:
                        window, slot, button, action, mode = struct.unpack_from(">BhBhB", packet)
                        if delayed and clicks == 1:
                            assert (window, slot, button, mode) == (0, 10, 0, 0)
                            assert packet[7:] == EMPTY
                            send_frame(sock, 0x32, struct.pack(">BhB", 0, action, 1), True)
                            clicks += 1
                            continue
                        assert (window, slot, button, mode) == (0, 9, 0, 0)
                        assert packet[7:] == TOOL
                        clicks += 1
                        send_frame(sock, 0x32, struct.pack(">BhB", 0, action, 0 if reject or delayed else 1), True)
                        if delayed:
                            slots[9] = EMPTY
                            snapshot = b"\0" + struct.pack(">h", 45) + b"".join(slots)
                            send_frame(sock, 0x2f if cursor_first else 0x30,
                                       b"\xff\xff\xff" + TOOL if cursor_first else snapshot, True)
                            until = time.monotonic() + 0.25
                            sock.settimeout(0.025)
                            while time.monotonic() < until:
                                try:
                                    delayed_packet = read_frame(sock, True)
                                except socket.timeout:
                                    continue
                                observed.append(delayed_packet)
                                if delayed_packet[0] == 0x0e:
                                    window2, slot2, button2, action2, mode2 = struct.unpack_from(">BhBhB", delayed_packet[1])
                                    assert (window2, slot2, button2, mode2) == (0, 10, 0, 0)
                                    assert delayed_packet[1][7:] == EMPTY
                                    send_frame(sock, 0x32, struct.pack(">BhB", 0, action2, 1), True)
                                    clicks += 1
                            sock.settimeout(4)
                            send_frame(sock, 0x30 if cursor_first else 0x2f,
                                       snapshot if cursor_first else b"\xff\xff\xff" + TOOL, True)
                            # Source applies independent S30/S2F updates in
                            # arrival order; no rejected-click rollback owner.
                            send_frame(sock, 0x2f, b"\xff\xff\xff" + TOOL, True)
                        if reject:
                            send_frame(sock, 0x30, b"\0" + struct.pack(">h", 45) + b"".join(slots), True)
                            send_frame(sock, 0x2f, b"\xff\xff\xff" + TOOL, True)
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
            raise AssertionError("inventory peer did not finish")
        if not errors.empty():
            raise errors.get()


class ClientInventoryTests(unittest.TestCase):
    def client(self, port, actions=None):
        command = [str(CLIENT), "--host", "127.0.0.1", "--port", str(port), "--name", "ClientTest",
                   "--headless", "--run-seconds", "1"]
        if actions:
            command += ["--inventory-actions", actions]
        result = subprocess.run(command, capture_output=True, text=True, timeout=8)
        return result, result.stdout + result.stderr

    def test_inventory_and_cursor_keep_exact_unknown_nbt_and_overstack(self):
        with inventory_peer() as (port, observed):
            result, output = self.client(port)
            self.assertEqual(result.returncode, 0, output)
            self.assertRegex(output, r"CLIENT_SLOT index=9 id=276 count=1 damage=7 nbt_size=%d nbt_crc=%08x" %
                             (len(TOOL_NBT), zlib.crc32(TOOL_NBT)))
            self.assertRegex(output, r"CLIENT_SLOT index=36 id=1 count=127 damage=2 nbt_size=0")
            self.assertRegex(output, r"CLIENT_CURSOR id=276 count=1 damage=7")
        self.assertFalse(any(kind == 0x10 for kind, _ in observed), "joining must not overwrite server hotbar")

    def test_rejected_click_ack_and_server_resync_are_applied(self):
        with inventory_peer(reject=True) as (port, observed):
            result, output = self.client(port, "9:0:0")
            self.assertEqual(result.returncode, 0, output)
            self.assertRegex(output, r"CLIENT_SLOT index=9 id=276 count=1 damage=7")
            self.assertRegex(output, r"inventory_rejections=1")
        self.assertTrue(any(kind == 0x0f and len(packet) == 4 and packet[3] == 1
                            for kind, packet in observed), "rejected transaction requires C0F acknowledgement")

    def test_truncated_window_items_disconnects_without_partial_commit(self):
        with inventory_peer(malformed=True) as (port, _):
            result, output = self.client(port)
            self.assertNotEqual(result.returncode, 0, output)
            self.assertRegex(output, r"Malformed")
            self.assertRegex(output, r"CLIENT_SLOT index=9 id=276 count=1 damage=7 nbt_size=%d nbt_crc=%08x" %
                             (len(TOOL_NBT), zlib.crc32(TOOL_NBT)))
            self.assertRegex(output, r"CLIENT_SLOT index=36 id=1 count=127 damage=2")

    def test_original_handler_ignores_window_minus_two(self):
        with inventory_peer(direct=True) as (port, _):
            result, output = self.client(port)
            self.assertEqual(result.returncode, 0, output)
            self.assertRegex(output, r"CLIENT_SLOT index=36 id=1 count=127 damage=2")

    def test_invalid_script_is_rejected_before_connecting(self):
        for actions in ["9:0:0,", "999999999999999999999:0:0", "9:0:0,,10:0:0"]:
            result = subprocess.run([str(CLIENT), "--headless", "--inventory-actions", actions],
                                    capture_output=True, text=True, timeout=3)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)

    def test_rejection_does_not_block_prediction_between_server_updates(self):
        for cursor_first in [False, True]:
            with inventory_peer(delayed=True, cursor_first=cursor_first) as (port, observed):
                result, output = self.client(port, "9:0:0,10:0:0")
                self.assertEqual(result.returncode, 0, output)
                self.assertRegex(output, r"CLIENT_CURSOR id=276 count=1 damage=7")
            self.assertEqual(sum(kind == 0x0e for kind, _ in observed), 2)

    def test_server_gamemode_change_revokes_creative_clone(self):
        with inventory_peer(survival=True) as (port, observed):
            result, output = self.client(port, "9:2:3")
            self.assertEqual(result.returncode, 0, output)
            self.assertRegex(output, r"gamemode=0")
        self.assertFalse(any(kind == 0x0e for kind, _ in observed), "creative clone must follow server gamemode")


if __name__ == "__main__":
    unittest.main(verbosity=2)
