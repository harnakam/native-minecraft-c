#include "item.h"
#include <stddef.h>
#include <string.h>
/* Legacy registry facts independently curated from public protocol data.
   See docs/third-party.md. This file contains no game resources or MCP code. */
typedef struct {
    int16_t id;
    uint8_t stack_limit;
    const char *name;
    const char *resource_name;
    uint8_t variants;
    int16_t metadata[32];
} item_definition;
static const item_definition items[] = {
    {1, 64, "Stone", "minecraft:stone", 7, {0, 1, 2, 3, 4, 5, 6}},
    {2, 64, "Grass Block", "minecraft:grass", 1, {0}},
    {3, 64, "Dirt", "minecraft:dirt", 3, {0, 1, 2}},
    {4, 64, "Cobblestone", "minecraft:cobblestone", 1, {0}},
    {5, 64, "Wooden Planks", "minecraft:planks", 6, {0, 1, 2, 3, 4, 5}},
    {6, 64, "Sapling", "minecraft:sapling", 6, {0, 1, 2, 3, 4, 5}},
    {7, 64, "Bedrock", "minecraft:bedrock", 1, {0}},
    {12, 64, "Sand", "minecraft:sand", 2, {0, 1}},
    {13, 64, "Gravel", "minecraft:gravel", 1, {0}},
    {14, 64, "Gold Ore", "minecraft:gold_ore", 1, {0}},
    {15, 64, "Iron Ore", "minecraft:iron_ore", 1, {0}},
    {16, 64, "Coal Ore", "minecraft:coal_ore", 1, {0}},
    {17, 64, "Wood", "minecraft:log", 4, {0, 1, 2, 3}},
    {18, 64, "Leaves", "minecraft:leaves", 4, {0, 1, 2, 3}},
    {19, 64, "Sponge", "minecraft:sponge", 2, {0, 1}},
    {20, 64, "Glass", "minecraft:glass", 1, {0}},
    {21, 64, "Lapis Lazuli Ore", "minecraft:lapis_ore", 1, {0}},
    {22, 64, "Lapis Lazuli Block", "minecraft:lapis_block", 1, {0}},
    {23, 64, "Dispenser", "minecraft:dispenser", 1, {0}},
    {24, 64, "Sandstone", "minecraft:sandstone", 3, {0, 1, 2}},
    {25, 64, "Note Block", "minecraft:noteblock", 1, {0}},
    {27, 64, "Powered Rail", "minecraft:golden_rail", 1, {0}},
    {28, 64, "Detector Rail", "minecraft:detector_rail", 1, {0}},
    {29, 64, "Sticky Piston", "minecraft:sticky_piston", 1, {0}},
    {30, 64, "Cobweb", "minecraft:web", 1, {0}},
    {31, 64, "Grass", "minecraft:tallgrass", 3, {0, 1, 2}},
    {32, 64, "Dead Bush", "minecraft:deadbush", 1, {0}},
    {33, 64, "Piston", "minecraft:piston", 1, {0}},
    {35, 64, "Wool", "minecraft:wool", 16, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
    {37, 64, "Dandelion", "minecraft:yellow_flower", 1, {0}},
    {38, 64, "Poppy", "minecraft:red_flower", 9, {0, 1, 2, 3, 4, 5, 6, 7, 8}},
    {39, 64, "Brown Mushroom", "minecraft:brown_mushroom", 1, {0}},
    {40, 64, "Red Mushroom", "minecraft:red_mushroom", 1, {0}},
    {41, 64, "Block of Gold", "minecraft:gold_block", 1, {0}},
    {42, 64, "Block of Iron", "minecraft:iron_block", 1, {0}},
    {44, 64, "Stone Slab", "minecraft:stone_slab", 8, {0, 1, 2, 3, 4, 5, 6, 7}},
    {45, 64, "Brick", "minecraft:brick_block", 1, {0}},
    {46, 64, "TNT", "minecraft:tnt", 1, {0}},
    {47, 64, "Bookshelf", "minecraft:bookshelf", 1, {0}},
    {48, 64, "Moss Stone", "minecraft:mossy_cobblestone", 1, {0}},
    {49, 64, "Obsidian", "minecraft:obsidian", 1, {0}},
    {50, 64, "Torch", "minecraft:torch", 1, {0}},
    {52, 64, "Monster Spawner", "minecraft:mob_spawner", 1, {0}},
    {53, 64, "Oak Wood Stairs", "minecraft:oak_stairs", 1, {0}},
    {54, 64, "Chest", "minecraft:chest", 1, {0}},
    {56, 64, "Diamond Ore", "minecraft:diamond_ore", 1, {0}},
    {57, 64, "Block of Diamond", "minecraft:diamond_block", 1, {0}},
    {58, 64, "Crafting Table", "minecraft:crafting_table", 1, {0}},
    {60, 64, "Farmland", "minecraft:farmland", 1, {0}},
    {61, 64, "Furnace", "minecraft:furnace", 1, {0}},
    {62, 64, "Furnace", "minecraft:lit_furnace", 0, {0}},
    {65, 64, "Ladder", "minecraft:ladder", 1, {0}},
    {66, 64, "Rail", "minecraft:rail", 1, {0}},
    {67, 64, "Cobblestone Stairs", "minecraft:stone_stairs", 1, {0}},
    {69, 64, "Lever", "minecraft:lever", 1, {0}},
    {70, 64, "Stone Pressure Plate", "minecraft:stone_pressure_plate", 1, {0}},
    {72, 64, "Wooden Pressure Plate", "minecraft:wooden_pressure_plate", 1, {0}},
    {73, 64, "Redstone Ore", "minecraft:redstone_ore", 1, {0}},
    {76, 64, "Redstone Torch", "minecraft:redstone_torch", 1, {0}},
    {77, 64, "Stone Button", "minecraft:stone_button", 1, {0}},
    {78, 64, "Snow", "minecraft:snow_layer", 1, {0}},
    {79, 64, "Ice", "minecraft:ice", 1, {0}},
    {80, 64, "Snow", "minecraft:snow", 1, {0}},
    {81, 64, "Cactus", "minecraft:cactus", 1, {0}},
    {82, 64, "Clay", "minecraft:clay", 1, {0}},
    {84, 64, "Jukebox", "minecraft:jukebox", 1, {0}},
    {85, 64, "Oak Fence", "minecraft:fence", 1, {0}},
    {86, 64, "Pumpkin", "minecraft:pumpkin", 1, {0}},
    {87, 64, "Netherrack", "minecraft:netherrack", 1, {0}},
    {88, 64, "Soul Sand", "minecraft:soul_sand", 1, {0}},
    {89, 64, "Glowstone", "minecraft:glowstone", 1, {0}},
    {91, 64, "Jack o'Lantern", "minecraft:lit_pumpkin", 1, {0}},
    {95, 64, "Stained Glass", "minecraft:stained_glass", 16, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
    {96, 64, "Wooden Trapdoor", "minecraft:trapdoor", 1, {0}},
    {97, 64, "Monster Egg", "minecraft:monster_egg", 6, {0, 1, 2, 3, 4, 5}},
    {98, 64, "Stone Bricks", "minecraft:stonebrick", 4, {0, 1, 2, 3}},
    {99, 64, "Brown Mushroom Block", "minecraft:brown_mushroom_block", 1, {0}},
    {100, 64, "Red Mushroom Block", "minecraft:red_mushroom_block", 1, {0}},
    {101, 64, "Iron Bars", "minecraft:iron_bars", 1, {0}},
    {102, 64, "Glass Pane", "minecraft:glass_pane", 1, {0}},
    {103, 64, "Melon", "minecraft:melon_block", 1, {0}},
    {106, 64, "Vines", "minecraft:vine", 1, {0}},
    {107, 64, "Oak Fence Gate", "minecraft:fence_gate", 1, {0}},
    {108, 64, "Brick Stairs", "minecraft:brick_stairs", 1, {0}},
    {109, 64, "Stone Brick Stairs", "minecraft:stone_brick_stairs", 1, {0}},
    {110, 64, "Mycelium", "minecraft:mycelium", 1, {0}},
    {111, 64, "Lily Pad", "minecraft:waterlily", 1, {0}},
    {112, 64, "Nether Brick", "minecraft:nether_brick", 1, {0}},
    {113, 64, "Nether Brick Fence", "minecraft:nether_brick_fence", 1, {0}},
    {114, 64, "Nether Brick Stairs", "minecraft:nether_brick_stairs", 1, {0}},
    {116, 64, "Enchantment Table", "minecraft:enchanting_table", 1, {0}},
    {120, 64, "End Portal Frame", "minecraft:end_portal_frame", 1, {0}},
    {121, 64, "End Stone", "minecraft:end_stone", 1, {0}},
    {122, 64, "Dragon Egg", "minecraft:dragon_egg", 1, {0}},
    {123, 64, "Redstone Lamp", "minecraft:redstone_lamp", 1, {0}},
    {126, 64, "Wood Slab", "minecraft:wooden_slab", 6, {0, 1, 2, 3, 4, 5}},
    {128, 64, "Sandstone Stairs", "minecraft:sandstone_stairs", 1, {0}},
    {129, 64, "Emerald Ore", "minecraft:emerald_ore", 1, {0}},
    {130, 64, "Ender Chest", "minecraft:ender_chest", 1, {0}},
    {131, 64, "Tripwire Hook", "minecraft:tripwire_hook", 1, {0}},
    {133, 64, "Block of Emerald", "minecraft:emerald_block", 1, {0}},
    {134, 64, "Spruce Wood Stairs", "minecraft:spruce_stairs", 1, {0}},
    {135, 64, "Birch Wood Stairs", "minecraft:birch_stairs", 1, {0}},
    {136, 64, "Jungle Wood Stairs", "minecraft:jungle_stairs", 1, {0}},
    {137, 64, "Command Block", "minecraft:command_block", 1, {0}},
    {138, 64, "Beacon", "minecraft:beacon", 1, {0}},
    {139, 64, "Cobblestone Wall", "minecraft:cobblestone_wall", 2, {0, 1}},
    {143, 64, "Wooden Button", "minecraft:wooden_button", 1, {0}},
    {145, 64, "Anvil", "minecraft:anvil", 3, {0, 1, 2}},
    {146, 64, "Trapped Chest", "minecraft:trapped_chest", 1, {0}},
    {147, 64, "Weighted Pressure Plate (Light)", "minecraft:light_weighted_pressure_plate", 1, {0}},
    {148, 64, "Weighted Pressure Plate (Heavy)", "minecraft:heavy_weighted_pressure_plate", 1, {0}},
    {151, 64, "Daylight Detector", "minecraft:daylight_detector", 1, {0}},
    {152, 64, "Block of Redstone", "minecraft:redstone_block", 1, {0}},
    {153, 64, "Nether Quartz", "minecraft:quartz_ore", 1, {0}},
    {154, 64, "Hopper", "minecraft:hopper", 1, {0}},
    {155, 64, "Block of Quartz", "minecraft:quartz_block", 3, {0, 1, 2}},
    {156, 64, "Quartz Stairs", "minecraft:quartz_stairs", 1, {0}},
    {157, 64, "Activator Rail", "minecraft:activator_rail", 1, {0}},
    {158, 64, "Dropper", "minecraft:dropper", 1, {0}},
    {159, 64, "Stained Clay", "minecraft:stained_hardened_clay", 16, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
    {160, 64, "Stained Glass Pane", "minecraft:stained_glass_pane", 16, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
    {161, 64, "Leaves", "minecraft:leaves2", 2, {0, 1}},
    {162, 64, "Wood", "minecraft:log2", 2, {0, 1}},
    {163, 64, "Acacia Wood Stairs", "minecraft:acacia_stairs", 1, {0}},
    {164, 64, "Dark Oak Wood Stairs", "minecraft:dark_oak_stairs", 1, {0}},
    {165, 64, "Slime Block", "minecraft:slime", 1, {0}},
    {166, 64, "Barrier", "minecraft:barrier", 1, {0}},
    {167, 64, "Iron Trapdoor", "minecraft:iron_trapdoor", 1, {0}},
    {168, 64, "Prismarine", "minecraft:prismarine", 3, {0, 1, 2}},
    {169, 64, "Sea Lantern", "minecraft:sea_lantern", 1, {0}},
    {170, 64, "Hay Bale", "minecraft:hay_block", 1, {0}},
    {171, 64, "Carpet", "minecraft:carpet", 16, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
    {172, 64, "Hardened Clay", "minecraft:hardened_clay", 1, {0}},
    {173, 64, "Block of Coal", "minecraft:coal_block", 1, {0}},
    {174, 64, "Packed Ice", "minecraft:packed_ice", 1, {0}},
    {175, 64, "Large Flowers", "minecraft:double_plant", 6, {0, 1, 2, 3, 4, 5}},
    {179, 64, "Red Sandstone", "minecraft:red_sandstone", 3, {0, 1, 2}},
    {180, 64, "Red Sandstone Stairs", "minecraft:red_sandstone_stairs", 1, {0}},
    {182, 64, "Red Sandstone Slab", "minecraft:stone_slab2", 1, {0}},
    {183, 64, "Spruce Fence Gate", "minecraft:spruce_fence_gate", 1, {0}},
    {184, 64, "Birch Fence Gate", "minecraft:birch_fence_gate", 1, {0}},
    {185, 64, "Jungle Fence Gate", "minecraft:jungle_fence_gate", 1, {0}},
    {186, 64, "Dark Oak Fence Gate", "minecraft:dark_oak_fence_gate", 1, {0}},
    {187, 64, "Acacia Fence Gate", "minecraft:acacia_fence_gate", 1, {0}},
    {188, 64, "Spruce Fence", "minecraft:spruce_fence", 1, {0}},
    {189, 64, "Birch Fence", "minecraft:birch_fence", 1, {0}},
    {190, 64, "Jungle Fence", "minecraft:jungle_fence", 1, {0}},
    {191, 64, "Dark Oak Fence", "minecraft:dark_oak_fence", 1, {0}},
    {192, 64, "Acacia Fence", "minecraft:acacia_fence", 1, {0}},
    {256, 1, "Iron Shovel", "minecraft:iron_shovel", 1, {0}},
    {257, 1, "Iron Pickaxe", "minecraft:iron_pickaxe", 1, {0}},
    {258, 1, "Iron Axe", "minecraft:iron_axe", 1, {0}},
    {259, 1, "Flint and Steel", "minecraft:flint_and_steel", 1, {0}},
    {260, 64, "Apple", "minecraft:apple", 1, {0}},
    {261, 1, "Bow", "minecraft:bow", 1, {0}},
    {262, 64, "Arrow", "minecraft:arrow", 1, {0}},
    {263, 64, "Coal", "minecraft:coal", 2, {0, 1}},
    {264, 64, "Diamond", "minecraft:diamond", 1, {0}},
    {265, 64, "Iron Ingot", "minecraft:iron_ingot", 1, {0}},
    {266, 64, "Gold Ingot", "minecraft:gold_ingot", 1, {0}},
    {267, 1, "Iron Sword", "minecraft:iron_sword", 1, {0}},
    {268, 1, "Wooden Sword", "minecraft:wooden_sword", 1, {0}},
    {269, 1, "Wooden Shovel", "minecraft:wooden_shovel", 1, {0}},
    {270, 1, "Wooden Pickaxe", "minecraft:wooden_pickaxe", 1, {0}},
    {271, 1, "Wooden Axe", "minecraft:wooden_axe", 1, {0}},
    {272, 1, "Stone Sword", "minecraft:stone_sword", 1, {0}},
    {273, 1, "Stone Shovel", "minecraft:stone_shovel", 1, {0}},
    {274, 1, "Stone Pickaxe", "minecraft:stone_pickaxe", 1, {0}},
    {275, 1, "Stone Axe", "minecraft:stone_axe", 1, {0}},
    {276, 1, "Diamond Sword", "minecraft:diamond_sword", 1, {0}},
    {277, 1, "Diamond Shovel", "minecraft:diamond_shovel", 1, {0}},
    {278, 1, "Diamond Pickaxe", "minecraft:diamond_pickaxe", 1, {0}},
    {279, 1, "Diamond Axe", "minecraft:diamond_axe", 1, {0}},
    {280, 64, "Stick", "minecraft:stick", 1, {0}},
    {281, 64, "Bowl", "minecraft:bowl", 1, {0}},
    {282, 1, "Mushroom Stew", "minecraft:mushroom_stew", 1, {0}},
    {283, 1, "Golden Sword", "minecraft:golden_sword", 1, {0}},
    {284, 1, "Golden Shovel", "minecraft:golden_shovel", 1, {0}},
    {285, 1, "Golden Pickaxe", "minecraft:golden_pickaxe", 1, {0}},
    {286, 1, "Golden Axe", "minecraft:golden_axe", 1, {0}},
    {287, 64, "String", "minecraft:string", 1, {0}},
    {288, 64, "Feather", "minecraft:feather", 1, {0}},
    {289, 64, "Gunpowder", "minecraft:gunpowder", 1, {0}},
    {290, 1, "Wooden Hoe", "minecraft:wooden_hoe", 1, {0}},
    {291, 1, "Stone Hoe", "minecraft:stone_hoe", 1, {0}},
    {292, 1, "Iron Hoe", "minecraft:iron_hoe", 1, {0}},
    {293, 1, "Diamond Hoe", "minecraft:diamond_hoe", 1, {0}},
    {294, 1, "Golden Hoe", "minecraft:golden_hoe", 1, {0}},
    {295, 64, "Seeds", "minecraft:wheat_seeds", 1, {0}},
    {296, 64, "Wheat", "minecraft:wheat", 1, {0}},
    {297, 64, "Bread", "minecraft:bread", 1, {0}},
    {298, 1, "Leather Cap", "minecraft:leather_helmet", 1, {0}},
    {299, 1, "Leather Tunic", "minecraft:leather_chestplate", 1, {0}},
    {300, 1, "Leather Pants", "minecraft:leather_leggings", 1, {0}},
    {301, 1, "Leather Boots", "minecraft:leather_boots", 1, {0}},
    {302, 1, "Chain Helmet", "minecraft:chainmail_helmet", 1, {0}},
    {303, 1, "Chain Chestplate", "minecraft:chainmail_chestplate", 1, {0}},
    {304, 1, "Chain Leggings", "minecraft:chainmail_leggings", 1, {0}},
    {305, 1, "Chain Boots", "minecraft:chainmail_boots", 1, {0}},
    {306, 1, "Iron Helmet", "minecraft:iron_helmet", 1, {0}},
    {307, 1, "Iron Chestplate", "minecraft:iron_chestplate", 1, {0}},
    {308, 1, "Iron Leggings", "minecraft:iron_leggings", 1, {0}},
    {309, 1, "Iron Boots", "minecraft:iron_boots", 1, {0}},
    {310, 1, "Diamond Helmet", "minecraft:diamond_helmet", 1, {0}},
    {311, 1, "Diamond Chestplate", "minecraft:diamond_chestplate", 1, {0}},
    {312, 1, "Diamond Leggings", "minecraft:diamond_leggings", 1, {0}},
    {313, 1, "Diamond Boots", "minecraft:diamond_boots", 1, {0}},
    {314, 1, "Golden Helmet", "minecraft:golden_helmet", 1, {0}},
    {315, 1, "Golden Chestplate", "minecraft:golden_chestplate", 1, {0}},
    {316, 1, "Golden Leggings", "minecraft:golden_leggings", 1, {0}},
    {317, 1, "Golden Boots", "minecraft:golden_boots", 1, {0}},
    {318, 64, "Flint", "minecraft:flint", 1, {0}},
    {319, 64, "Raw Porkchop", "minecraft:porkchop", 1, {0}},
    {320, 64, "Cooked Porkchop", "minecraft:cooked_porkchop", 1, {0}},
    {321, 64, "Painting", "minecraft:painting", 1, {0}},
    {322, 64, "Golden Apple", "minecraft:golden_apple", 2, {0, 1}},
    {323, 16, "Sign", "minecraft:sign", 1, {0}},
    {324, 64, "Oak Door", "minecraft:wooden_door", 1, {0}},
    {325, 16, "Bucket", "minecraft:bucket", 1, {0}},
    {326, 1, "Water Bucket", "minecraft:water_bucket", 1, {0}},
    {327, 1, "Lava Bucket", "minecraft:lava_bucket", 1, {0}},
    {328, 1, "Minecart", "minecraft:minecart", 1, {0}},
    {329, 1, "Saddle", "minecraft:saddle", 1, {0}},
    {330, 64, "Iron Door", "minecraft:iron_door", 1, {0}},
    {331, 64, "Redstone", "minecraft:redstone", 1, {0}},
    {332, 16, "Snowball", "minecraft:snowball", 1, {0}},
    {333, 1, "Boat", "minecraft:boat", 1, {0}},
    {334, 64, "Leather", "minecraft:leather", 1, {0}},
    {335, 1, "Milk", "minecraft:milk_bucket", 1, {0}},
    {336, 64, "Brick", "minecraft:brick", 1, {0}},
    {337, 64, "Clay", "minecraft:clay_ball", 1, {0}},
    {338, 64, "Sugar Canes", "minecraft:reeds", 1, {0}},
    {339, 64, "Paper", "minecraft:paper", 1, {0}},
    {340, 64, "Book", "minecraft:book", 1, {0}},
    {341, 64, "Slimeball", "minecraft:slime_ball", 1, {0}},
    {342, 1, "Minecart with Chest", "minecraft:chest_minecart", 1, {0}},
    {343, 1, "Minecart with Furnace", "minecraft:furnace_minecart", 1, {0}},
    {344, 16, "Egg", "minecraft:egg", 1, {0}},
    {345, 64, "Compass", "minecraft:compass", 1, {0}},
    {346, 1, "Fishing Rod", "minecraft:fishing_rod", 1, {0}},
    {347, 64, "Clock", "minecraft:clock", 1, {0}},
    {348, 64, "Glowstone Dust", "minecraft:glowstone_dust", 1, {0}},
    {349, 64, "Fish", "minecraft:fish", 4, {0, 1, 2, 3}},
    {350, 64, "Cooked Fish", "minecraft:cooked_fish", 2, {0, 1}},
    {351, 64, "Dye", "minecraft:dye", 16, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
    {352, 64, "Bone", "minecraft:bone", 1, {0}},
    {353, 64, "Sugar", "minecraft:sugar", 1, {0}},
    {354, 1, "Cake", "minecraft:cake", 1, {0}},
    {355, 1, "Bed", "minecraft:bed", 1, {0}},
    {356, 64, "Redstone Repeater", "minecraft:repeater", 1, {0}},
    {357, 64, "Cookie", "minecraft:cookie", 1, {0}},
    {358, 64, "Map", "minecraft:filled_map", 1, {0}},
    {359, 1, "Shears", "minecraft:shears", 1, {0}},
    {360, 64, "Melon", "minecraft:melon", 1, {0}},
    {361, 64, "Pumpkin Seeds", "minecraft:pumpkin_seeds", 1, {0}},
    {362, 64, "Melon Seeds", "minecraft:melon_seeds", 1, {0}},
    {363, 64, "Raw Beef", "minecraft:beef", 1, {0}},
    {364, 64, "Steak", "minecraft:cooked_beef", 1, {0}},
    {365, 64, "Raw Chicken", "minecraft:chicken", 1, {0}},
    {366, 64, "Cooked Chicken", "minecraft:cooked_chicken", 1, {0}},
    {367, 64, "Rotten Flesh", "minecraft:rotten_flesh", 1, {0}},
    {368, 16, "Ender Pearl", "minecraft:ender_pearl", 1, {0}},
    {369, 64, "Blaze Rod", "minecraft:blaze_rod", 1, {0}},
    {370, 64, "Ghast Tear", "minecraft:ghast_tear", 1, {0}},
    {371, 64, "Gold Nugget", "minecraft:gold_nugget", 1, {0}},
    {372, 64, "Nether Wart", "minecraft:nether_wart", 1, {0}},
    {373, 1, "Potion", "minecraft:potion", 1, {0}},
    {374, 64, "Glass Bottle", "minecraft:glass_bottle", 1, {0}},
    {375, 64, "Spider Eye", "minecraft:spider_eye", 1, {0}},
    {376, 64, "Fermented Spider Eye", "minecraft:fermented_spider_eye", 1, {0}},
    {377, 64, "Blaze Powder", "minecraft:blaze_powder", 1, {0}},
    {378, 64, "Magma Cream", "minecraft:magma_cream", 1, {0}},
    {379, 64, "Brewing Stand", "minecraft:brewing_stand", 1, {0}},
    {380, 64, "Cauldron", "minecraft:cauldron", 1, {0}},
    {381, 64, "Eye of Ender", "minecraft:ender_eye", 1, {0}},
    {382, 64, "Glistering Melon", "minecraft:speckled_melon", 1, {0}},
    {383, 64, "Spawn Egg", "minecraft:spawn_egg", 27, {50, 51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 65, 66, 67, 68, 90, 91, 92, 93, 94, 95, 96, 98, 100, 101, 120}},
    {384, 64, "Bottle o' Enchanting", "minecraft:experience_bottle", 1, {0}},
    {385, 64, "Fire Charge", "minecraft:fire_charge", 1, {0}},
    {386, 1, "Book and Quill", "minecraft:writable_book", 1, {0}},
    {387, 16, "Written Book", "minecraft:written_book", 1, {0}},
    {388, 64, "Emerald", "minecraft:emerald", 1, {0}},
    {389, 64, "Item Frame", "minecraft:item_frame", 1, {0}},
    {390, 64, "Flower Pot", "minecraft:flower_pot", 1, {0}},
    {391, 64, "Carrot", "minecraft:carrot", 1, {0}},
    {392, 64, "Potato", "minecraft:potato", 1, {0}},
    {393, 64, "Baked Potato", "minecraft:baked_potato", 1, {0}},
    {394, 64, "Poisonous Potato", "minecraft:poisonous_potato", 1, {0}},
    {395, 64, "Empty Map", "minecraft:map", 1, {0}},
    {396, 64, "Golden Carrot", "minecraft:golden_carrot", 1, {0}},
    {397, 64, "Skull", "minecraft:skull", 5, {0, 1, 2, 3, 4}},
    {398, 1, "Carrot on a Stick", "minecraft:carrot_on_a_stick", 1, {0}},
    {399, 64, "Nether Star", "minecraft:nether_star", 1, {0}},
    {400, 64, "Pumpkin Pie", "minecraft:pumpkin_pie", 1, {0}},
    {401, 64, "Firework Rocket", "minecraft:fireworks", 1, {0}},
    {402, 64, "Firework Star", "minecraft:firework_charge", 1, {0}},
    {403, 1, "Enchanted Book", "minecraft:enchanted_book", 1, {0}},
    {404, 64, "Redstone Comparator", "minecraft:comparator", 1, {0}},
    {405, 64, "Nether Brick", "minecraft:netherbrick", 1, {0}},
    {406, 64, "Nether Quartz", "minecraft:quartz", 1, {0}},
    {407, 1, "Minecart with TNT", "minecraft:tnt_minecart", 1, {0}},
    {408, 1, "Minecart with Hopper", "minecraft:hopper_minecart", 1, {0}},
    {409, 64, "Prismarine Shard", "minecraft:prismarine_shard", 1, {0}},
    {410, 64, "Prismarine Crystals", "minecraft:prismarine_crystals", 1, {0}},
    {411, 64, "Raw Rabbit", "minecraft:rabbit", 1, {0}},
    {412, 64, "Cooked Rabbit", "minecraft:cooked_rabbit", 1, {0}},
    {413, 1, "Rabbit Stew", "minecraft:rabbit_stew", 1, {0}},
    {414, 64, "Rabbit's Foot", "minecraft:rabbit_foot", 1, {0}},
    {415, 64, "Rabbit Hide", "minecraft:rabbit_hide", 1, {0}},
    {416, 16, "Armor Stand", "minecraft:armor_stand", 1, {0}},
    {417, 1, "Iron Horse Armor", "minecraft:iron_horse_armor", 1, {0}},
    {418, 1, "Gold Horse Armor", "minecraft:golden_horse_armor", 1, {0}},
    {419, 1, "Diamond Horse Armor", "minecraft:diamond_horse_armor", 1, {0}},
    {420, 64, "Lead", "minecraft:lead", 1, {0}},
    {421, 64, "Name Tag", "minecraft:name_tag", 1, {0}},
    {422, 1, "Minecart with Command Block", "minecraft:command_block_minecart", 1, {0}},
    {423, 64, "Raw Mutton", "minecraft:mutton", 1, {0}},
    {424, 64, "Cooked Mutton", "minecraft:cooked_mutton", 1, {0}},
    {425, 16, "Banner", "minecraft:banner", 16, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
    {427, 64, "Spruce Door", "minecraft:spruce_door", 1, {0}},
    {428, 64, "Birch Door", "minecraft:birch_door", 1, {0}},
    {429, 64, "Jungle Door", "minecraft:jungle_door", 1, {0}},
    {430, 64, "Acacia Door", "minecraft:acacia_door", 1, {0}},
    {431, 64, "Dark Oak Door", "minecraft:dark_oak_door", 1, {0}},
    {2256, 1, "13 Disc", "minecraft:record_13", 1, {0}},
    {2257, 1, "Cat Disc", "minecraft:record_cat", 1, {0}},
    {2258, 1, "Blocks Disc", "minecraft:record_blocks", 1, {0}},
    {2259, 1, "Chirp Disc", "minecraft:record_chirp", 1, {0}},
    {2260, 1, "Far Disc", "minecraft:record_far", 1, {0}},
    {2261, 1, "Mall Disc", "minecraft:record_mall", 1, {0}},
    {2262, 1, "Mellohi Disc", "minecraft:record_mellohi", 1, {0}},
    {2263, 1, "Stal Disc", "minecraft:record_stal", 1, {0}},
    {2264, 1, "Strad Disc", "minecraft:record_strad", 1, {0}},
    {2265, 1, "Ward Disc", "minecraft:record_ward", 1, {0}},
    {2266, 1, "11 Disc", "minecraft:record_11", 1, {0}},
    {2267, 1, "Wait Disc", "minecraft:record_wait", 1, {0}},
};
static const item_definition *definition(int16_t id) {
    size_t low = 0, high = sizeof(items) / sizeof(items[0]);
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (items[mid].id < id) low = mid + 1;
        else high = mid;
    }
    return low < sizeof(items) / sizeof(items[0]) && items[low].id == id ? &items[low] : NULL;
}
bool mc_item_valid(int16_t id) { return definition(id) != NULL; }
unsigned mc_item_stack_limit(int16_t id) {
    const item_definition *item = definition(id);
    return item ? item->stack_limit : 0;
}
const char *mc_item_name(int16_t id) {
    const item_definition *item = definition(id);
    return item ? item->name : id == -1 ? "Empty" : "Unknown item";
}
const char *mc_item_resource_name(int16_t id) {
    const item_definition *item = definition(id);
    return item ? item->resource_name : id == -1 ? "minecraft:air" : NULL;
}
bool mc_item_from_resource_name(const char *name, int16_t *id) {
    if (!name || !id) return false;
    if (!strcmp(name, "air") || !strcmp(name, "minecraft:air")) { *id = -1; return true; }
    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        if (!strcmp(name, items[i].resource_name) || !strcmp(name, items[i].resource_name + 10)) {
            *id = items[i].id; return true;
        }
    }
    /* Historical saves also allow the decimal registry ID in a string. */
    unsigned value = 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p) {
        if (*p < '0' || *p > '9' || value > 3276) return false;
        value = value * 10 + (*p - '0');
    }
    if (!*name || value > 32767 || !mc_item_valid((int16_t)value)) return false;
    *id = (int16_t)value; return true;
}
unsigned mc_item_creative_count(void) {
    unsigned count = 0;
    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i) count += items[i].variants;
    return count;
}
/* Boolean registry facts verified independently against protocol47's item set. */
bool mc_item_has_subtypes(int16_t id) {
    switch (id) {
        case 1: case 3: case 5: case 6: case 12: case 17: case 18: case 19:
        case 24: case 31: case 35: case 37: case 38: case 44: case 78: case 95:
        case 97: case 98: case 126: case 139: case 145: case 155: case 159:
        case 160: case 161: case 162: case 168: case 171: case 175: case 179:
        case 182: case 263: case 322: case 349: case 350: case 351: case 358:
        case 373: case 383: case 397: case 425: return true;
        default: return false;
    }
}
bool mc_item_creative_at(unsigned index, int16_t *id, int16_t *damage) {
    if (!id || !damage) return false;
    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        if (index < items[i].variants) { *id = items[i].id; *damage = items[i].metadata[index]; return true; }
        index -= items[i].variants;
    }
    return false;
}
bool mc_item_block_state(int16_t id, int16_t damage, uint16_t *state) {
    if (!state || !mc_item_valid(id) || damage < 0) return false;
    int block = id;
    unsigned metadata = 0;
    if (id < 198) {
        const item_definition *item = definition(id);
        /* Registry-only lit_furnace has no creative-tab entry, but its
           ItemBlock still accepts the base metadata when supplied by a slot. */
        bool valid = id==62 && damage==0;
        for (unsigned i = 0; i < item->variants; ++i)
            if (item->metadata[i] == damage) valid = true;
        if (!valid) return false;
        metadata = (unsigned)damage;
        /* Player-placed leaves do not decay; anvil item damage is not rotation. */
        if (id == 18 || id == 161) metadata |= 4;
        if (id == 145) metadata <<= 2;
    } else {
        switch (id) {
            case 295: block = 59; break;
            case 323: block = 63; break;
            case 324: block = 64; break;
            case 330: block = 71; break;
            case 331: block = 55; break;
            case 338: block = 83; break;
            case 354: block = 92; break;
            case 355: block = 26; break;
            case 356: block = 93; break;
            case 361: block = 104; break;
            case 362: block = 105; break;
            case 372: block = 115; break;
            case 379: block = 117; break;
            case 380: block = 118; break;
            case 390: block = 140; break;
            case 391: block = 141; break;
            case 392: block = 142; break;
            case 397: block = 144; metadata = 1; break;
            case 404: block = 149; break;
            case 425: block = 176; break;
            case 427: block = 193; break;
            case 428: block = 194; break;
            case 429: block = 195; break;
            case 430: block = 196; break;
            case 431: block = 197; break;
            default: return false;
        }
    }
    *state = (uint16_t)((unsigned)block * 16u + metadata);
    return true;
}
