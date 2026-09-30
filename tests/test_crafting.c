#include "crafting/crafting.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static unsigned checks;
#define CHECK(v) do { ++checks; if (!(v)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#v); exit(1); } } while (0)
static void clear(mc_inventory *inventory) { mc_inventory_free(inventory); }
static int64_t field_integer(const mc_nbt *nbt,const char *parent,const char *field) {
    mc_nbt_view root,value; int64_t number=0;
    CHECK(mc_nbt_root(nbt,&root));
    if (parent) CHECK(mc_nbt_find(&root,parent,&root));
    CHECK(mc_nbt_find(&root,field,&value) && mc_nbt_get_integer(&value,&number)); return number;
}
static void metadata(mc_slot *slot,const uint8_t *bytes,size_t count) {
    mc_buf input; mc_buf_init(&input); mc_put_bytes(&input,bytes,count);
    CHECK(mc_nbt_read(&input,&slot->nbt)); mc_buf_free(&input);
}
static void test_preview_shapes_and_colors(void) {
    mc_inventory inventory; mc_inventory_init(&inventory);
    for (int color=0;color<16;color++) {
        clear(&inventory); CHECK(mc_slot_set(&inventory.slots[3],35,9,(int16_t)color));
        CHECK(mc_slot_set(&inventory.slots[4],35,1,(int16_t)color)); CHECK(mc_crafting_update(&inventory));
        CHECK(inventory.slots[0].item_id==171 && inventory.slots[0].count==3 && inventory.slots[0].damage==color);
        CHECK(inventory.slots[3].count==9);
        CHECK(mc_slot_set(&inventory.slots[4],35,1,(int16_t)((color+1)%16))); CHECK(mc_crafting_update(&inventory));
        CHECK(inventory.slots[0].item_id==-1);
        clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],35,1,0)); CHECK(mc_slot_set(&inventory.slots[4],351,64,(int16_t)color));
        CHECK(mc_crafting_update(&inventory)); CHECK(inventory.slots[0].item_id==35 && inventory.slots[0].damage==15-color);
    }
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],265,1,0)); CHECK(mc_slot_set(&inventory.slots[4],265,1,0));
    CHECK(mc_crafting_update(&inventory)); CHECK(inventory.slots[0].item_id==359); /* mirrored shears */
    CHECK(mc_slot_set(&inventory.slots[2],1,1,0)); CHECK(mc_crafting_update(&inventory)); CHECK(inventory.slots[0].item_id==-1);
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[2],5,1,0)); CHECK(mc_slot_set(&inventory.slots[4],5,1,5));
    CHECK(mc_crafting_update(&inventory)); CHECK(inventory.slots[0].item_id==280 && inventory.slots[0].count==4);
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[4],17,127,2)); CHECK(mc_crafting_update(&inventory));
    CHECK(inventory.slots[0].item_id==5 && inventory.slots[0].damage==2 && inventory.slots[0].count==4);
    mc_inventory_free(&inventory);
}
static void test_output_transactions(void) {
    mc_inventory inventory; mc_inventory_init(&inventory);
    mc_slot returned; mc_slot_init(&returned); mc_crafting_effects effects; mc_crafting_effects_init(&effects);
    CHECK(mc_slot_set(&inventory.slots[1],17,2,0));
    CHECK(mc_slot_set(&inventory.slots[0],5,4,0));
    /* Actual 1.8.9: right-click takes all four planks and consumes one log. */
    CHECK(mc_crafting_click(&inventory,0,1,0,&returned,&effects));
    CHECK(inventory.cursor.item_id==5 && inventory.cursor.count==4);
    CHECK(inventory.slots[1].count==1 && returned.count==4);
    CHECK(inventory.slots[0].count==4 && !effects.count);
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],17,2,0)); CHECK(mc_slot_set(&inventory.cursor,5,63,0));
    CHECK(mc_crafting_click(&inventory,0,0,0,&returned,&effects));
    CHECK(inventory.cursor.count==63 && inventory.slots[1].count==2 && returned.count==4);
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],17,3,0));
    CHECK(mc_crafting_click(&inventory,0,0,1,&returned,&effects));
    CHECK(inventory.slots[44].item_id==5 && inventory.slots[44].count==12 && inventory.slots[1].item_id==-1 && returned.count==4);
    clear(&inventory); for (unsigned i=9;i<45;i++) CHECK(mc_slot_set(&inventory.slots[i],1,64,0));
    CHECK(mc_slot_set(&inventory.slots[44],5,63,0)); CHECK(mc_slot_set(&inventory.slots[1],17,2,0));
    CHECK(mc_crafting_click(&inventory,0,0,1,&returned,&effects));
    /* Original 1.8.9 partial shift quirk: inserts one of four, consumes a log,
       discards three. This is intentional target behavior, not a failed save. */
    CHECK(inventory.slots[44].count==64 && inventory.slots[1].count==1 && !effects.count && returned.count==4);
    for (int button=0;button<2;button++) {
        clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],17,2,0));
        CHECK(mc_crafting_click(&inventory,0,button,4,&returned,&effects));
        CHECK(effects.count==1 && effects.dropped[0].count==4 && inventory.slots[1].count==1 && returned.item_id==-1);
    }
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],17,2,0)); CHECK(mc_slot_set(&inventory.slots[36],1,10,0));
    CHECK(mc_crafting_click(&inventory,0,0,2,&returned,&effects));
    CHECK(inventory.slots[36].item_id==5 && inventory.slots[36].count==4 && inventory.slots[37].item_id==1 && inventory.slots[37].count==10);
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],17,2,0)); CHECK(mc_slot_set(&inventory.slots[36],5,32,0));
    CHECK(mc_crafting_click(&inventory,0,0,2,&returned,&effects)); CHECK(inventory.slots[36].count==36);
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],17,2,0));
    CHECK(mc_crafting_click(&inventory,0,2,3,&returned,&effects)); CHECK(inventory.cursor.count==64 && inventory.slots[1].count==2);
    CHECK(mc_crafting_click(&inventory,0,0,6,&returned,&effects)); CHECK(inventory.cursor.count==64 && inventory.slots[1].count==2);
    clear(&inventory); CHECK(mc_slot_set(&inventory.cursor,17,2,0));
    CHECK(mc_crafting_click(&inventory,1,0,0,&returned,&effects)); CHECK(inventory.slots[0].item_id==5 && inventory.slots[1].count==2);
    CHECK(!mc_crafting_click(&inventory,0,9,2,&returned,&effects)); CHECK(inventory.slots[1].count==2);
    CHECK(!mc_crafting_click(&inventory,0,0,0,&inventory.cursor,&effects));
    CHECK(!mc_crafting_click(&inventory,0,1,5,&returned,&effects)); /* No drag-start. */
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],41,1,0)); CHECK(mc_slot_set(&inventory.cursor,266,4,8));
    CHECK(mc_crafting_click(&inventory,0,0,0,&returned,&effects)); CHECK(inventory.cursor.count==13 && inventory.cursor.damage==8 && inventory.slots[1].item_id==-1);
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],41,1,0)); CHECK(mc_slot_set(&inventory.slots[44],266,4,8));
    CHECK(mc_crafting_click(&inventory,0,0,1,&returned,&effects)); CHECK(inventory.slots[44].count==13 && inventory.slots[44].damage==8);
    mc_slot_free(&returned); mc_crafting_effects_free(&effects); mc_inventory_free(&inventory);
}
static void test_repair_and_leather(void) {
    mc_inventory inventory; mc_inventory_init(&inventory);
    const int32_t expected[]={1644825,10040115,6717235,6704179,3361970,8339378,5013401,10066329,5000268,15892389,8375321,15066419,6724056,11685080,14188339,16777215};
    for (int dye=0;dye<16;dye++) {
        CHECK(mc_slot_set(&inventory.slots[1],298,1,0)); CHECK(mc_slot_set(&inventory.slots[2],351,1,(int16_t)dye));
        CHECK(mc_crafting_update(&inventory)); CHECK(field_integer(&inventory.slots[0].nbt,"display","color")==expected[dye]);
    }
    clear(&inventory);
    CHECK(mc_slot_set(&inventory.slots[1],276,1,1500)); CHECK(mc_slot_set(&inventory.slots[4],276,1,1500));
    const uint8_t marker[]={10,0,0,1,0,1,'x',177,0}; metadata(&inventory.slots[1],marker,sizeof(marker));
    CHECK(mc_crafting_update(&inventory)); CHECK(inventory.slots[0].item_id==276 && inventory.slots[0].damage==1361 && !inventory.slots[0].nbt.size);
    inventory.slots[1].count=2; CHECK(mc_crafting_update(&inventory)); CHECK(inventory.slots[0].item_id==-1);
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],298,2,7)); metadata(&inventory.slots[1],marker,sizeof(marker));
    CHECK(mc_slot_set(&inventory.slots[2],351,1,1)); CHECK(mc_crafting_update(&inventory));
    CHECK(inventory.slots[0].count==1 && inventory.slots[0].damage==7 && field_integer(&inventory.slots[0].nbt,"display","color")==10040115);
    CHECK(field_integer(&inventory.slots[0].nbt,NULL,"x")==-79);
    CHECK(mc_slot_set(&inventory.slots[3],351,1,15)); CHECK(mc_crafting_update(&inventory));
    CHECK(field_integer(&inventory.slots[0].nbt,"display","color")==13408665);
    mc_inventory_free(&inventory);
}
static void test_fireworks(void) {
    mc_inventory inventory; mc_inventory_init(&inventory);
    CHECK(mc_slot_set(&inventory.slots[1],339,1,0)); CHECK(mc_slot_set(&inventory.slots[2],289,1,0));
    CHECK(mc_crafting_update(&inventory)); CHECK(inventory.slots[0].item_id==401 && inventory.slots[0].count==1 && !inventory.slots[0].nbt.size);
    CHECK(mc_slot_set(&inventory.slots[3],402,1,0)); CHECK(mc_crafting_update(&inventory));
    CHECK(field_integer(&inventory.slots[0].nbt,"Fireworks","Flight")==1);
    mc_nbt_view root,fireworks,list; CHECK(mc_nbt_root(&inventory.slots[0].nbt,&root)); CHECK(mc_nbt_find(&root,"Fireworks",&fireworks));
    CHECK(mc_nbt_find(&fireworks,"Explosions",&list) && list.type==9 && list.size==5);
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],289,1,0)); CHECK(mc_slot_set(&inventory.slots[2],351,1,1));
    CHECK(mc_slot_set(&inventory.slots[3],264,1,0)); CHECK(mc_slot_set(&inventory.slots[4],348,1,0)); CHECK(mc_crafting_update(&inventory));
    CHECK(field_integer(&inventory.slots[0].nbt,"Explosion","Type")==0);
    CHECK(field_integer(&inventory.slots[0].nbt,"Explosion","Trail")==1 && field_integer(&inventory.slots[0].nbt,"Explosion","Flicker")==1);
    CHECK(mc_slot_copy(&inventory.slots[1],&inventory.slots[0])); mc_slot_free(&inventory.slots[3]); mc_slot_free(&inventory.slots[4]);
    CHECK(mc_slot_set(&inventory.slots[2],351,1,4)); CHECK(mc_crafting_update(&inventory));
    CHECK(mc_nbt_root(&inventory.slots[0].nbt,&root)); CHECK(mc_nbt_find(&root,"Explosion",&fireworks));
    CHECK(mc_nbt_find(&fireworks,"FadeColors",&list) && list.type==11 && list.size==8 && list.data[7]==(2437522&255));
    CHECK(mc_slot_copy(&inventory.slots[3],&inventory.slots[0])); CHECK(mc_slot_set(&inventory.slots[1],339,1,0)); CHECK(mc_slot_set(&inventory.slots[2],289,3,0));
    CHECK(mc_crafting_update(&inventory)); CHECK(inventory.slots[0].item_id==401 && field_integer(&inventory.slots[0].nbt,"Fireworks","Flight")==1);
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],402,1,0)); CHECK(mc_slot_set(&inventory.slots[2],351,1,1));
    CHECK(mc_crafting_update(&inventory)); CHECK(inventory.slots[0].item_id==-1);
    const uint8_t empty[]={10,0,0,0}; metadata(&inventory.slots[1],empty,sizeof(empty)); CHECK(mc_crafting_update(&inventory));
    CHECK(inventory.slots[0].item_id==402 && mc_nbt_equal(&inventory.slots[0].nbt,&inventory.slots[1].nbt));
    mc_inventory_free(&inventory);
}
static void test_book_map_banner_and_close(void) {
    mc_inventory inventory; mc_inventory_init(&inventory); mc_slot returned; mc_slot_init(&returned);
    mc_crafting_effects effects; mc_crafting_effects_init(&effects);
    const uint8_t book[]={10,0,0,3,0,10,'g','e','n','e','r','a','t','i','o','n',0,0,0,0,8,0,5,'t','i','t','l','e',0,6,0xe6,0x97,0xa5,0xe6,0x9c,0xac,1,0,1,'x',177,0};
    CHECK(mc_slot_set(&inventory.slots[1],387,1,0)); metadata(&inventory.slots[1],book,sizeof(book));
    CHECK(mc_slot_set(&inventory.slots[2],386,3,0)); CHECK(mc_crafting_click(&inventory,0,0,0,&returned,&effects));
    CHECK(inventory.cursor.item_id==387 && inventory.cursor.count==1 && field_integer(&inventory.cursor.nbt,NULL,"generation")==1);
    CHECK(inventory.slots[1].count==1 && field_integer(&inventory.slots[1].nbt,NULL,"generation")==0 && inventory.slots[2].count==2);
    CHECK(field_integer(&inventory.cursor.nbt,NULL,"x")==-79);
    mc_nbt_view root,title; char text[32]; CHECK(mc_nbt_root(&inventory.cursor.nbt,&root)); CHECK(mc_nbt_find(&root,"title",&title));
    CHECK(mc_nbt_get_string(&title,text,sizeof(text)) && !strcmp(text,"\xe6\x97\xa5\xe6\x9c\xac"));
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],358,1,4)); CHECK(mc_slot_set(&inventory.slots[2],395,3,0));
    CHECK(mc_crafting_update(&inventory)); CHECK(inventory.slots[0].item_id==358 && inventory.slots[0].count==2 && inventory.slots[0].damage==4);
    clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],425,1,2)); CHECK(mc_slot_set(&inventory.slots[2],351,1,1)); CHECK(mc_slot_set(&inventory.slots[3],45,1,0));
    CHECK(mc_crafting_update(&inventory)); CHECK(inventory.slots[0].item_id==425);
    CHECK(mc_slot_copy(&inventory.slots[1],&inventory.slots[0])); CHECK(mc_slot_set(&inventory.slots[2],425,1,2)); mc_slot_free(&inventory.slots[3]);
    CHECK(mc_crafting_click(&inventory,0,0,0,&returned,&effects));
    CHECK(inventory.cursor.item_id==425 && inventory.slots[1].item_id==425 && inventory.slots[2].item_id==-1);
    CHECK(mc_slot_set(&inventory.slots[2],425,1,3)); CHECK(mc_crafting_update(&inventory)); CHECK(inventory.slots[0].item_id==-1);
    for (unsigned i=1;i<=4;i++) CHECK(mc_slot_set(&inventory.slots[i],17,(uint8_t)i,0));
    CHECK(mc_crafting_close(&inventory,&effects)); CHECK(effects.count==5 && effects.dropped[0].item_id==425);
    CHECK(inventory.cursor.item_id==-1); for (unsigned i=0;i<=4;i++) CHECK(inventory.slots[i].item_id==-1);
    CHECK(mc_crafting_close(&inventory,&effects) && !effects.count);
    mc_crafting_effects_free(&effects); mc_slot_free(&returned); mc_inventory_free(&inventory);
}
static void test_match_validation_and_bounds(void) {
    mc_slot grid[4],result,left[4]; mc_slot_init(&result);
    for (unsigned i=0;i<4;i++) { mc_slot_init(&grid[i]); mc_slot_init(&left[i]); }
    CHECK(mc_slot_set(&grid[3],17,1,0)); CHECK(mc_slot_set(&result,1,7,0));
    CHECK(!mc_crafting_match(grid,0,2,&result,left) && result.item_id==1 && result.count==7);
    CHECK(!mc_crafting_match(grid,4,2,&result,left) && result.count==7);
    CHECK(!mc_crafting_match(grid,2,2,&grid[3],left) && grid[3].item_id==17);
    CHECK(!mc_crafting_match(grid,2,2,&result,grid) && grid[3].item_id==17);
    grid[3].count=0; CHECK(!mc_crafting_match(grid,2,2,&result,left) && result.count==7); grid[3].count=1;
    const uint8_t broken[]={10,0,0,3,0,1,'x',0};
    grid[3].nbt.data=malloc(sizeof(broken)); CHECK(grid[3].nbt.data!=NULL); memcpy(grid[3].nbt.data,broken,sizeof(broken)); grid[3].nbt.size=sizeof(broken);
    CHECK(!mc_crafting_match(grid,2,2,&result,left) && result.count==7); mc_nbt_free(&grid[3].nbt);
    CHECK(mc_crafting_match(grid,2,2,&result,left)); CHECK(result.item_id==5 && result.count==4);
    /* Bounded NBT builders must fail atomically when a huge book's added
       generation field cannot fit into the protocol's 2 MiB root limit. */
    mc_slot_free(&grid[3]); CHECK(mc_slot_set(&grid[0],387,1,0)); CHECK(mc_slot_set(&grid[1],386,1,0));
    mc_buf payload; mc_buf_init(&payload); mc_put_u8(&payload,10); mc_put_i16(&payload,0);
    mc_put_u8(&payload,7); mc_put_i16(&payload,1); mc_put_u8(&payload,'x');
    mc_put_i32(&payload,MC_MAX_PACKET-20); uint8_t *blob=calloc(MC_MAX_PACKET-20,1); CHECK(blob!=NULL);
    mc_put_bytes(&payload,blob,MC_MAX_PACKET-20); mc_put_u8(&payload,0); free(blob);
    CHECK(mc_nbt_read(&payload,&grid[0].nbt)); mc_buf_free(&payload);
    CHECK(!mc_crafting_match(grid,2,2,&result,left)); CHECK(result.item_id==5 && result.count==4);
    CHECK(grid[0].count==1 && grid[1].count==1);
    for (unsigned i=0;i<4;i++) { mc_slot_free(&grid[i]); mc_slot_free(&left[i]); } mc_slot_free(&result);
}
static void test_original_numeric_generation(void) {
    /* NBTTagFloat/Double.getInt delegates to MathHelper.floor, including Java
       saturating casts followed by signed wrapping at negative overflow. */
    const double values[]={-0.5,-1.5,0.5,1.5,-INFINITY,INFINITY,NAN,-2147483648.5};
    const int expected[]={0,-1,1,2,INT32_MAX,INT32_MAX,1,INT32_MAX};
    mc_inventory inventory; mc_inventory_init(&inventory);
    for (unsigned type=5;type<=6;type++) for (unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++) {
        if (type==5 && i==7) continue; /* Float rounds this value to INT_MIN. */
        clear(&inventory); CHECK(mc_slot_set(&inventory.slots[1],387,1,0)); CHECK(mc_slot_set(&inventory.slots[2],386,1,0));
        mc_buf tag; mc_buf_init(&tag); mc_put_u8(&tag,10); mc_put_i16(&tag,0);
        mc_put_u8(&tag,(uint8_t)type); mc_put_i16(&tag,10); mc_put_bytes(&tag,"generation",10);
        if (type==5) mc_put_f32(&tag,(float)values[i]); else mc_put_f64(&tag,values[i]);
        mc_put_u8(&tag,0); CHECK(mc_nbt_read(&tag,&inventory.slots[1].nbt)); mc_buf_free(&tag);
        CHECK(mc_crafting_update(&inventory));
        if (expected[i]==INT32_MAX) CHECK(inventory.slots[0].item_id==-1);
        else CHECK(inventory.slots[0].item_id==387 && field_integer(&inventory.slots[0].nbt,NULL,"generation")==expected[i]);
    }
    mc_inventory_free(&inventory);
}
int main(void) {
    test_preview_shapes_and_colors(); test_output_transactions(); test_repair_and_leather(); test_fireworks(); test_book_map_banner_and_close(); test_match_validation_and_bounds();
    test_original_numeric_generation();
    printf("Crafting: %u checks passed\n",checks); return 0;
}
