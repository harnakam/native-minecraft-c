# Source-method port implementation plan

**Goal:** Translate the supplied Java original into C/C++ under its package/class hierarchy, retaining methods, state and call order. Existing behavioral code does not count as a source port. This is the first integrated increment; complete Minecraft 1.8.9 compatibility remains the larger objective.

**Architecture:** Named class methods replace actual runtime call paths. Native ownership, buffers, sockets, rendering and storage are explicit adapters. Each port records incomplete dependencies instead of presenting the entire class as complete. Java originals, MCP, mappings, game resources and private differential fixtures remain excluded from public Git.

**Spec:** User clarification on 2026-10-01, `docs/porting.md`, `docs/compatibility.md`.

- [x] Inspect original InventoryCrafting, InventoryCraftResult, MapData, ItemMap and C08PacketPlayerBlockPlacement classes; establish real differences in notification order, per-player map state and packet lifecycle.
- [x] Port C08 constructors, state, read/write/getters/dispatch; connect both network directions; compare original Java quantization, nullable construction and position semantics.
- [x] Port and wire crafting inventory/result methods, retaining each immediate notification; test original no-notification operations and result whole-stack extraction.
- [x] Port selected MapData/MapInfo and ItemMap methods; connect all main-inventory updates and per-viewer packets; retain transient state across durable working copies.
- [x] Run strict Windows and Linux builds, meaningful class/network checks, original-Java differential fixtures and affected sanitizers after source freeze.
- [x] Independently review actual class-body integration and remaining dependency boundaries.
- [ ] Correct documentation, stage only public C/C++ source/tests/docs, audit Git blobs, push under harnakam and verify exact-commit CI.

Next dependency increment: original ItemStack identity/count semantics, Slot/Container notification order and original recipe/CraftingManager classes. Full GUI/controller/handler/world/entity class ports remain open. No completion claim for the full objective is permitted by this increment.
