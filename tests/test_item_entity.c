#include "entity/item/item_entity.h"
#include "item/item.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(v) do { ++checks; if (!(v)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#v); exit(1); } } while (0)
static void entity(mc_item_entity *e,int id,unsigned count) {
    mc_item_entity_init(e); e->eid=id; e->x=-0.01; e->y=65; e->z=1.5;
    CHECK(mc_slot_set(&e->item,1,(uint8_t)count,0));
}
static void marker(mc_nbt *n,uint8_t value) {
    const uint8_t bytes[]={10,0,0,1,0,1,'m',value,0};
    mc_buf b={0}; mc_put_bytes(&b,bytes,sizeof(bytes)); CHECK(mc_nbt_read(&b,n)); mc_buf_free(&b);
}
static void named(mc_buf *b,uint8_t type,const char *name) {
    mc_put_u8(b,type); mc_put_i16(b,(int16_t)strlen(name)); mc_put_bytes(b,name,strlen(name));
}
static void add_unknown(mc_nbt *n,const char *name,uint8_t value) {
    mc_buf b={0}; CHECK(n->size>=4); mc_put_bytes(&b,n->data,n->size-1);
    named(&b,1,name); mc_put_u8(&b,value); mc_put_u8(&b,0);
    mc_nbt replaced={0}; CHECK(mc_nbt_read(&b,&replaced)); mc_nbt_free(n); *n=replaced; mc_buf_free(&b);
}
static void large_tag(mc_nbt *n,size_t count) {
    mc_buf b={0}; mc_put_u8(&b,10); mc_put_i16(&b,0); named(&b,7,"large"); mc_put_i32(&b,(int32_t)count);
    uint8_t *zero=calloc(count,1); CHECK(zero!=NULL); mc_put_bytes(&b,zero,count); free(zero); mc_put_u8(&b,0);
    CHECK(!b.failed && mc_nbt_read(&b,n)); mc_buf_free(&b);
}
static void add_item_unknown(mc_nbt *n) {
    mc_nbt_view root,item; CHECK(mc_nbt_root(n,&root) && mc_nbt_find(&root,"Item",&item) && item.type==10);
    size_t end=(size_t)(item.data-n->data)+item.size-1;
    mc_buf b={0}; mc_put_bytes(&b,n->data,end); named(&b,8,"ForeignItem");
    const uint8_t text[]={0,6,0xe6,0x9c,0xac,0xe6,0x96,0x87}; mc_put_bytes(&b,text,sizeof(text));
    mc_put_bytes(&b,n->data+end,n->size-end); mc_nbt next={0}; CHECK(mc_nbt_read(&b,&next));
    mc_nbt_free(n); *n=next; mc_buf_free(&b);
}
static void wire(void) {
    mc_item_entity e; entity(&e,300,2); e.vx=5; e.vy=-5; e.vz=0.125;
    mc_buf b={0}; CHECK(mc_item_entity_spawn(&e,&b));
    CHECK(mc_get_varint(&b)==0x0e && mc_get_varint(&b)==300 && mc_get_u8(&b)==2);
    CHECK(mc_get_i32(&b)==-1 && mc_get_i32(&b)==2080 && mc_get_i32(&b)==48);
    CHECK(mc_get_u8(&b)==0 && mc_get_u8(&b)==0 && mc_get_i32(&b)==1);
    CHECK(mc_get_i16(&b)==31200 && mc_get_i16(&b)==-31200 && mc_get_i16(&b)==1000 && b.pos==b.len);
    marker(&e.item.nbt,7); CHECK(mc_item_entity_metadata(&e,&b));
    CHECK(mc_get_varint(&b)==0x1c && mc_get_varint(&b)==300 && mc_get_u8(&b)==0xaa);
    mc_slot decoded; mc_slot_init(&decoded); CHECK(mc_slot_read(&b,&decoded));
    CHECK(mc_slot_equal(&e.item,&decoded) && mc_get_u8(&b)==0x7f && b.pos==b.len);
    CHECK(mc_item_entity_velocity(&e,&b)); CHECK(mc_get_varint(&b)==0x12 && mc_get_varint(&b)==300);
    CHECK(mc_get_i16(&b)==31200 && mc_get_i16(&b)==-31200 && mc_get_i16(&b)==1000);
    size_t old=b.len; uint8_t first=b.data[0]; e.x=NAN;
    CHECK(!mc_item_entity_spawn(&e,&b) && b.len==old && b.data[0]==first);
    mc_slot_free(&decoded); mc_buf_free(&b); mc_item_entity_free(&e);
}
static void ownership(void) {
    mc_item_entity e,copy; entity(&e,1,4); mc_item_entity_init(&copy); marker(&e.item.nbt,1);
    CHECK(mc_item_entity_copy(&copy,&e)); CHECK(copy.item.nbt.data!=e.item.nbt.data);
    mc_item_entities list,other; mc_item_entities_init(&list); mc_item_entities_init(&other);
    CHECK(mc_item_entities_add(&list,&e)); CHECK(!mc_item_entities_add(&list,&e) && list.count==1);
    CHECK(mc_item_entities_copy(&other,&list)); CHECK(other.entries[0].item.nbt.data!=list.entries[0].item.nbt.data);
    CHECK(mc_item_entities_find(&other,1)==other.entries && !mc_item_entities_find(&other,2));
    CHECK(mc_item_entities_remove(&other,1) && !mc_item_entities_remove(&other,1));
    for (unsigned i=1;i<MC_MAX_ITEM_ENTITIES;i++) { e.eid=(int32_t)i+1; CHECK(mc_item_entities_add(&list,&e)); }
    e.eid=2000; CHECK(!mc_item_entities_add(&list,&e) && list.count==MC_MAX_ITEM_ENTITIES);
    mc_item_entities_free(&other); mc_item_entities_free(&list); mc_item_entity_free(&copy); mc_item_entity_free(&e);
}
static void signed_entity_ids(void) {
    const int32_t ids[]={0,-1,INT32_MIN,INT32_MAX};
    mc_item_entities a,b; mc_item_entities_init(&a); mc_item_entities_init(&b);
    mc_item_entity e; entity(&e,0,1); marker(&e.item.nbt,7);
    mc_buf packet={0};
    for (size_t i=0;i<sizeof(ids)/sizeof(ids[0]);i++) {
        e.eid=ids[i]; CHECK(mc_item_entity_valid(&e)); CHECK(mc_item_entities_add(&a,&e));
        CHECK(mc_item_entity_spawn(&e,&packet)); CHECK(mc_get_varint(&packet)==0x0e && mc_get_varint(&packet)==ids[i]);
        CHECK(mc_item_entity_metadata(&e,&packet)); CHECK(mc_get_varint(&packet)==0x1c && mc_get_varint(&packet)==ids[i]);
        CHECK(mc_item_entity_velocity(&e,&packet)); CHECK(mc_get_varint(&packet)==0x12 && mc_get_varint(&packet)==ids[i]);
    }
    mc_nbt encoded={0}; CHECK(mc_item_entities_encode(&a,&encoded)); CHECK(mc_item_entities_decode(&encoded,&b));
    CHECK(b.count==4);
    for (size_t i=0;i<sizeof(ids)/sizeof(ids[0]);i++) {
        mc_item_entity *found=mc_item_entities_find(&b,ids[i]);
        CHECK(found && found->eid==ids[i] && mc_slot_equal(&e.item,&found->item));
        CHECK(mc_item_entities_remove(&b,ids[i]) && !mc_item_entities_find(&b,ids[i]));
    }
    mc_buf_free(&packet); mc_nbt_free(&encoded); mc_item_entities_free(&a); mc_item_entities_free(&b); mc_item_entity_free(&e);
}
static void physics(void) {
    mc_world *world=malloc(sizeof(*world)); CHECK(world!=NULL); mc_world_init(world,1);
    CHECK(mc_world_set(world,0,63,0,16));
    mc_item_entity e; entity(&e,1,1); e.x=0.5; e.y=65; e.z=0.5; e.pickup_delay=40;
    CHECK(mc_item_entity_tick(&e,world)); CHECK(fabs(e.y-64.96)<1e-6 && fabs(e.vy+0.0392)<1e-6);
    CHECK(e.age==1 && e.pickup_delay==39);
    for (unsigned i=0;i<60;i++) CHECK(mc_item_entity_tick(&e,world));
    CHECK(fabs(e.y-64)<1e-7 && e.on_ground && fabs(e.vy)<1e-7 && !e.pickup_delay);
    e.age=5999; CHECK(!mc_item_entity_tick(&e,world) && e.age==6000);
    e.age=-32768; e.pickup_delay=32767; CHECK(mc_item_entity_tick(&e,world));
    CHECK(e.age==-32768 && e.pickup_delay==32767);
    CHECK(mc_world_set(world,0,63,0,44<<4)); e.y=64; e.vy=0;
    for (unsigned i=0;i<30;i++) CHECK(mc_item_entity_tick(&e,world));
    CHECK(fabs(e.y-63.5)<1e-7);
    CHECK(mc_world_set(world,0,63,0,85<<4)); e.y=64.7; e.vy=0;
    for (unsigned i=0;i<10;i++) CHECK(mc_item_entity_tick(&e,world));
    CHECK(fabs(e.y-64.5)<1e-7);
    e.y=-65; CHECK(!mc_item_entity_tick(&e,world));
    CHECK(mc_world_set(world,0,64,0,10<<4)); e.x=0.5; e.y=64.5; e.z=0.5; e.age=0; e.health=5;
    CHECK(mc_item_entity_tick(&e,world) && e.health==1); CHECK(!mc_item_entity_tick(&e,world) && e.health==0);
    CHECK(mc_world_set(world,0,64,0,16)); e.x=0.5; e.y=64.5; e.z=0.5; e.age=0; e.health=5;
    double x=e.x,y=e.y,z=e.z; CHECK(mc_item_entity_tick(&e,world));
    CHECK(e.x!=x || e.y!=y || e.z!=z);
    mc_item_entity_free(&e); mc_world_free(world); free(world);
}
static unsigned inventory_count(const mc_inventory *inventory) {
    unsigned n=0; for(int i=9;i<45;i++) n+=inventory->slots[i].count; return n;
}
static void pickup(void) {
    mc_item_entity e; entity(&e,1,10); strcpy(e.owner,"Alice");
    CHECK(mc_item_entity_pickup_eligible(&e,"Alice") && !mc_item_entity_pickup_eligible(&e,"Bob"));
    e.age=5799; CHECK(!mc_item_entity_pickup_eligible(&e,"Bob")); e.age=5800; CHECK(mc_item_entity_pickup_eligible(&e,"Bob"));
    e.pickup_delay=1; CHECK(!mc_item_entity_pickup_eligible(&e,"Alice")); e.pickup_delay=0;
    e.x=0; e.y=64; e.z=0; CHECK(mc_item_entity_pickup_near(&e,0,64,0));
    CHECK(!mc_item_entity_pickup_near(&e,1.425,64,0) && mc_item_entity_pickup_near(&e,1.424,64,0));
    mc_inventory inventory; mc_inventory_init(&inventory);
    for(int i=9;i<45;i++) CHECK(mc_slot_set(&inventory.slots[i],1,64,0));
    inventory.slots[36].count=60;
    unsigned inserted=999,discarded=999;
    CHECK(mc_item_entity_pickup(&e,&inventory,false,&inserted,&discarded));
    CHECK(inserted==4 && discarded==0 && e.item.count==6 && inventory_count(&inventory)==36*64);
    CHECK(mc_item_entity_pickup(&e,&inventory,true,&inserted,&discarded));
    CHECK(inserted==0 && discarded==6 && e.item.item_id==-1);
    mc_item_entity_free(&e); entity(&e,2,5); CHECK(mc_slot_set(&e.item,310,2,3));
    CHECK(mc_item_entity_pickup(&e,&inventory,false,&inserted,&discarded)); CHECK(!inserted && !discarded && e.item.count==2);
    mc_slot_free(&inventory.slots[9]); CHECK(mc_item_entity_pickup(&e,&inventory,false,&inserted,&discarded));
    CHECK(inserted==2 && inventory.slots[9].count==2 && e.item.item_id==-1);
    mc_item_entity_free(&e); entity(&e,3,5); marker(&e.item.nbt,1); inventory.slots[10].count=63; marker(&inventory.slots[10].nbt,2);
    CHECK(mc_item_entity_pickup(&e,&inventory,false,&inserted,&discarded)); CHECK(!inserted && e.item.count==5);
    mc_slot_free(&inventory.slots[36]); mc_slot_free(&inventory.slots[37]);
    CHECK(mc_slot_set(&e.item,290,2,1));
    const uint8_t unbreakable[]={10,0,0,1,0,11,'U','n','b','r','e','a','k','a','b','l','e',1,0};
    mc_buf b={0}; mc_put_bytes(&b,unbreakable,sizeof(unbreakable)); CHECK(mc_nbt_read(&b,&e.item.nbt)); mc_buf_free(&b);
    CHECK(mc_item_entity_pickup(&e,&inventory,false,&inserted,&discarded));
    CHECK(inserted==2 && inventory.slots[36].count==1 && inventory.slots[37].count==1);
    const double values[]={-0.5,0.5,NAN,INFINITY,-INFINITY};
    const bool unbreakable_value[]={true,false,false,true,true};
    for (unsigned i=0;i<5;i++) {
        mc_slot_free(&inventory.slots[36]); mc_slot_free(&inventory.slots[37]);
        CHECK(mc_slot_set(&e.item,290,2,1)); mc_buf tag={0}; named(&tag,10,""); named(&tag,6,"Unbreakable");
        mc_put_f64(&tag,values[i]); mc_put_u8(&tag,0); CHECK(mc_nbt_read(&tag,&e.item.nbt)); mc_buf_free(&tag);
        CHECK(mc_item_entity_pickup(&e,&inventory,false,&inserted,&discarded));
        CHECK(inserted==2 && inventory.slots[36].count==(unbreakable_value[i] ? 1 : 2) &&
            inventory.slots[37].count==(unbreakable_value[i] ? 1 : 0));
    }
    mc_inventory_free(&inventory); mc_item_entity_free(&e);
}
static void merge(void) {
    mc_item_entity a,b; entity(&a,1,10); entity(&b,2,20); a.age=50; b.age=100; a.pickup_delay=20;
    strcpy(a.owner,"Alice"); strcpy(b.owner,"Bob");
    CHECK(mc_item_entity_merge(&a,&b)); CHECK(a.item.item_id==-1 && b.item.count==30 && b.age==50 && b.pickup_delay==20);
    CHECK(!strcmp(b.owner,"Bob")); CHECK(mc_slot_set(&a.item,1,40,0));
    CHECK(!mc_item_entity_merge(&a,&b) && a.item.count==40 && b.item.count==30);
    a.item.count=1; marker(&a.item.nbt,1); CHECK(!mc_item_entity_merge(&a,&b));
    mc_nbt_free(&a.item.nbt); a.age=-32768; CHECK(!mc_item_entity_merge(&a,&b));
    CHECK(!mc_item_has_subtypes(266) && mc_item_has_subtypes(35) && mc_item_has_subtypes(358) && !mc_item_has_subtypes(276));
    a.age=0; CHECK(mc_slot_set(&a.item,266,5,7)); CHECK(mc_slot_set(&b.item,266,4,8));
    CHECK(mc_item_entity_merge(&a,&b) && a.item.count==9 && a.item.damage==7 && b.item.item_id==-1);
    CHECK(mc_slot_set(&a.item,35,5,7)); CHECK(mc_slot_set(&b.item,35,4,8)); CHECK(!mc_item_entity_merge(&a,&b));
    mc_item_entity_free(&a); mc_item_entity_free(&b);
}
static void capacity(void) {
    mc_item_entities a,b; mc_item_entities_init(&a); mc_item_entities_init(&b);
    mc_item_entity e; entity(&e,1,1); large_tag(&e.item.nbt,1100000);
    CHECK(mc_item_entities_add(&a,&e)); e.eid=2;
    CHECK(!mc_item_entities_add(&a,&e) && a.count==1 && a.entries[0].eid==1);
    CHECK(mc_item_entities_copy(&b,&a) && b.count==1);
    mc_nbt old={0}; CHECK(mc_item_entities_encode(&a,&old)); size_t size=old.size;
    large_tag(&a.original_nbt,1100000);
    CHECK(!mc_item_entities_encode(&a,&old) && old.size==size);
    CHECK(!mc_item_entities_copy(&b,&a) && b.count==1);
    mc_nbt_free(&a.original_nbt); a.original_nbt.data=malloc(1); CHECK(a.original_nbt.data!=NULL);
    a.original_nbt.data[0]=10; a.original_nbt.size=1;
    CHECK(!mc_item_entities_encode(&a,&old) && old.size==size);
    mc_nbt_free(&old); mc_item_entities_free(&a); mc_item_entities_free(&b); mc_item_entity_free(&e);
}
static void persistence(void) {
    mc_item_entities a,b; mc_item_entities_init(&a); mc_item_entities_init(&b);
    mc_item_entity e; entity(&e,17,127); e.age=333; e.pickup_delay=40; e.health=4; e.vx=0.13; e.vy=-0.3;
    strcpy(e.owner,"Alice"); strcpy(e.thrower,"Bob"); marker(&e.item.nbt,9); CHECK(mc_item_entities_add(&a,&e));
    mc_nbt encoded={0},again={0}; CHECK(mc_item_entities_encode(&a,&encoded));
    CHECK(mc_item_entities_decode(&encoded,&b) && b.count==1 && b.entries[0].eid==17);
    CHECK(mc_slot_equal(&b.entries[0].item,&e.item) && b.entries[0].age==333 && b.entries[0].pickup_delay==40);
    CHECK(b.entries[0].health==4 && !strcmp(b.entries[0].owner,"Alice") && !strcmp(b.entries[0].thrower,"Bob"));
    CHECK(fabs(b.entries[0].vx-e.vx)<1e-12); CHECK(mc_item_entities_encode(&b,&again)); CHECK(mc_nbt_equal(&encoded,&again));
    add_unknown(&b.original_nbt,"ForeignRoot",41); add_unknown(&b.entries[0].original_nbt,"ForeignEntity",42);
    add_item_unknown(&b.entries[0].original_nbt);
    CHECK(mc_item_entities_encode(&b,&again)); mc_item_entities_free(&a); CHECK(mc_item_entities_decode(&again,&a));
    mc_nbt_view root,tag; int64_t value;
    CHECK(mc_nbt_root(&a.original_nbt,&root) && mc_nbt_find(&root,"ForeignRoot",&tag) && mc_nbt_get_integer(&tag,&value) && value==41);
    CHECK(mc_nbt_root(&a.entries[0].original_nbt,&root) && mc_nbt_find(&root,"ForeignEntity",&tag) && mc_nbt_get_integer(&tag,&value) && value==42);
    mc_nbt_view item; char text[16]; CHECK(mc_nbt_find(&root,"Item",&item) && mc_nbt_find(&item,"ForeignItem",&tag));
    CHECK(mc_nbt_get_string(&tag,text,sizeof(text)) && !strcmp(text,"\xe6\x9c\xac\xe6\x96\x87"));
    CHECK(mc_nbt_find(&item,"Count",&tag)); size_t count_offset=(size_t)(tag.data-a.entries[0].original_nbt.data);
    a.entries[0].original_nbt.data[count_offset]=0;
    mc_nbt bad={0}; CHECK(mc_item_entities_encode(&a,&bad));
    /* Encoding replaces known Item fields from authoritative Slot state. */
    CHECK(mc_item_entities_decode(&bad,&b) && b.entries[0].item.count==127); mc_nbt_free(&bad);
    a.entries[0].original_nbt.data[count_offset]=127;
    CHECK(mc_nbt_root(&again,&root) && mc_nbt_find(&root,"Entities",&tag));
    mc_nbt_view list_item; CHECK(mc_nbt_list_get(&tag,0,&list_item) && mc_nbt_find(&list_item,"Item",&item) && mc_nbt_find(&item,"Count",&tag));
    count_offset=(size_t)(tag.data-again.data); again.data[count_offset]=0;
    CHECK(!mc_item_entities_decode(&again,&b) && b.count==1 && b.entries[0].item.count==127); again.data[count_offset]=127;
    CHECK(mc_nbt_root(&a.original_nbt,&root) && mc_nbt_find(&root,"Version",&tag));
    size_t offset=(size_t)(tag.data-a.original_nbt.data); a.original_nbt.data[offset+3]=2;
    CHECK(!mc_item_entities_decode(&a.original_nbt,&b) && b.count==1 && b.entries[0].eid==17);
    a.original_nbt.data[offset+3]=1;
    CHECK(mc_nbt_find(&root,"Entities",&tag)); offset=(size_t)(tag.data-a.original_nbt.data);
    a.original_nbt.data[offset+4]=2; CHECK(!mc_item_entities_decode(&a.original_nbt,&b) && b.count==1);
    a.original_nbt.data[offset+4]=1;
    for(size_t len=0;len<again.size;len++) {
        mc_nbt short_nbt={again.data,len}; CHECK(!mc_item_entities_decode(&short_nbt,&a) && a.count==1 && a.entries[0].eid==17);
    }
    a.entries[0].x=INFINITY; CHECK(!mc_item_entities_encode(&a,&encoded) && encoded.size!=0);
    mc_item_entities_free(&a); mc_item_entities_free(&b); mc_item_entity_free(&e); mc_nbt_free(&encoded); mc_nbt_free(&again);
}
int main(void) { wire(); ownership(); signed_entity_ids(); physics(); pickup(); merge(); capacity(); persistence(); printf("item entity: %u checks passed\n",checks); return 0; }
