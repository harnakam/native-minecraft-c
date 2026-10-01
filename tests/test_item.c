#include "item/item.h"
#include "block/block.h"
#include <stdio.h>
#include <string.h>
#define CHECK(expression) do { if (!(expression)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); return 1; } } while (0)
int main(void) {
    CHECK(!mc_item_valid(-1) && !mc_item_valid(0) && !mc_item_valid(426));
    CHECK(!mc_item_valid(198) && !mc_item_valid(432) && !mc_item_valid(2268));
    CHECK(mc_item_valid(431) && mc_item_valid(2256) && mc_item_valid(2267));
    CHECK(mc_item_stack_limit(276) == 1 && mc_item_stack_limit(368) == 16);
    CHECK(mc_item_stack_limit(5) == 64 && mc_item_stack_limit(387) == 16);
    CHECK(mc_item_stack_limit(-1) == 0 && strcmp(mc_item_name(276), "Diamond Sword") == 0);
    CHECK(strcmp(mc_item_resource_name(276), "minecraft:diamond_sword") == 0);
    int16_t resolved = 7;
    CHECK(mc_item_from_resource_name("minecraft:diamond_sword", &resolved) && resolved == 276);
    CHECK(mc_item_from_resource_name("diamond_sword", &resolved) && resolved == 276);
    CHECK(mc_item_from_resource_name("276", &resolved) && resolved == 276);
    CHECK(mc_item_from_resource_name("minecraft:air", &resolved) && resolved == -1);
    CHECK(!mc_item_from_resource_name("other:diamond_sword", &resolved) && resolved == -1);
    CHECK(!mc_item_from_resource_name("32768", &resolved) && !mc_item_from_resource_name("", &resolved));
    unsigned registered = 0;
    for (int id = 1; id <= 2267; ++id) if (mc_item_valid((int16_t)id)) ++registered;
    CHECK(registered == 337);
    CHECK(mc_item_valid(62) && mc_item_stack_limit(62)==64 && !mc_item_has_subtypes(62));
    CHECK(mc_item_from_resource_name("minecraft:lit_furnace", &resolved) && resolved==62);
    uint16_t state = 12345;
    CHECK(mc_item_block_state(62,0,&state) && state==62*16);
    CHECK(mc_item_block_state(35, 14, &state) && state == (35 * 16 + 14));
    CHECK(mc_item_block_state(17, 3, &state) && state == (17 * 16 + 3));
    CHECK(!mc_item_block_state(17, 4, &state));
    CHECK(mc_item_block_state(162, 1, &state) && state == (162 * 16 + 1));
    CHECK(mc_item_block_state(18, 1, &state) && state == (18 * 16 + 5));
    CHECK(mc_item_block_state(145, 2, &state) && state == (145 * 16 + 8));
    CHECK(mc_item_block_state(330, 0, &state) && state == 71 * 16);
    CHECK(!mc_item_block_state(276, 0, &state) && !mc_item_block_state(35, 16, &state));
    CHECK(!mc_item_block_state(1, -1, &state) && !mc_item_block_state(1, 0, NULL));
    int16_t id, damage;
    CHECK(mc_item_creative_at(0, &id, &damage) && id == 1 && damage == 0);
    CHECK(mc_item_creative_at(6, &id, &damage) && id == 1 && damage == 6);
    CHECK(!mc_item_creative_at(mc_item_creative_count(), &id, &damage));
    for (unsigned i = 0; i < mc_item_creative_count(); ++i) {
        CHECK(mc_item_creative_at(i, &id, &damage) && mc_item_valid(id) && damage >= 0);
    }
    mc_box boxes[3];
    CHECK(mc_block_collision(44 * 16, boxes) == 1 && boxes[0].max_y == 0.5f);
    CHECK(mc_block_collision(44 * 16 + 8, boxes) == 1 && boxes[0].min_y == 0.5f);
    CHECK(mc_block_collision(53 * 16, boxes) == 2 && boxes[1].min_x == 0.5f);
    CHECK(mc_block_collision(78 * 16, boxes) == 0);
    CHECK(mc_block_collision(78 * 16 + 7, boxes) == 1 && boxes[0].max_y == 0.875f);
    CHECK(mc_block_collision(171 * 16, boxes) == 1 && boxes[0].max_y == 0.0625f);
    CHECK(mc_block_collision(107 * 16 + 4, boxes) == 0);
    CHECK(mc_block_replaceable(9 * 16) && !mc_block_replaceable(1 * 16));
    CHECK(mc_block_opaque(1 * 16) && !mc_block_opaque(20 * 16));
    CHECK(!mc_block_valid(198 * 16) && mc_block_valid(197 * 16));
    puts("Legacy item registry and simple collision contracts passed");
    return 0;
}
