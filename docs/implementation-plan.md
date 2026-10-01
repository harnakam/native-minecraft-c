# C919 implementation plan

The active objective is a faithful translation of the supplied Java original and full target-version compatibility. `compatibility.md` defines completion, and `porting.md` records actual class/method coverage. Matching packages or independently recreating behavior does not satisfy source translation. The initial native runtime steps below are historical; their unported dependencies remain part of the active objective.

The Timer increment is published at `8e6257b`: all ten fields, constructor, updateTimer and the called MathHelper clamp are connected to the canonical managed client graph. Full World/Entity/player ticks and Minecraft.runGameLoop/runTick remain open.

The current increment replaces native xorshift with an independently implemented Java 8 scalar Random API dependency, separate traced World/player/item streams and one external lazy process Math.random stream. Translate MathHelper.getRandomUuid and the named Entity UUID NBT statements; preserve inherited initialization draw order, aliases, getter reevaluation, failure prefixes and profile UUID restoration. Connect these to real server drops/pickup, client object spawn and save/load. Verify against actual JDK/JAR records, strict Windows/WSL full tests, sanitized graph/network lifetime, independent review and an audit of actual public index blobs before publication.

Continue with full Entity/EntityLivingBase/EntityPlayer/World constructor and NBT bodies, including conditional EntityPlayerMP spawn draws and XpSeed's zero-value nextInt, before claiming complete initialization or persistence parity. Implement Gaussian only with verified StrictMath dependencies: ordinary host log differed from the target JDK at 37 tested inputs. Native process locks serialize complete calls; they do not reproduce every JDK primitive-CAS interleaving. Complete gameplay, physics, input/game loop, rendering/resources, authentication, world storage and all requirements in compatibility.md remain part of the objective.

## Historical initial native runtime plan

1. Network module: endian-safe codecs, signed VarInt and packed position, offline UUID, bounded TCP framing, zlib compression, Winsock/POSIX transport. Test byte-level contracts and malformed input.
2. Shared world: dynamic bounded chunks, negative-coordinate mapping, original seeded terrain, transactional persistence. Test generation, edits, reload, and corrupt-file rejection.
3. Dedicated server: protocol-47 status/login, authoritative creative world, visible players, movement, keepalives, chat, block edits and hotbar. Validate independent real TCP clients and restart persistence.
4. Native Windows client: real network play session, chunk/entity parsing, OpenGL procedural materials, input, collision, ray targeting, HUD, chat and disconnect states. Build and exercise headless networking; document manual GUI checks separately.
5. Integrate CMake/CTest, English technical docs and Japanese usage docs, continuous integration, public file allowlist audit. Review independently, fix failures, then create and push a public harnakam repository using authenticated gh.
