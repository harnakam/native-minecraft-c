#include "item_entity.h"
#include "block/block.h"
#include "item/item.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool compound_or_empty(const mc_nbt *n) {
    return n && (n->size ? n->data && n->data[0]==10 && mc_nbt_validate(n->data,n->size) : !n->data);
}
static bool name_valid(const char name[17]) {
    for (unsigned i=0;i<17;i++) {
        unsigned char c=(unsigned char)name[i];
        if (!c) return true;
        if (!(c=='_' || (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9'))) return false;
    }
    return false;
}
void mc_item_entity_init(mc_item_entity *e) {
    if (!e) return;
    memset(e,0,sizeof(*e)); e->health=5; mc_slot_init(&e->item);
}
void mc_item_entity_free(mc_item_entity *e) {
    if (!e) return;
    mc_slot_free(&e->item); mc_nbt_free(&e->original_nbt); mc_item_entity_init(e);
}
bool mc_item_entity_valid(const mc_item_entity *e) {
    if (!e || !isfinite(e->x) || !isfinite(e->y) || !isfinite(e->z) ||
        fabs(e->x)>30000000 || fabs(e->y)>30000000 || fabs(e->z)>30000000 ||
        !isfinite(e->vx) || !isfinite(e->vy) || !isfinite(e->vz) ||
        fabs(e->vx)>10 || fabs(e->vy)>10 || fabs(e->vz)>10 ||
        e->age<INT16_MIN || e->age>INT16_MAX || e->pickup_delay<0 || e->pickup_delay>INT16_MAX ||
        e->health<0 || e->health>INT16_MAX || !name_valid(e->owner) || !name_valid(e->thrower) ||
        !compound_or_empty(&e->original_nbt)) return false;
    const mc_slot *s=&e->item;
    return mc_item_valid(s->item_id) && s->count>=1 && s->count<=127 && s->damage>=0 && compound_or_empty(&s->nbt);
}
bool mc_item_entity_copy(mc_item_entity *destination,const mc_item_entity *source) {
    if (!destination || !mc_item_entity_valid(source)) return false;
    if (destination==source) return true;
    mc_item_entity copied=*source; mc_slot_init(&copied.item); mc_nbt_init(&copied.original_nbt);
    if (!mc_slot_copy(&copied.item,&source->item) || !mc_nbt_copy(&copied.original_nbt,&source->original_nbt)) {
        mc_item_entity_free(&copied); return false;
    }
    mc_item_entity_free(destination); *destination=copied; return true;
}
void mc_item_entities_init(mc_item_entities *e) { if (e) memset(e,0,sizeof(*e)); }
void mc_item_entities_free(mc_item_entities *e) {
    if (!e) return;
    for (size_t i=0;i<e->count;i++) mc_item_entity_free(&e->entries[i]);
    free(e->entries); mc_nbt_free(&e->original_nbt); mc_item_entities_init(e);
}
static bool list_shape(const mc_item_entities *e) {
    return e && e->count<=e->capacity && e->capacity<=MC_MAX_ITEM_ENTITIES &&
        (e->capacity ? e->entries!=NULL : !e->entries) &&
        (e->original_nbt.size ? e->original_nbt.data && e->original_nbt.size<=MC_NBT_MAX_BYTES &&
         e->original_nbt.data[0]==10 : !e->original_nbt.data);
}
mc_item_entity *mc_item_entities_find(mc_item_entities *e,int32_t eid) {
    if (!list_shape(e)) return NULL;
    for (size_t i=0;i<e->count;i++) if (e->entries[i].eid==eid) return &e->entries[i];
    return NULL;
}
static bool add_unchecked(mc_item_entities *e,const mc_item_entity *source) {
    if (!list_shape(e) || !mc_item_entity_valid(source) || e->count>=MC_MAX_ITEM_ENTITIES ||
        mc_item_entities_find(e,source->eid)) return false;
    mc_item_entity copy; mc_item_entity_init(&copy);
    if (!mc_item_entity_copy(&copy,source)) return false;
    if (e->count==e->capacity) {
        size_t capacity=e->capacity ? e->capacity*2 : 8;
        if (capacity>MC_MAX_ITEM_ENTITIES) capacity=MC_MAX_ITEM_ENTITIES;
        mc_item_entity *entries=realloc(e->entries,capacity*sizeof(*entries));
        if (!entries) { mc_item_entity_free(&copy); return false; }
        e->entries=entries; e->capacity=capacity;
    }
    e->entries[e->count++]=copy; return true;
}
bool mc_item_entities_add(mc_item_entities *e,const mc_item_entity *source) {
    if (!add_unchecked(e,source)) return false;
    mc_nbt encoded={0}; bool okay=mc_item_entities_encode(e,&encoded); mc_nbt_free(&encoded);
    if (!okay) { e->count--; mc_item_entity_free(&e->entries[e->count]); }
    return okay;
}
bool mc_item_entities_remove(mc_item_entities *e,int32_t eid) {
    mc_item_entity *found=mc_item_entities_find(e,eid); if (!found) return false;
    size_t index=(size_t)(found-e->entries); mc_item_entity_free(found);
    if (index+1<e->count) memmove(found,found+1,(e->count-index-1)*sizeof(*found));
    e->count--; return true;
}
bool mc_item_entities_copy(mc_item_entities *destination,const mc_item_entities *source) {
    if (!destination || !list_shape(source)) return false;
    if (destination==source) return true;
    mc_item_entities copy; mc_item_entities_init(&copy);
    if (!mc_nbt_copy(&copy.original_nbt,&source->original_nbt)) return false;
    for (size_t i=0;i<source->count;i++) {
        if (!add_unchecked(&copy,&source->entries[i])) { mc_item_entities_free(&copy); return false; }
    }
    mc_nbt encoded={0}; bool okay=mc_item_entities_encode(&copy,&encoded); mc_nbt_free(&encoded);
    if (!okay) { mc_item_entities_free(&copy); return false; }
    mc_item_entities_free(destination); *destination=copy; return true;
}

typedef struct { double low[3],high[3]; } entity_box;
static entity_box bounds(const mc_item_entity *e) {
    entity_box box={{e->x-0.125,e->y,e->z-0.125},{e->x+0.125,e->y+0.25,e->z+0.125}}; return box;
}
static double collision_offset(const mc_world *world,entity_box *box,unsigned axis,double delta) {
    double lo[3],hi[3];
    for (unsigned i=0;i<3;i++) { lo[i]=box->low[i]; hi[i]=box->high[i]; }
    if (delta<0) lo[axis]+=delta; else hi[axis]+=delta;
    /* A fence/wall reaches half a block beyond its containing Y cell. */
    int min_x=(int)floor(lo[0]),max_x=(int)floor(hi[0]);
    int min_y=(int)floor(lo[1])-1,max_y=(int)floor(hi[1]);
    int min_z=(int)floor(lo[2]),max_z=(int)floor(hi[2]);
    for (int y=min_y;y<=max_y;y++) for (int z=min_z;z<=max_z;z++) for (int x=min_x;x<=max_x;x++) {
        mc_box shapes[3]; unsigned n=mc_block_collision(mc_world_get(world,x,y,z),shapes);
        for (unsigned i=0;i<n;i++) {
            double low[3]={x+shapes[i].min_x,y+shapes[i].min_y,z+shapes[i].min_z};
            double high[3]={x+shapes[i].max_x,y+shapes[i].max_y,z+shapes[i].max_z};
            bool overlap=true;
            for (unsigned j=0;j<3;j++) if (j!=axis && (box->high[j]<=low[j] || box->low[j]>=high[j])) overlap=false;
            if (!overlap) continue;
            if (delta>0 && box->high[axis]<=low[axis]) {
                double distance=low[axis]-box->high[axis]; if (distance<delta) delta=distance;
            } else if (delta<0 && box->low[axis]>=high[axis]) {
                double distance=high[axis]-box->low[axis]; if (distance>delta) delta=distance;
            }
        }
    }
    box->low[axis]+=delta; box->high[axis]+=delta; return delta;
}
static uint32_t random_word(uint32_t n) {
    n^=n>>16; n*=UINT32_C(0x7feb352d); n^=n>>15; n*=UINT32_C(0x846ca68b); return n^(n>>16);
}
static double random_signed(uint32_t seed) {
    double a=(random_word(seed)&0xffffffu)/16777216.0;
    double b=(random_word(seed+1)&0xffffffu)/16777216.0;
    return (a-b)*0.2;
}
static bool full_cube(uint16_t state) {
    mc_box boxes[3]; unsigned count=mc_block_collision(state,boxes);
    return count==1 && mc_block_opaque(state) && boxes[0].min_x==0 && boxes[0].min_y==0 && boxes[0].min_z==0 &&
        boxes[0].max_x==1 && boxes[0].max_y==1 && boxes[0].max_z==1;
}
static bool push_out(mc_item_entity *e,const mc_world *world) {
    entity_box box=bounds(e); bool overlaps=false;
    for (int y=(int)floor(box.low[1])-1;y<=(int)floor(box.high[1]);y++)
        for (int z=(int)floor(box.low[2]);z<=(int)floor(box.high[2]);z++)
            for (int x=(int)floor(box.low[0]);x<=(int)floor(box.high[0]);x++) {
                mc_box shapes[3]; unsigned n=mc_block_collision(mc_world_get(world,x,y,z),shapes);
                for (unsigned i=0;i<n;i++) if (box.high[0]>x+shapes[i].min_x && box.low[0]<x+shapes[i].max_x &&
                    box.high[1]>y+shapes[i].min_y && box.low[1]<y+shapes[i].max_y &&
                    box.high[2]>z+shapes[i].min_z && box.low[2]<z+shapes[i].max_z) overlaps=true;
            }
    int x=(int)floor(e->x),y=(int)floor(e->y+0.125),z=(int)floor(e->z);
    if (!overlaps && !full_cube(mc_world_get(world,x,y,z))) return false;
    unsigned direction=2; double nearest=10000;
    const int offsets[5][3]={{-1,0,0},{1,0,0},{0,1,0},{0,0,-1},{0,0,1}};
    const double distances[5]={e->x-x,1-(e->x-x),1-(e->y+0.125-y),e->z-z,1-(e->z-z)};
    for (unsigned i=0;i<5;i++) if (!full_cube(mc_world_get(world,x+offsets[i][0],y+offsets[i][1],z+offsets[i][2])) && distances[i]<nearest) {
        nearest=distances[i]; direction=i;
    }
    double speed=0.1+(random_word((uint32_t)e->eid+e->ticks)&0xffffffu)/16777216.0*0.2;
    if (direction<2) e->vx=direction==0 ? -speed : speed;
    else if (direction==2) e->vy=speed;
    else e->vz=direction==3 ? -speed : speed;
    return true;
}
bool mc_item_entity_tick(mc_item_entity *e,const mc_world *world) {
    if (!world || !mc_item_entity_valid(e) || !e->health || e->age>=6000 || e->y< -64) return false;
    e->ticks++;
    if (e->pickup_delay>0 && e->pickup_delay!=32767) e->pickup_delay--;
    double previous_x=e->x,previous_y=e->y,previous_z=e->z;
    unsigned material=mc_world_get(world,(int)floor(e->x),(int)floor(e->y),(int)floor(e->z))>>4;
    if (material==10 || material==11) e->health=e->health>4 ? e->health-4 : 0;
    else if ((material==51 && e->ticks%20==0) || material==81) e->health=e->health>0 ? e->health-1 : 0;
    if (!e->health) return false;
    e->vy-=0.03999999910593033;
    bool no_clip=push_out(e,world);
    entity_box box=bounds(e); double old_vy=e->vy;
    double dy=e->vy,dx=e->vx,dz=e->vz;
    if (no_clip) {
        box.low[0]+=dx; box.high[0]+=dx; box.low[1]+=dy; box.high[1]+=dy; box.low[2]+=dz; box.high[2]+=dz;
    } else {
        dy=collision_offset(world,&box,1,e->vy); dx=collision_offset(world,&box,0,e->vx); dz=collision_offset(world,&box,2,e->vz);
    }
    e->x=(box.low[0]+box.high[0])*0.5; e->y=box.low[1]; e->z=(box.low[2]+box.high[2])*0.5;
    e->on_ground=dy!=old_vy && old_vy<0;
    if (dx!=e->vx) e->vx=0;
    if (dz!=e->vz) e->vz=0;
    unsigned below=mc_world_get(world,(int)floor(e->x),(int)floor(e->y)-1,(int)floor(e->z))>>4;
    if (dy!=old_vy) e->vy=below==165 && old_vy<0 ? -old_vy : 0;
    bool moved=(int)previous_x!=(int)e->x || (int)previous_y!=(int)e->y || (int)previous_z!=(int)e->z;
    if (moved || e->ticks%25==0) {
        material=mc_world_get(world,(int)floor(e->x),(int)floor(e->y),(int)floor(e->z))>>4;
        if (material==10 || material==11) {
            e->vy=0.20000000298023224; e->vx=random_signed((uint32_t)e->eid+e->ticks*4u);
            e->vz=random_signed((uint32_t)e->eid+e->ticks*4u+2u);
        }
    }
    float friction=0.98f;
    if (e->on_ground) {
        float slipperiness=(below==79 || below==174) ? 0.98f : below==165 ? 0.8f : 0.6f;
        friction=slipperiness*0.98f;
    }
    e->vx*=friction; e->vz*=friction; e->vy*=0.9800000190734863;
    if (e->on_ground) e->vy*= -0.5;
    if (e->age!=-32768) e->age++;
    return e->age<6000 && e->y>= -64;
}
bool mc_item_entity_pickup_eligible(const mc_item_entity *e,const char *name) {
    return mc_item_entity_valid(e) && e->health>0 && e->age<6000 && e->pickup_delay==0 && name &&
        (!e->owner[0] || !strcmp(e->owner,name) || e->age>=5800);
}
bool mc_item_entity_pickup_near(const mc_item_entity *e,double x,double y,double z) {
    if (!mc_item_entity_valid(e) || !isfinite(x) || !isfinite(y) || !isfinite(z)) return false;
    return e->x+0.125>x-1.3 && e->x-0.125<x+1.3 && e->z+0.125>z-1.3 && e->z-0.125<z+1.3 &&
        e->y+0.25>y-0.5 && e->y<y+2.3;
}
static bool damageable(int id) {
    return (id>=256 && id<=259) || id==261 || (id>=267 && id<=279) ||
        (id>=283 && id<=286) || (id>=290 && id<=294) || (id>=298 && id<=317) || id==346 || id==359 || id==398;
}
static bool damaged_item(const mc_slot *slot) {
    if (!damageable(slot->item_id) || !slot->damage) return false;
    mc_nbt_view root,field; int64_t value;
    if (slot->nbt.size && mc_nbt_root(&slot->nbt,&root) && mc_nbt_find(&root,"Unbreakable",&field)) {
        if (mc_nbt_get_integer(&field,&value) && (uint8_t)value!=0) return false;
        double number;
        if ((field.type==5 || field.type==6) && mc_nbt_get_number(&field,&number)) {
            int32_t integral=isnan(number) ? 0 : number>=INT32_MAX ? INT32_MAX : number<=INT32_MIN ? INT32_MIN : (int32_t)number;
            uint32_t floor_value=(uint32_t)integral;
            if (number<integral) --floor_value;
            if ((floor_value&255u)!=0) return false;
        }
    }
    return true;
}
static int main_slot(unsigned index) { return index<9 ? 36+(int)index : (int)index; }
static bool merge_matches(const mc_slot *a,const mc_slot *b) {
    return a->item_id==b->item_id && (!mc_item_has_subtypes(a->item_id) || a->damage==b->damage) && mc_nbt_equal(&a->nbt,&b->nbt);
}
bool mc_item_entity_pickup(mc_item_entity *e,mc_inventory *inventory,bool creative,unsigned *inserted,unsigned *discarded) {
    if (!inventory || !inserted || !discarded || inserted==discarded || !mc_item_entity_valid(e)) return false;
    mc_inventory next; mc_inventory_init(&next); mc_item_entity item; mc_item_entity_init(&item);
    if (!mc_inventory_copy(&next,inventory) || !mc_item_entity_copy(&item,e)) {
        mc_inventory_free(&next); mc_item_entity_free(&item); return false;
    }
    unsigned initial=item.item.count,remaining=initial;
    bool damaged=damaged_item(&item.item);
    if (damaged) {
        for (unsigned i=0;i<36;i++) {
            mc_slot *slot=&next.slots[main_slot(i)];
            if (slot->item_id==-1) {
                if (!mc_slot_copy(slot,&item.item)) goto failed;
                remaining=0; break;
            }
        }
    } else {
        unsigned limit=mc_item_stack_limit(item.item.item_id);
        for (unsigned pass=0;pass<2 && remaining;pass++) for (unsigned i=0;i<36 && remaining;i++) {
            mc_slot *slot=&next.slots[main_slot(i)];
            if (pass==0 && (slot->item_id==-1 || !merge_matches(slot,&item.item))) continue;
            if (pass==1 && slot->item_id!=-1) continue;
            unsigned available=slot->count<limit ? limit-slot->count : 0;
            unsigned moved=remaining<available ? remaining : available;
            if (!moved) continue;
            if (slot->item_id==-1) { if (!mc_slot_copy(slot,&item.item)) goto failed; slot->count=0; }
            slot->count=(uint8_t)(slot->count+moved); remaining-=moved;
        }
    }
    unsigned consumed=creative ? remaining : 0;
    if (remaining==0 || creative) mc_slot_free(&item.item);
    else item.item.count=(uint8_t)remaining;
    mc_inventory_free(inventory); *inventory=next;
    mc_item_entity_free(e); *e=item; *inserted=initial-remaining; *discarded=consumed; return true;
failed:
    mc_inventory_free(&next); mc_item_entity_free(&item); return false;
}
bool mc_item_entity_merge(mc_item_entity *a,mc_item_entity *b) {
    if (a==b || !mc_item_entity_valid(a) || !mc_item_entity_valid(b) ||
        !a->health || !b->health || a->age>=6000 || b->age>=6000 ||
        a->pickup_delay==32767 || b->pickup_delay==32767 || a->age==-32768 || b->age==-32768 ||
        !merge_matches(&a->item,&b->item) || a->item.count+b->item.count>mc_item_stack_limit(a->item.item_id)) return false;
    mc_item_entity *destination=a->item.count>b->item.count ? a : b;
    mc_item_entity *source=destination==a ? b : a;
    destination->item.count=(uint8_t)(a->item.count+b->item.count);
    if (source->pickup_delay>destination->pickup_delay) destination->pickup_delay=source->pickup_delay;
    if (source->age<destination->age) destination->age=source->age;
    mc_slot_free(&source->item); return true;
}

static int16_t wire_speed(double velocity) {
    if (velocity>3.9) velocity=3.9;
    if (velocity< -3.9) velocity= -3.9;
    return (int16_t)(velocity*8000);
}
static bool packet_finish(mc_buf *destination,mc_buf *packet) {
    if (packet->failed) { mc_buf_free(packet); return false; }
    mc_buf_free(destination); *destination=*packet; return true;
}
bool mc_item_entity_spawn(const mc_item_entity *e,mc_buf *output) {
    if (!output || !mc_item_entity_valid(e)) return false;
    mc_buf packet={0}; mc_put_varint(&packet,0x0e); mc_put_varint(&packet,e->eid); mc_put_u8(&packet,2);
    mc_put_i32(&packet,(int32_t)floor(e->x*32)); mc_put_i32(&packet,(int32_t)floor(e->y*32)); mc_put_i32(&packet,(int32_t)floor(e->z*32));
    mc_put_u8(&packet,0); mc_put_u8(&packet,0); mc_put_i32(&packet,1);
    mc_put_i16(&packet,wire_speed(e->vx)); mc_put_i16(&packet,wire_speed(e->vy)); mc_put_i16(&packet,wire_speed(e->vz));
    return packet_finish(output,&packet);
}
bool mc_item_entity_metadata(const mc_item_entity *e,mc_buf *output) {
    if (!output || !mc_item_entity_valid(e)) return false;
    mc_buf packet={0}; mc_put_varint(&packet,0x1c); mc_put_varint(&packet,e->eid); mc_put_u8(&packet,0xaa);
    mc_slot_write(&packet,&e->item); mc_put_u8(&packet,0x7f); return packet_finish(output,&packet);
}
bool mc_item_entity_velocity(const mc_item_entity *e,mc_buf *output) {
    if (!output || !mc_item_entity_valid(e)) return false;
    mc_buf packet={0}; mc_put_varint(&packet,0x12); mc_put_varint(&packet,e->eid);
    mc_put_i16(&packet,wire_speed(e->vx)); mc_put_i16(&packet,wire_speed(e->vy)); mc_put_i16(&packet,wire_speed(e->vz));
    return packet_finish(output,&packet);
}

static void named(mc_buf *b,uint8_t type,const char *name) {
    mc_put_u8(b,type); mc_put_i16(b,(int16_t)strlen(name)); mc_put_bytes(b,name,strlen(name));
}
static void string_tag(mc_buf *b,const char *name,const char *value) {
    named(b,8,name); mc_put_i16(b,(int16_t)strlen(value)); mc_put_bytes(b,value,strlen(value));
}
static bool raw_name(const mc_nbt *n,const char *name) {
    size_t length=strlen(name);
    return n->size>=3 && (((size_t)n->data[1]<<8)|n->data[2])==length && n->size>=3+length && !memcmp(n->data+3,name,length);
}
/* Walk named children once. This avoids repeatedly validating/scanning a large
   list for each index and preserves untouched tag encodings byte for byte. */
static bool unknown_fields(mc_buf *output,const mc_nbt *original,const char *const *known,size_t known_count) {
    if (!original->size) return true;
    mc_nbt_view root; if (!mc_nbt_root(original,&root) || root.type!=10) return false;
    mc_buf input={0}; input.data=(uint8_t *)root.data; input.len=root.size;
    while (input.pos<input.len && input.data[input.pos]) {
        mc_nbt child={0}; if (!mc_nbt_read(&input,&child)) return false;
        bool skip=false; for (size_t i=0;i<known_count;i++) if (raw_name(&child,known[i])) skip=true;
        if (!skip) mc_put_bytes(output,child.data,child.size);
        mc_nbt_free(&child); if (output->failed) return false;
    }
    return input.pos+1==input.len && input.data[input.pos]==0;
}
static void compound_root(mc_buf *output,const mc_nbt *original) {
    if (original->size) {
        size_t prefix=3+(((size_t)original->data[1]<<8)|original->data[2]); mc_put_bytes(output,original->data,prefix);
    } else { mc_put_u8(output,10); mc_put_i16(output,0); }
}
static bool payload_to_nbt(const mc_nbt_view *view,mc_nbt *output) {
    if (view->type!=10) return false;
    mc_buf b={0}; mc_put_u8(&b,10); mc_put_i16(&b,0); mc_put_bytes(&b,view->data,view->size);
    bool okay=!b.failed && mc_nbt_read(&b,output); mc_buf_free(&b); return okay;
}
static bool item_encode(const mc_item_entity *e,mc_buf *output) {
    static const char *const known[]={"id","Count","Damage","tag"};
    mc_nbt prior={0}; mc_nbt_view root,view;
    if (e->original_nbt.size && mc_nbt_root(&e->original_nbt,&root) && mc_nbt_find(&root,"Item",&view)) {
        if (!payload_to_nbt(&view,&prior)) return false;
    }
    named(output,10,"Item");
    bool okay=unknown_fields(output,&prior,known,sizeof(known)/sizeof(*known)); mc_nbt_free(&prior);
    if (!okay) return false;
    string_tag(output,"id",mc_item_resource_name(e->item.item_id));
    named(output,1,"Count"); mc_put_u8(output,e->item.count);
    named(output,2,"Damage"); mc_put_i16(output,e->item.damage);
    if (e->item.nbt.size) {
        if (!mc_nbt_root(&e->item.nbt,&view)) return false;
        named(output,10,"tag"); mc_put_bytes(output,view.data,view.size);
    }
    mc_put_u8(output,0); return !output->failed;
}
static bool entity_encode(const mc_item_entity *e,mc_buf *output) {
    static const char *const known[]={"id","C919EntityId","Pos","Motion","Age","PickupDelay","Health","Item","Owner","Thrower","OnGround"};
    if (!mc_item_entity_valid(e) || !unknown_fields(output,&e->original_nbt,known,sizeof(known)/sizeof(*known))) return false;
    string_tag(output,"id","Item"); named(output,3,"C919EntityId"); mc_put_i32(output,e->eid);
    named(output,9,"Pos"); mc_put_u8(output,6); mc_put_i32(output,3); mc_put_f64(output,e->x); mc_put_f64(output,e->y); mc_put_f64(output,e->z);
    named(output,9,"Motion"); mc_put_u8(output,6); mc_put_i32(output,3); mc_put_f64(output,e->vx); mc_put_f64(output,e->vy); mc_put_f64(output,e->vz);
    named(output,2,"Age"); mc_put_i16(output,(int16_t)e->age);
    named(output,2,"PickupDelay"); mc_put_i16(output,(int16_t)e->pickup_delay);
    named(output,2,"Health"); mc_put_i16(output,(int16_t)e->health);
    named(output,1,"OnGround"); mc_put_u8(output,e->on_ground ? 1 : 0);
    if (e->owner[0]) string_tag(output,"Owner",e->owner);
    if (e->thrower[0]) string_tag(output,"Thrower",e->thrower);
    return item_encode(e,output);
}
bool mc_item_entities_encode(const mc_item_entities *e,mc_nbt *output) {
    static const char *const known[]={"Version","Entities"};
    if (!output || !list_shape(e) || !compound_or_empty(&e->original_nbt)) return false;
    mc_buf b={0}; compound_root(&b,&e->original_nbt);
    if (!unknown_fields(&b,&e->original_nbt,known,2)) goto failed;
    named(&b,3,"Version"); mc_put_i32(&b,1); named(&b,9,"Entities"); mc_put_u8(&b,10); mc_put_i32(&b,(int32_t)e->count);
    for (size_t i=0;i<e->count;i++) {
        for (size_t j=0;j<i;j++) if (e->entries[j].eid==e->entries[i].eid) goto failed;
        if (!entity_encode(&e->entries[i],&b)) goto failed;
        mc_put_u8(&b,0);
    }
    mc_put_u8(&b,0);
    if (b.failed || !mc_nbt_read(&b,output)) goto failed;
    mc_buf_free(&b); return true;
failed:
    mc_buf_free(&b); return false;
}
static bool field_integer(const mc_nbt_view *root,const char *name,uint8_t type,int64_t *value) {
    mc_nbt_view field; return mc_nbt_find(root,name,&field) && field.type==type && mc_nbt_get_integer(&field,value);
}
static bool vector_decode(const mc_nbt_view *root,const char *name,double values[3]) {
    mc_nbt_view list; if (!mc_nbt_find(root,name,&list) || list.type!=9 || list.size!=29 || list.data[0]!=6 ||
        list.data[1]!=0 || list.data[2]!=0 || list.data[3]!=0 || list.data[4]!=3) return false;
    mc_buf input={0}; input.data=(uint8_t *)list.data; input.pos=5; input.len=list.size;
    for (unsigned i=0;i<3;i++) { values[i]=mc_get_f64(&input); if (!isfinite(values[i])) return false; }
    return !input.failed;
}
static bool optional_name(const mc_nbt_view *root,const char *name,char output[17]) {
    mc_nbt_view view;
    if (!mc_nbt_find(root,name,&view)) { output[0]=0; return true; }
    return view.type==8 && mc_nbt_get_string(&view,output,17) && name_valid(output);
}
static bool entity_decode(const mc_nbt *nbt,mc_item_entity *e) {
    mc_nbt_view root,view,item; char name[64]; int64_t value; double pos[3],motion[3];
    if (!mc_nbt_root(nbt,&root) || root.type!=10 || !mc_nbt_find(&root,"id",&view) ||
        !mc_nbt_get_string(&view,name,sizeof(name)) || strcmp(name,"Item") ||
        !field_integer(&root,"C919EntityId",3,&value) || value<INT32_MIN || value>INT32_MAX) return false;
    e->eid=(int32_t)value;
    if (!vector_decode(&root,"Pos",pos) || !vector_decode(&root,"Motion",motion)) return false;
    e->x=pos[0]; e->y=pos[1]; e->z=pos[2]; e->vx=motion[0]; e->vy=motion[1]; e->vz=motion[2];
    if (!field_integer(&root,"Age",2,&value)) return false;
    e->age=(int)value;
    if (!field_integer(&root,"PickupDelay",2,&value) || value<0) return false;
    e->pickup_delay=(int)value;
    if (!field_integer(&root,"Health",2,&value) || value<0) return false;
    e->health=(int)value;
    if (mc_nbt_find(&root,"OnGround",&view)) {
        if (view.type!=1 || !mc_nbt_get_integer(&view,&value) || (value!=0 && value!=1)) return false;
        e->on_ground=value!=0;
    }
    if (!optional_name(&root,"Owner",e->owner) || !optional_name(&root,"Thrower",e->thrower) ||
        !mc_nbt_find(&root,"Item",&item) || item.type!=10 || !mc_nbt_find(&item,"id",&view)) return false;
    int16_t id;
    if (view.type==8) { if (!mc_nbt_get_string(&view,name,sizeof(name)) || !mc_item_from_resource_name(name,&id)) return false; }
    else { if (!mc_nbt_get_integer(&view,&value) || value<0 || value>INT16_MAX || !mc_item_valid((int16_t)value)) return false; id=(int16_t)value; }
    int64_t count,damage;
    if (!field_integer(&item,"Count",1,&count) || count<1 || count>127 || !field_integer(&item,"Damage",2,&damage) || damage<0 ||
        !mc_slot_set(&e->item,id,(uint8_t)count,(int16_t)damage)) return false;
    if (mc_nbt_find(&item,"tag",&view) && !payload_to_nbt(&view,&e->item.nbt)) return false;
    return mc_nbt_copy(&e->original_nbt,nbt) && mc_item_entity_valid(e);
}
static bool next_compound(mc_buf *input,mc_nbt *nbt) {
    mc_buf b={0}; mc_put_u8(&b,10); mc_put_i16(&b,0);
    while (input->pos<input->len && input->data[input->pos]) {
        mc_nbt child={0}; if (!mc_nbt_read(input,&child)) { mc_buf_free(&b); return false; }
        mc_put_bytes(&b,child.data,child.size); mc_nbt_free(&child);
        if (b.failed) { mc_buf_free(&b); return false; }
    }
    if (input->pos==input->len || mc_get_u8(input)!=0) { mc_buf_free(&b); return false; }
    mc_put_u8(&b,0); bool okay=!b.failed && mc_nbt_read(&b,nbt); mc_buf_free(&b); return okay;
}
bool mc_item_entities_decode(const mc_nbt *input,mc_item_entities *output) {
    if (!output || !input) return false;
    mc_nbt_view root,list; int64_t version;
    if (!mc_nbt_root(input,&root) || root.type!=10 || !field_integer(&root,"Version",3,&version) || version!=1 ||
        !mc_nbt_find(&root,"Entities",&list) || list.type!=9 || list.size<5 || list.data[0]!=10) return false;
    mc_buf wire={0}; wire.data=(uint8_t *)list.data; wire.len=list.size; wire.pos=1;
    int32_t count=mc_get_i32(&wire); if (count<0 || (unsigned)count>MC_MAX_ITEM_ENTITIES) return false;
    mc_item_entities next; mc_item_entities_init(&next);
    if (!mc_nbt_copy(&next.original_nbt,input)) return false;
    for (int32_t i=0;i<count;i++) {
        mc_nbt nbt={0}; mc_item_entity e; mc_item_entity_init(&e);
        bool okay=next_compound(&wire,&nbt) && entity_decode(&nbt,&e) && add_unchecked(&next,&e);
        mc_nbt_free(&nbt); mc_item_entity_free(&e);
        if (!okay) { mc_item_entities_free(&next); return false; }
    }
    if (wire.failed || wire.pos!=wire.len) { mc_item_entities_free(&next); return false; }
    mc_nbt encoded={0}; bool okay=mc_item_entities_encode(&next,&encoded); mc_nbt_free(&encoded);
    if (!okay) { mc_item_entities_free(&next); return false; }
    mc_item_entities_free(output); *output=next; return true;
}
