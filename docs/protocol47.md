# Java Edition protocol 47

C919 targets the Minecraft Java Edition 1.8.9 network protocol, numbered 47. The compatibility boundary is the wire format; the project supplies its own C implementation, world storage, presentation, and generated visuals. It does not distribute game archives, decompiled sources, textures, sounds, language files, or resource packs.

## Transport and data types

TCP is a byte stream. One receive can contain part of a packet or several packets. Every normal frame begins with a VarInt byte length, followed by a VarInt packet ID and its fields. The outer length is limited to three bytes (21 bits). Integers, IEEE floating-point values, and UUID bytes use network byte order. VarInts use seven payload bits per byte, with at most five bytes for a signed 32-bit value; negative values are encoded without ZigZag conversion.

Strings contain a VarInt UTF-8 byte count followed by those bytes. The codec validates UTF-8, requires output space for the terminating zero, and rejects embedded zero bytes. Packet-specific name and message limits are applied by their consumers.

The 1.8 packed block position is an eight-byte value with X in bits 38–63, Y in bits 26–37, and Z in bits 0–25. X and Z are signed 26-bit values; Y is signed 12-bit. This order differs from newer protocol versions.

After Set Compression, each frame body instead starts with a VarInt uncompressed length. Zero means the following packet is uncompressed and below the threshold. A positive length means the remaining bytes are a complete zlib stream for the packet ID and fields. Compression changes at a frame boundary. C919 limits inflated packets to 2 MiB and each receive/send queue to 8 MiB; invalid lengths, malformed zlib, mismatched decompressed sizes, and queue overflow close the connection with a diagnostic. Returned packets own their memory independently of the receive queue.

## Connection states

Packet IDs are scoped by state and direction. C→S means client to server.

| State | Direction / ID | Fields in order |
| --- | --- | --- |
| Handshake | C→S `00` | protocol VarInt (`47`), host String, port u16, next state VarInt (`1` status or `2` login) |
| Status | C→S `00` | no fields |
| Status | S→C `00` | status JSON String |
| Status | both `01` | i64 ping payload, echoed unchanged |
| Login | C→S `00` | username String, up to 16 characters |
| Login | S→C `00` | disconnect JSON String |
| Login | S→C `01` | server ID String, VarInt-length public-key bytes, VarInt-length verification-token bytes |
| Login | C→S `01` | VarInt-length encrypted secret bytes, VarInt-length encrypted token bytes |
| Login | S→C `02` | UUID String with hyphens, username String; next state is Play |
| Login | S→C `03` | compression threshold VarInt; negative disables compression |

TCP end-of-stream ends reads independently of writes. Buffered packets remain available across application ticks, and replies can still be queued and flushed. The connection closes when those packets have been drained and the send queue is empty. An incomplete frame at end-of-stream fails immediately instead of remaining indefinitely incomplete.

The included session uses offline login for local and LAN play. Offline UUIDs are MD5-based version 3 UUIDs of the UTF-8 string `OfflinePlayer:` followed by the case-sensitive username. This convention identifies names; it does not authenticate ownership.

Online-mode servers require the encrypted login exchange, AES-128/CFB8 stream encryption, RSA PKCS#1 v1.5 handling, and a valid account session accepted by the session service. C919 currently reports that requirement instead of pretending to authenticate. Login packet structure alone does not provide online-mode support.

## Play packets used by the implementation

| Direction / ID | Purpose | Fields in order |
| --- | --- | --- |
| both `00` | keep alive | signed VarInt token, echoed unchanged |
| S→C `01` | join world | entity i32, mode u8, dimension i8, difficulty u8, maximum players u8, level type String, reduced debug bool |
| C→S `01` | chat | String, up to 100 characters |
| S→C `02` | chat | JSON String, display position u8 |
| S→C `04` | entity equipment | entity VarInt, equipment slot i16, Slot |
| S→C `05` | spawn position | packed Position |
| C→S `03` | player ground state | bool |
| C→S `04` | player position | X/Y/Z f64, ground bool |
| C→S `05` | player orientation | yaw/pitch f32, ground bool |
| C→S `06` | position and orientation | X/Y/Z f64, yaw/pitch f32, ground bool |
| C→S `07` | digging | action VarInt, Position, face u8 |
| C→S `08` | placement | Position, face u8, Slot, cursor X/Y/Z u8 in sixteenths |
| S→C `08` | authoritative player position | X/Y/Z f64, yaw/pitch f32, relative flags u8 |
| C→S `09` | selected hotbar slot | i16, range 0–8 |
| S→C `0c` | spawn another player | entity VarInt, UUID 16 bytes, X/Y/Z i32, yaw/pitch u8, held item i16, metadata |
| C→S `10` | creative inventory | inventory slot i16, Slot |
| C→S `0e` | click window | window u8, slot i16, button i8, action i16, mode i8, returned Slot |
| C→S `0f` | transaction acknowledgement | window i8, action i16, accepted bool |
| S→C `2f` | set slot | window i8, slot i16, Slot |
| S→C `30` | window items | window u8, count i16, Slots |
| S→C `32` | confirm transaction | window i8, action i16, accepted bool |
| S→C `13` | remove entities | count VarInt, entity VarInts |
| S→C `15` | relative entity movement | entity VarInt, signed X/Y/Z i8 deltas, ground bool |
| S→C `16` | entity orientation | entity VarInt, yaw/pitch u8, ground bool |
| S→C `17` | entity movement and orientation | entity VarInt, signed X/Y/Z i8, yaw/pitch u8, ground bool |
| S→C `18` | entity teleport | entity VarInt, X/Y/Z i32, yaw/pitch u8, ground bool |
| S→C `21` | chunk | chunk X/Z i32, full bool, section mask u16, byte count VarInt, data |
| S→C `22` | several changed blocks | chunk X/Z i32, record count VarInt; each record: horizontal u8, Y u8, state VarInt |
| S→C `23` | changed block | Position, state VarInt |
| S→C `26` | several full chunks | sky-light bool, count VarInt, X/Z i32 and mask u16 for every chunk, then chunk data |
| S→C `2b` | game state change | reason u8, value f32; reason 3 changes the local game mode |
| S→C `38` | player list | action VarInt, count VarInt, action-specific entries |
| S→C `40` | disconnect | JSON String |

An authoritative player position uses relative bits X=`1`, Y=`2`, Z=`4`, yaw=`8`, pitch=`16`. The client applies those offsets and replies with absolute C→S `06`, initially with ground=false. Protocol 47 has no separate teleport confirmation packet. Entity coordinates are fixed-point values in units of 1/32 block; entity angle bytes represent turns in 1/256 increments.

Player-list ADD entries contain UUID, name, property count and properties, game-mode VarInt, latency VarInt, and an optional display JSON String. ADD must precede that player's spawn packet. An empty entity metadata list consists of its terminating byte `7f`. REMOVE entries contain only the UUID.

A Slot begins with an i16 item ID. `-1` is empty. Otherwise it contains item count i8, item damage i16, and an NBT root; byte `00` represents absent NBT. Creative hotbar inventory slots are 36–44. Digging actions are start=`0`, abort=`1`, finish=`2`; faces are down=`0`, up=`1`, north=`2`, south=`3`, west=`4`, east=`5`.

Slot storage preserves positive signed-byte counts through 127, including overstacked items from an external server. A creative mutation accepts counts through 64, while normal placement applies item/slot limits. Window 0 has crafting result 0, inputs 1–4, armor 5–8, main inventory 9–35 and hotbar 36–44. Crafting-result transactions and actual dropped item entities remain unsupported.

The NBT codec handles target-version tags 0–11, bounded to depth 64 and 2 MiB. It preserves the exact named-root encoding and unknown fields. Strings use Java modified UTF-8, with UTF-16 surrogate-pair conversion for text getters. Compound comparisons use names and values rather than encoded field order. Invalid reads leave prior owned data unchanged. Gzip loading validates the complete stream, CRC and size; saves flush an exclusive temporary file before replacing the destination.

Click Window's claimed Slot is the container operation's return value, not the new cursor. A legitimate operation is applied before comparing that return value. A mismatch produces a negative confirmation and authoritative inventory/cursor resync; further mutation stays locked until the acknowledgement matches the original rejected action. Invalid operations are rejected without committing. The native client sends one pending transaction at a time and uses the server's inventory state.

## Chunk layout

A set mask bit includes one 16×16×16 section, ordered from bottom to top. The data groups all included sections' block arrays first, then all block-light arrays, then all sky-light arrays when present, and finally 256 biome bytes for a full chunk. These groups must not be interleaved section by section.

Each block array has 4096 little-endian u16 states. Its index is `(localY << 8) | (localZ << 4) | localX`; a legacy state is `(blockID << 4) | metadata`. Each light array is 2048 bytes with two four-bit values per byte. Overworld chunks have sky light; Nether and End chunks do not. A full chunk with a zero section mask unloads that column. Partial chunks update only included sections. Multi-block records store local X in the upper four horizontal bits and local Z in the lower four.

## Verification and public references

`tests/test_protocol.c` checks literal wire bytes, malformed inputs, compressed frames from an independent zlib encoder, boundary sizes, historical UUID vectors, and real loopback TCP with queued partial sends. It also checks truncated end-of-stream frames and a write-half-closed peer whose 80 requests are processed across ticks and all receive replies before closure. End-to-end interoperability must also use an independent peer: a legitimate Java 1.8.9 installation or a separately installed protocol library. Testing only C919 against itself cannot establish external compatibility.

Public machine-readable packet contracts are maintained by [PrismarineJS](https://github.com/PrismarineJS/minecraft-data/blob/master/data/pc/1.8/protocol.json). Its own [framing](https://github.com/PrismarineJS/node-minecraft-protocol/blob/master/src/transforms/framing.js), [compression](https://github.com/PrismarineJS/node-minecraft-protocol/blob/master/src/transforms/compression.js), and [encrypted-login implementation](https://github.com/PrismarineJS/node-minecraft-protocol/blob/master/src/client/encrypt.js) provide independent interoperability references. These references are not bundled dependencies of C919.
