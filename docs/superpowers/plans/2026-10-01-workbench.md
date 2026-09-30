# Workbench and MapData Implementation Plan

This earlier behavioral implementation plan is superseded for source-port acceptance by `2026-10-01-source-port.md`. It remains the record of the connected runtime needed to exercise translated classes.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add working protocol 47 workbench crafting, including multiplayer transactions, remaining items, durable map scaling, and the native client screen.

**Architecture:** A shared container maps an independent nine-slot crafting grid onto the existing player inventory and cursor. The server commits player, dropped-item, and MapData snapshots through one recoverable journal before publishing results. The client predicts through the same container rules and reconciles using window identity and authoritative packets.

**Tech Stack:** C11, existing zlib, CMake/Ninja, Win32 OpenGL, Python integration tests, GCC on Windows and Linux.

**Spec:** `docs/compatibility.md`; the user's complete Minecraft Java 1.8.9 compatibility objective. This plan is one incremental deliverable toward that objective.

## Global Constraints

- Protocol 47 / Minecraft Java 1.8.9; use the existing `src/minecraft/net/minecraft` hierarchy.
- Publish only C/C++ implementation, native adapters, tests and documentation; identify translated class methods accurately. Exclude Minecraft resources, binaries, MCP, mappings, and private reference fixtures.
- Player inventory and cursor have one owner; container mapping must not duplicate storage.
- Failed preparation preserves every owner; a committed checkpoint failure stops mutations and recovers forward on restart.
- Bound allocations and NBT, preserve unknown saved fields, and run strict Windows and Linux builds.

## Review Focus

- Reopened windows and delayed acknowledgments must not apply an old prediction to a new container.
- A failed third staging snapshot must preserve player, items, maps and the recoverable journal.
- Owned effect vectors larger than ten must preserve every drop and roll back cleanly at the size limit.
- A crash while a workbench is open must preserve the nine inputs and shared cursor exactly once.
- Missing MapData and scaling change authoritative IDs without letting client prediction allocate IDs.

---

### Task 1: Shared containers and full crafting grids

**Files:** `src/minecraft/net/minecraft/inventory/container.{c,h}`, `crafting/crafting.{c,h}`, `inventory/inventory.{c,h}` under the same root; `tests/test_container.c`, `tests/test_crafting.c`, `tests/test_inventory.c`.

**Interfaces:** `mc_container` owns only workbench slots 0..9; mapped slots 10..36 reference player 9..35 and 37..45 reference player 36..44. `mc_container_update`, `mc_container_click`, `mc_container_close` take the player and container; contextual crafting uses `mc_crafting_context` with `mc_maps *maps`, position, spawn and authority. `mc_crafting_effects_append` owns a copied drop in a bounded vector.

- [ ] Add failing tests for 365 static recipes, fixed banner shapes, nine-input special recipes, all click modes, repeated cake remainders, and effect-vector limits.
- [ ] Implement atomic shared ownership, complete recipe matching, and map scaling through Task 2's working MapData.
- [ ] Pass strict unit tests and sanitizer checks; compare numerical behavior with private target fixtures.
- [ ] Review and commit the integrated source with the other tasks after the publication audit.

### Task 2: Durable MapData

**Files:** `src/minecraft/net/minecraft/world/map.{c,h}`, `tests/test_map.c`.

**Interfaces:** `mc_maps_copy/encode/decode`, `mc_maps_resolve/create/scale_preview/on_crafted`, `mc_map_packet`, `mc_maps_receive`, and `mc_map_update_terrain`. Owning outputs remain unchanged on failure. Authoritative missing-map resolution creates a scale-3 spawn-centered map; scaling creates a new ID and preserves item NBT.

- [ ] Add failing tests for centers, unknown NBT fields, missing IDs, scaling, malformed S34 rectangles, and allocation limits.
- [ ] Implement MapData ownership, NBT and wire encoding, terrain surveying, and contextual map creation/scaling.
- [ ] Pass strict tests and sanitizer checks; compare behavior against privately executed target classes.
- [ ] Review and commit the integrated source after the publication audit.

### Task 3: Server lifecycle and recoverable transfer

**Files:** `src/minecraft/net/minecraft/server/server.c`, `util/transfer.{c,h}`; `tests/test_transfer.c`, `tests/test_workbench_network.py`; `CMakeLists.txt`, `scripts/test-linux.sh`.

**Interfaces:** `mc_transfer_prepare_all/commit_all` add an optional MapData snapshot while preserving old two-file APIs and version-1 recovery. Open workbench uses type `minecraft:crafting_table`, advertised size 0, 46 mapped slots and IDs 1..100.

- [ ] Add failing three-file recovery tests, including corrupt staging and failure after the journal commit point.
- [ ] Implement version-2 transfer manifests and validate all snapshots before replacing any destination.
- [ ] Add network tests for empty-hand block activation, mapped clicks, false ACK unlock, ID-agnostic close, range/block invalidation, remainder drops, map scaling, and restart recovery.
- [ ] Wire actual block activation, shared containers, player persistence, world drops and MapData to durable transactions; send authoritative resyncs and map updates.
- [ ] Pass targeted real-server tests, then the Windows CTest and WSL integration suites.
- [ ] Review and commit the integrated source after the publication audit.

### Task 4: Native client window and UI

**Files:** `src/minecraft/net/minecraft/client/client.{c,h}`, `client/renderer.{c,h}`; `tests/test_client_containers.py`.

**Interfaces:** Consume Task 1's mapped container without copying player storage. Pending clicks carry window ID, action and generation. Prediction waits for full window plus cursor resync after rejection. `--use-block x,y,z` aims and uses the real native block-interaction path.

- [ ] Add failing compressed-peer tests for OpenWindow parsing, shared slots, cursor, stale confirmation, reopen, rejection, close and malformed packets.
- [ ] Implement workbench state, native controls and a shared rendering/hit-test geometry for 46 slots.
- [ ] Exercise native client against the real server and inspect hidden OpenGL framebuffer fixtures at two window sizes.
- [ ] Pass strict client tests and review the integrated behavior.
- [ ] Update public compatibility documentation, audit explicit staged UTF-8 paths, push to the authorized public repository and verify exact-commit CI.
