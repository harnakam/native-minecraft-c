#include "world/map.h"
#include "item/ItemMap.h"
#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(v) do { ++checks; if (!(v)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#v); exit(1); } } while (0)
static void centers(void) {
    /* Independent 1.8.9 runtime alignment facts, including the -64 boundary. */
    static const struct { double x,z; uint8_t scale; int32_t cx,cz; } cases[]={
        {-65,65,0,-128,128},{-64,64,0,0,128},{-63.99,63.99,0,0,0},
        {64,-64,0,128,0},{100,-50,2,192,192},{-65,1024,3,-576,1472},
        {-576,1472,4,-1088,960},{0,0,1,64,64},{-2048,2048,3,-1600,2496}
    };
    for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        int32_t x=7,z=9;
        CHECK(mc_map_center(cases[i].x,cases[i].z,cases[i].scale,&x,&z));
        CHECK(x==cases[i].cx && z==cases[i].cz);
    }
    int32_t x=7,z=9;
    CHECK(!mc_map_center(NAN,0,0,&x,&z) && x==7 && z==9);
    CHECK(!mc_map_center(0,INFINITY,0,&x,&z) && x==7 && z==9);
    CHECK(!mc_map_center(0,0,5,&x,&z));
    CHECK(!mc_map_center(DBL_MAX,0,4,&x,&z));
}
static void named(mc_buf *b,uint8_t type,const char *name) {
    mc_put_u8(b,type); mc_put_i16(b,(int16_t)strlen(name)); mc_put_bytes(b,name,strlen(name));
}
static void marker(mc_nbt *n,const char *name,uint8_t value) {
    mc_buf b={0}; mc_put_u8(&b,10); mc_put_i16(&b,0); named(&b,1,name); mc_put_u8(&b,value); mc_put_u8(&b,0);
    CHECK(mc_nbt_read(&b,n)); mc_buf_free(&b);
}
static void add_unknown(mc_nbt *n,const char *name,uint8_t value) {
    mc_buf b={0}; CHECK(n->size>=4); mc_put_bytes(&b,n->data,n->size-1);
    named(&b,1,name); mc_put_u8(&b,value); mc_put_u8(&b,0);
    CHECK(mc_nbt_read(&b,n)); mc_buf_free(&b);
}
static int64_t field(const mc_nbt *n,const char *name) {
    mc_nbt_view root,v; int64_t value;
    CHECK(mc_nbt_root(n,&root) && mc_nbt_find(&root,name,&v) && mc_nbt_get_integer(&v,&value)); return value;
}
static void nbt_body(mc_nbt *n,int scale,int width,int height,size_t length) {
    mc_buf b={0}; mc_put_u8(&b,10); mc_put_i16(&b,0);
    named(&b,1,"dimension"); mc_put_u8(&b,255); named(&b,3,"xCenter"); mc_put_i32(&b,123);
    named(&b,3,"zCenter"); mc_put_i32(&b,-321); named(&b,1,"scale"); mc_put_u8(&b,(uint8_t)scale);
    named(&b,2,"width"); mc_put_i16(&b,(int16_t)width); named(&b,2,"height"); mc_put_i16(&b,(int16_t)height);
    named(&b,7,"colors"); mc_put_i32(&b,(int32_t)length);
    for (size_t i=0;i<length;i++) mc_put_u8(&b,(uint8_t)(i+1));
    mc_put_u8(&b,0); CHECK(mc_nbt_read(&b,n)); mc_buf_free(&b);
}
static void body_codec(void) {
    mc_nbt n={0},encoded={0}; mc_map_info map={0},copy={0};
    nbt_body(&n,5,128,128,16384); add_unknown(&n,"ForeignMap",17);
    CHECK(mc_map_info_decode(7,&n,&map));
    CHECK(map.metadata_known && map.id==7 && map.center_x==123 && map.center_z==-321 && map.dimension==-1 && map.scale==4);
    CHECK(map.colors[0]==1 && map.colors[16383]==0);
    CHECK(mc_map_info_encode(&map,&encoded));
    CHECK(field(&encoded,"scale")==4 && field(&encoded,"width")==128 && field(&encoded,"ForeignMap")==17);
    CHECK(mc_map_info_copy(&copy,&map)); CHECK(copy.original_nbt.data!=map.original_nbt.data);
    mc_nbt_free(&n); mc_map_info_free(&map); CHECK(copy.colors[0]==1 && field(&copy.original_nbt,"ForeignMap")==17);
    for (int scale=-1;scale<=5;scale+=2) {
        nbt_body(&n,scale,2,2,4); CHECK(mc_map_info_decode(8,&n,&map)); CHECK(map.scale==(scale<0?0:scale>4?4:scale));
        CHECK(map.colors[63+63*128]==1 && map.colors[64+63*128]==2 && map.colors[63+64*128]==3 && map.colors[64+64*128]==4);
        CHECK(map.colors[0]==0 && mc_map_info_encode(&map,&encoded) && field(&encoded,"width")==128);
    }
    int32_t oldid=map.id; uint8_t oldpixel=map.colors[63+63*128];
    nbt_body(&n,0,128,128,4); CHECK(!mc_map_info_decode(99,&n,&map) && map.id==oldid && map.colors[63+63*128]==oldpixel);
    nbt_body(&n,0,129,1,129); CHECK(!mc_map_info_decode(99,&n,&map));
    nbt_body(&n,0,-1,0,0); CHECK(!mc_map_info_decode(99,&n,&map));
    nbt_body(&n,0,0,0,0); CHECK(mc_map_info_decode(INT32_MIN,&n,&map) && map.colors[0]==0);
    map.scale=5; size_t oldsize=encoded.size; CHECK(!mc_map_info_encode(&map,&encoded) && encoded.size==oldsize);
    mc_nbt_free(&n); mc_nbt_free(&encoded); mc_map_info_free(&map); mc_map_info_free(&copy);
}
static void allocation_and_crafting(void) {
    mc_maps maps={0},copy={0}; mc_slot source,preview,out; mc_slot_init(&source); mc_slot_init(&preview); mc_slot_init(&out);
    maps.next_id=10; CHECK(mc_slot_set(&source,358,3,4)); marker(&source.nbt,"custom",7);
    CHECK(!mc_maps_scale_preview(&maps,&source,&preview) && preview.item_id==-1 && maps.next_id==10);
    CHECK(mc_maps_resolve(&maps,&source,-65,1024,0));
    CHECK(source.damage==10 && source.count==3 && field(&source.nbt,"custom")==7 && maps.next_id==11 && maps.count==1);
    const mc_map_info *m=mc_maps_find_const(&maps,10); CHECK(m && m->scale==3 && m->center_x==-576 && m->center_z==1472 && m->colors[0]==0);
    CHECK(mc_maps_resolve(&maps,&source,0,0,0) && source.damage==10 && maps.next_id==11);
    CHECK(mc_maps_scale_preview(&maps,&source,&preview)); CHECK(preview.count==1 && preview.damage==10 && field(&preview.nbt,"map_is_scaling")==1 && field(&preview.nbt,"custom")==7);
    CHECK(mc_maps_copy(&copy,&maps)); CHECK(mc_maps_on_crafted(&copy,&preview,0,0,0));
    CHECK(preview.damage==11 && maps.count==1 && copy.count==2 && copy.next_id==12);
    m=mc_maps_find_const(&copy,11); CHECK(m && m->scale==4 && m->center_x==-1088 && m->center_z==960 && m->colors[0]==0);
    CHECK(field(&preview.nbt,"map_is_scaling")==1 && field(&preview.nbt,"custom")==7);
    CHECK(!mc_maps_scale_preview(&copy,&preview,&out));
    mc_slot_free(&preview); CHECK(mc_slot_set(&preview,358,1,999)); marker(&preview.nbt,"map_is_scaling",1);
    CHECK(mc_maps_on_crafted(&copy,&preview,-65,1024,0)); CHECK(preview.damage==13 && copy.count==4 && copy.next_id==14);
    CHECK(mc_maps_find_const(&copy,12)->scale==3 && mc_maps_find_const(&copy,13)->scale==4);
    CHECK(mc_maps_create(&copy,&out,64,-64,0,0)); CHECK(out.item_id==358 && out.damage==14 && out.count==1 && !out.nbt.size);
    m=mc_maps_find_const(&copy,14); CHECK(m && m->center_x==128 && m->center_z==0 && m->scale==0);
    CHECK(mc_maps_on_crafted(&copy,&out,0,0,0) && copy.next_id==15);
    size_t count=copy.count; int32_t next=copy.next_id; int16_t olddamage=out.damage;
    CHECK(!mc_maps_create(&copy,&out,NAN,0,0,0) && copy.count==count && copy.next_id==next && out.damage==olddamage);
    CHECK(!mc_maps_create(&copy,&out,0,0,128,0));
    copy.next_id=INT16_MAX; CHECK(mc_maps_create(&copy,&out,0,0,0,0) && out.damage==INT16_MAX && copy.next_id==INT16_MAX+1);
    CHECK(!mc_maps_create(&copy,&out,0,0,0,0) && out.damage==INT16_MAX && copy.next_id==INT16_MAX+1);
    mc_slot_free(&source); mc_slot_free(&preview); mc_slot_free(&out); mc_maps_free(&maps); mc_maps_free(&copy);
}
static void numeric_scaling_markers(void) {
    /* Runtime facts for the target's numeric-to-byte floor/cast boundary. */
    static const struct { double number; bool single,wide; } cases[]={
        {-.5,true,true},{-1.5,true,true},{255.9,true,true},{256.1,false,false},
        {-256.1,true,true},{INFINITY,true,true},{-INFINITY,true,true},{NAN,false,false},
        {2147483648.0,true,true},{-2147483649.0,false,true},{4294967296.0,true,true}
    };
    for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) for (unsigned type=5;type<=6;type++) {
        mc_maps maps={0}; mc_map_info old={0}; old.id=4; old.scale=1; old.metadata_known=true;
        CHECK(mc_maps_add(&maps,&old)); maps.next_id=10;
        mc_slot item; mc_slot_init(&item); CHECK(mc_slot_set(&item,358,1,4));
        mc_buf b={0}; mc_put_u8(&b,10); mc_put_i16(&b,0); named(&b,(uint8_t)type,"map_is_scaling");
        if (type==5) mc_put_f32(&b,(float)cases[i].number); else mc_put_f64(&b,cases[i].number);
        mc_put_u8(&b,0); CHECK(mc_nbt_read(&b,&item.nbt));
        bool expected=type==5 ? cases[i].single : cases[i].wide;
        CHECK(mc_maps_on_crafted(&maps,&item,0,0,0));
        CHECK(item.damage==(expected?10:4) && maps.next_id==(expected?11:10));
        CHECK(maps.count==(expected?2u:1u));
        CHECK(item.nbt.size==b.len && !memcmp(item.nbt.data,b.data,b.len));
        if (expected) CHECK(mc_maps_find_const(&maps,10)->scale==2);
        mc_buf_free(&b); mc_slot_free(&item); mc_maps_free(&maps);
    }
}
static unsigned packet_rectangle(mc_buf *packet,unsigned *x,unsigned *z,unsigned *height) {
    packet->pos=0; CHECK(mc_get_varint(packet)==0x34); (void)mc_get_varint(packet); (void)mc_get_u8(packet);
    int32_t count=mc_get_varint(packet); CHECK(count>=0 && count<=256);
    for (int32_t i=0;i<count;i++) { (void)mc_get_u8(packet); (void)mc_get_u8(packet); (void)mc_get_u8(packet); }
    unsigned width=mc_get_u8(packet); *height=width ? mc_get_u8(packet) : 0;
    *x=width ? mc_get_u8(packet) : 0; *z=width ? mc_get_u8(packet) : 0; CHECK(!packet->failed); return width;
}
static void map_info_lifecycle(void) {
    mc_map_info map={0},copy={0}; map.id=4; map.metadata_known=true;
    mc_inventory first,second; mc_inventory_init(&first); mc_inventory_init(&second);
    CHECK(mc_slot_set(&first.slots[36],358,1,4)); CHECK(mc_slot_set(&second.slots[9],358,1,4));
    mc_map_player players[]={ {0,"First",&first,0,0,90,0,true}, {-1,"Second",&second,-63.01,0,0,0,true} };
    CHECK(mc_MapData_updateVisiblePlayers(&map,&first.slots[36],&players[0],players,2,0));
    CHECK(mc_MapData_updateVisiblePlayers(&map,&second.slots[9],&players[1],players,2,0));
    CHECK(map.icon_count==2 && map.icons[0].direction==4 && map.icons[1].type==6 && map.icons[1].x==-128);
    mc_buf packet={0}; unsigned x,z,height;
    CHECK(mc_MapData_getMapPacket(&map,&first.slots[36],0,&packet)==1);
    CHECK(packet_rectangle(&packet,&x,&z,&height)==128 && height==128 && x==0 && z==0);
    CHECK(mc_MapData_getMapPacket(&map,&second.slots[9],-1,&packet)==1);
    CHECK(mc_MapData_getMapPacket(&map,&first.slots[36],0,&packet)==1);
    CHECK(packet_rectangle(&packet,&x,&z,&height)==0);
    for (unsigned i=0;i<4;i++) CHECK(mc_MapData_getMapPacket(&map,&first.slots[36],0,&packet)==0);
    CHECK(mc_MapData_getMapPacket(&map,&first.slots[36],0,&packet)==1 && packet_rectangle(&packet,&x,&z,&height)==0);
    mc_MapData_updateMapData(&map,3,5); mc_MapData_updateMapData(&map,7,8);
    CHECK(mc_map_info_copy(&copy,&map) && copy.tracking!=map.tracking);
    CHECK(mc_MapData_getMapPacket(&copy,&first.slots[36],0,&packet)==1);
    CHECK(packet_rectangle(&packet,&x,&z,&height)==5 && height==4 && x==3 && z==5);
    CHECK(mc_MapData_getMapInfo(&map,0)->dirty && !mc_MapData_getMapInfo(&copy,0)->dirty);
    CHECK(mc_MapData_getMapPacket(&map,&first.slots[36],0,&packet)==1);
    CHECK(packet_rectangle(&packet,&x,&z,&height)==5 && height==4 && x==3 && z==5);
    CHECK(mc_MapData_getMapInfo(&map,-1)->dirty);
    mc_nbt n={0}; CHECK(mc_map_info_encode(&map,&n)); CHECK(mc_map_info_decode(4,&n,&copy) && copy.tracking==NULL);
    CHECK(mc_MapData_updateDecorations(&map,0,"First",320,0,0,0) && map.icon_count==1);
    map.dimension=-1; CHECK(mc_MapData_updateDecorations(&map,0,"Nether",0,0,0,10));
    CHECK(map.icons[map.icon_count-1].direction==3);
    mc_nbt_free(&n); mc_buf_free(&packet); mc_inventory_free(&first); mc_inventory_free(&second); mc_map_info_free(&map); mc_map_info_free(&copy);
}
static void item_map_update_lifecycle(void) {
    mc_world *world=malloc(sizeof(*world)); CHECK(world!=NULL); mc_world_init(world,1);
    for (int z=0;z<16;z++) for (int x=0;x<16;x++) CHECK(mc_world_set(world,x,20,z,2<<4));
    mc_map_info map={0}; map.metadata_known=true; bool changed=false;
    CHECK(mc_ItemMap_updateMapData(&map,world,1,0,0,0,false,&changed) && changed);
    unsigned colored=0; for (size_t i=0;i<MC_MAP_PIXELS;i++) if (map.colors[i]) colored++;
    CHECK(colored==240); /* Original finite-chunk fixture: phase1 then continuation. */
    CHECK(mc_MapData_getMapInfo(&map,1)->update_counter==1);
    CHECK(mc_ItemMap_updateMapData(&map,world,2,0,0,0,false,&changed) && !changed);
    CHECK(mc_MapData_getMapInfo(&map,2)->update_counter==1 && mc_MapData_getMapInfo(&map,1)->update_counter==1);
    CHECK(mc_ItemMap_updateMapData(&map,world,1,0,0,0,false,&changed));
    CHECK(mc_MapData_getMapInfo(&map,1)->update_counter==2);
    mc_maps maps={0}; mc_inventory inventory; mc_inventory_init(&inventory);
    CHECK(mc_slot_set(&inventory.slots[9],358,1,999));
    mc_map_player viewer={1,"Viewer",&inventory,0,0,0,0,true};
    CHECK(mc_ItemMap_onUpdate(&maps,world,&inventory.slots[9],&viewer,&viewer,1,false,0,0,false,0,&changed) && changed);
    CHECK(inventory.slots[9].damage==0 && maps.count==1);
    CHECK(mc_MapData_getMapInfo(mc_maps_find(&maps,0),1)->update_counter==0);
    CHECK(mc_ItemMap_onUpdate(&maps,world,&inventory.slots[9],&viewer,&viewer,1,false,0,0,false,0,&changed) && !changed);
    mc_buf packet={0}; CHECK(mc_ItemMap_createMapDataPacket(&maps,&inventory.slots[9],1,&packet)==1);
    mc_maps copy={0}; CHECK(mc_maps_copy(&copy,&maps));
    CHECK(mc_MapData_getMapInfo(mc_maps_find(&copy,0),1)->dirty==false);
    CHECK(mc_ItemMap_createMapDataPacket(&copy,&inventory.slots[9],1,&packet)==1);
    CHECK(mc_MapData_getMapInfo(mc_maps_find(&maps,0),1)->packet_counter==0 && mc_MapData_getMapInfo(mc_maps_find(&copy,0),1)->packet_counter==1);
    CHECK(mc_slot_set(&inventory.slots[5],358,1,999));
    CHECK(mc_ItemMap_createMapDataPacket_at(&maps,&inventory.slots[5],1,-65,1024,-1,&packet)==0);
    CHECK(inventory.slots[5].damage==1 && mc_maps_find_const(&maps,1)->scale==3 && mc_maps_find_const(&maps,1)->center_x==-576);
    CHECK(mc_slot_set(&inventory.slots[10],358,1,123));
    CHECK(mc_ItemMap_getMapData(&maps,&inventory.slots[10],true,0,0,0)==NULL && inventory.slots[10].damage==123 && maps.count==2);
    mc_map_info_free(&map); mc_maps_free(&maps); mc_maps_free(&copy); mc_inventory_free(&inventory); mc_world_free(world); free(world); mc_buf_free(&packet);
}
static void store_codec_and_ownership(void) {
    mc_maps maps={0},copy={0},decoded={0}; mc_map_info map={0}; map.id=4; map.metadata_known=true; map.scale=1; map.center_x=100; map.center_z=-50;
    marker(&map.original_nbt,"ForeignData",19); CHECK(mc_maps_add(&maps,&map)); CHECK(maps.next_id==5);
    CHECK(!mc_maps_add(&maps,&map) && maps.count==1); marker(&maps.original_nbt,"ForeignRoot",23);
    CHECK(mc_maps_copy(&copy,&maps)); CHECK(copy.entries!=maps.entries && copy.entries[0].original_nbt.data!=maps.entries[0].original_nbt.data && copy.original_nbt.data!=maps.original_nbt.data);
    copy.entries[0].colors[0]=44; maps.entries[0].colors[0]=77;
    mc_nbt n={0},bad={0}; CHECK(mc_maps_encode(&copy,&n) && mc_maps_decode(&n,&decoded));
    CHECK(decoded.count==1 && decoded.next_id==5 && decoded.entries[0].colors[0]==44 && field(&decoded.entries[0].original_nbt,"ForeignData")==19 && field(&decoded.original_nbt,"ForeignRoot")==23);
    mc_maps_free(&copy); mc_nbt_free(&n); CHECK(decoded.entries[0].colors[0]==44);
    CHECK(mc_maps_encode(&maps,&bad)); add_unknown(&bad,"NextId",1); CHECK(!mc_maps_decode(&bad,&decoded) && decoded.entries[0].colors[0]==44);
    /* Foreign fields on a store list entry survive, as do root/Data fields. */
    CHECK(mc_maps_encode(&maps,&n)); mc_nbt_view root,list,entry,foreign;
    CHECK(mc_nbt_root(&n,&root) && mc_nbt_find(&root,"Maps",&list) && mc_nbt_list_get(&list,0,&entry));
    size_t end=(size_t)(entry.data-n.data)+entry.size-1; mc_buf edited={0}; mc_put_bytes(&edited,n.data,end);
    named(&edited,1,"ForeignEntry"); mc_put_u8(&edited,29); mc_put_bytes(&edited,n.data+end,n.size-end);
    CHECK(mc_nbt_read(&edited,&bad)); mc_buf_free(&edited); CHECK(mc_maps_decode(&bad,&decoded)); CHECK(mc_maps_encode(&decoded,&n));
    CHECK(mc_nbt_root(&n,&root) && mc_nbt_find(&root,"Maps",&list) && mc_nbt_list_get(&list,0,&entry) && mc_nbt_find(&entry,"ForeignEntry",&foreign));
    mc_map_info client={0}; client.id=INT32_MIN; CHECK(mc_maps_add(&maps,&client));
    CHECK(mc_maps_copy(&copy,&maps) && copy.count==2); CHECK(!mc_maps_encode(&copy,&n));
    mc_maps_free(&copy); mc_maps_free(&decoded); mc_maps_free(&maps); mc_nbt_free(&bad); mc_nbt_free(&n); mc_map_info_free(&map);
}
static void wire_and_rollback(void) {
    mc_map_info map={0}; map.id=300; map.scale=2; map.icon_count=1; map.icons[0]=(mc_map_icon){6,15,-128,127};
    map.colors[4+5*128]=41; map.colors[5+5*128]=42; map.colors[4+6*128]=43; map.colors[5+6*128]=44;
    mc_buf b={0}; CHECK(mc_map_packet(&map,4,5,2,2,&b));
    CHECK(mc_get_varint(&b)==0x34 && mc_get_varint(&b)==300 && mc_get_u8(&b)==2 && mc_get_varint(&b)==1);
    CHECK(mc_get_u8(&b)==0x6f && mc_get_u8(&b)==128 && mc_get_u8(&b)==127);
    CHECK(mc_get_u8(&b)==2 && mc_get_u8(&b)==2 && mc_get_u8(&b)==4 && mc_get_u8(&b)==5 && mc_get_varint(&b)==4);
    CHECK(mc_get_u8(&b)==41 && mc_get_u8(&b)==42 && mc_get_u8(&b)==43 && mc_get_u8(&b)==44 && b.pos==b.len);
    b.pos=0; CHECK(mc_get_varint(&b)==0x34); mc_maps maps={0},copy={0}; CHECK(mc_maps_receive(&maps,&b) && b.pos==b.len && maps.next_id==0);
    const mc_map_info *m=mc_maps_find_const(&maps,300); CHECK(m && !m->metadata_known && m->colors[4+5*128]==41 && m->colors[5+6*128]==44 && m->icons[0].x==-128);
    CHECK(mc_maps_copy(&copy,&maps) && !copy.entries[0].metadata_known);
    mc_slot item,out; mc_slot_init(&item); mc_slot_init(&out); CHECK(mc_slot_set(&item,358,1,300));
    CHECK(!mc_maps_scale_preview(&maps,&item,&out) && !mc_maps_resolve(&maps,&item,0,0,0));
    CHECK(mc_map_packet(&map,0,0,0,0,&b)); b.pos=0; CHECK(mc_get_varint(&b)==0x34 && mc_maps_receive(&maps,&b));
    CHECK(mc_maps_find_const(&maps,300)->colors[5+6*128]==44);
    /* A truncated rectangle must not apply an earlier complete row. */
    CHECK(mc_map_packet(&map,4,5,2,2,&b)); b.len--; b.pos=0; CHECK(mc_get_varint(&b)==0x34);
    size_t pos=b.pos; CHECK(!mc_maps_receive(&maps,&b) && b.pos==pos && maps.entries[0].colors[4+5*128]==41 && maps.count==1);
    mc_buf_clear(&b); mc_put_varint(&b,300); mc_put_u8(&b,1); mc_put_varint(&b,0); mc_put_u8(&b,2); mc_put_u8(&b,1); mc_put_u8(&b,127); mc_put_u8(&b,0); mc_put_varint(&b,2); mc_put_i16(&b,0);
    CHECK(!mc_maps_receive(&maps,&b) && maps.entries[0].scale==2);
    mc_buf_clear(&b); mc_put_varint(&b,-1); mc_put_u8(&b,1); mc_put_varint(&b,0); mc_put_u8(&b,0);
    CHECK(mc_maps_receive(&maps,&b) && mc_maps_find_const(&maps,-1)!=NULL && maps.next_id==0);
    mc_buf_clear(&b); mc_put_varint(&b,300); mc_put_u8(&b,5); mc_put_varint(&b,0); mc_put_u8(&b,0); CHECK(!mc_maps_receive(&maps,&b));
    mc_buf_clear(&b); mc_put_varint(&b,300); mc_put_u8(&b,2); mc_put_varint(&b,257); mc_put_u8(&b,0); CHECK(!mc_maps_receive(&maps,&b));
    CHECK(mc_map_packet(&map,0,0,0,0,&b)); mc_put_u8(&b,0); b.pos=0; CHECK(mc_get_varint(&b)==0x34 && !mc_maps_receive(&maps,&b));
    CHECK(mc_map_packet(&map,0,0,128,128,&b)); size_t length=b.len;
    CHECK(!mc_map_packet(&map,127,0,2,1,&b) && b.len==length);
    mc_slot_free(&item); mc_slot_free(&out); mc_buf_free(&b); mc_maps_free(&maps); mc_maps_free(&copy);
}
static void palette(void) {
    static const uint8_t grass[4][3]={{89,125,39},{109,153,48},{127,178,56},{67,94,29}};
    for (unsigned shade=0;shade<4;shade++) { uint8_t rgb[3]={0}; CHECK(mc_map_pixel_rgb((uint8_t)(4+shade),rgb) && !memcmp(rgb,grass[shade],3)); }
    uint8_t rgb[3]={9,8,7}; CHECK(mc_map_pixel_rgb(50,rgb) && rgb[0]==64 && rgb[1]==64 && rgb[2]==255);
    CHECK(!mc_map_pixel_rgb(255,rgb) && rgb[0]==64 && rgb[1]==64 && rgb[2]==255);
    CHECK(!mc_map_pixel_rgb(4,NULL));
}
static void terrain(void) {
    mc_world *world=malloc(sizeof(*world)); CHECK(world!=NULL); mc_world_init(world,1);
    for (int z=0;z<16;z++) for (int x=0;x<16;x++) CHECK(mc_world_set(world,x,20,z,2<<4));
    mc_map_info map={0}; map.metadata_known=true; bool changed=false;
    CHECK(mc_map_update_terrain(&map,world,0,0,0,0,&changed) && changed);
    CHECK(map.colors[65+66*128]!=0); /* A changed column continues the next column. */
    for (uint32_t tick=0;tick<16;tick++) CHECK(mc_map_update_terrain(&map,world,0,0,0,tick,&changed));
    /* Flat grass is palette1/shade1. The first explored edge has height rise. */
    CHECK(map.colors[64+65*128]==5 && map.colors[65+65*128]==5 && map.colors[64+64*128]==6);
    CHECK(map.colors[63+65*128]==0 && world->count==1);
    CHECK(mc_map_update_terrain(&map,world,0,0,1,0,&changed) && !changed);
    CHECK(!mc_map_update_terrain(&map,world,NAN,0,0,0,&changed));
    for (int z=0;z<16;z++) for (int x=0;x<16;x++) {
        CHECK(mc_world_set(world,x,20,z,9<<4)); CHECK(mc_world_set(world,x,19,z,9<<4)); CHECK(mc_world_set(world,x,18,z,9<<4)); CHECK(mc_world_set(world,x,17,z,1<<4));
    }
    for (uint32_t tick=0;tick<16;tick++) CHECK(mc_map_update_terrain(&map,world,0,0,0,tick,&changed));
    CHECK(map.colors[64+66*128]==50 && map.colors[65+66*128]==49);
    CHECK(mc_world_set(world,0,20,0,35<<4|14)); CHECK(mc_world_set(world,0,21,0,20<<4));
    CHECK(mc_map_update_terrain(&map,world,0,0,0,0,&changed) && changed);
    CHECK(map.colors[64+64*128]/4==28); /* Transparent glass leaves red wool visible. */
    CHECK(mc_world_set(world,0,20,0,(5<<4)|9));
    CHECK(mc_map_update_terrain(&map,world,0,0,0,0,&changed) && map.colors[64+64*128]/4==13);
    mc_world_unload(world,0,0); uint8_t explored=map.colors[64+64*128];
    CHECK(mc_map_update_terrain(&map,world,0,0,0,0,&changed) && !changed && map.colors[64+64*128]==explored);
    mc_world_free(world); free(world); mc_map_info_free(&map);
}
static void limits_and_rollback(void) {
    mc_maps maps={0}; mc_map_info map={0}; map.metadata_known=true;
    for (unsigned i=0;i<MC_MAX_MAPS;i++) { map.id=(int32_t)i; CHECK(mc_maps_add(&maps,&map)); }
    map.id=1000; CHECK(!mc_maps_add(&maps,&map) && maps.count==MC_MAX_MAPS);
    mc_slot out; mc_slot_init(&out); CHECK(mc_slot_set(&out,1,3,0)); int32_t next=maps.next_id;
    CHECK(!mc_maps_create(&maps,&out,0,0,0,0) && out.item_id==1 && out.count==3 && maps.next_id==next && maps.count==MC_MAX_MAPS);
    mc_nbt snapshot={0}; CHECK(mc_maps_encode(&maps,&snapshot)); mc_maps decoded={0}; CHECK(mc_maps_decode(&snapshot,&decoded) && decoded.count==MC_MAX_MAPS);
    mc_maps_free(&decoded); mc_maps_free(&maps); mc_nbt_free(&snapshot);
    /* Individually valid unknown NBT may still exceed the combined snapshot. */
    mc_buf b={0}; mc_put_u8(&b,10); mc_put_i16(&b,0); named(&b,7,"large");
    size_t count=MC_NBT_MAX_BYTES-64; mc_put_i32(&b,(int32_t)count); uint8_t *zeros=calloc(count,1); CHECK(zeros!=NULL);
    mc_put_bytes(&b,zeros,count); free(zeros); mc_put_u8(&b,0); CHECK(mc_nbt_read(&b,&map.original_nbt)); mc_buf_free(&b);
    marker(&snapshot,"keep",31); size_t prior=snapshot.size;
    CHECK(!mc_map_info_encode(&map,&snapshot) && snapshot.size==prior && field(&snapshot,"keep")==31);
    CHECK(!mc_maps_add(&maps,&map) && maps.count==0 && maps.next_id==0);
    marker(&maps.original_nbt,"keep",19); maps.next_id=0;
    CHECK(mc_nbt_copy(&maps.original_nbt,&map.original_nbt));
    CHECK(!mc_maps_create(&maps,&out,0,0,0,0) && maps.count==0 && maps.next_id==0 && out.item_id==1 && out.count==3);
    mc_maps_free(&maps); mc_slot_free(&out); mc_map_info_free(&map); mc_nbt_free(&snapshot);
}
int main(void) { centers(); body_codec(); allocation_and_crafting(); numeric_scaling_markers(); map_info_lifecycle(); item_map_update_lifecycle(); store_codec_and_ownership(); wire_and_rollback(); palette(); terrain(); limits_and_rollback(); printf("map: %u checks passed\n",checks); return 0; }
