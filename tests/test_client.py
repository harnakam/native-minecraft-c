"""Exercise the real C client against C919 and an independent compressed wire peer."""
import contextlib
import os
from pathlib import Path
import queue
import socket
import struct
import subprocess
import tempfile
import threading
import unittest
import zlib

from test_multiplayer import Peer, read_vint, running_server, string, vint

ROOT = Path(__file__).resolve().parents[1]
CLIENT = Path(os.environ.get("C919_CLIENT", str(ROOT / "build" / "c919-client.exe")))


def exact(sock, count):
    data = bytearray()
    while len(data) < count:
        part = sock.recv(count - len(data))
        if not part:
            raise EOFError
        data.extend(part)
    return bytes(data)


def read_frame(sock, compressed=False):
    prefix = bytearray()
    for _ in range(5):
        prefix.extend(exact(sock, 1))
        if not prefix[-1] & 128:
            break
    size, _ = read_vint(prefix)
    if not 0 < size <= 2 * 1024 * 1024:
        raise ValueError("frame out of bounds")
    data = exact(sock, size)
    if compressed:
        uncompressed, offset = read_vint(data)
        data = zlib.decompress(data[offset:]) if uncompressed else data[offset:]
        if uncompressed and len(data) != uncompressed:
            raise ValueError("decompression size mismatch")
    packet_id, offset = read_vint(data)
    return packet_id, data[offset:]


def send_frame(sock, packet_id, payload=b"", compressed=False):
    data = vint(packet_id) + payload
    if compressed:
        data = vint(len(data)) + zlib.compress(data) if len(data) >= 64 else b"\0" + data
    sock.sendall(vint(len(data)) + data)


@contextlib.contextmanager
def independent_server(encrypted=False, malformed=False, remote_chunk=False, above_world=False):
    listener = socket.socket()
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(6)
    port = listener.getsockname()[1]
    errors = queue.Queue()
    observed = []

    def serve():
        try:
            sock, _ = listener.accept()
            with sock:
                sock.settimeout(4)
                kind, handshake = read_frame(sock)
                assert kind == 0 and read_vint(handshake)[0] == 47
                kind, _ = read_frame(sock)
                assert kind == 0
                if encrypted:
                    send_frame(sock, 1, string("") + vint(0) + vint(0))
                    try:
                        sock.recv(1)
                    except OSError:
                        pass
                    return
                send_frame(sock, 3, vint(64))
                send_frame(sock, 2, string("00000000-0000-3000-8000-000000000001") + string("ClientTest"), True)
                send_frame(sock, 1, struct.pack(">iBbBB", 1, 1, 0, 0, 8) + string("default") + b"\0", True)
                spawn_y = 300 if above_world else 18 if remote_chunk else 7
                send_frame(sock, 8, struct.pack(">dddffB", 8.5, spawn_y, 8.5, 0, 0, 0), True)
                send_frame(sock, 0x39, struct.pack(">Bff", 13, 0.05, 0.1), True)
                states = bytearray(8192)
                for y in range(5):
                    for z in range(16):
                        for x in range(16):
                            struct.pack_into("<H", states, (y * 256 + z * 16 + x) * 2, (7 if y == 0 else 2 if y == 4 else 1) << 4)
                data = bytes(states) + bytes(2048) + b"\xff" * 2048 + b"\1" * 256
                chunk = struct.pack(">iiBH", 1 if remote_chunk else 0, 0, 1, 1) + vint(len(data)) + data
                if malformed:
                    chunk = struct.pack(">iiBH", 0, 0, 1, 1) + vint(1) + b"\0"
                send_frame(sock, 0x21, chunk, True)
                send_frame(sock, 0, vint(771), True)
                while True:
                    try:
                        observed.append(read_frame(sock, True))
                    except EOFError:
                        break
        except Exception as exc:
            errors.put(exc)

    thread = threading.Thread(target=serve, daemon=True)
    thread.start()
    try:
        yield port, observed
    finally:
        thread.join(6)
        listener.close()
        if thread.is_alive():
            raise AssertionError("independent peer did not finish")
        if not errors.empty():
            raise errors.get()


class ClientTests(unittest.TestCase):
    def run_client(self, port):
        result = subprocess.run([str(CLIENT), "--host", "127.0.0.1", "--port", str(port),
                                 "--name", "ClientTest", "--headless", "--run-seconds", "1"],
                                capture_output=True, text=True, timeout=8)
        return result, result.stdout + result.stderr

    def test_c_client_receives_real_multiplayer_world(self):
        with tempfile.TemporaryDirectory() as folder, running_server(Path(folder) / "world.c919") as port:
            peer = Peer(port)
            try:
                peer.login("OtherPlayer")
                result, output = self.run_client(port)
                self.assertEqual(result.returncode, 0, output)
                self.assertRegex(output, r"joined=1 positioned=1 chunks=25")
                self.assertRegex(output, r"players=1")
            finally:
                peer.close()

    def test_compressed_external_peer_and_keepalive(self):
        with independent_server() as (port, observed):
            result, output = self.run_client(port)
            self.assertEqual(result.returncode, 0, output)
            self.assertRegex(output, r"joined=1 positioned=1 chunks=1")
        self.assertTrue(any(kind == 0 and read_vint(data)[0] == 771 for kind, data in observed))
        self.assertTrue(any(kind == 6 for kind, _ in observed), "position correction must be acknowledged")

    def test_online_login_is_explicit_error(self):
        with independent_server(encrypted=True) as (port, _):
            result, output = self.run_client(port)
            self.assertNotEqual(result.returncode, 0, output)
            self.assertRegex(output.lower(), r"online|encrypt|authentication")

    def test_spawn_waits_for_its_own_chunk_before_gravity(self):
        with independent_server(remote_chunk=True) as (port, observed):
            result, output = self.run_client(port)
            self.assertEqual(result.returncode, 0, output)
            self.assertRegex(output, r"position=8\.500,18\.000,8\.500")
        movements = [struct.unpack_from(">ddd", data) for kind, data in observed if kind in (4, 6)]
        self.assertTrue(movements)
        self.assertTrue(all(y == 18 for _, y, _ in movements), movements)

    def test_player_can_descend_from_above_build_height(self):
        with independent_server(above_world=True) as (port, observed):
            result, output = self.run_client(port)
            self.assertEqual(result.returncode, 0, output)
        movements = [struct.unpack_from(">ddd", data) for kind, data in observed if kind in (4, 6)]
        self.assertTrue(any(y < 300 for _, y, _ in movements), movements)

    def test_malformed_chunk_is_rejected(self):
        with independent_server(malformed=True) as (port, _):
            result, output = self.run_client(port)
            self.assertNotEqual(result.returncode, 0, output)


if __name__ == "__main__":
    unittest.main(verbosity=2)
