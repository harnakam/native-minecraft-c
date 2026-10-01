#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-linux
base=src/minecraft/net/minecraft
cc=${CC:-gcc}
archiver=${AR:-ar}
core_objects=
for source in util/NativeStrictMathLog util/NativeStrictMathSqrt util/NativeEntityIDRuntime util/AxisAlignedBB command/CommandResultStats entity/player/PlayerCapabilities world/WorldSettingsGameType network/play/client/C13PacketPlayerAbilities network/play/server/S39PacketPlayerAbilities network/protocol network/transport network/PacketBuffer network/NativePacket network/NetHandlerPlayServer network/GameplayPacketRouter client/network/NetHandlerPlayClient client/multiplayer/PlayerControllerMP client/entity/EntityPlayerSP client/native_runtime network/play/client/C08PacketPlayerBlockPlacement network/play/client/C08PacketPlayerBlockPlacementValue \
    network/play/client/C07PacketPlayerDigging network/play/client/C09PacketHeldItemChange client/gui/GuiIngameHotbar client/native_timer_clock \
    network/play/client/C0DPacketCloseWindow network/play/client/C0EPacketClickWindow \
    network/play/client/C0FPacketConfirmTransaction network/play/client/C10PacketCreativeInventoryAction \
    network/play/server/S2EPacketCloseWindow network/play/server/S2FPacketSetSlot \
    network/play/server/S30PacketWindowItems network/play/server/S32PacketConfirmTransaction \
    network/play/server/S1CPacketEntityMetadata entity/DataWatcher entity/Entity entity/EntityLivingBase util/CombatTracker entity/player/EntityPlayer entity/player/EntityPlayerProfile util/NativeGameProfile util/FoodStats inventory/InventoryBasic inventory/InventoryEnderChest entity/SharedMonsterAttributes entity/ai/attributes/AttributeCollections entity/ai/attributes/AttributeModifier entity/ai/attributes/BaseAttribute entity/ai/attributes/BaseAttributeMap entity/ai/attributes/IAttribute entity/ai/attributes/IAttributeInstance entity/ai/attributes/ModifiableAttributeInstance entity/ai/attributes/RangedAttribute entity/ai/attributes/ServersideAttributeMap \
    entity/EntityUUIDNBT world/world world/storage world/map world/WorldDataStorage world/storage/MapStorage world/storage/SaveDataMemoryStorage network/play/client/C03PacketPlayer network/play/client/C0BPacketEntityAction world/storage/MapData nbt/nbt \
    nbt/NBTBase nbt/NBTPrimitive nbt/NBTSizeTracker nbt/NBTString \
    nbt/NBTTagEnd nbt/NBTTagByte nbt/NBTTagShort nbt/NBTTagInt nbt/NBTTagLong \
    nbt/NBTTagFloat nbt/NBTTagDouble nbt/NBTTagString nbt/NBTTagByteArray \
    nbt/NBTTagIntArray nbt/NBTTagList nbt/NBTTagCompound \
    item/item item/ItemMap item/ItemEmptyMap item/ItemStack item/ItemStackCrafting item/ItemStackUse item/ItemAnimation item/ItemStackAnimation item/ItemMapCreated item/ItemMapData item/crafting/RecipeBookCloning block/block \
    item/crafting/IRecipe item/crafting/ShapedRecipes item/crafting/ShapelessRecipes item/crafting/CraftingManager item/crafting/FurnaceRecipes \
    item/crafting/RecipesMapCloning item/crafting/RecipesMapExtending item/crafting/RecipeRepairItem \
    item/crafting/RecipesArmorDyes item/crafting/RecipeFireworks item/ItemArmor \
    item/crafting/RecipesBanners tileentity/EnumBannerPattern tileentity/TileEntityBanner \
    inventory/inventory inventory/InventoryCrafting inventory/InventoryCraftResult \
    inventory/Slot inventory/SlotCrafting inventory/Container inventory/ContainerPlayer \
    inventory/ContainerWorkbench inventory/inventory_dispatch inventory/container_runtime \
    entity/player/InventoryPlayer entity/player/InventoryPlayerAnimations entity/player/EntityPlayerMPWindows entity/player/EntityPlayerMPStats entity/player/EntityPlayerDrops crafting/crafting crafting/value_inventory \
    entity/item/item_entity entity/item/EntityItem entity/item/NativeItemMotion server/native_gameplay server/management/ItemInWorldManagerUse stats/StatBase stats/StatCrafting stats/StatList stats/Achievement stats/StatFileWriter stats/StatisticsFile \
    util/transfer util/MCObjectHeap util/MCGameplay util/MCGameplayWorld util/MCGameplayPlayer util/MCGameplayPackets util/MCPacketQueue util/MCGameplayClientPackets util/MCGameplayCrafting util/MCGameplayStorage util/MovementInput util/MathHelper util/Timer util/NativeJavaRandom util/NativeJavaRandomRuntime util/NativeJavaUUID; do
    object="build-linux/core-$(printf '%s' "$source" | tr / _).o"
    numeric_flags=
    case "$source" in util/NativeJavaRandom|util/NativeStrictMathLog|util/NativeStrictMathSqrt) numeric_flags="-ffp-contract=off -fno-fast-math";; esac
    "$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror $numeric_flags -I"$base" -c "$base/$source.c" -o "$object"
    core_objects="$core_objects $object"
done
rm -f build-linux/libc919-core.a
# Every generated object path is a fixed, whitespace-free build-linux path.
# Intentional word splitting passes each object to the archiver separately.
"$archiver" rcs build-linux/libc919-core.a $core_objects
for target in source_walking source_movement_packets source_entity_actions source_sprinting source_map_storage server_lifetime source_player_constructor source_player_profile source_attributes source_player_leaves source_living_constructor java_gaussian player_capabilities source_entity_constructor source_ability_packets protocol gameplay_client_frame source_packet_placement source_empty_map source_action_packets source_client_actions source_server_use source_map_refs source_inventory_animation source_hotbar source_timer java_random native_random_runtime source_uuid source_uuid_nbt native_item_motion native_packets packet_placement packet_itemstack inventory_packets server_inventory_packets world item nbt inventory inventory_crafting container crafting map item_entity transfer object_heap nbt_objects itemstack source_container source_crafting source_item_crafting gameplay_graph source_recipes source_dynamic_recipes source_armor_fireworks source_banners source_entity_item source_stats gameplay_owners source_server_handler source_player_windows gameplay_packets source_client_handler source_player_drops gameplay_crafting source_craft_stats gameplay_storage source_data_watcher source_entity_metadata source_mp_stats; do
    thread_flags=
    if [ "$target" = native_random_runtime ]; then thread_flags=-pthread; fi
    "$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$base" \
        "tests/test_$target.c" build-linux/libc919-core.a -lz -lm $thread_flags -o "build-linux/test-$target"
    "build-linux/test-$target"
done
"$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$base" \
    tests/test_client_timer.c "$base/client/renderer.c" \
    build-linux/libc919-core.a -lz -lm -o build-linux/test-client-timer
build-linux/test-client-timer
"$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$base" \
    "$base/server/server.c" build-linux/libc919-core.a -lz -lm -o build-linux/c919-server
C919_SERVER="$(pwd)/build-linux/c919-server" python3 tests/test_multiplayer.py
C919_SERVER="$(pwd)/build-linux/c919-server" python3 tests/test_inventory_network.py
C919_SERVER="$(pwd)/build-linux/c919-server" python3 tests/test_drops_crafting_network.py
C919_SERVER="$(pwd)/build-linux/c919-server" python3 tests/test_workbench_network.py
C919_SERVER="$(pwd)/build-linux/c919-server" python3 tests/test_source_graph_network.py
python3 tests/test_public_audit.py
"$cc" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$base" \
    "$base/client/client.c" "$base/client/renderer.c" \
    build-linux/libc919-core.a -lz -lm -o build-linux/c919-client
build-linux/c919-client --self-test
C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client.py
C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client_inventory.py
C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client_entities.py
C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client_containers.py
C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client_source_graph.py
C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client_animation.py

C919_SERVER="$(pwd)/build-linux/c919-server" C919_CLIENT="$(pwd)/build-linux/c919-client" python3 tests/test_client_abilities.py
