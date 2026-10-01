#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-linux
base=src/minecraft/net/minecraft
cc=${CC:-gcc}
archiver=${AR:-ar}
core_objects=
for source in network/protocol network/transport network/PacketBuffer network/NetHandlerPlayServer network/GameplayPacketRouter client/network/NetHandlerPlayClient client/multiplayer/PlayerControllerMP network/play/client/C08PacketPlayerBlockPlacement network/play/client/C08PacketPlayerBlockPlacementValue \
    network/play/client/C0DPacketCloseWindow network/play/client/C0EPacketClickWindow \
    network/play/client/C0FPacketConfirmTransaction network/play/client/C10PacketCreativeInventoryAction \
    network/play/server/S2EPacketCloseWindow network/play/server/S2FPacketSetSlot \
    network/play/server/S30PacketWindowItems network/play/server/S32PacketConfirmTransaction \
    network/play/server/S1CPacketEntityMetadata entity/DataWatcher entity/Entity \
    world/world world/storage world/map world/storage/MapData nbt/nbt \
    nbt/NBTBase nbt/NBTPrimitive nbt/NBTSizeTracker nbt/NBTString \
    nbt/NBTTagEnd nbt/NBTTagByte nbt/NBTTagShort nbt/NBTTagInt nbt/NBTTagLong \
    nbt/NBTTagFloat nbt/NBTTagDouble nbt/NBTTagString nbt/NBTTagByteArray \
    nbt/NBTTagIntArray nbt/NBTTagList nbt/NBTTagCompound \
    item/item item/ItemMap item/ItemEmptyMap item/ItemStack item/ItemStackCrafting item/ItemMapCreated item/ItemMapData item/crafting/RecipeBookCloning block/block \
    item/crafting/IRecipe item/crafting/ShapedRecipes item/crafting/ShapelessRecipes item/crafting/CraftingManager item/crafting/FurnaceRecipes \
    item/crafting/RecipesMapCloning item/crafting/RecipesMapExtending item/crafting/RecipeRepairItem \
    item/crafting/RecipesArmorDyes item/crafting/RecipeFireworks item/ItemArmor \
    item/crafting/RecipesBanners tileentity/EnumBannerPattern tileentity/TileEntityBanner \
    inventory/inventory inventory/InventoryCrafting inventory/InventoryCraftResult \
    inventory/Slot inventory/SlotCrafting inventory/Container inventory/ContainerPlayer \
    inventory/ContainerWorkbench inventory/inventory_dispatch inventory/container_runtime \
    entity/player/InventoryPlayer entity/player/EntityPlayerMPWindows entity/player/EntityPlayerMPStats entity/player/EntityPlayerDrops crafting/crafting crafting/value_inventory \
    entity/item/item_entity entity/item/EntityItem stats/StatBase stats/StatCrafting stats/StatList stats/Achievement stats/StatFileWriter stats/StatisticsFile \
    util/transfer util/MCObjectHeap util/MCGameplay util/MCGameplayWorld util/MCGameplayPlayer util/MCGameplayPackets util/MCPacketQueue util/MCGameplayClientPackets util/MCGameplayCrafting util/MCGameplayStorage util/MathHelper; do
    object="build-linux/core-$(printf '%s' "$source" | tr / _).o"
    "$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$base" -c "$base/$source.c" -o "$object"
    core_objects="$core_objects $object"
done
rm -f build-linux/libc919-core.a
# Every generated object path is a fixed, whitespace-free build-linux path.
# Intentional word splitting passes each object to the archiver separately.
"$archiver" rcs build-linux/libc919-core.a $core_objects
for target in protocol gameplay_client_frame source_packet_placement source_empty_map packet_placement packet_itemstack inventory_packets server_inventory_packets world item nbt inventory inventory_crafting container crafting map item_entity transfer object_heap nbt_objects itemstack source_container source_crafting source_item_crafting gameplay_graph source_recipes source_dynamic_recipes source_armor_fireworks source_banners source_entity_item source_stats gameplay_owners source_server_handler source_player_windows gameplay_packets source_client_handler source_player_drops gameplay_crafting source_craft_stats gameplay_storage source_data_watcher source_entity_metadata source_mp_stats; do
    "$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$base" \
        "tests/test_$target.c" build-linux/libc919-core.a -lz -lm -o "build-linux/test-$target"
    "build-linux/test-$target"
done
"$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$base" \
    "$base/server/server.c" build-linux/libc919-core.a -lz -lm -o build-linux/c919-server
C919_SERVER="$(pwd)/build-linux/c919-server" python3 tests/test_multiplayer.py
C919_SERVER="$(pwd)/build-linux/c919-server" python3 tests/test_inventory_network.py
C919_SERVER="$(pwd)/build-linux/c919-server" python3 tests/test_drops_crafting_network.py
C919_SERVER="$(pwd)/build-linux/c919-server" python3 tests/test_workbench_network.py
python3 tests/test_public_audit.py
"$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$base" \
    "$base/client/client.c" "$base/client/renderer.c" \
    build-linux/libc919-core.a -lz -lm -o build-linux/c919-client
build-linux/c919-client --self-test
C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client.py
C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client_inventory.py
C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client_entities.py
C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client_containers.py
