#include "item/ItemMap.h"
#include "util/MCGameplayPlayer.h"
#include "util/MCGameplayCrafting.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static unsigned checks;
#define CHECK(v) do { ++checks; if (!(v)) { fprintf(stderr,"map refs %u at %d: %s\n",checks,__LINE__,#v); exit(1); } } while (0)
/* Map-only fixtures use real source constructor/grid calls. Reaching a craft,
   achievement or drop effect is a test failure, never a production no-op. */
static bool no_craft(ItemStack *s,MCObject *w,MCObject *p,int32_t n) {(void)s;(void)w;(void)n;MCObjectHeap_fail(p->heap);return false;}
static bool no_stat(MCObject *p,mc_crafting_achievement a) {(void)a;MCObjectHeap_fail(p->heap);return false;}
static bool no_drop(MCObject *p,ItemStack *s,bool scatter) {(void)s;(void)scatter;MCObjectHeap_fail(p->heap);return false;}
static ItemStack *recipe(InventoryCrafting *g,MCObject *o) {return CraftingManager_findMatchingRecipe(((MCGameplayWorld *)o)->manager,g,o,NULL,NULL);}
static ItemStackArray *remaining(InventoryCrafting *g,MCObject *o) {return CraftingManager_func_180303_b(((MCGameplayWorld *)o)->manager,g,o);}
static MCGameplayWorld *setup(MCGameplay *game,const mc_world *terrain) {
    CHECK(MCGameplay_init(game,32*1024*1024));
    CraftingManager *manager=CraftingManager_newEmpty(game->heap); CHECK(manager);
    MCGameplayWorld *world=MCGameplayWorld_new(game->heap,MCGameplay_get(game),terrain,manager); CHECK(world);
    CHECK(MCGameplay_setWorld(game,(MCObject *)world)); return world;
}
static MCGameplayPlayer *actor(MCGameplay *game,MCGameplayWorld *world,unsigned index,const char *name) {
    static mc_crafting_dispatch deps;
    const MCGameplayCraftingEffects effects={no_craft,no_stat,no_drop};
    CHECK(MCGameplayCrafting_nativeDispatch(&deps,&effects)); deps.findMatchingRecipe=recipe; deps.getRemainingItems=remaining;
    MCGameplayPlayer *p=MCGameplayPlayer_new(world,NBTString_fromASCII(game->heap,name),NULL,&deps); CHECK(p);
    char uuid[37]; snprintf(uuid,sizeof(uuid),"00000000-0000-0000-0000-%012u",index+1);
    CHECK(MCGameplay_setPlayer(game,index,uuid,(MCObject *)p)); p->living.entity.dimension=world->dimension;
    return p;
}
static ItemStack *held(MCGameplayPlayer *p,int32_t count,int32_t damage,int slot) {
    ItemStack *s=ItemStack_new(p->living.entity.object.heap,ItemStack_registryItem(358),count,damage); CHECK(s);
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,slot,s)); return s;
}
static mc_map_info *add_map(MCGameplayWorld *w,int32_t id,uint8_t scale) {
    mc_map_info m={0}; m.id=id; m.scale=scale; m.dimension=(int8_t)w->dimension; m.metadata_known=true;
    CHECK(mc_maps_add(&w->maps,&m)); return mc_maps_find(&w->maps,id);
}
static unsigned rectangle(mc_buf *b,int32_t id,unsigned *x,unsigned *z,unsigned *height) {
    b->pos=0; CHECK(mc_get_varint(b)==0x34 && mc_get_varint(b)==id); (void)mc_get_u8(b);
    int32_t n=mc_get_varint(b); CHECK(n>=0 && n<=256);
    for (int32_t i=0;i<n;i++) { (void)mc_get_u8(b); (void)mc_get_u8(b); (void)mc_get_u8(b); }
    unsigned width=mc_get_u8(b); *height=width ? mc_get_u8(b) : 0; *x=width ? mc_get_u8(b) : 0; *z=width ? mc_get_u8(b) : 0;
    CHECK(!b->failed); return width;
}
static void lifecycle(void) {
    MCGameplay game={0}; MCGameplayWorld *w=setup(&game,NULL);
    MCGameplayPlayer *a=actor(&game,w,0,"A"),*b=actor(&game,w,1,"B"); a->living.entity.entityId=0; b->living.entity.entityId=-1;
    ItemStack *s=held(a,0,4,0),*second=held(b,-1,4,38); mc_map_info *m=add_map(w,4,0); bool changed=true;
    CHECK(ItemMap_onUpdate(s,w,(MCObject *)a,0,false,&changed) && !changed);
    CHECK(ItemMap_onUpdate(second,w,(MCObject *)b,2,false,&changed) && !changed);
    CHECK(m->icon_count==2 && InventoryPlayer_hasItemStack(a->inventory,s) && InventoryPlayer_hasItemStack(b->inventory,s));
    mc_MapInfo *ia=MapData_getMapInfo(m,w,a),*ib=MapData_getMapInfo(m,w,b); CHECK(ia && ib && ia!=ib);
    CHECK(ia->update_counter==0 && ib->update_counter==0);
    b->living.entity.entityId=0; CHECK(MapData_getMapInfo(m,w,b)==ia); b->living.entity.entityId=-1;
    mc_buf packet={0}; unsigned x,z,h;
    CHECK(ItemMap_createMapDataPacket(s,w,a,&packet)==1 && rectangle(&packet,4,&x,&z,&h)==128 && h==128);
    CHECK(ItemMap_createMapDataPacket(second,w,b,&packet)==1 && rectangle(&packet,4,&x,&z,&h)==128);
    CHECK(ItemMap_createMapDataPacket(s,w,a,&packet)==1 && rectangle(&packet,4,&x,&z,&h)==0);
    for (int i=0;i<4;i++) CHECK(ItemMap_createMapDataPacket(s,w,a,&packet)==0);
    CHECK(ItemMap_createMapDataPacket(s,w,a,&packet)==1 && rectangle(&packet,4,&x,&z,&h)==0);
    mc_MapData_updateMapData(m,3,5); mc_MapData_updateMapData(m,7,8);
    CHECK(m->dirty && ItemMap_createMapDataPacket(s,w,a,&packet)==1 && rectangle(&packet,4,&x,&z,&h)==5 && x==3 && z==5 && h==4);
    ia->packet_counter=INT32_MAX;
    CHECK(ItemMap_createMapDataPacket(s,w,a,&packet)==0 && ia->packet_counter==(uint32_t)INT32_MAX+1);
    CHECK(ItemMap_createMapDataPacket(s,w,a,&packet)==0 && ia->packet_counter==(uint32_t)INT32_MAX+2);
    ia->packet_counter=(uint32_t)-5;
    CHECK(ItemMap_createMapDataPacket(s,w,a,&packet)==1 && ia->packet_counter==(uint32_t)-4);
    a->living.entity.isDead=b->living.entity.isDead=true;
    CHECK(MapData_updateVisiblePlayers(m,w,a,s));
    CHECK(MapData_getMapPacket(m,s,w,a,&packet)==0 && MapData_getMapPacket(m,second,w,b,&packet)==1);
    CHECK(MapData_updateVisiblePlayers(m,w,b,second));
    CHECK(MapData_getMapPacket(m,second,w,b,&packet)==0);
    CHECK(!MCObjectHeap_failed(game.heap)); mc_buf_free(&packet); CHECK(MCGameplay_free(&game));
}
static NBTTagCompound *decorations(ItemStack *s,NBTString *key,double x,double rot) {
    NBTTagCompound *root=NBTTagCompound_new(s->object.heap),*entry=NBTTagCompound_new(s->object.heap); NBTTagList *list=NBTTagList_new(s->object.heap);
    CHECK(root && entry && list && NBTTagCompound_setString_ascii(entry,"id",key));
    CHECK(NBTTagCompound_setDouble_ascii(entry,"x",x) && NBTTagCompound_setDouble_ascii(entry,"z",0));
    CHECK(NBTTagCompound_setDouble_ascii(entry,"rot",rot) && NBTTagCompound_setByte_ascii(entry,"type",5));
    CHECK(NBTTagList_appendTag(list,(NBTBase *)entry) && NBTTagCompound_setTag_ascii(root,"Decorations",(NBTBase *)list));
    CHECK(ItemStack_setTagCompound(s,root)); return entry;
}
static bool named_a(const MCObject *o,void *context) {(void)context;return NBTString_equalsASCII(((const MCGameplayPlayer *)o)->gameProfile->name,"A");}
static void nbt_and_graph_identity(void) {
    MCGameplay game={0}; MCGameplayWorld *w=setup(&game,NULL); MCGameplayPlayer *p=actor(&game,w,0,"A");
    /* This MapData snapshot fixture starts at the map centre, independently
       of EntityPlayer constructor spawn coordinates. */
    CHECK(Entity_setPosition(&p->living.entity,0,1,0));
    ItemStack *s=held(p,1,4,0); mc_map_info *m=add_map(w,4,0);
    const uint16_t units[]={'x',0,0xd800,'x'}; NBTString *key=NBTString_fromUTF16(game.heap,units,4); CHECK(key);
    NBTTagCompound *entry=decorations(s,key,-63.01,90); NBTTagCompound *tag=s->stackTagCompound;
    CHECK(NBTTagCompound_setTag_ascii(tag,"cycle",(NBTBase *)tag));
    CHECK(MapData_updateVisiblePlayers(m,w,p,s) && s->stackTagCompound==tag && m->icon_count==2 && m->icons[1].type==6 && m->icons[1].x==-128);
    CHECK(NBTTagCompound_setDouble_ascii(entry,"x",40));
    CHECK(MapData_updateVisiblePlayers(m,w,p,s) && m->icons[1].x==-128); /* Existing key is never overwritten by NBT. */
    CHECK(NBTTagCompound_removeTag_ascii(tag,"Decorations"));
    CHECK(MCGameplay_setPlayer(&game,0,NULL,NULL));
    CHECK(MCObjectHeap_collect(game.heap)); /* Player + isolated UTF16 key remain MapData strong edges. */
    CHECK(MapData_getMapInfo(m,w,p) && NBTString_length(key)==4 && NBTString_units(key)[2]==0xd800);
    MCGameplayTransaction tx={0}; CHECK(MCGameplay_begin(&game,&tx)); MCGameplayWorld *cw=(MCGameplayWorld *)MCGameplay_get(&tx.working)->world;
    MCGameplayPlayer *cp=(MCGameplayPlayer *)MCObjectHeap_findObject(tx.working.heap,p->living.entity.object.klass,named_a,NULL); CHECK(cp && cp!=p && ((MCGameplayWorld *)(cp->living.entity.worldObj))==cw);
    NBTTagCompound *ctag=InventoryPlayer_getStackInSlot(cp->inventory,0)->stackTagCompound;
    CHECK(ctag!=tag && NBTTagCompound_getTag_ascii(ctag,"cycle")== (NBTBase *)ctag);
    mc_map_info *cm=mc_maps_find(&cw->maps,4); CHECK(cm && cm->tracking!=m->tracking && cm->icon_count==2);
    CHECK(MCObjectHeap_collect(tx.working.heap)); mc_buf packet={0};
    CHECK(MapData_getMapPacket(cm,InventoryPlayer_getStackInSlot(cp->inventory,0),cw,cp,&packet)==1);
    mc_MapInfo *before=MapData_getMapInfo(m,w,p),*after=MapData_getMapInfo(cm,cw,cp); CHECK(before->dirty && !after->dirty && before!=after);
    cp->living.entity.posX=30; CHECK(MapData_updateVisiblePlayers(cm,cw,cp,InventoryPlayer_getStackInSlot(cp->inventory,0)) && cm->icons[0].x==60 && m->icons[0].x==0);
    CHECK(MCGameplay_abort(&tx)); CHECK(MapData_getMapInfo(m,w,p)->dirty && m->icon_count==2);
    mc_buf_free(&packet); CHECK(MCGameplay_free(&game));
}
static void branches_and_failures(void) {
    MCGameplay game={0}; MCGameplayWorld *w=setup(&game,NULL); MCGameplayPlayer *p=actor(&game,w,0,"A"); bool changed=true;
    w->remote=true; CHECK(ItemMap_onUpdate(NULL,w,NULL,0,true,&changed) && !changed && w->maps.count==0);
    w->remote=false; w->spawnX=-65; w->spawnZ=1024; ItemStack *s=held(p,-1,999,0);
    CHECK(ItemMap_onUpdate(s,w,(MCObject *)w,0,true,&changed) && changed && s->itemDamage==0 && w->maps.count==1);
    mc_map_info *m=mc_maps_find(&w->maps,0); CHECK(m && m->scale==3 && m->center_x==-576 && m->center_z==1472);
    mc_buf b={0}; CHECK(ItemMap_createMapDataPacket(s,w,p,&b)==0);
    CHECK(ItemMap_createMapDataPacket(s,w,NULL,&b)==0);
    p->living.entity.dimension=1; CHECK(ItemMap_onUpdate(s,w,(MCObject *)p,0,false,&changed) && !changed && m->icon_count==0);
    w->dimension=1; CHECK(ItemMap_updateMapData(w,(MCObject *)p,m,&changed) && !changed); /* mismatch bypasses terrain */
    w->dimension=0; MCGameplayTransaction tx={0}; CHECK(MCGameplay_begin(&game,&tx));
    MCGameplayWorld *cw=(MCGameplayWorld *)MCGameplay_get(&tx.working)->world; MCGameplayPlayer *cp=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
    ItemStack *cs=InventoryPlayer_getStackInSlot(cp->inventory,0); CHECK(!ItemMap_onUpdate(cs,cw,(MCObject *)cp,0,true,&changed));
    CHECK(MCObjectHeap_failed(tx.working.heap) && MapData_getMapInfo(m,w,p)->update_counter==0); CHECK(MCGameplay_abort(&tx));
    CHECK(MCGameplay_begin(&game,&tx)); cw=(MCGameplayWorld *)MCGameplay_get(&tx.working)->world; cp=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0]; cs=InventoryPlayer_getStackInSlot(cp->inventory,0);
    cs->itemFrame=(EntityItemFrame *)cp; CHECK(!MapData_updateVisiblePlayers(mc_maps_find(&cw->maps,0),cw,cp,cs) && MCObjectHeap_failed(tx.working.heap)); CHECK(MCGameplay_abort(&tx));
    CHECK(!MCObjectHeap_failed(game.heap) && s->itemFrame==NULL);
    CHECK(MCGameplay_begin(&game,&tx)); cw=(MCGameplayWorld *)MCGameplay_get(&tx.working)->world; cp=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
    CHECK(!MapData_updateVisiblePlayers(mc_maps_find(&cw->maps,0),cw,cp,s) && MCObjectHeap_failed(tx.working.heap)); CHECK(MCGameplay_abort(&tx));
    mc_buf_free(&b); CHECK(MCGameplay_free(&game));
}
static void mutable_entity_keys(void) {
    MCGameplay game={0}; MCGameplayWorld *w=setup(&game,NULL); MCGameplayPlayer *p=actor(&game,w,0,"A"); ItemStack *s=held(p,1,4,0); mc_map_info *m=add_map(w,4,0);
    p->living.entity.entityId=0; mc_MapInfo *old=MapData_getMapInfo(m,w,p); CHECK(old); old->update_counter=11;
    p->living.entity.entityId=7; mc_MapInfo *newer=MapData_getMapInfo(m,w,p); CHECK(newer && newer!=old); newer->update_counter=17;
    p->living.entity.isDead=true; CHECK(MapData_updateVisiblePlayers(m,w,p,s));
    p->living.entity.isDead=false; p->living.entity.entityId=0;
    /* Removal of the first list element removes the current hash-7 node; the
       hash-0 node still retains the original MapInfo outside playersArrayList. */
    CHECK(MapData_getMapInfo(m,w,p)==old && old->update_counter==11);
    mc_buf b={0}; CHECK(MapData_getMapPacket(m,s,w,p,&b)==1); p->living.entity.entityId=7;
    mc_MapInfo *third=MapData_getMapInfo(m,w,p); CHECK(third && third!=old && third!=newer && third->update_counter==0);
    CHECK(!MCObjectHeap_failed(game.heap)); mc_buf_free(&b); CHECK(MCGameplay_free(&game));
}
static void packet_failure_rollback(void) {
    MCGameplay game={0}; MCGameplayWorld *w=setup(&game,NULL); MCGameplayPlayer *p=actor(&game,w,0,"A"); ItemStack *s=held(p,1,4,0); mc_map_info *m=add_map(w,4,0);
    CHECK(MapData_updateVisiblePlayers(m,w,p,s)); MCGameplayTransaction tx={0}; CHECK(MCGameplay_begin(&game,&tx));
    MCGameplayWorld *cw=(MCGameplayWorld *)MCGameplay_get(&tx.working)->world; MCGameplayPlayer *cp=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
    mc_map_info *cm=mc_maps_find(&cw->maps,4); mc_MapInfo *info=MapData_getMapInfo(cm,cw,cp); CHECK(info); info->max_x=255;
    mc_buf out={0}; mc_put_u8(&out,42); CHECK(MapData_getMapPacket(cm,InventoryPlayer_getStackInSlot(cp->inventory,0),cw,cp,&out)==-1);
    CHECK(info->dirty && out.len==1 && out.data[0]==42 && MCObjectHeap_failed(tx.working.heap)); CHECK(MCGameplay_abort(&tx));
    CHECK(MapData_getMapInfo(m,w,p)->dirty && !MCObjectHeap_failed(game.heap)); mc_buf_free(&out); CHECK(MCGameplay_free(&game));
}
static void survey(void) {
    mc_world terrain; mc_world_init(&terrain,0); CHECK(mc_world_set(&terrain,0,20,0,2u<<4));
    MCGameplay game={0}; MCGameplayWorld *w=setup(&game,&terrain); MCGameplayPlayer *p=actor(&game,w,0,"A"); ItemStack *s=held(p,1,4,0); mc_map_info *m=add_map(w,4,0); bool changed=false;
    CHECK(ItemMap_onUpdate(s,w,(MCObject *)p,0,false,&changed) && !changed && MapData_getMapInfo(m,w,p)->update_counter==0);
    for (unsigned i=0;i<16;i++) CHECK(ItemMap_onUpdate(s,w,(MCObject *)p,0,true,&changed));
    CHECK(MapData_getMapInfo(m,w,p)->update_counter==16 && m->dirty && m->colors[64+64*128]!=0);
    CHECK(!MCObjectHeap_failed(game.heap)); CHECK(MCGameplay_free(&game)); mc_world_free(&terrain);
}
static void nullable_hash_key(void) {
    MCGameplay game={0}; MCGameplayWorld *w=setup(&game,NULL); MCGameplayPlayer *p=actor(&game,w,0,"A"); ItemStack *s=held(p,1,4,0); mc_map_info *m=add_map(w,4,0); mc_buf b={0};
    CHECK(MapData_getMapPacket(m,s,w,NULL,&b)==0);
    mc_MapInfo *nullInfo=MapData_getMapInfo(m,w,NULL),*playerInfo=MapData_getMapInfo(m,w,p);
    CHECK(nullInfo && playerInfo && nullInfo!=playerInfo && MapData_getMapInfo(m,w,NULL)==nullInfo);
    CHECK(MapData_getMapPacket(m,s,w,NULL,&b)==1 && !nullInfo->dirty && playerInfo->dirty);
    CHECK(MCObjectHeap_collect(game.heap)); CHECK(MapData_getMapInfo(m,w,NULL)==nullInfo);
    mc_buf_free(&b); CHECK(MCGameplay_free(&game));
}
static void packet_fact(mc_map_info *map,ItemStack *stack,MCGameplayWorld *world,MCGameplayPlayer *p,char *out,size_t size) {
    mc_buf b={0}; int result=MapData_getMapPacket(map,stack,world,p,&b); CHECK(result>=0);
    if (!result) snprintf(out,size,"none");
    else { b.pos=0; CHECK(mc_get_varint(&b)==0x34); uLong crc=crc32(0,b.data+b.pos,(uInt)(b.len-b.pos)); snprintf(out,size,"%zu:%lu",b.len-b.pos,(unsigned long)crc); }
    mc_buf_free(&b);
}
static void facts(void) {
    const double xs[]={-.5,-1,-63,-63.01,63,63.01,-319.9,-320,320,NAN,INFINITY,-INFINITY};
    const float yaws[]={0,-8,90,359.9f,NAN,-INFINITY,FLT_MAX}; unsigned rows=0;
    for (unsigned xi=0;xi<sizeof xs/sizeof *xs;xi++) for (unsigned yi=0;yi<sizeof yaws/sizeof *yaws;yi++)
    for (int dim=-1;dim<=1;dim++) for (int scale=0;scale<=4;scale+=4) for (int ti=0;ti<2;ti++) {
        MCGameplay game={0}; MCGameplayWorld *w=setup(&game,NULL); w->dimension=dim; w->worldTime=ti?INT64_MAX:0;
        MCGameplayPlayer *p=actor(&game,w,0,"A"); p->living.entity.posX=xs[xi]; p->living.entity.posZ=-xs[xi]; p->living.entity.rotationYaw=yaws[yi];
        ItemStack *s=held(p,(int32_t)(xi%3)-1,4,xi%2==0?0:38); mc_map_info *m=add_map(w,4,(uint8_t)scale);
        const uint16_t units[]={'k','e','y',0,0xd800}; NBTString *key=NBTString_fromUTF16(game.heap,units,5); CHECK(key);
        NBTTagCompound *entry=decorations(s,key,xs[xi],(double)yaws[yi]); CHECK(NBTTagCompound_setDouble_ascii(entry,"z",-xs[xi]));
        CHECK(NBTTagCompound_setFloat_ascii(entry,"type",-.5f)); NBTTagCompound *tag=s->stackTagCompound; bool changed=true;
        CHECK(ItemMap_onUpdate(s,w,(MCObject *)p,0,false,&changed) && !changed && s->stackTagCompound==tag);
        CHECK(MapData_getMapInfo(m,w,p)->update_counter==0);
        char seq[256]="",value[40],dirty[40]; size_t at=0;
        for (int i=0;i<8;i++) { packet_fact(m,s,w,p,value,sizeof(value)); int n=snprintf(seq+at,sizeof(seq)-at,"%s%s",i?",":"",value); CHECK(n>=0 && (size_t)n<sizeof(seq)-at); at+=(size_t)n; }
        mc_MapData_updateMapData(m,3,5); mc_MapData_updateMapData(m,7,8); packet_fact(m,s,w,p,dirty,sizeof(dirty));
        printf("%u %u %d %d %d %zu %s %s\n",xi,yi,dim,scale,ti,m->icon_count,seq,dirty); ++rows;
        CHECK(MCGameplay_free(&game));
    }
    fprintf(stderr,"native map refs: %u vectors, %u assertions\n",rows,checks);
}
static void nullable_profile_name(void) {
    MCGameplay game={0};MCGameplayWorld *w=setup(&game,NULL);
    MCGameplayPlayer *p=actor(&game,w,0,"NameFixture");
    NativeGameProfile *profile=NativeGameProfile_new(game.heap,p->living.entity.entityUniqueID,NULL);CHECK(profile);
    p->gameProfile=profile;MCObjectHeap_touch(game.heap);
    CHECK(EntityPlayer_getName(p)==NULL&&!MCObjectHeap_failed(game.heap));
    ItemStack *s=held(p,1,4,0);mc_map_info *m=add_map(w,4,0);
    CHECK(MapData_updateVisiblePlayers(m,w,p,s)&&m->icon_count==1);
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,0,NULL));
    CHECK(MapData_updateVisiblePlayers(m,w,p,s)&&m->icon_count==0);
    CHECK(!MCObjectHeap_failed(game.heap)&&MCGameplay_free(&game));
    w=setup(&game,NULL);p=actor(&game,w,0,"SkippedName");s=held(p,1,4,0);m=add_map(w,4,0);
    p->gameProfile=NULL;p->living.entity.dimension=w->dimension+1;MCObjectHeap_touch(game.heap);
    CHECK(MapData_updateVisiblePlayers(m,w,p,s)&&m->icon_count==0&&!MCObjectHeap_failed(game.heap));
    p->living.entity.dimension=w->dimension;p->living.entity.isDead=true;MCObjectHeap_touch(game.heap);
    CHECK(MapData_updateVisiblePlayers(m,w,p,s)&&m->icon_count==0&&!MCObjectHeap_failed(game.heap));
    CHECK(MCGameplay_free(&game));
    w=setup(&game,NULL);p=actor(&game,w,0,"BrokenProfile");s=held(p,1,4,0);m=add_map(w,4,0);
    p->gameProfile=NULL;MCObjectHeap_touch(game.heap);
    CHECK(!MapData_updateVisiblePlayers(m,w,p,s)&&MCObjectHeap_failed(game.heap));
    CHECK(m->icon_count==0&&!MCObjectHeap_hasBorrowers(game.heap));CHECK(MCGameplay_free(&game));
}
int main(int argc,char **argv) { if (argc==2 && !strcmp(argv[1],"--facts")) { facts();return 0; } nullable_profile_name(); lifecycle(); nbt_and_graph_identity(); branches_and_failures(); mutable_entity_keys(); packet_failure_rollback(); nullable_hash_key(); survey(); printf("source map refs: %u checks passed\n",checks); return 0; }
