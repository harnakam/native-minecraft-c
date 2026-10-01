# ItemStack identity and NBT source-port implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the original ItemStack/NBT reference semantics and original crafting/inventory methods into the live client/server, including book remainder aliases across inventory, world drops and pickup.

**Architecture:** One native tracked heap owns mutable objects with stable identities and cycles. Original copy methods recursively copy each edge, while a native transaction snapshot uses one memo for all live roots. Wire and saved NBT intentionally reconstruct distinct objects; no persistent identity IDs or renderer-owned value mirrors are added. Named original classes replace the relevant call paths; native registry, rendering and persistence adapters remain identified.

**Tech Stack:** C11, existing zlib, Windows UCRT64 and Linux GCC, native Win32/OpenGL, Python network tests, private Java8 reference fixtures.

**Spec:** User's source-translation clarification; `docs/porting.md` and `docs/compatibility.md`. Actual original methods and private differential evidence determine the port; existing behavior does not override them.

## Global Constraints

- Retain original package/class paths and method/control-flow correspondence.
- Keep ItemStack signed int fields, nullable references and non-null zero-count objects distinct.
- Compound/List/array getters and setters retain original mutable identity; UTF-16 includes NUL and unpaired surrogates.
- Keep original ItemStack/NBT copy separate from graph-preserving transaction clone.
- Commit all affected player/item/map snapshots with one recoverable manifest. Validate every staged file before changing any target.
- Exclude original Java, MCP, mappings, JARs, resources and private fixtures from public Git.
- Do not publish or present the new foundation as completed runtime migration until actual gameplay paths use it.

## Review Focus

- A book remainder can share its object with both a crafting grid and world item, then mutate another player's state on pickup.
- Graph snapshots must retain aliases, cycles and native transient fields; failure must preserve every original owner.
- Original copy intentionally breaks repeated child aliases and resets ItemStack animation/frame/cache fields.
- Count0/negative count wire and save values are not empty references; gameplay validation is a separate original-handler concern.
- GC/adoption must not invalidate live borrowed pointers or pending client transaction roots.

### Task 1: Native lifetime adapter and original NBT classes

**Files:** `util/MCObjectHeap.{c,h}`, original named classes under `nbt/`, `tests/test_object_heap.c`, `tests/test_nbt_objects.c`.

- [x] Implement iterative traced collection, scopes, all-root memo snapshots and in-place adoption; test cycles, aliases, stale/adoption failure and budgets.
- [ ] Port NBTBase/NBTPrimitive, primitive tags, mutable Compound/List/arrays, UTF-16 String and NBTSizeTracker; separate wire adapter from named class bodies.
- [x] Verify original numeric, equality, alias, copy, empty-list and modified-UTF contracts with strict builds and sanitizers.

### Task 2: Original ItemStack, inventory and crafting methods

**Files:** `item/ItemStack.{c,h}`, `entity/player/InventoryPlayer.{c,h}`, `inventory/InventoryCrafting.{c,h}`, `inventory/InventoryCraftResult.{c,h}`, `inventory/Slot{,Crafting}.{c,h}`, `inventory/Container{,Player,Workbench}.{c,h}`, `item/crafting/RecipeBookCloning.{c,h}` and original Item registry dependency.

- [x] Port actual constructors/copy/split/tag/equality/count/damage methods and original insertion/decrement/set operations.
- [x] Port book matching/result/remainder and SlotCrafting ordering; preserve the same remainder object instead of value-based count corrections.
- [ ] Replace value-owning runtime inventory/container state with reference-owning named class state. Keep value snapshots only at external boundaries.

### Task 3: Live ownership, wire, rendering and atomic multi-owner persistence

**Files:** native inventory/container/crafting adapters, item entity, client/renderer/server, `util/transfer.{c,h}`, relevant C/Python tests, CMake/Linux scripts.

- [x] Add bounded version-3 group journal and preserve version-1/2 recovery.
- [ ] Share heap across all live player/container/cursor/entity/effect and client authoritative/pending roots; snapshot/adopt the entire alias closure.
- [ ] Encode, render and save canonical references; preserve null versus count0 and original byte/short narrowing.
- [ ] Exercise original book cases and cross-player aliased pickup through actual runtime. Reject malformed input and preserve owners at I/O/graph failures.

### Task 4: Verification and publication

- [ ] Independently review named methods and real caller wiring against the supplied original.
- [ ] Run affected strict Windows/Linux tests, sanitizer checks and original-game differential fixtures; then full suites.
- [ ] Update exact port/dependency boundaries, audit only source/tests/docs and push under harnakam; verify exact-commit CI.

Complete source translation and Minecraft compatibility remain active beyond this increment. Remaining world/entity/stats/GUI dependencies must stay explicit; neither green tests nor new class names imply they are complete.
