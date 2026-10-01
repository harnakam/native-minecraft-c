"""Independent ability packets exercise canonical client/server capabilities.

These tests observe Source flags, speed bits, mode authority and graph aborts;
they do not claim complete flight physics or translated World/GUI classes.
"""
import contextlib
from pathlib import Path
import queue
import re
import socket
import struct
import subprocess
import tempfile
import threading
import unittest

from test_client import CLIENT, read_frame, send_frame
from test_inventory_network import InventoryPeer, canonical_slot, wire_slot
from test_multiplayer import Peer, read_vint, running_server, string, vint


def abilities(flags, fly_bits, walk_bits):
    return struct.pack(">BII", flags, fly_bits, walk_bits)


@contextlib.contextmanager
def ability_peer(packets, early_close=False):
    """A compressed protocol-47 peer; every state input is real wire data."""
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
                kind, handshake = read_frame(sock)
                assert kind == 0 and read_vint(handshake)[0] == 47
                assert read_frame(sock)[0] == 0
                send_frame(sock, 3, vint(64))
                send_frame(sock, 2, string("00000000-0000-3000-8000-000000000001") + string("ClientTest"), True)
                send_frame(sock, 1, struct.pack(">iBbBB", 1, 1, 0, 0, 8) + string("default") + b"\0", True)
                send_frame(sock, 8, struct.pack(">dddffB", 8.5, 7, 8.5, 0, 0, 0), True)
                data = bytes(8192 + 2048) + b"\xff" * 2048 + b"\1" * 256
                send_frame(sock, 0x21, struct.pack(">iiBH", 0, 0, 1, 1) + vint(len(data)) + data, True)
                for kind, payload in packets:
                    send_frame(sock, kind, payload, True)
                # Its echo proves that all preceding good packets were read.
                if not early_close:
                    send_frame(sock, 0, vint(891), True)
                while True:
                    try:
                        observed.append(read_frame(sock, True))
                    except (EOFError, ConnectionResetError):
                        break
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
            raise AssertionError("ability peer did not finish")
        if not errors.empty():
            raise errors.get()


class ClientAbilityTests(unittest.TestCase):
    def client(self, port):
        result = subprocess.run([str(CLIENT), "--host", "127.0.0.1", "--port", str(port),
                                 "--name", "ClientTest", "--headless", "--run-seconds", "1"],
                                capture_output=True, text=True, timeout=8)
        return result, result.stdout + result.stderr

    def state(self, output):
        found = re.search(r"CLIENT_ABILITIES flags=(\d+) fly_bits=([0-9a-f]{8}) "
                          r"walk_bits=([0-9a-f]{8}) controller_mode=(-?\d+)", output)
        self.assertIsNotNone(found, output)
        return int(found[1]), int(found[2], 16), int(found[3], 16), int(found[4])

    def completed(self, observed):
        self.assertTrue(any(kind == 0 and read_vint(data)[0] == 891 for kind, data in observed),
                        "the final keepalive must be echoed")
        self.assertGreaterEqual(sum(kind in (4, 6) for kind, _ in observed), 2,
                                "state must survive later client movement frames")

    def test_s39_preserves_raw_speed_bits_and_reserved_flags_after_ticks(self):
        with ability_peer([(0x39, abilities(0xa2, 0x7fc12345, 0x80000000))]) as (port, observed):
            result, output = self.client(port)
            self.assertEqual(result.returncode, 0, output)
            self.assertEqual(self.state(output), (18, 0x7fc12345, 0x80000000, 1))
        self.completed(observed)

    def test_s39_capabilities_do_not_change_creative_controller_mode(self):
        with ability_peer([(0x39, abilities(0, 0xbf000000, 0xbe800000))]) as (port, observed):
            result, output = self.client(port)
            self.assertEqual(result.returncode, 0, output)
            self.assertEqual(self.state(output), (16, 0xbf000000, 0xbe800000, 1))
            self.assertRegex(output, r"gamemode=1")
        self.completed(observed)

    def test_mode_configuration_changes_flags_without_changing_speeds(self):
        # Entering Creative after Spectator preserves Source isFlying=true.
        for modes, expected in [([2], 0), ([3], 7), ([3, 1], 31)]:
            with self.subTest(modes=modes):
                packets = [(0x39, abilities(15, 0x80000000, 0x7fc12345))]
                packets.extend((0x2b, struct.pack(">Bf", 3, mode)) for mode in modes)
                with ability_peer(packets) as (port, observed):
                    result, output = self.client(port)
                    self.assertEqual(result.returncode, 0, output)
                    self.assertEqual(self.state(output), (expected, 0x80000000, 0x7fc12345, modes[-1]))
                self.completed(observed)

    def test_malformed_s39_does_not_adopt_partial_packet_fields(self):
        valid = abilities(6, 0x3e800000, 0x80000000)
        for malformed in [abilities(15, 0x3f000000, 0x3f800000)[:-1],
                          abilities(15, 0x3f000000, 0x3f800000) + b"x"]:
            with self.subTest(length=len(malformed)):
                with ability_peer([(0x39, valid), (0x39, malformed)], early_close=True) as (port, _):
                    result, output = self.client(port)
                    self.assertNotEqual(result.returncode, 0, output)
                    self.assertRegex(output, r"Malformed")
                    self.assertEqual(self.state(output), (22, 0x3e800000, 0x80000000, 1))

    def test_server_initial_creative_abilities_have_flying_clear(self):
        with tempfile.TemporaryDirectory() as folder, running_server(Path(folder) / "world.c919") as port:
            peer = Peer(port)
            try:
                peer.login("AbilitiesInitial")
                initial = [payload for kind, payload in peer.initial if kind == 0x39]
                self.assertEqual(len(initial), 1)
                self.assertEqual(initial[0], abilities(13, 0x3d4ccccd, 0x3dcccccd))
            finally:
                peer.close()

    def test_server_c13_reserved_flags_and_nan_do_not_grant_creative_or_disconnect(self):
        with tempfile.TemporaryDirectory() as folder, running_server(Path(folder) / "world.c919", gamemode=0) as port:
            peer, status = InventoryPeer(port), None
            try:
                peer.expected_gamemode = 0
                peer.login("AbilityAuthority")
                before, before_cursor = peer.snapshot()
                peer.send(0x13, abilities(0xff, 0x7fc12345, 0xff800000))
                peer.send(0x10, struct.pack(">h", 36) + wire_slot(264, 1))
                after, after_cursor = peer.snapshot()
                self.assertEqual([canonical_slot(slot) for slot in after],
                                 [canonical_slot(slot) for slot in before])
                self.assertEqual(canonical_slot(after_cursor), canonical_slot(before_cursor))
                # Source C13 only changes allowed isFlying. A forged creative
                # flag must not make the subsequent Source C10 legal.
                peer.send(1, string("AbilityAuthorityStillAlive"))
                self.assertIn(b"AbilityAuthorityStillAlive", peer.wait(2))
                status = Peer(port)
                status.handshake(1)
                status.send(0)
                self.assertIn(b'"protocol":47', status.wait(0))
            finally:
                peer.close()
                if status is not None:
                    status.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
