# C919 implementation plan

1. Network module: endian-safe codecs, signed VarInt and packed position, offline UUID, bounded TCP framing, zlib compression, Winsock/POSIX transport. Test byte-level contracts and malformed input.
2. Shared world: dynamic bounded chunks, negative-coordinate mapping, original seeded terrain, transactional persistence. Test generation, edits, reload, and corrupt-file rejection.
3. Dedicated server: protocol-47 status/login, authoritative creative world, visible players, movement, keepalives, chat, block edits and hotbar. Validate independent real TCP clients and restart persistence.
4. Native Windows client: real network play session, chunk/entity parsing, OpenGL procedural materials, input, collision, ray targeting, HUD, chat and disconnect states. Build and exercise headless networking; document manual GUI checks separately.
5. Integrate CMake/CTest, English technical docs and Japanese usage docs, continuous integration, public file allowlist audit. Review independently, fix failures, then create and push a public harnakam repository using authenticated gh.
