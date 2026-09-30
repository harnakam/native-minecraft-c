# Verification record — 2026-10-01

The tested deliverable is an original C11 creative voxel client/server targeting the Java Edition 1.8.9 wire protocol (47). This record does not assert complete Minecraft feature parity.

## Verified

- Windows UCRT64 GCC 15.2.0 Release build through CMake/Ninja, with `-std=c11 -Wall -Wextra -Wpedantic -Werror`: passed.
- Windows CTest: all six groups passed: protocol, world, multiplayer, public-audit, client-self-test, client-network.
- Network codec/transport: 495 checks, including fragmented/coalesced frames, signed VarInt, packed positions, UTF-8, offline UUID, zlib, malformed input, bounded queues, partial writes and read-half-close backlog draining.
- World: negative coordinates, deterministic original terrain, block edits, transactional save/reload, corrupt-load rollback and save failure.
- Independent Python TCP peers: eight server tests, covering status/login, two players, movement/chat/edit visibility, restart persistence, Japanese chat, malformed input, failed-save rollback, half-close response and a height-255 tower at the original spawn.
- Actual C client: six independent network tests, covering the real C server, external compressed framing, keepalive and position acknowledgement, explicit online-mode rejection, malformed chunk rejection, unloaded spawn protection and descent from above build height.
- Public file audit: six real Git fixture tests, including staged forbidden/binary contents even when the working tree has been changed back to clean text.
- WSL Ubuntu: strict GCC builds and complete `scripts/test-linux.sh` passed, including network, world, eight multiplayer tests and six audit tests.
- WSL AddressSanitizer/UndefinedBehaviorSanitizer: all 495 protocol/transport checks passed without diagnostics.
- Independent [PrismarineJS implementation](https://github.com/PrismarineJS/node-minecraft-protocol), `minecraft-protocol` 1.68.0: two version-1.8.9 clients received 25 chunks each and synchronized Japanese chat, movement and digging. The library was used only under ignored `.local/interop/`.
- Native rendering: a hidden Win32 window rendered the actual received world through OpenGL on an AMD Radeon RX 6700 XT. The 1280×800 framebuffer was inspected for terrain, trees, HUD, crosshair, hotbar and connection information. Generated screenshots are ignored build outputs.
- Independent code review: material findings fixed and re-reviewed for EOF draining, position arithmetic, spawn selection, staged-blob auditing, Unicode message limits and unloaded terrain physics.

## Unverified or unsupported

Vanilla Minecraft GUI gameplay and live keyboard/mouse/IME input were not manually exercised. The native framebuffer and protocol paths were exercised; GUI input wiring was inspected and targeted helpers were tested. An online-mode server requires authentication/encryption that this initial version explicitly does not implement.

The renderer uses independently generated opaque cube materials. It does not reproduce all block shapes, transparency, textures or lighting. Chat text and `extra` fields are supported; translation, rich styles and localization are simplified. The dedicated server implements a nine-material creative palette and a bounded 7×7-chunk world. Full inventory actions, Survival, crafting, mobs, redstone, Anvil saves and Java mods are not implemented.

GitHub-hosted CI is configured separately. Its status must be checked from the actual pushed commit; local results alone do not establish remote CI success.
