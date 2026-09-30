#include "crafting/crafting.h"
#include "world/map.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(value) do { ++checks; if (!(value)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#value); exit(1); } } while (0)
static unsigned checks;
static void clear(mc_inventory *player,mc_container *table) { mc_inventory_free(player); mc_container_free(table); }
static mc_crafting_context context(bool creative) { mc_crafting_context value; memset(&value,0,sizeof(value)); value.creative=creative; return value; }
static void put_grid(mc_container *table,const int ids[9],uint8_t count) {
    for (unsigned i=0;i<9;i++) if (ids[i]>=0) CHECK(mc_slot_set(&table->slots[i+1],(int16_t)ids[i],count,0));
}
static void cake(mc_container *table,uint8_t count) {
    const int ids[]={335,335,335,353,344,353,296,296,296}; put_grid(table,ids,count);
}
static void metadata(mc_slot *slot,const uint8_t *bytes,size_t size) {
    mc_buf input={(uint8_t *)bytes,size,size,0,false}; CHECK(mc_nbt_read(&input,&slot->nbt)); CHECK(input.pos==size);
}
static int64_t integer(const mc_nbt *nbt,const char *parent,const char *name) {
    mc_nbt_view root,field; int64_t value; CHECK(mc_nbt_root(nbt,&root));
    if (parent) { CHECK(mc_nbt_find(&root,parent,&field)); root=field; }
    CHECK(mc_nbt_find(&root,name,&field)); CHECK(mc_nbt_get_integer(&field,&value)); return value;
}
static void test_mapping_and_shift(void) {
    mc_inventory player; mc_inventory_init(&player); mc_container table; mc_container_init(&table,MC_CONTAINER_WORKBENCH);
    CHECK(mc_container_slot_count(&table)==46);
    CHECK(mc_container_get(&player,&table,9)==&table.slots[9]);
    CHECK(mc_container_get(&player,&table,10)==&player.slots[9]);
    CHECK(mc_container_get(&player,&table,36)==&player.slots[35]);
    CHECK(mc_container_get(&player,&table,37)==&player.slots[36]);
    CHECK(mc_container_get(&player,&table,45)==&player.slots[44]);
    CHECK(!mc_container_get(&player,&table,46));
    mc_slot answer,drop; mc_slot_init(&answer); mc_slot_init(&drop);
    CHECK(mc_slot_set(&player.slots[9],1,5,0));
    CHECK(mc_container_inventory_click_result(&player,&table,10,0,1,&answer,&drop));
    CHECK(player.slots[9].item_id==-1 && player.slots[36].count==5 && answer.count==5);
    CHECK(mc_container_inventory_click_result(&player,&table,37,0,1,&answer,&drop));
    CHECK(player.slots[9].count==5 && player.slots[36].item_id==-1);
    mc_slot_free(&answer); mc_slot_free(&drop); mc_container_free(&table); mc_inventory_free(&player);
}
static void test_three_by_three_recipe(void) {
    mc_slot grid[9],remaining[9],output; mc_slot_init(&output);
    for (unsigned i=0;i<9;i++) { mc_slot_init(&grid[i]); mc_slot_init(&remaining[i]); CHECK(mc_slot_set(&grid[i],i==4 ? 264 : 1,1,0)); }
    /* Eight cobblestone around an empty center is a furnace, not the old
       player-grid subset. Extra center items invalidate that shaped recipe. */
    for (unsigned i=0;i<9;i++) if (i!=4) CHECK(mc_slot_set(&grid[i],4,1,0));
    mc_slot_free(&grid[4]); CHECK(mc_crafting_match(grid,3,3,&output,remaining)); CHECK(output.item_id==61);
    CHECK(mc_slot_set(&grid[4],264,1,0)); CHECK(mc_crafting_match(grid,3,3,&output,remaining)); CHECK(output.item_id==-1);
    for (unsigned i=0;i<9;i++) { mc_slot_free(&grid[i]); mc_slot_free(&remaining[i]); } mc_slot_free(&output);
}
static void test_static_shapes_and_remainders(void) {
    mc_inventory player; mc_inventory_init(&player); mc_container table; mc_container_init(&table,MC_CONTAINER_WORKBENCH);
    const int pickaxe[]={264,264,264,-1,280,-1,-1,280,-1}; put_grid(&table,pickaxe,2);
    CHECK(mc_container_update(&player,&table,NULL)); CHECK(table.slots[0].item_id==278);
    clear(&player,&table); const int left_axe[]={5,5,-1,5,280,-1,-1,280,-1}; put_grid(&table,left_axe,1);
    CHECK(mc_container_update(&player,&table,NULL)); CHECK(table.slots[0].item_id==271);
    clear(&player,&table); const int right_axe[]={-1,5,5,-1,280,5,-1,280,-1}; put_grid(&table,right_axe,1);
    CHECK(mc_container_update(&player,&table,NULL)); CHECK(table.slots[0].item_id==271);
    clear(&player,&table); for (unsigned i=7;i<=9;i++) CHECK(mc_slot_set(&table.slots[i],338,1,0));
    CHECK(mc_container_update(&player,&table,NULL)); CHECK(table.slots[0].item_id==339 && table.slots[0].count==3);
    CHECK(mc_slot_set(&table.slots[1],1,1,0)); CHECK(mc_container_update(&player,&table,NULL)); CHECK(table.slots[0].item_id==-1);
    clear(&player,&table); cake(&table,1); mc_slot output,remaining[9]; mc_slot_init(&output); for (unsigned i=0;i<9;i++) mc_slot_init(&remaining[i]);
    CHECK(mc_crafting_match(&table.slots[1],3,3,&output,remaining)); CHECK(output.item_id==354 && output.count==1);
    for (unsigned i=0;i<9;i++) CHECK(remaining[i].item_id==(i<3 ? 325 : -1));
    mc_crafting_effects effects; mc_crafting_effects_init(&effects); mc_slot returned; mc_slot_init(&returned);
    CHECK(mc_container_click(&player,&table,NULL,0,1,0,&returned,&effects)); CHECK(player.cursor.item_id==354 && returned.item_id==354 && !effects.count);
    for (unsigned i=1;i<=9;i++) CHECK(table.slots[i].item_id==(i<=3 ? 325 : -1));
    mc_slot_free(&returned); mc_crafting_effects_free(&effects); mc_slot_free(&output); for (unsigned i=0;i<9;i++) mc_slot_free(&remaining[i]);
    mc_container_free(&table); mc_inventory_free(&player);
}
static void test_output_modes_and_legacy_shift(void) {
    mc_inventory player; mc_inventory_init(&player); mc_container table; mc_container_init(&table,MC_CONTAINER_WORKBENCH);
    mc_slot returned; mc_slot_init(&returned); mc_crafting_effects effects; mc_crafting_effects_init(&effects); mc_crafting_context creative=context(true),survival=context(false);
    CHECK(mc_slot_set(&table.slots[9],17,3,0)); CHECK(mc_container_click(&player,&table,&creative,0,0,1,&returned,&effects));
    CHECK(player.slots[44].item_id==5 && player.slots[44].count==12 && table.slots[9].item_id==-1 && returned.count==4);
    clear(&player,&table); for (unsigned i=9;i<45;i++) CHECK(mc_slot_set(&player.slots[i],1,64,0));
    CHECK(mc_slot_set(&player.slots[44],5,63,0)); CHECK(mc_slot_set(&table.slots[1],17,2,0));
    CHECK(mc_container_click(&player,&table,&creative,0,0,1,&returned,&effects));
    CHECK(player.slots[44].count==64 && table.slots[1].count==1 && returned.count==4 && !effects.count);
    for (int button=0;button<2;button++) {
        clear(&player,&table); CHECK(mc_slot_set(&table.slots[1],17,2,0)); CHECK(mc_container_click(&player,&table,&creative,0,button,4,&returned,&effects));
        CHECK(returned.item_id==-1 && effects.count==1 && effects.dropped[0].count==4 && table.slots[1].count==1);
    }
    clear(&player,&table); CHECK(mc_slot_set(&table.slots[1],17,2,0)); CHECK(mc_slot_set(&player.slots[36],1,10,0));
    CHECK(mc_container_click(&player,&table,&creative,0,0,2,&returned,&effects));
    CHECK(player.slots[36].item_id==5 && player.slots[36].count==4 && player.slots[37].item_id==1 && player.slots[37].count==10 && returned.item_id==-1);
    clear(&player,&table); CHECK(mc_slot_set(&table.slots[1],17,2,0));
    CHECK(!mc_container_click(&player,&table,&survival,0,2,3,&returned,&effects)); CHECK(player.cursor.item_id==-1 && table.slots[1].count==2);
    CHECK(mc_container_click(&player,&table,&creative,0,2,3,&returned,&effects)); CHECK(player.cursor.count==64 && table.slots[1].count==2);
    CHECK(!mc_container_click(&player,&table,&survival,-999,8,5,&returned,&effects)); CHECK(!table.drag_active);
    mc_slot_free(&returned); mc_crafting_effects_free(&effects); mc_container_free(&table); mc_inventory_free(&player);
}
static void test_shift_repeated_cake(void) {
    mc_inventory player; mc_inventory_init(&player); mc_container table; mc_container_init(&table,MC_CONTAINER_WORKBENCH);
    mc_slot returned; mc_slot_init(&returned); mc_crafting_effects effects; mc_crafting_effects_init(&effects);
    for (unsigned creative=0;creative<2;creative++) {
        clear(&player,&table); cake(&table,10); for (unsigned i=9;i<44;i++) CHECK(mc_slot_set(&player.slots[i],1,64,0));
        mc_crafting_context ctx=context(creative!=0); CHECK(mc_container_click(&player,&table,&ctx,0,0,1,&returned,&effects));
        CHECK(player.slots[44].item_id==354 && player.slots[44].count==1 && returned.item_id==354);
        CHECK(effects.count==(creative ? 0u : 3u));
        for (size_t i=0;i<effects.count;i++) CHECK(effects.dropped[i].item_id==325 && effects.dropped[i].count==1);
        for (unsigned i=1;i<=9;i++) CHECK(table.slots[i].count==9);
        clear(&player,&table); cake(&table,10); CHECK(mc_container_click(&player,&table,&ctx,0,0,1,&returned,&effects));
        unsigned cakes=0; for (unsigned i=9;i<45;i++) if (player.slots[i].item_id==354) { cakes+=player.slots[i].count; CHECK(player.slots[i].count==1); }
        CHECK(cakes==10 && !effects.count && player.slots[36].item_id==325 && player.slots[36].count==16 && player.slots[37].count==11);
        for (unsigned i=1;i<=9;i++) CHECK(table.slots[i].item_id==(i<=3 ? 325 : -1));
    }
    mc_slot_free(&returned); mc_crafting_effects_free(&effects); mc_container_free(&table); mc_inventory_free(&player);
}
static void test_shared_slots_drag_and_close(void) {
    mc_inventory player; mc_inventory_init(&player); mc_container table; mc_container_init(&table,MC_CONTAINER_WORKBENCH);
    mc_slot returned; mc_slot_init(&returned); mc_crafting_effects effects; mc_crafting_effects_init(&effects); mc_crafting_context ctx=context(true);
    CHECK(mc_slot_set(&table.slots[9],1,5,0)); CHECK(mc_container_click(&player,&table,&ctx,9,0,1,&returned,&effects));
    CHECK(player.slots[9].count==5 && table.slots[9].item_id==-1);
    CHECK(mc_slot_set(&player.slots[9],310,2,7)); CHECK(mc_container_click(&player,&table,&ctx,10,0,1,&returned,&effects));
    CHECK(player.slots[36].item_id==310 && player.slots[36].count==2 && player.slots[5].item_id==-1);
    clear(&player,&table); CHECK(mc_slot_set(&player.cursor,1,9,0)); CHECK(mc_container_click(&player,&table,&ctx,-999,0,5,&returned,&effects));
    const int targets[]={1,9,10,45}; for (unsigned i=0;i<4;i++) CHECK(mc_container_click(&player,&table,&ctx,targets[i],1,5,&returned,&effects));
    CHECK(mc_container_click(&player,&table,&ctx,-999,2,5,&returned,&effects)); CHECK(player.cursor.count==1);
    for (unsigned i=0;i<4;i++) CHECK(mc_container_get(&player,&table,targets[i])->count==2);
    clear(&player,&table); CHECK(mc_slot_set(&player.cursor,264,7,0));
    for (unsigned i=1;i<=9;i++) CHECK(mc_slot_set(&table.slots[i],1,(uint8_t)i,0));
    CHECK(mc_container_close(&player,&table,&effects)); CHECK(effects.count==10 && effects.dropped[0].item_id==264);
    for (unsigned i=1;i<=9;i++) CHECK(effects.dropped[i].count==i && table.slots[i].item_id==-1);
    CHECK(player.cursor.item_id==-1 && !table.drag_active); CHECK(mc_container_close(&player,&table,&effects) && !effects.count);
    CHECK(!mc_container_click(&player,&table,&ctx,46,0,0,&returned,&effects));
    CHECK(!mc_container_click(&player,&table,&ctx,1,0,0,&table.slots[9],&effects));
    mc_slot drop; mc_slot_init(&drop); CHECK(!mc_container_inventory_click_result(&player,&table,1,0,0,&player.slots[5],&drop)); mc_slot_free(&drop);
    mc_slot_free(&returned); mc_crafting_effects_free(&effects); mc_container_free(&table); mc_inventory_free(&player);
}
static void test_dynamic_nine_inputs(void) {
    mc_inventory player; mc_inventory_init(&player); mc_container table; mc_container_init(&table,MC_CONTAINER_WORKBENCH);
    const uint8_t book[]={10,0,0,3,0,10,'g','e','n','e','r','a','t','i','o','n',0,0,0,0,0};
    CHECK(mc_slot_set(&table.slots[5],387,1,0)); metadata(&table.slots[5],book,sizeof(book));
    for (unsigned i=1;i<=9;i++) if (i!=5) CHECK(mc_slot_set(&table.slots[i],386,1,0));
    CHECK(mc_container_update(&player,&table,NULL)); CHECK(table.slots[0].item_id==387 && table.slots[0].count==8 && integer(&table.slots[0].nbt,NULL,"generation")==1);
    mc_slot returned; mc_slot_init(&returned); mc_crafting_effects effects; mc_crafting_effects_init(&effects);
    CHECK(mc_container_click(&player,&table,NULL,0,0,0,&returned,&effects)); CHECK(player.cursor.count==8 && table.slots[5].count==1 && !effects.count);
    clear(&player,&table); CHECK(mc_slot_set(&table.slots[5],358,1,4)); for (unsigned i=1;i<=9;i++) if (i!=5) CHECK(mc_slot_set(&table.slots[i],395,1,0));
    CHECK(mc_container_update(&player,&table,NULL)); CHECK(table.slots[0].count==9 && table.slots[0].damage==4);
    clear(&player,&table); CHECK(mc_slot_set(&table.slots[1],289,1,0)); for (unsigned i=2;i<=9;i++) CHECK(mc_slot_set(&table.slots[i],351,1,(int16_t)(i-2)));
    CHECK(mc_container_update(&player,&table,NULL)); mc_nbt_view root,explosion,colors;
    CHECK(mc_nbt_root(&table.slots[0].nbt,&root)); CHECK(mc_nbt_find(&root,"Explosion",&explosion)); CHECK(mc_nbt_find(&explosion,"Colors",&colors) && colors.type==11 && colors.size==36);
    clear(&player,&table); for (unsigned i=1;i<=9;i++) CHECK(mc_slot_set(&table.slots[i],i==1 ? 339 : i<=4 ? 289 : 402,1,0));
    CHECK(mc_container_update(&player,&table,NULL)); CHECK(table.slots[0].item_id==401 && integer(&table.slots[0].nbt,"Fireworks","Flight")==3);
    mc_slot_free(&returned); mc_crafting_effects_free(&effects); mc_container_free(&table); mc_inventory_free(&player);
}
static void test_banner_shapes(void) {
    /* Independent numeric goldens from actual 1.8.9 pattern results. A banner
       can occupy any non-dye cell, but the dye cells use fixed 3x3 positions. */
    const char *names[]={"bl","br","tl","tr","bs","ts","ls","rs","cs","ms","drs","dls","ss","cr","sc","bt","tt","bts","tts","ld","rd","lud","rud","mc","mr","vh","hh","vhr","hhb","bo","gra","gru"};
    const unsigned masks[]={64,256,1,4,448,7,73,292,146,56,273,84,45,341,186,336,21,168,42,11,416,200,38,16,170,219,63,438,504,495,149,338};
    mc_inventory player; mc_inventory_init(&player); mc_container table; mc_container_init(&table,MC_CONTAINER_WORKBENCH);
    for (unsigned p=0;p<32;p++) {
        clear(&player,&table); int empty=-1; for (unsigned i=0;i<9;i++) {
            if (masks[p]&(1u<<i)) CHECK(mc_slot_set(&table.slots[i+1],351,1,1)); else empty=(int)i;
        }
        CHECK(mc_slot_set(&table.slots[empty+1],425,1,2)); CHECK(mc_container_update(&player,&table,NULL));
        mc_nbt_view root,bet,list,entry,pattern; char value[8];
        CHECK(table.slots[0].item_id==425 && table.slots[0].damage==2); CHECK(mc_nbt_root(&table.slots[0].nbt,&root));
        CHECK(mc_nbt_find(&root,"BlockEntityTag",&bet)); CHECK(mc_nbt_find(&bet,"Patterns",&list)); CHECK(mc_nbt_list_get(&list,0,&entry));
        CHECK(mc_nbt_find(&entry,"Pattern",&pattern)); CHECK(mc_nbt_get_string(&pattern,value,sizeof(value))); CHECK(!strcmp(value,names[p]));
    }
    mc_container_free(&table); mc_inventory_free(&player);
}
static void map_grid(mc_container *table) {
    for (unsigned i=1;i<=9;i++) CHECK(mc_slot_set(&table->slots[i],i==5 ? 358 : 339,1,i==5 ? 4 : 0));
    const uint8_t custom[]={10,0,0,1,0,6,'c','u','s','t','o','m',7,0}; metadata(&table->slots[5],custom,sizeof(custom));
}
static void known_map(mc_maps *maps) {
    mc_map_info map; mc_map_info_init(&map); map.id=4; map.metadata_known=true;
    map.scale=1; map.center_x=100; map.center_z=-50; map.colors[0]=77;
    CHECK(mc_maps_add(maps,&map)); maps->next_id=10; mc_map_info_free(&map);
}
static void test_map_context_and_placement(void) {
    mc_inventory player; mc_inventory_init(&player); mc_container table; mc_container_init(&table,MC_CONTAINER_WORKBENCH);
    mc_maps maps; mc_maps_init(&maps); mc_crafting_context ctx=context(true); ctx.maps=&maps; ctx.authoritative=true; ctx.spawn_x=-65; ctx.spawn_z=1024;
    mc_slot returned,preview,left[9]; mc_slot_init(&returned); mc_slot_init(&preview); for (unsigned i=0;i<9;i++) mc_slot_init(&left[i]);
    mc_crafting_effects effects; mc_crafting_effects_init(&effects);
    const int modes[]={0,0,1,2,4,4,3,6};
    for (unsigned trial=0;trial<8;trial++) {
        clear(&player,&table); mc_maps_free(&maps); known_map(&maps); map_grid(&table);
        CHECK(mc_crafting_match_context(&table.slots[1],3,3,&ctx,&preview,left));
        CHECK(preview.item_id==358 && preview.damage==4 && integer(&preview.nbt,NULL,"map_is_scaling")==1);
        CHECK(maps.count==1 && maps.next_id==10 && table.slots[5].damage==4);
        CHECK(mc_container_update(&player,&table,&ctx)); CHECK(mc_slot_equal(&preview,&table.slots[0]));
        if (trial==1) { CHECK(mc_slot_copy(&player.cursor,&preview)); player.cursor.count=3; }
        int button=trial==5 ? 1 : trial==6 ? 2 : 0;
        CHECK(mc_container_click(&player,&table,&ctx,0,button,modes[trial],&returned,&effects));
        if (trial>=6) {
            CHECK(maps.count==1 && maps.next_id==10 && table.slots[5].count==1 && returned.item_id==-1);
            CHECK(player.cursor.item_id==(trial==6 ? 358 : -1)); if (trial==6) CHECK(player.cursor.count==64 && player.cursor.damage==4);
            continue;
        }
        CHECK(maps.count==2 && maps.next_id==11 && mc_maps_find_const(&maps,4)->colors[0]==77);
        const mc_map_info *created=mc_maps_find_const(&maps,10);
        CHECK(created && created->scale==2 && created->center_x==192 && created->center_z==192 && !created->colors[0]);
        for (unsigned i=1;i<=9;i++) CHECK(table.slots[i].item_id==-1);
        if (trial<2) {
            CHECK(player.cursor.damage==10 && player.cursor.count==(trial ? 4 : 1) && returned.damage==4);
            CHECK(integer(&player.cursor.nbt,NULL,"custom")==7 && integer(&player.cursor.nbt,NULL,"map_is_scaling")==1 && !effects.count);
        } else if (trial==2 || trial==3) {
            mc_slot *destination=&player.slots[trial==2 ? 44 : 36];
            /* Actual onCreated runs after placement in these two modes: the
               new MapData is saved, but the received stack keeps old ID4. */
            CHECK(destination->damage==4 && destination->count==1 && !effects.count);
            CHECK(returned.item_id==(trial==2 ? 358 : -1));
        } else CHECK(returned.item_id==-1 && effects.count==1 && effects.dropped[0].damage==10);
    }
    clear(&player,&table); mc_maps_free(&maps); maps.next_id=10; map_grid(&table);
    CHECK(mc_crafting_match_context(&table.slots[1],3,3,&ctx,&preview,left)); CHECK(preview.item_id==-1 && !maps.count && maps.next_id==10 && table.slots[5].damage==4);
    CHECK(mc_container_update(&player,&table,&ctx)); CHECK(table.slots[5].damage==10 && table.slots[0].damage==10 && maps.count==1 && maps.next_id==11);
    const mc_map_info *missing=mc_maps_find_const(&maps,10); CHECK(missing && missing->scale==3 && missing->center_x==-576 && missing->center_z==1472);
    CHECK(mc_container_click(&player,&table,&ctx,0,0,0,&returned,&effects)); CHECK(player.cursor.damage==11 && maps.count==2 && maps.next_id==12);
    CHECK(mc_maps_find_const(&maps,11)->scale==4 && returned.damage==10);
    clear(&player,&table); mc_maps_free(&maps); map_grid(&table); ctx.authoritative=false;
    CHECK(mc_container_update(&player,&table,&ctx) && table.slots[0].item_id==-1 && !maps.count);
    CHECK(mc_slot_set(&table.slots[0],358,1,4)); const uint8_t scaling[]={10,0,0,1,0,14,'m','a','p','_','i','s','_','s','c','a','l','i','n','g',1,0}; metadata(&table.slots[0],scaling,sizeof(scaling));
    CHECK(mc_container_update(&player,&table,&ctx) && table.slots[0].item_id==358);
    CHECK(mc_container_click(&player,&table,&ctx,0,0,0,&returned,&effects)); CHECK(player.cursor.damage==4 && returned.damage==4 && !maps.count && maps.next_id==0);
    for (unsigned i=1;i<=9;i++) CHECK(table.slots[i].item_id==-1);
    /* Exhausted authoritative map IDs reject atomically, including an already
       initialized return stack and caller's prior effects. */
    clear(&player,&table); mc_maps_free(&maps); known_map(&maps); map_grid(&table); ctx.authoritative=true;
    CHECK(mc_container_update(&player,&table,&ctx)); maps.next_id=INT16_MAX+1;
    CHECK(mc_slot_set(&returned,264,7,0)); mc_crafting_effects_free(&effects); CHECK(mc_crafting_effects_append(&effects,&returned));
    CHECK(!mc_container_click(&player,&table,&ctx,0,0,0,&returned,&effects));
    CHECK(player.cursor.item_id==-1 && table.slots[5].damage==4 && table.slots[1].count==1 && table.slots[0].damage==4);
    CHECK(maps.count==1 && maps.next_id==INT16_MAX+1 && returned.item_id==264 && returned.count==7 && effects.count==1 && effects.dropped[0].count==7);
    mc_slot_free(&returned); mc_slot_free(&preview); for (unsigned i=0;i<9;i++) mc_slot_free(&left[i]);
    mc_crafting_effects_free(&effects); mc_maps_free(&maps); mc_container_free(&table); mc_inventory_free(&player);
}
static void blob(mc_nbt *nbt,size_t length) {
    uint8_t *zero=calloc(length,1); CHECK(zero!=NULL); mc_buf bytes; mc_buf_init(&bytes);
    mc_put_u8(&bytes,10); mc_put_i16(&bytes,0); mc_put_u8(&bytes,7); mc_put_i16(&bytes,1); mc_put_u8(&bytes,'x'); mc_put_i32(&bytes,(int32_t)length); mc_put_bytes(&bytes,zero,length); mc_put_u8(&bytes,0);
    CHECK(!bytes.failed && mc_nbt_read(&bytes,nbt)); free(zero); mc_buf_free(&bytes);
}
static void test_effects_capacity_and_atomicity(void) {
    mc_crafting_effects effects; mc_crafting_effects_init(&effects); mc_slot item; mc_slot_init(&item); CHECK(mc_slot_set(&item,1,1,0));
    for (unsigned i=0;i<MC_CRAFTING_MAX_EFFECTS;i++) CHECK(mc_crafting_effects_append(&effects,&item));
    CHECK(effects.count==1024 && effects.bytes==6144 && !mc_crafting_effects_append(&effects,&item));
    CHECK(effects.count==1024 && effects.dropped[1023].count==1); mc_crafting_effects_free(&effects);
    CHECK(mc_crafting_effects_append(&effects,&item));
    for (unsigned i=0;i<20;i++) CHECK(mc_crafting_effects_append(&effects,&effects.dropped[0]));
    CHECK(effects.count==21 && effects.dropped[20].item_id==1); mc_crafting_effects_free(&effects);
    blob(&item.nbt,1100000); CHECK(mc_crafting_effects_append(&effects,&item));
    size_t bytes=effects.bytes; CHECK(!mc_crafting_effects_append(&effects,&item) && effects.count==1 && effects.bytes==bytes); mc_crafting_effects_free(&effects);
    mc_inventory player; mc_inventory_init(&player); mc_container table; mc_container_init(&table,MC_CONTAINER_WORKBENCH); mc_maps maps; mc_maps_init(&maps); maps.next_id=10;
    mc_crafting_context ctx=context(true); ctx.authoritative=true; ctx.maps=&maps;
    map_grid(&table); CHECK(mc_nbt_copy(&table.slots[5].nbt,&item.nbt));
    /* Missing MapData would allocate ID10 while deriving this giant preview;
       the aggregate wire cap rejects and restores both owners and maps. */
    CHECK(!mc_container_update(&player,&table,&ctx)); CHECK(!maps.count && maps.next_id==10 && table.slots[5].damage==4 && table.slots[0].item_id==-1);
    CHECK(!mc_container_click(&player,&table,&ctx,0,2,3,&item,&effects)); CHECK(item.item_id==1 && item.nbt.size>1100000 && !effects.count && !maps.count && maps.next_id==10);
    CHECK(table.slots[5].damage==4 && table.slots[1].count==1 && player.cursor.item_id==-1);
    mc_slot_free(&item); mc_crafting_effects_free(&effects); mc_maps_free(&maps); mc_container_free(&table); mc_inventory_free(&player);
}
static void test_result_metadata_legacy_merge(void) {
    mc_inventory player; mc_inventory_init(&player); mc_container table; mc_container_init(&table,MC_CONTAINER_WORKBENCH);
    mc_slot returned; mc_slot_init(&returned); mc_crafting_effects effects; mc_crafting_effects_init(&effects);
    CHECK(mc_slot_set(&table.slots[1],41,1,0)); CHECK(mc_slot_set(&player.cursor,266,2,7));
    /* Actual SlotCrafting normal output merges non-subtype item damage7 with
       crafted damage0, unlike ordinary strict-metadata container placement. */
    CHECK(mc_container_click(&player,&table,NULL,0,0,0,&returned,&effects)); CHECK(player.cursor.count==11 && player.cursor.damage==7 && returned.damage==0 && returned.count==9);
    clear(&player,&table); CHECK(mc_slot_set(&table.slots[1],41,1,0)); CHECK(mc_slot_set(&player.slots[44],266,2,7));
    CHECK(mc_container_click(&player,&table,NULL,0,0,1,&returned,&effects)); CHECK(player.slots[44].count==11 && player.slots[44].damage==7 && returned.damage==0);
    mc_slot_free(&returned); mc_crafting_effects_free(&effects); mc_container_free(&table); mc_inventory_free(&player);
}
static void test_number_key_slot_ownership(void) {
    mc_inventory player; mc_inventory_init(&player); mc_container table; mc_container_init(&table,MC_CONTAINER_WORKBENCH);
    mc_slot returned; mc_slot_init(&returned); mc_crafting_effects effects; mc_crafting_effects_init(&effects);
    CHECK(mc_slot_set(&table.slots[1],1,5,0)); CHECK(mc_slot_set(&player.slots[36],5,10,0)); CHECK(mc_slot_set(&player.slots[37],5,30,0));
    CHECK(mc_container_click(&player,&table,NULL,1,0,2,&returned,&effects));
    CHECK(table.slots[1].item_id==-1 && player.slots[36].item_id==1 && player.slots[36].count==5 && player.slots[37].count==40);
    clear(&player,&table); CHECK(mc_slot_set(&player.slots[36],1,127,0));
    CHECK(mc_container_click(&player,&table,NULL,1,0,2,&returned,&effects)); CHECK(table.slots[1].count==127 && player.slots[36].item_id==-1);
    clear(&player,&table); CHECK(mc_slot_set(&player.slots[36],1,127,0));
    CHECK(mc_container_click(&player,&table,NULL,10,0,2,&returned,&effects)); CHECK(player.slots[9].count==127 && player.slots[36].item_id==-1);
    clear(&player,&table); CHECK(mc_slot_set(&table.slots[1],1,5,0)); for (unsigned i=9;i<45;i++) CHECK(mc_slot_set(&player.slots[i],5,64,0));
    CHECK(mc_container_click(&player,&table,NULL,1,0,2,&returned,&effects)); CHECK(table.slots[1].count==5 && player.slots[36].item_id==5);
    for (unsigned damage=0;damage<2;damage++) {
        for (unsigned creative=0;creative<2;creative++) {
            clear(&player,&table); CHECK(mc_slot_set(&table.slots[1],1,5,0)); for (unsigned i=9;i<45;i++) CHECK(mc_slot_set(&player.slots[i],5,64,0));
            CHECK(mc_slot_set(&player.slots[36],276,2,(int16_t)damage)); mc_slot_free(&player.slots[37]); mc_crafting_context ctx=context(creative!=0);
            CHECK(mc_container_click(&player,&table,&ctx,1,0,2,&returned,&effects));
            /* Actual number fallback ignores InventoryPlayer.add's leftover:
               one undamaged tool disappears when just one target is empty.
               Damaged overstack tools instead move whole into that slot. */
            CHECK(player.slots[37].item_id==276 && player.slots[37].count==(damage ? 2 : 1) && player.slots[37].damage==(int16_t)damage);
            CHECK(player.slots[36].item_id==1 && player.slots[36].count==5 && table.slots[1].item_id==-1 && !effects.count);
        }
    }
    clear(&player,&table); mc_container_free(&table); mc_container_init(&table,MC_CONTAINER_PLAYER);
    CHECK(mc_slot_set(&player.slots[36],310,2,0)); CHECK(mc_container_click(&player,&table,NULL,5,0,2,&returned,&effects));
    CHECK(player.slots[5].count==2 && player.slots[36].item_id==-1);
    mc_container_free(&table); mc_container_init(&table,MC_CONTAINER_WORKBENCH);
    for (unsigned creative=0;creative<2;creative++) {
        clear(&player,&table); cake(&table,1); for (unsigned i=1;i<=3;i++) table.slots[i].count=2;
        for (unsigned i=9;i<45;i++) CHECK(mc_slot_set(&player.slots[i],1,64,0));
        CHECK(mc_slot_set(&player.slots[36],5,10,0)); mc_slot_free(&player.slots[37]); mc_crafting_context ctx=context(creative!=0);
        CHECK(mc_container_click(&player,&table,&ctx,0,0,2,&returned,&effects));
        CHECK(player.slots[36].item_id==354 && player.slots[37].item_id==5 && player.slots[37].count==10 && effects.count==(creative ? 0u : 3u));
        for (unsigned i=1;i<=3;i++) CHECK(table.slots[i].item_id==335 && table.slots[i].count==1);
        for (size_t i=0;i<effects.count;i++) CHECK(effects.dropped[i].item_id==325);
    }
    mc_slot_free(&returned); mc_crafting_effects_free(&effects); mc_container_free(&table); mc_inventory_free(&player);
}
static void test_original_clone_ignores_button(void) {
    const int buttons[]={0,2,-1,127};
    for (unsigned i=0;i<sizeof(buttons)/sizeof(buttons[0]);i++) {
        mc_inventory player; mc_inventory_init(&player); mc_container table; mc_container_init(&table,MC_CONTAINER_WORKBENCH);
        mc_slot returned; mc_slot_init(&returned); mc_crafting_effects effects; mc_crafting_effects_init(&effects);
        CHECK(mc_slot_set(&table.slots[1],17,2,0));
        CHECK(mc_container_click(&player,&table,NULL,0,buttons[i],3,&returned,&effects));
        CHECK(player.cursor.item_id==5 && player.cursor.count==64 && table.slots[1].count==2 && returned.item_id==-1 && !effects.count);
        mc_slot_free(&player.cursor);
        CHECK(mc_container_click(&player,&table,NULL,1,buttons[i],3,&returned,&effects));
        CHECK(player.cursor.item_id==17 && player.cursor.count==64 && table.slots[1].count==2);
        mc_slot_free(&player.cursor); mc_crafting_context survival=context(false);
        CHECK(!mc_container_click(&player,&table,&survival,0,buttons[i],3,&returned,&effects));
        CHECK(!mc_container_click(&player,&table,&survival,1,buttons[i],3,&returned,&effects));
        CHECK(player.cursor.item_id==-1 && table.slots[1].count==2);
        mc_slot_free(&returned); mc_crafting_effects_free(&effects); mc_container_free(&table); mc_inventory_free(&player);
    }
}
int main(void) {
    test_mapping_and_shift(); test_three_by_three_recipe(); test_static_shapes_and_remainders(); test_output_modes_and_legacy_shift();
    test_shift_repeated_cake(); test_shared_slots_drag_and_close(); test_dynamic_nine_inputs(); test_banner_shapes();
    test_map_context_and_placement(); test_effects_capacity_and_atomicity(); test_result_metadata_legacy_merge();
    test_number_key_slot_ownership();
    test_original_clone_ignores_button();
    printf("Container: %u checks passed\n",checks); return 0;
}
