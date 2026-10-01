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
    ${MC_SOURCE}/network/protocol.c
    ${MC_SOURCE}/network/PacketBuffer.c
    ${MC_SOURCE}/network/NetHandlerPlayServer.c
    ${MC_SOURCE}/network/GameplayPacketRouter.c
    ${MC_SOURCE}/client/network/NetHandlerPlayClient.c
    ${MC_SOURCE}/network/transport.c
    ${MC_SOURCE}/network/play/client/C08PacketPlayerBlockPlacement.c
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
    ${MC_SOURCE}/block/block.c
    ${MC_SOURCE}/inventory/inventory.c
    ${MC_SOURCE}/inventory/InventoryCrafting.c
    ${MC_SOURCE}/inventory/InventoryCraftResult.c
    ${MC_SOURCE}/entity/player/InventoryPlayer.c
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
    ${MC_SOURCE}/util/MCGameplayCrafting.c
    ${MC_SOURCE}/util/MCGameplayStorage.c
    ${MC_SOURCE}/util/MathHelper.c
    ${MC_SOURCE}/world/world.c
    ${MC_SOURCE}/world/map.c
    ${MC_SOURCE}/world/storage/MapData.c
    ${MC_SOURCE}/world/storage.c)
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
    foreach(name protocol packet_placement packet_itemstack inventory_packets server_inventory_packets world item nbt inventory inventory_crafting container crafting map item_entity transfer object_heap nbt_objects itemstack source_container source_crafting source_item_crafting gameplay_graph source_recipes source_dynamic_recipes source_armor_fireworks source_banners source_entity_item source_stats gameplay_owners source_server_handler source_player_windows gameplay_packets source_client_handler source_player_drops gameplay_crafting source_craft_stats gameplay_storage source_data_watcher source_entity_metadata source_mp_stats)
        add_executable(test-${name} tests/test_${name}.c)
        target_link_libraries(test-${name} PRIVATE c919-core)
        c919_warnings(test-${name})
        add_test(NAME ${name} COMMAND test-${name})
    endforeach()
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
endif()
