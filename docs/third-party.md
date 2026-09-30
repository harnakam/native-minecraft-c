# External references and dependencies

The runtime links zlib under its own license; it is not vendored. Windows uses the system Win32 and OpenGL APIs.

The optional interoperability checks use the MIT-licensed PrismarineJS `minecraft-protocol` and `minecraft-data` npm packages installed locally under ignored `.local/interop`. They are not redistributed in this repository.

The original C item registry was checked against the public numerical IDs, stack limits and English labels in [PrismarineJS Minecraft 1.8 item data](https://github.com/PrismarineJS/minecraft-data/blob/master/data/pc/1.8/items.json). Its lookup, placement and enumeration code is original. The raw dataset, Minecraft textures, models, sounds, language files, JARs and MCP source are not bundled. The reference's legacy log and spawn-egg variants were curated rather than accepted as game behaviour: log IDs 17 and 162 have separate species metadata, and creative spawn eggs list spawnable mobs.

Registry availability does not imply implementation of every item's use, crafting, entity or tile-entity behaviour. The compatibility ledger tracks those separately.

The item subtype flags and 2×2 numeric recipe facts were verified with original local 1.8.9 runtime objects through independently written reflection fixtures. Dynamic recipe logic, matching, transactions and rendering are original C code. Private game code, resources, reflection harnesses and raw registries are not redistributed. PrismarineJS recipe data was used as a comparison reference; its inaccurate legacy entries were not adopted as target behaviour.
