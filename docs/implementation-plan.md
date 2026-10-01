# C919 implementation plan

The active objective is a faithful translation of the supplied Java original and full target-version compatibility. `compatibility.md` defines completion, and `porting.md` records actual class/method coverage. Matching packages or independently recreating behavior does not satisfy source translation. The initial native runtime steps below are historical; their unported dependencies remain part of the active objective.

The current increment translates all Timer fields, its constructor and updateTimer plus the called MathHelper clamp. Bind it into the canonical managed client graph, update once before network graph adoption, feed signed elapsedTicks to the existing inventory/item phases and the source renderPartialTicks to hotbar rendering. Keep original first-update, drift, wrap, nonfinite values and excess-tick discard; keep input once per frame until the full original input/game loop is translated. Verify against independent actual-JAR clock records, real client frame/input/TCP and graph lifetime, strict Windows/WSL builds, sanitizers and public blob audit before publication. Full World/Entity/player ticks and Minecraft.runGameLoop/runTick stay open.

## Historical initial native runtime plan

1. Network module: endian-safe codecs, signed VarInt and packed position, offline UUID, bounded TCP framing, zlib compression, Winsock/POSIX transport. Test byte-level contracts and malformed input.
2. Shared world: dynamic bounded chunks, negative-coordinate mapping, original seeded terrain, transactional persistence. Test generation, edits, reload, and corrupt-file rejection.
3. Dedicated server: protocol-47 status/login, authoritative creative world, visible players, movement, keepalives, chat, block edits and hotbar. Validate independent real TCP clients and restart persistence.
4. Native Windows client: real network play session, chunk/entity parsing, OpenGL procedural materials, input, collision, ray targeting, HUD, chat and disconnect states. Build and exercise headless networking; document manual GUI checks separately.
5. Integrate CMake/CTest, English technical docs and Japanese usage docs, continuous integration, public file allowlist audit. Review independently, fix failures, then create and push a public harnakam repository using authenticated gh.
