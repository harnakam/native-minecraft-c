cmake_minimum_required(VERSION 3.20)
project(c919 VERSION 0.4.0 LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
include(CTest)
set(MC_SOURCE src/minecraft/net/minecraft)
set(ZLIB_USE_STATIC_LIBS ON)
find_package(ZLIB REQUIRED)
add_library(c919-core STATIC
    ${MC_SOURCE}/util/NativeStrictMathLog.c
    ${MC_SOURCE}/util/NativeStrictMathSqrt.c
    ${MC_SOURCE}/util/NativeEntityIDRuntime.c
    ${MC_SOURCE}/util/AxisAlignedBB.c
    ${MC_SOURCE}/command/CommandResultStats.c
    ${MC_SOURCE}/entity/player/PlayerCapabilities.c
    ${MC_SOURCE}/world/WorldSettingsGameType.c
    ${MC_SOURCE}/network/play/client/C13PacketPlayerAbilities.c
    ${MC_SOURCE}/network/play/server/S39PacketPlayerAbilities.c
    ${MC_SOURCE}/network/protocol.c
    ${MC_SOURCE}/network/PacketBuffer.c
    ${MC_SOURCE}/network/NativePacket.c
    ${MC_SOURCE}/network/NetHandlerPlayServer.c
    ${MC_SOURCE}/network/GameplayPacketRouter.c
    ${MC_SOURCE}/client/network/NetHandlerPlayClient.c
    ${MC_SOURCE}/client/multiplayer/PlayerControllerMP.c
    ${MC_SOURCE}/client/entity/AbstractClientPlayer.c
    ${MC_SOURCE}/client/entity/EntityPlayerSP.c
    ${MC_SOURCE}/client/gui/GuiIngameHotbar.c
    ${MC_SOURCE}/client/native_runtime.c
    ${MC_SOURCE}/client/native_timer_clock.c
    ${MC_SOURCE}/network/transport.c
    ${MC_SOURCE}/network/play/client/C08PacketPlayerBlockPlacement.c
    ${MC_SOURCE}/network/play/client/C07PacketPlayerDigging.c
    ${MC_SOURCE}/network/play/client/C09PacketHeldItemChange.c
    ${MC_SOURCE}/network/play/client/C08PacketPlayerBlockPlacementValue.c
    ${MC_SOURCE}/network/play/client/C0DPacketCloseWindow.c
    ${MC_SOURCE}/network/play/client/C0EPacketClickWindow.c
    ${MC_SOURCE}/network/play/client/C0FPacketConfirmTransaction.c
    ${MC_SOURCE}/network/play/client/C10PacketCreativeInventoryAction.c
    ${MC_SOURCE}/network/play/server/S2EPacketCloseWindow.c
    ${MC_SOURCE}/network/play/server/S2FPacketSetSlot.c
    ${MC_SOURCE}/network/play/server/S30PacketWindowItems.c
    ${MC_SOURCE}/network/play/server/S32PacketConfirmTransaction.c
    ${MC_SOURCE}/network/play/server/S1CPacketEntityMetadata.c
    ${MC_SOURCE}/entity/DataWatcher.c
    ${MC_SOURCE}/entity/Entity.c
    ${MC_SOURCE}/entity/EntityLivingBase.c
    ${MC_SOURCE}/util/CombatTracker.c
    ${MC_SOURCE}/entity/player/EntityPlayer.c
    ${MC_SOURCE}/entity/player/EntityPlayerMP.c
    ${MC_SOURCE}/entity/player/EntityPlayerProfile.c
    ${MC_SOURCE}/util/NativeGameProfile.c
    ${MC_SOURCE}/util/FoodStats.c
    ${MC_SOURCE}/inventory/InventoryBasic.c
    ${MC_SOURCE}/inventory/InventoryEnderChest.c
    ${MC_SOURCE}/entity/SharedMonsterAttributes.c
    ${MC_SOURCE}/entity/ai/attributes/AttributeCollections.c
    ${MC_SOURCE}/entity/ai/attributes/AttributeModifier.c
    ${MC_SOURCE}/entity/ai/attributes/BaseAttribute.c
    ${MC_SOURCE}/entity/ai/attributes/BaseAttributeMap.c
    ${MC_SOURCE}/entity/ai/attributes/IAttribute.c
    ${MC_SOURCE}/entity/ai/attributes/IAttributeInstance.c
    ${MC_SOURCE}/entity/ai/attributes/ModifiableAttributeInstance.c
    ${MC_SOURCE}/entity/ai/attributes/RangedAttribute.c
    ${MC_SOURCE}/entity/ai/attributes/ServersideAttributeMap.c
    ${MC_SOURCE}/entity/EntityUUIDNBT.c
    ${MC_SOURCE}/nbt/nbt.c
    ${MC_SOURCE}/nbt/NBTBase.c
    ${MC_SOURCE}/nbt/NBTPrimitive.c
    ${MC_SOURCE}/nbt/NBTSizeTracker.c
    ${MC_SOURCE}/nbt/NBTString.c
    ${MC_SOURCE}/nbt/NBTTagEnd.c
    ${MC_SOURCE}/nbt/NBTTagByte.c
    ${MC_SOURCE}/nbt/NBTTagShort.c
    ${MC_SOURCE}/nbt/NBTTagInt.c
    ${MC_SOURCE}/nbt/NBTTagLong.c
    ${MC_SOURCE}/nbt/NBTTagFloat.c
    ${MC_SOURCE}/nbt/NBTTagDouble.c
    ${MC_SOURCE}/nbt/NBTTagString.c
    ${MC_SOURCE}/nbt/NBTTagByteArray.c
    ${MC_SOURCE}/nbt/NBTTagIntArray.c
    ${MC_SOURCE}/nbt/NBTTagList.c
    ${MC_SOURCE}/nbt/NBTTagCompound.c
    ${MC_SOURCE}/item/item.c
    ${MC_SOURCE}/item/ItemStack.c
    ${MC_SOURCE}/item/ItemStackCrafting.c
    ${MC_SOURCE}/item/ItemStackUse.c
    ${MC_SOURCE}/item/ItemAnimation.c
    ${MC_SOURCE}/item/ItemStackAnimation.c
    ${MC_SOURCE}/item/ItemMapCreated.c
    ${MC_SOURCE}/item/ItemMapData.c
    ${MC_SOURCE}/item/crafting/RecipeBookCloning.c
    ${MC_SOURCE}/item/crafting/IRecipe.c
    ${MC_SOURCE}/item/crafting/ShapedRecipes.c
    ${MC_SOURCE}/item/crafting/ShapelessRecipes.c
    ${MC_SOURCE}/item/crafting/CraftingManager.c
    ${MC_SOURCE}/item/crafting/FurnaceRecipes.c
    ${MC_SOURCE}/item/crafting/RecipesMapCloning.c
    ${MC_SOURCE}/item/crafting/RecipesMapExtending.c
    ${MC_SOURCE}/item/crafting/RecipeRepairItem.c
    ${MC_SOURCE}/item/crafting/RecipesArmorDyes.c
    ${MC_SOURCE}/item/crafting/RecipeFireworks.c
    ${MC_SOURCE}/item/crafting/RecipesBanners.c
    ${MC_SOURCE}/tileentity/EnumBannerPattern.c
    ${MC_SOURCE}/tileentity/TileEntityBanner.c
    ${MC_SOURCE}/item/ItemArmor.c
    ${MC_SOURCE}/item/ItemMap.c
    ${MC_SOURCE}/item/ItemEmptyMap.c
    ${MC_SOURCE}/block/block.c
    ${MC_SOURCE}/inventory/inventory.c
    ${MC_SOURCE}/inventory/InventoryCrafting.c
    ${MC_SOURCE}/inventory/InventoryCraftResult.c
    ${MC_SOURCE}/entity/player/InventoryPlayer.c
    ${MC_SOURCE}/entity/player/InventoryPlayerAnimations.c
    ${MC_SOURCE}/entity/player/EntityPlayerMPWindows.c
    ${MC_SOURCE}/entity/player/EntityPlayerMPStats.c
    ${MC_SOURCE}/entity/player/EntityPlayerDrops.c
    ${MC_SOURCE}/inventory/Slot.c
    ${MC_SOURCE}/inventory/SlotCrafting.c
    ${MC_SOURCE}/inventory/Container.c
    ${MC_SOURCE}/inventory/ContainerPlayer.c
    ${MC_SOURCE}/inventory/ContainerWorkbench.c
    ${MC_SOURCE}/inventory/inventory_dispatch.c
    ${MC_SOURCE}/inventory/container_runtime.c
    ${MC_SOURCE}/crafting/crafting.c
    ${MC_SOURCE}/crafting/value_inventory.c
    ${MC_SOURCE}/entity/item/item_entity.c
    ${MC_SOURCE}/entity/item/EntityItem.c
    ${MC_SOURCE}/entity/item/NativeItemMotion.c
    ${MC_SOURCE}/server/native_gameplay.c
    ${MC_SOURCE}/server/management/ItemInWorldManagerUse.c
    ${MC_SOURCE}/stats/StatBase.c
    ${MC_SOURCE}/stats/StatCrafting.c
    ${MC_SOURCE}/stats/StatList.c
    ${MC_SOURCE}/stats/Achievement.c
    ${MC_SOURCE}/stats/StatFileWriter.c
    ${MC_SOURCE}/stats/StatisticsFile.c
    ${MC_SOURCE}/util/transfer.c
    ${MC_SOURCE}/util/MCObjectHeap.c
    ${MC_SOURCE}/util/MCGameplay.c
    ${MC_SOURCE}/util/MCGameplayWorld.c
    ${MC_SOURCE}/util/MCGameplayPlayer.c
    ${MC_SOURCE}/util/MCGameplayPackets.c
    ${MC_SOURCE}/util/MCPacketQueue.c
    ${MC_SOURCE}/util/MCGameplayClientPackets.c
    ${MC_SOURCE}/util/MCGameplayCrafting.c
    ${MC_SOURCE}/util/MCGameplayStorage.c
    ${MC_SOURCE}/util/MovementInput.c
    ${MC_SOURCE}/util/MathHelper.c
    ${MC_SOURCE}/util/BlockPos.c
    ${MC_SOURCE}/util/NativeReferenceList.c
    ${MC_SOURCE}/util/NativeWallClock.c
    ${MC_SOURCE}/world/border/WorldBorder.c
    ${MC_SOURCE}/server/management/ItemInWorldManager.c
    ${MC_SOURCE}/util/NativeJavaRandom.c
    ${MC_SOURCE}/util/NativeJavaRandomRuntime.c
    ${MC_SOURCE}/util/NativeJavaUUID.c
    ${MC_SOURCE}/util/Timer.c
    ${MC_SOURCE}/world/NativeWorld.c
    ${MC_SOURCE}/world/map.c
    ${MC_SOURCE}/world/WorldDataStorage.c
    ${MC_SOURCE}/world/storage/MapStorage.c
    ${MC_SOURCE}/world/storage/SaveDataMemoryStorage.c
    ${MC_SOURCE}/network/play/client/C03PacketPlayer.c
    ${MC_SOURCE}/network/play/client/C0BPacketEntityAction.c
    ${MC_SOURCE}/world/storage/MapData.c
    ${MC_SOURCE}/world/storage.c
    ${MC_SOURCE}/scoreboard/IScoreObjectiveCriteria.c
    ${MC_SOURCE}/scoreboard/Score.c
    ${MC_SOURCE}/scoreboard/ScoreDummyCriteria.c
    ${MC_SOURCE}/scoreboard/ScoreObjective.c
    ${MC_SOURCE}/scoreboard/Scoreboard.c
    ${MC_SOURCE}/stats/ObjectiveStat.c
    ${MC_SOURCE}/util/IntHashMap.c
    ${MC_SOURCE}/util/NativeCalendar.c
    ${MC_SOURCE}/util/NativeFloatArray.c
    ${MC_SOURCE}/util/NativeHashMap.c
    ${MC_SOURCE}/util/NativeHashSet.c
    ${MC_SOURCE}/util/NativeJavaNumber.c
    ${MC_SOURCE}/util/NativeJavaString.c
    ${MC_SOURCE}/util/NativePrimitiveArray.c
    ${MC_SOURCE}/util/NativeSortedStringMap.c
    ${MC_SOURCE}/world/ChunkCoordIntPair.c
    ${MC_SOURCE}/world/EnumDifficulty.c
    ${MC_SOURCE}/world/GameRules.c
    ${MC_SOURCE}/world/World.c
    ${MC_SOURCE}/world/WorldProvider.c
    ${MC_SOURCE}/world/WorldProviderEnd.c
    ${MC_SOURCE}/world/WorldProviderHell.c
    ${MC_SOURCE}/world/WorldProviderSurface.c
    ${MC_SOURCE}/world/WorldSettings.c
    ${MC_SOURCE}/world/WorldType.c
    ${MC_SOURCE}/world/storage/WorldInfo.c
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${MC_SOURCE}/util/NativeJavaRandom.c
        ${MC_SOURCE}/util/NativeStrictMathLog.c ${MC_SOURCE}/util/NativeStrictMathSqrt.c
        ${MC_SOURCE}/world/WorldProvider.c ${MC_SOURCE}/world/WorldProviderHell.c
        PROPERTIES COMPILE_OPTIONS "-ffp-contract=off;-fno-fast-math")
endif()
target_include_directories(c919-core PUBLIC ${MC_SOURCE})
target_link_libraries(c919-core PUBLIC ZLIB::ZLIB)
if(WIN32)
    target_link_libraries(c919-core PUBLIC ws2_32)
    target_compile_definitions(c919-core PUBLIC WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601)
else()
    target_link_libraries(c919-core PUBLIC m)
endif()
function(c919_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()
c919_warnings(c919-core)
add_executable(c919-server ${MC_SOURCE}/server/server.c)
target_link_libraries(c919-server PRIVATE c919-core)
c919_warnings(c919-server)
add_executable(c919-client ${MC_SOURCE}/client/client.c ${MC_SOURCE}/client/renderer.c)
target_link_libraries(c919-client PRIVATE c919-core)
c919_warnings(c919-client)
if(WIN32)
    target_link_libraries(c919-client PRIVATE opengl32 gdi32 user32)
endif()
if(BUILD_TESTING)
    foreach(name source_world_constructor source_world_info source_world_provider source_int_hash_map source_scoreboard native_hash_map native_hash_set native_world_views native_java_number native_sorted_string_map gameplay_world_indexes source_player_mp_runtime source_player_mp_constructor source_item_manager source_world_border source_player_sp_constructor source_client_bootstrap source_walking source_movement_packets source_entity_actions source_sprinting source_map_storage server_lifetime source_player_constructor source_player_profile source_attributes source_player_leaves source_living_constructor java_gaussian player_capabilities source_entity_constructor source_ability_packets protocol gameplay_client_frame source_packet_placement source_empty_map source_action_packets source_client_actions source_server_use source_map_refs source_inventory_animation source_hotbar source_timer java_random native_random_runtime source_uuid source_uuid_nbt native_item_motion native_packets packet_placement packet_itemstack inventory_packets server_inventory_packets world item nbt inventory inventory_crafting container crafting map item_entity transfer object_heap nbt_objects itemstack source_container source_crafting source_item_crafting gameplay_graph source_recipes source_dynamic_recipes source_armor_fireworks source_banners source_entity_item source_stats gameplay_owners source_server_handler source_player_windows gameplay_packets source_client_handler source_player_drops gameplay_crafting source_craft_stats gameplay_storage source_data_watcher source_entity_metadata source_mp_stats)
        add_executable(test-${name} tests/test_${name}.c)
        target_link_libraries(test-${name} PRIVATE c919-core)
        c919_warnings(test-${name})
        add_test(NAME ${name} COMMAND test-${name})
    endforeach()
    if(UNIX)
        find_package(Threads REQUIRED)
        target_link_libraries(test-native_random_runtime PRIVATE Threads::Threads)
    endif()
    # This fixture exercises the actual client's internal frame/input seams.
    # Its source includes client.c once and links the real renderer separately.
    add_executable(test-client-timer tests/test_client_timer.c ${MC_SOURCE}/client/renderer.c)
    target_link_libraries(test-client-timer PRIVATE c919-core)
    if(WIN32)
        target_link_libraries(test-client-timer PRIVATE opengl32 gdi32 user32)
    endif()
    c919_warnings(test-client-timer)
    add_test(NAME client-timer COMMAND test-client-timer)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME multiplayer COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_multiplayer.py)
    set_tests_properties(multiplayer PROPERTIES ENVIRONMENT "C919_SERVER=$<TARGET_FILE:c919-server>" TIMEOUT 90)
    add_test(NAME inventory-network COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_inventory_network.py)
    set_tests_properties(inventory-network PROPERTIES ENVIRONMENT "C919_SERVER=$<TARGET_FILE:c919-server>" TIMEOUT 90)
    add_test(NAME drops-crafting-network COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_drops_crafting_network.py)
    set_tests_properties(drops-crafting-network PROPERTIES ENVIRONMENT "C919_SERVER=$<TARGET_FILE:c919-server>" TIMEOUT 90)
    add_test(NAME workbench-network COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_workbench_network.py)
    set_tests_properties(workbench-network PROPERTIES ENVIRONMENT "C919_SERVER=$<TARGET_FILE:c919-server>" TIMEOUT 90)
    add_test(NAME public-audit COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_public_audit.py)
    set_tests_properties(public-audit PROPERTIES TIMEOUT 60)
    add_test(NAME client-self-test COMMAND c919-client --self-test)
    add_test(NAME client-network COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_client.py)
    set_tests_properties(client-network PROPERTIES ENVIRONMENT "C919_SERVER=$<TARGET_FILE:c919-server>;C919_CLIENT=$<TARGET_FILE:c919-client>" TIMEOUT 60)
    add_test(NAME client-inventory COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_client_inventory.py)
    set_tests_properties(client-inventory PROPERTIES ENVIRONMENT "C919_SERVER=$<TARGET_FILE:c919-server>;C919_CLIENT=$<TARGET_FILE:c919-client>" TIMEOUT 60)
    add_test(NAME client-entities COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_client_entities.py)
    set_tests_properties(client-entities PROPERTIES ENVIRONMENT "C919_SERVER=$<TARGET_FILE:c919-server>;C919_CLIENT=$<TARGET_FILE:c919-client>" TIMEOUT 90)
    add_test(NAME client-containers COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_client_containers.py)
    set_tests_properties(client-containers PROPERTIES ENVIRONMENT "C919_SERVER=$<TARGET_FILE:c919-server>;C919_CLIENT=$<TARGET_FILE:c919-client>" TIMEOUT 90)
    add_test(NAME client-source-graph COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_client_source_graph.py)
    set_tests_properties(client-source-graph PROPERTIES ENVIRONMENT "C919_SERVER=$<TARGET_FILE:c919-server>;C919_CLIENT=$<TARGET_FILE:c919-client>" TIMEOUT 60)
    add_test(NAME client-animation COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_client_animation.py)
    set_tests_properties(client-animation PROPERTIES ENVIRONMENT "C919_SERVER=$<TARGET_FILE:c919-server>;C919_CLIENT=$<TARGET_FILE:c919-client>" TIMEOUT 60)
    add_test(NAME source-graph-network COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_source_graph_network.py)
    set_tests_properties(source-graph-network PROPERTIES ENVIRONMENT "C919_SERVER=$<TARGET_FILE:c919-server>" TIMEOUT 60)
    add_test(NAME client-abilities COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_client_abilities.py)
    set_tests_properties(client-abilities PROPERTIES ENVIRONMENT "C919_SERVER=$<TARGET_FILE:c919-server>;C919_CLIENT=$<TARGET_FILE:c919-client>" TIMEOUT 90)
endif()
