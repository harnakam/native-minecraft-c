# C919 native multiplayer design

Target: Minecraft Java Edition 1.8.9, wire protocol 47, as identified by the local MCP configuration. MCP is a private reference only. No MCP source, binary, mappings, Minecraft assets, or runtime data are distributable inputs.

The initial implementation is an original C11 creative voxel client and dedicated server, with live multiplayer interoperability as the priority. It is not a complete Minecraft port. Existing empty `src/minecraft/net/minecraft` directories are used for readable module boundaries: network framing and codecs, shared world and storage, authoritative server, and native Windows client rendering/input.

The server accepts protocol-47 status/login/play sessions, sends independently generated terrain, maintains players, and synchronizes movement, chat, and validated block edits. Offline identities are explicitly local/LAN identities. The client negotiates framing/compression, receives actual server chunks and entities, renders procedural original block materials, and sends gameplay actions. Persistence is a validated versioned C919 world format; Anvil saves and Java mods are not supported.

Wire parsers bound lengths and reject invalid data. Slow clients have bounded queues. Invalid coordinates, non-finite movement, out-of-range edits, malformed handshakes, and duplicate names are rejected. The server defaults to loopback. Online-mode session authentication and encrypted login are not supported; the client must report this clearly instead of pretending to connect.

Verification: C tests for codecs and world boundaries/persistence; independent Python TCP tests for status/login, fragmented packets, malformed input, multi-client movement/chat/block broadcasts, disconnect cleanup and save/reload; Windows release build; public index audit. Vanilla GUI interoperability is only claimed if actually exercised.
