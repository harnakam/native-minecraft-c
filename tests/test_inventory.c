#include "inventory/inventory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(v) do { ++checks; if (!(v)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#v); exit(1); } } while (0)

static void put_marker(mc_slot *slot,uint8_t value) {
    const uint8_t bytes[]={10,0,0,1,0,1,'m',value,0};
    mc_buf b; mc_buf_init(&b); mc_put_bytes(&b,bytes,sizeof(bytes)); CHECK(mc_nbt_read(&b,&slot->nbt)); mc_buf_free(&b);
}
static unsigned total(const mc_inventory *inventory,const mc_slot *dropped) {
    unsigned count=inventory->cursor.count;
    for (int i=0;i<MC_PLAYER_INVENTORY_SIZE;i++) count+=inventory->slots[i].count;
    if (dropped) count+=dropped->count;
    return count;
}
static bool same_inventory(const mc_inventory *a,const mc_inventory *b) {
    for (int i=0;i<MC_PLAYER_INVENTORY_SIZE;i++) if (!mc_slot_equal(&a->slots[i],&b->slots[i])) return false;
    return mc_slot_equal(&a->cursor,&b->cursor) && a->drag_active==b->drag_active &&
           a->drag_mode==b->drag_mode && a->drag_slots==b->drag_slots;
}
static void test_slot_codec(void) {
    mc_slot a,b; mc_slot_init(&a); mc_slot_init(&b); CHECK(a.item_id == -1 && !a.count && !a.nbt.size);
    CHECK(mc_slot_set(&a,387,1,0)); put_marker(&a,7);
    mc_buf wire; mc_buf_init(&wire); CHECK(mc_slot_write(&wire,&a));
    const uint8_t expected[]={1,0x83,1,0,0,10,0,0,1,0,1,'m',7,0};
    CHECK(wire.len == sizeof(expected) && !memcmp(wire.data,expected,sizeof(expected)));
    CHECK(mc_slot_read(&wire,&b) && mc_slot_equal(&a,&b) && b.nbt.data != a.nbt.data);
    CHECK(!mc_slot_set(&b,1,128,0) && mc_slot_equal(&a,&b));
    CHECK(!mc_slot_set(&b,1,1,-1) && mc_slot_equal(&a,&b));
    CHECK(!mc_slot_set(&b,1,0,0)); CHECK(!mc_slot_set(&b,2000,1,0));
    for (size_t length=0;length<sizeof(expected);length++) {
        mc_buf short_wire; mc_buf_init(&short_wire); mc_put_bytes(&short_wire,expected,length);
        CHECK(!mc_slot_read(&short_wire,&b) && short_wire.failed && short_wire.pos == 0 && mc_slot_equal(&a,&b));
        mc_buf_free(&short_wire);
    }
    mc_buf_clear(&wire); mc_put_i16(&wire,1); mc_put_u8(&wire,0); mc_put_i16(&wire,0); mc_put_u8(&wire,0);
    CHECK(!mc_slot_read(&wire,&b) && wire.pos == 0 && mc_slot_equal(&a,&b));
    mc_buf_clear(&wire); mc_put_i16(&wire,1); mc_put_u8(&wire,1); mc_put_i16(&wire,0);
    const uint8_t wrong_root[]={1,0,0,9}; mc_put_bytes(&wire,wrong_root,sizeof(wrong_root));
    CHECK(!mc_slot_read(&wire,&b) && wire.pos == 0 && mc_slot_equal(&a,&b));
    mc_buf_clear(&wire); mc_put_i16(&wire,-1); CHECK(mc_slot_read(&wire,&b) && b.item_id == -1 && !b.nbt.data);
    CHECK(mc_slot_set(&b,-1,0,0)); CHECK(mc_slot_set(&a,1,64,0)); CHECK(mc_slot_set(&b,1,1,0));
    CHECK(mc_slot_can_stack(&a,&b)); put_marker(&a,1); put_marker(&b,2); CHECK(!mc_slot_can_stack(&a,&b));
    mc_slot_free(&a); mc_slot_free(&b); mc_buf_free(&wire);
}
static void test_normal_click_and_returns(void) {
    mc_inventory inventory; mc_inventory_init(&inventory); mc_slot dropped,returned; mc_slot_init(&dropped); mc_slot_init(&returned);
    CHECK(mc_slot_set(&inventory.slots[9],1,5,0));
    CHECK(mc_inventory_click_result(&inventory,9,1,0,&returned,&dropped));
    CHECK(inventory.cursor.count == 3 && inventory.slots[9].count == 2 && returned.count == 5 && !dropped.count);
    CHECK(mc_inventory_click(&inventory,10,1,0,&dropped)); CHECK(inventory.cursor.count == 2 && inventory.slots[10].count == 1);
    CHECK(mc_inventory_click(&inventory,9,0,0,&dropped)); CHECK(inventory.slots[9].count == 4 && inventory.cursor.item_id == -1);
    CHECK(total(&inventory,&dropped) == 5);
    CHECK(mc_slot_set(&inventory.cursor,2,3,0)); CHECK(mc_inventory_click(&inventory,9,1,0,&dropped));
    CHECK(inventory.cursor.item_id == 1 && inventory.cursor.count == 4 && inventory.slots[9].item_id == 2 && inventory.slots[9].count == 3);
    CHECK(mc_inventory_click(&inventory,-999,1,0,&dropped)); CHECK(dropped.item_id == 1 && dropped.count == 1 && inventory.cursor.count == 3);
    CHECK(mc_inventory_click(&inventory,-999,0,0,&dropped)); CHECK(dropped.count == 3 && inventory.cursor.item_id == -1);
    mc_slot_free(&returned); mc_slot_free(&dropped); mc_inventory_free(&inventory);
}
static void test_stack_limits_shift_and_armor(void) {
    mc_inventory inventory; mc_inventory_init(&inventory); mc_slot dropped,returned; mc_slot_init(&dropped); mc_slot_init(&returned);
    CHECK(mc_slot_set(&inventory.slots[9],332,15,0)); CHECK(mc_slot_set(&inventory.cursor,332,3,0));
    CHECK(mc_inventory_click(&inventory,9,0,0,&dropped)); CHECK(inventory.slots[9].count == 16 && inventory.cursor.count == 2);
    mc_slot_free(&inventory.cursor); CHECK(mc_slot_set(&inventory.slots[10],1,30,0)); CHECK(mc_slot_set(&inventory.slots[36],1,60,0));
    CHECK(mc_inventory_click_result(&inventory,10,0,1,&returned,&dropped));
    CHECK(returned.item_id == 1 && returned.count == 30 && inventory.slots[10].item_id == -1 && inventory.slots[36].count == 64 && inventory.slots[37].count == 26);
    CHECK(mc_slot_set(&inventory.slots[11],298,1,4)); CHECK(mc_inventory_click(&inventory,11,0,1,&dropped));
    CHECK(inventory.slots[5].item_id == 298 && inventory.slots[11].item_id == -1);
    CHECK(mc_slot_set(&inventory.cursor,1,4,0)); unsigned before=total(&inventory,NULL);
    CHECK(mc_inventory_click(&inventory,6,0,0,&dropped)); CHECK(inventory.slots[6].item_id == -1 && inventory.cursor.count == 4 && total(&inventory,NULL) == before);
    CHECK(!mc_inventory_accepts_slot(5,&inventory.cursor)); mc_slot_free(&inventory.cursor);
    CHECK(mc_slot_set(&inventory.cursor,86,4,0)); CHECK(mc_inventory_accepts_slot(5,&inventory.cursor)); CHECK(mc_inventory_slot_limit(5,&inventory.cursor) == 1);
    CHECK(mc_inventory_click(&inventory,5,0,0,&dropped)); CHECK(inventory.slots[5].item_id == 298 && inventory.cursor.item_id == 86 && inventory.cursor.count == 4);
    CHECK(mc_inventory_click(&inventory,5,0,1,&dropped)); CHECK(inventory.slots[5].item_id == -1);
    CHECK(mc_inventory_click(&inventory,5,0,0,&dropped)); CHECK(inventory.slots[5].item_id == 86 && inventory.slots[5].count == 1 && inventory.cursor.count == 3);
    mc_slot_free(&returned); mc_slot_free(&dropped); mc_inventory_free(&inventory);
}
static void test_hotbar_drop_clone_and_rollback(void) {
    mc_inventory inventory,before; mc_inventory_init(&inventory); mc_inventory_init(&before); mc_slot dropped; mc_slot_init(&dropped);
    CHECK(mc_slot_set(&inventory.slots[9],1,8,0)); CHECK(mc_slot_set(&inventory.slots[36],2,4,0)); put_marker(&inventory.slots[9],3);
    CHECK(mc_inventory_click(&inventory,9,0,2,&dropped)); CHECK(inventory.slots[9].item_id == 2 && inventory.slots[36].item_id == 1);
    CHECK(mc_inventory_click(&inventory,36,0,4,&dropped)); CHECK(dropped.count == 1 && inventory.slots[36].count == 7 && dropped.nbt.size);
    CHECK(mc_inventory_click(&inventory,36,1,4,&dropped)); CHECK(dropped.count == 7 && inventory.slots[36].item_id == -1);
    CHECK(mc_inventory_click(&inventory,9,2,3,&dropped)); CHECK(inventory.cursor.item_id == 2 && inventory.cursor.count == 64);
    CHECK(mc_inventory_copy(&before,&inventory)); const int invalid[][3]={{45,0,0},{-1,0,0},{9,9,2},{9,0,7},{0,0,0},{9,3,5}};
    for (size_t i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        CHECK(!mc_inventory_click(&inventory,invalid[i][0],invalid[i][1],invalid[i][2],&dropped)); CHECK(same_inventory(&inventory,&before));
    }
    CHECK(mc_inventory_copy(&inventory,&inventory)); mc_inventory_free(&before); mc_inventory_free(&inventory); mc_slot_free(&dropped);
}
static void test_drag_distribution_and_double_collect(void) {
    mc_inventory inventory; mc_inventory_init(&inventory); mc_slot dropped; mc_slot_init(&dropped);
    CHECK(mc_slot_set(&inventory.cursor,1,10,0)); CHECK(mc_inventory_click(&inventory,-999,0,5,&dropped));
    CHECK(mc_inventory_click(&inventory,9,1,5,&dropped)); CHECK(mc_inventory_click(&inventory,10,1,5,&dropped)); CHECK(mc_inventory_click(&inventory,11,1,5,&dropped));
    CHECK(mc_inventory_click(&inventory,11,1,5,&dropped)); /* Selecting twice does not change the divisor. */
    CHECK(mc_inventory_click(&inventory,-999,2,5,&dropped));
    CHECK(inventory.slots[9].count == 3 && inventory.slots[10].count == 3 && inventory.slots[11].count == 3 && inventory.cursor.count == 1 && !inventory.drag_active);
    CHECK(total(&inventory,NULL) == 10);
    CHECK(mc_inventory_click(&inventory,-999,4,5,&dropped)); CHECK(mc_inventory_click(&inventory,12,5,5,&dropped));
    CHECK(mc_inventory_click(&inventory,13,5,5,&dropped)); CHECK(mc_inventory_click(&inventory,-999,6,5,&dropped));
    CHECK(inventory.slots[12].count == 1 && inventory.slots[13].item_id == -1 && inventory.cursor.item_id == -1 && total(&inventory,NULL) == 10);
    CHECK(mc_slot_set(&inventory.cursor,1,1,0)); CHECK(mc_slot_set(&inventory.slots[14],1,60,0));
    CHECK(mc_slot_set(&inventory.slots[15],1,5,0)); put_marker(&inventory.slots[15],9);
    CHECK(mc_inventory_click(&inventory,16,0,6,&dropped)); CHECK(inventory.cursor.count == 64 && inventory.slots[15].count == 5 && inventory.slots[14].count == 7);
    mc_slot_free(&inventory.cursor); CHECK(mc_slot_set(&inventory.cursor,1,64,0));
    CHECK(mc_inventory_click(&inventory,-999,8,5,&dropped)); CHECK(mc_inventory_click(&inventory,20,9,5,&dropped)); CHECK(mc_inventory_click(&inventory,21,9,5,&dropped));
    CHECK(mc_inventory_click(&inventory,-999,10,5,&dropped)); CHECK(inventory.slots[20].count == 64 && inventory.slots[21].count == 64 && inventory.cursor.item_id == -1);
    mc_inventory_free(&inventory); mc_slot_free(&dropped);
}
static void test_oversized_wire_and_existing_inventory(void) {
    mc_slot item,decoded,dropped; mc_slot_init(&item); mc_slot_init(&decoded); mc_slot_init(&dropped);
    mc_buf wire; mc_buf_init(&wire);
    CHECK(mc_slot_set(&item,276,2,9)); put_marker(&item,4);
    CHECK(mc_slot_write(&wire,&item) && mc_slot_read(&wire,&decoded) && mc_slot_equal(&item,&decoded));
    CHECK(mc_slot_set(&item,332,127,0)); mc_buf_clear(&wire);
    CHECK(mc_slot_write(&wire,&item) && mc_slot_read(&wire,&decoded) && decoded.count==127);
    CHECK(!mc_slot_set(&item,332,128,0) && item.count==127);
    wire.pos=0; wire.data[2]=128;
    CHECK(!mc_slot_read(&wire,&decoded) && wire.pos==0 && decoded.count==127);
    item.count=128; mc_buf_clear(&wire);
    CHECK(!mc_slot_write(&wire,&item) && !wire.len); mc_slot_free(&item);

    mc_inventory inventory; mc_inventory_init(&inventory);
    CHECK(mc_slot_set(&inventory.cursor,276,2,9)); put_marker(&inventory.cursor,4);
    CHECK(mc_inventory_click(&inventory,9,0,0,&dropped));
    CHECK(inventory.slots[9].count==1 && inventory.cursor.count==1 && total(&inventory,NULL)==2);
    CHECK(mc_slot_set(&inventory.slots[10],276,2,9)); put_marker(&inventory.slots[10],4);
    CHECK(mc_inventory_click(&inventory,10,0,0,&dropped));
    CHECK(inventory.slots[10].count==2 && inventory.cursor.count==1 && total(&inventory,NULL)==4);
    mc_slot_free(&inventory.cursor);
    /* Creative inventory may store arbitrary stacks in armor slots; taking them
       out is valid, while later normal placement still checks armor capacity. */
    CHECK(mc_slot_set(&inventory.slots[5],1,100,0));
    CHECK(mc_inventory_click(&inventory,5,0,0,&dropped));
    CHECK(inventory.cursor.count==100 && inventory.slots[5].item_id==-1);
    CHECK(mc_inventory_click(&inventory,5,0,0,&dropped));
    CHECK(inventory.cursor.count==100 && inventory.slots[5].item_id==-1);
    CHECK(mc_inventory_click(&inventory,11,0,0,&dropped));
    CHECK(inventory.slots[11].count==64 && inventory.cursor.count==36);
    mc_slot_free(&inventory.cursor);
    CHECK(mc_slot_set(&inventory.slots[36],1,100,0)); CHECK(mc_slot_set(&inventory.slots[12],1,10,0));
    unsigned before=total(&inventory,NULL);
    CHECK(mc_inventory_click(&inventory,12,0,1,&dropped));
    CHECK(inventory.slots[36].count==100 && inventory.slots[37].count==10 && total(&inventory,NULL)==before);
    CHECK(mc_slot_set(&inventory.cursor,1,4,0)); before=total(&inventory,NULL);
    CHECK(mc_inventory_click(&inventory,-999,0,5,&dropped)); CHECK(mc_inventory_click(&inventory,36,1,5,&dropped));
    CHECK(mc_inventory_click(&inventory,13,1,5,&dropped)); CHECK(mc_inventory_click(&inventory,-999,2,5,&dropped));
    CHECK(inventory.slots[36].count==100 && inventory.slots[13].count==4 && total(&inventory,NULL)==before);
    CHECK(mc_slot_set(&inventory.cursor,1,100,0)); before=total(&inventory,NULL);
    CHECK(mc_inventory_click(&inventory,14,0,6,&dropped)); CHECK(inventory.cursor.count==100 && total(&inventory,NULL)==before);
    CHECK(mc_slot_set(&inventory.cursor,1,10,0));
    CHECK(mc_inventory_click(&inventory,-999,0,5,&dropped)); CHECK(mc_inventory_click(&inventory,14,1,5,&dropped));
    CHECK(mc_slot_set(&inventory.slots[14],264,5,0)); before=total(&inventory,NULL);
    CHECK(mc_inventory_click(&inventory,-999,2,5,&dropped));
    CHECK(inventory.slots[14].item_id==264 && inventory.slots[14].count==5 && inventory.cursor.item_id==1 &&
          inventory.cursor.count==10 && total(&inventory,NULL)==before);
    mc_inventory_free(&inventory); mc_slot_free(&item); mc_slot_free(&decoded); mc_slot_free(&dropped); mc_buf_free(&wire);
}
static void test_legacy_shift_overstacks(void) {
    /* Golden outputs checked against actual 1.8.9 ContainerPlayer: an empty
       shift destination receives the whole existing overstack. */
    const int ids[]={310,1,276},counts[]={2,127,2},destinations[]={5,36,36};
    for (unsigned i=0;i<3;i++) {
        mc_inventory inventory; mc_inventory_init(&inventory);
        mc_slot returned,dropped; mc_slot_init(&returned); mc_slot_init(&dropped);
        CHECK(mc_slot_set(&inventory.slots[9],(int16_t)ids[i],(uint8_t)counts[i],7));
        put_marker(&inventory.slots[9],3);
        CHECK(mc_inventory_click_result(&inventory,9,0,1,&returned,&dropped));
        CHECK(inventory.slots[9].item_id==-1 && inventory.slots[destinations[i]].item_id==ids[i]);
        CHECK(inventory.slots[destinations[i]].count==counts[i] && returned.count==counts[i]);
        CHECK(returned.damage==7 && returned.nbt.size && total(&inventory,&dropped)==(unsigned)counts[i]);
        if (i==0) {
            /* Taking it out is permitted; replacing it with a normal click
               still puts only one helmet into the armor slot. */
            CHECK(mc_inventory_click(&inventory,5,0,0,&dropped));
            CHECK(inventory.cursor.count==2 && inventory.slots[5].item_id==-1);
            CHECK(mc_inventory_click(&inventory,5,0,0,&dropped));
            CHECK(inventory.slots[5].count==1 && inventory.cursor.count==1 && total(&inventory,NULL)==2);
        }
        mc_slot_free(&returned); mc_slot_free(&dropped); mc_inventory_free(&inventory);
    }
}

static void test_non_subtype_damage_merge_context(void) {
    mc_inventory inventory; mc_inventory_init(&inventory); mc_slot returned,dropped; mc_slot_init(&returned); mc_slot_init(&dropped);
    CHECK(mc_slot_set(&inventory.slots[9],266,5,7)); CHECK(mc_slot_set(&inventory.slots[36],266,4,8));
    CHECK(mc_inventory_click_result(&inventory,9,0,1,&returned,&dropped));
    /* Actual 1.8.9: shift ignores damage for items without subtypes. Normal
       clicks, drag and collect continue to compare damage exactly. */
    CHECK(inventory.slots[36].count==9 && inventory.slots[36].damage==8 && inventory.slots[9].item_id==-1);
    CHECK(mc_slot_set(&inventory.slots[9],266,5,7)); CHECK(mc_slot_set(&inventory.cursor,266,4,8));
    CHECK(mc_inventory_click(&inventory,9,0,0,&dropped));
    CHECK(inventory.slots[9].count==4 && inventory.slots[9].damage==8 && inventory.cursor.count==5 && inventory.cursor.damage==7);
    mc_slot_free(&returned); mc_slot_free(&dropped); mc_inventory_free(&inventory);
}
int main(void) {
    test_slot_codec(); test_normal_click_and_returns(); test_stack_limits_shift_and_armor();
    test_hotbar_drop_clone_and_rollback(); test_drag_distribution_and_double_collect();
    test_oversized_wire_and_existing_inventory();
    test_legacy_shift_overstacks();
    test_non_subtype_damage_merge_context();
    printf("inventory: %u checks passed\n",checks); return 0;
}
