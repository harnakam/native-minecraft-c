"""Independent socket-level protocol 47 integration tests; no game client required."""
import contextlib
import json
import math
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
import unittest
import uuid


ROOT = Path(__file__).resolve().parents[1]
SERVER = Path(os.environ.get("C919_SERVER", str(ROOT / "build" / "c919-server.exe")))
if not SERVER.exists() and os.name != "nt" and "C919_SERVER" not in os.environ:
    SERVER = ROOT / "build" / "c919-server"


def vint(value):
    value &= 0xFFFFFFFF
    out = bytearray()
    while True:
        byte = value & 127
        value >>= 7
        out.append(byte | (128 if value else 0))
        if not value:
            return bytes(out)


def read_vint(data, offset=0):
    value = 0
    for shift in range(0, 35, 7):
        byte = data[offset]
        offset += 1
        value |= (byte & 127) << shift
        if not byte & 128:
            return value, offset
    raise ValueError("oversized VarInt")


def string(value):
    encoded = value.encode("utf-8")
    return vint(len(encoded)) + encoded


def read_string(data, offset=0):
    length, offset = read_vint(data, offset)
    return data[offset:offset + length].decode("utf-8"), offset + length


def position(x, y, z):
    return struct.pack(">Q", ((x & 0x3FFFFFF) << 38) | ((y & 4095) << 26) | (z & 0x3FFFFFF))


def slot(item):
    return struct.pack(">hbhb", item, 1, 0, 0)


def flat_world(path, state=2 << 4, height=32):
    """Author native C919WRL1 terrain, with a real block plane and air above."""
    coordinates = [(x, z) for z in range(-3, 4) for x in range(-3, 4)]
    data = bytearray(b"C919WRL1" + struct.pack("<II", 919, len(coordinates)))
    runs = ((height * 256, 1 << 4), (256, state),
            ((255 - height) * 256, 0))
    for x, z in coordinates:
        data.extend(struct.pack("<iiI", x, z, len(runs)))
        for count, block in runs:
            data.extend(struct.pack("<IH", count, block))
    Path(path).write_bytes(data)


class Peer:
    def __init__(self, port):
        self.socket = socket.create_connection(("127.0.0.1", port), timeout=3)
        self.socket.settimeout(4)
        self.entity = None
        self.spawn = None
        self.chunks = {}
        self.initial = []

    def close(self):
        self.socket.close()

    def send(self, packet_id, payload=b"", fragmented=False):
        data = vint(packet_id) + payload
        frame = vint(len(data)) + data
        if fragmented:
            for byte in frame:
                self.socket.sendall(bytes([byte]))
        else:
            self.socket.sendall(frame)

    def exact(self, count):
        out = bytearray()
        while len(out) < count:
            chunk = self.socket.recv(count - len(out))
            if not chunk:
                raise EOFError("server closed connection")
            out.extend(chunk)
        return bytes(out)

    def packet(self):
        prefix = bytearray()
        for _ in range(5):
            prefix.extend(self.exact(1))
            if not prefix[-1] & 128:
                break
        length, _ = read_vint(prefix)
        if not 0 < length <= 2 * 1024 * 1024:
            raise ValueError("invalid packet size")
        data = self.exact(length)
        packet_id, offset = read_vint(data)
        return packet_id, data[offset:]

    def wait(self, packet_id, predicate=lambda payload: True, seconds=5):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            kind, payload = self.packet()
            if kind == packet_id and predicate(payload):
                return payload
            if kind == 0 and self.entity is not None:
                self.send(0, payload)
        raise AssertionError(f"packet {packet_id:#x} not received")

    def handshake(self, state, version=47, fragmented=False):
        self.send(0, vint(version) + string("127.0.0.1") + struct.pack(">H", self.socket.getpeername()[1]) + vint(state), fragmented)

    def login(self, name, fragmented=False):
        self.handshake(2, fragmented=fragmented)
        self.send(0, string(name), fragmented)
        packet_id, payload = self.packet()
        assert packet_id == 2, (packet_id, payload)
        uuid_text, offset = read_string(payload)
        returned_name, _ = read_string(payload, offset)
        digest = bytearray(__import__("hashlib").md5(("OfflinePlayer:" + name).encode()).digest())
        digest[6] = (digest[6] & 15) | 48
        digest[8] = (digest[8] & 63) | 128
        assert uuid_text == str(uuid.UUID(bytes=bytes(digest)))
        assert returned_name == name
        while self.spawn is None or len(self.chunks) < 25:
            packet_id, payload = self.packet()
            self.initial.append((packet_id, payload))
            if packet_id == 1:
                self.entity = struct.unpack_from(">i", payload)[0]
                assert payload[4] == getattr(self, "expected_gamemode", 1), "unexpected server gamemode"
            elif packet_id == 8:
                self.spawn = struct.unpack_from(">dddffB", payload)
                self.send(6, struct.pack(">dddffB", *self.spawn[:5], 0))
            elif packet_id == 0x21:
                x, z, full, mask = struct.unpack_from(">iiBH", payload)
                length, offset = read_vint(payload, 11)
                self.chunks[(x, z)] = (mask, payload[offset:offset + length])
                assert full == 1
        return self

    def block(self, x, y, z):
        mask, data = self.chunks[(x // 16, z // 16)]
        section = y // 16
        if not mask & (1 << section):
            return 0
        preceding = sum(bool(mask & (1 << bit)) for bit in range(section))
        offset = preceding * 8192 + ((y % 16) * 256 + (z % 16) * 16 + x % 16) * 2
        return struct.unpack_from("<H", data, offset)[0]

    def surface(self, x, z):
        return next(y for y in range(255, -1, -1) if self.block(x, y, z))


@contextlib.contextmanager
def running_server(world, crash=False, gamemode=1):
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    log = tempfile.TemporaryFile(mode="w+b")
    process = subprocess.Popen([str(SERVER), "--bind", "127.0.0.1", "--port", str(port),
                                "--world", str(world), "--gamemode", str(gamemode),
                                "--run-seconds", "30"], stdout=log, stderr=log)
    try:
        deadline = time.monotonic() + 5
        while True:
            if process.poll() is not None:
                log.seek(0)
                raise AssertionError("server exited: " + log.read().decode(errors="replace"))
            try:
                with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                    break
            except OSError:
                if time.monotonic() > deadline:
                    raise AssertionError("server did not listen within 5 seconds")
                time.sleep(0.02)
        yield port
    finally:
        if process.poll() is None:
            process.kill() if crash else process.terminate()
        try:
            process.wait(timeout=4)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=4)
        log.close()


class MultiplayerTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(SERVER.is_file(), f"dedicated server not built: {SERVER}")
        self.directory = tempfile.TemporaryDirectory(prefix="c919-server-test-")
        self.world = Path(self.directory.name) / "world.c919"
        self.peers = []

    def tearDown(self):
        for peer in self.peers:
            peer.close()
        if hasattr(self, "directory"):
            self.directory.cleanup()

    def peer(self, port):
        peer = Peer(port)
        self.peers.append(peer)
        return peer

    def test_status_fragmentation_and_protocol_rejection(self):
        with running_server(self.world) as port:
            peer = self.peer(port)
            peer.handshake(1, fragmented=True)
            peer.send(0)
            response = json.loads(read_string(peer.wait(0))[0])
            self.assertEqual(response["version"]["protocol"], 47)
            self.assertEqual(response["players"]["online"], 0)
            peer.send(1, struct.pack(">q", 123456789))
            self.assertEqual(peer.wait(1), struct.pack(">q", 123456789))
            bad = self.peer(port)
            bad.handshake(2, version=48)
            reason = read_string(bad.wait(0))[0]
            self.assertIn("47", reason)
            malformed = self.peer(port)
            malformed.send(0, vint(47) + string("host") + struct.pack(">H", port) + vint(3))
            with self.assertRaises((EOFError, ConnectionResetError)):
                malformed.packet()

    def test_half_closed_status_request_still_receives_response(self):
        with running_server(self.world) as port:
            peer = self.peer(port)
            peer.handshake(1)
            peer.send(0)
            peer.socket.shutdown(socket.SHUT_WR)
            response = json.loads(read_string(peer.wait(0))[0])
            self.assertEqual(response["version"]["protocol"], 47)

    def test_tower_at_original_spawn_does_not_break_new_logins(self):
        flat_world(self.world)
        with running_server(self.world) as port:
            builder = self.peer(port).login("TowerBuilder")
            ground = builder.surface(8, 8)
            for y in range(ground + 1, 256):
                builder.send(6, struct.pack(">dddffB", 8.5, min(y + 1.0, 254.0), 8.5, 0, 0, 0))
                builder.send(8, position(8, y - 1, 8) + bytes([1]) + slot(1) + bytes([8, 16, 8]))
                changed = builder.wait(0x23, lambda p: p[:8] == position(8, y, 8))
                self.assertEqual(read_vint(changed, 8)[0], 16)
            newcomer = self.peer(port).login("TowerVisitor")
            x, y, z = newcomer.spawn[:3]
            # The actual MP constructor may legitimately select the tower's
            # column. Its top-solid result is then y=256, rather than a native
            # spawn search choosing another column.
            self.assertTrue(all(math.isfinite(value) for value in (x, y, z)))
            self.assertEqual(y, newcomer.surface(math.floor(x), math.floor(z)) + 1)
            self.assertEqual(newcomer.block(math.floor(x), math.floor(y), math.floor(z)), 0)

    def test_source_mp_ordinary_admission_uses_random_column_and_top_solid_height(self):
        flat_world(self.world)
        with running_server(self.world) as port:
            first = self.peer(port).login("SourceSpawnA")
            second = self.peer(port).login("SourceSpawnB")
            self.assertNotEqual(first.entity, second.entity)
            for peer in (first, second):
                x, y, z = peer.spawn[:3]
                # Native World spawn=(8,8), protection16: original MP uses
                # nextInt(20)-10 for each axis, and centers the resulting block.
                self.assertTrue(-2 <= math.floor(x) <= 17)
                self.assertTrue(-2 <= math.floor(z) <= 17)
                self.assertEqual((x - math.floor(x), z - math.floor(z)), (0.5, 0.5))
                self.assertEqual(y, 33.0)
                self.assertEqual(peer.block(math.floor(x), 32, math.floor(z)), 2 << 4)
                self.assertEqual(peer.block(math.floor(x), 33, math.floor(z)), 0)
            spawn = next(payload for kind, payload in second.initial if kind == 0x0c)
            entity, offset = read_vint(spawn)
            self.assertEqual(entity, first.entity)
            self.assertEqual(struct.unpack_from(">iii", spawn, offset + 16),
                             tuple(math.floor(v * 32) for v in first.spawn[:3]))

    def test_source_action_authority_and_movement_packet_decoders(self):
        with tempfile.TemporaryDirectory() as folder, running_server(Path(folder) / "world.c919") as port:
            peer = Peer(port)
            try:
                peer.login("WalkingSource")
                x, y, z, yaw, pitch = peer.spawn[:5]
                # Original processEntityAction uses the handler's bound player,
                # ignores packet entityID, and accepts signed auxData.
                for action in (0, 3, 4, 1, 5, 6):
                    peer.send(0x0b, vint(-12345) + vint(action) + vint(-1))
                # Source readUnsignedByte != 0 permits all ground byte values;
                # the separate native server policy validates coordinates.
                peer.send(3, b"\xff")
                peer.send(4, struct.pack(">dddB", x, y, z, 255))
                peer.send(5, struct.pack(">ffB", yaw, pitch, 255))
                peer.send(6, struct.pack(">dddffB", x, y, z, yaw, pitch, 255))
                peer.send(1, string("source movement decoded"))
                message = read_string(peer.wait(2))[0]
                self.assertIn("source movement decoded", message)
            finally:
                peer.close()

    def test_two_players_movement_chat_edit_and_visibility(self):
        with running_server(self.world) as port:
            alice = self.peer(port).login("Alice", fragmented=True)
            bob = self.peer(port).login("Bob")
            alice_info = next(i for i, item in enumerate(bob.initial) if item[0] == 0x38)
            alice_spawn = next(i for i, item in enumerate(bob.initial) if item[0] == 0x0C)
            self.assertLess(alice_info, alice_spawn)
            alice.wait(0x38)
            spawned = alice.wait(0x0C)
            self.assertEqual(read_vint(spawned)[0], bob.entity)
            x, y, z = alice.spawn[:3]
            alice.send(6, struct.pack(">dddffB", x + 0.25, y, z, 40, 10, 1))
            moved = bob.wait(0x18, lambda p: read_vint(p)[0] == alice.entity)
            _, offset = read_vint(moved)
            self.assertEqual(struct.unpack_from(">iii", moved, offset), (int((x + 0.25) * 32), int(y * 32), int(z * 32)))
            alice.send(1, string("hello socket test"))
            self.assertIn("hello socket test", read_string(alice.wait(2))[0])
            self.assertIn("Alice", read_string(bob.wait(2))[0])
            japanese = "日本語" * 20
            alice.send(1, string(japanese))
            self.assertIn(japanese, read_string(alice.wait(2))[0])
            self.assertIn(japanese, read_string(bob.wait(2))[0])
            # Select glass, then place on the ground under the spawn column.
            block_x, block_z = math.floor(x), math.floor(z)
            ground = math.floor(y) - 1
            self.assertNotEqual(alice.block(block_x, ground, block_z), 0)
            alice.send(9, struct.pack(">h", 5))
            alice.send(0x10, struct.pack(">h", 41) + slot(20))
            inventory = alice.wait(0x2F, lambda p: struct.unpack_from(">h", p, 1)[0] == 41)
            self.assertEqual(struct.unpack_from(">h", inventory, 3)[0], 20)
            alice.send(8, position(block_x, ground, block_z) + bytes([1]) + slot(20) + bytes([8, 16, 8]))
            expected = position(block_x, ground + 1, block_z)
            change = bob.wait(0x23, lambda p: p[:8] == expected)
            self.assertEqual(read_vint(change, 8)[0], 20 << 4)
            alice.wait(0x23, lambda p: p[:8] == expected)
            alice.send(7, vint(0) + expected + bytes([1]))
            self.assertEqual(read_vint(bob.wait(0x23, lambda p: p[:8] == expected), 8)[0], 0)
            # Crossing the initial window loads the world border for this player.
            new_chunk = (-3 if block_x // 16 == 1 else 3, block_z // 16)
            self.assertNotIn(new_chunk, alice.chunks)
            alice.send(6, struct.pack(">dddffB", new_chunk[0] * 16 + 0.5, y, z, 0, 0, 0))
            border = alice.wait(0x21, lambda p: struct.unpack_from(">ii", p) == new_chunk)
            self.assertEqual(struct.unpack_from(">ii", border), new_chunk)
            bob.close()
            removed = alice.wait(0x13)
            count, offset = read_vint(removed)
            self.assertEqual(count, 1)
            self.assertEqual(read_vint(removed, offset)[0], bob.entity)

    def test_edit_is_durable_and_invalid_reach_is_corrected(self):
        with running_server(self.world) as port:
            alice = self.peer(port).login("Builder")
            x, y, z = math.floor(alice.spawn[0]), math.floor(alice.spawn[1]) - 1, math.floor(alice.spawn[2])
            alice.send(8, position(x, y, z) + bytes([1]) + slot(1) + bytes([8, 16, 8]))
            edited = position(x, y + 1, z)
            self.assertEqual(read_vint(alice.wait(0x23, lambda p: p[:8] == edited), 8)[0], 16)
            self.assertTrue(self.world.is_file())
            far = position(x + 16, y, z)
            original = alice.block(x + 16, y, z)
            alice.send(7, vint(0) + far + bytes([1]))
            correction = alice.wait(0x23, lambda p: p[:8] == far)
            self.assertEqual(read_vint(correction, 8)[0], original)
            alice.close()
        with running_server(self.world) as port:
            restored = self.peer(port).login("Reader")
            self.assertEqual(restored.block(x, y + 1, z), 16)

    def test_invalid_players_and_nonfinite_movement_disconnect(self):
        with running_server(self.world) as port:
            bad = self.peer(port)
            bad.handshake(2)
            bad.send(0, string("invalid name"))
            self.assertEqual(bad.packet()[0], 0)
            alice = self.peer(port).login("Duplicate")
            duplicate = self.peer(port)
            duplicate.handshake(2)
            duplicate.send(0, string("Duplicate"))
            self.assertEqual(duplicate.packet()[0], 0)
            alice.send(6, struct.pack(">dddffB", math.nan, *alice.spawn[1:5], 0))
            self.assertIn("movement", read_string(alice.wait(0x40))[0].lower())
            hotbar = self.peer(port).login("Hotbar")
            hotbar.send(9, struct.pack(">h", 9))
            self.assertIn("hotbar", read_string(hotbar.wait(0x40))[0].lower())

    def test_persistence_failure_preserves_committed_world(self):
        with running_server(self.world) as port:
            alice = self.peer(port).login("DiskFailure")
            committed = self.world.read_bytes()
            # A directory at the temporary filename reliably prevents writing,
            # including when the test runs with administrative privileges.
            Path(str(self.world) + ".tmp").mkdir()
            x, y, z = math.floor(alice.spawn[0]), math.floor(alice.spawn[1]) - 1, math.floor(alice.spawn[2])
            alice.send(8, position(x, y, z) + bytes([1]) + slot(1) + bytes([8, 16, 8]))
            self.assertIn("rolled back", read_string(alice.wait(0x40))[0])
            self.assertEqual(self.world.read_bytes(), committed)

    def test_corrupt_existing_world_is_fatal_and_preserved(self):
        self.world.write_bytes(b"invalid saved world")
        result = subprocess.run([str(SERVER), "--world", str(self.world), "--run-seconds", "1"],
                                capture_output=True, timeout=5)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"Could not load existing world", result.stderr)
        self.assertEqual(self.world.read_bytes(), b"invalid saved world")


if __name__ == "__main__":
    unittest.main()
