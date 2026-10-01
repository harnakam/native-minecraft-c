#include "item/ItemEmptyMap.h"
#include "item/ItemMapData.h"
#include "entity/player/EntityPlayerDrops.h"
#include "entity/player/EntityPlayerMPStats.h"
#include "stats/StatFileWriter.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) {fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} } while(0)
typedef struct {
    MCObject object;
    MCGameplayPlayer *player;
    ItemStack *input,*dropped;
    EntityItem *entity;
    StatBase *stat;
    unsigned callbacks,failAt,lookups,triggers,drops;
    bool nullStat,nullDrop,spawnRejected;
} Fixture;
static void trace(MCObject *o,MCObjectVisitor v,void *c) {
    Fixture *f=(Fixture *)o;f->player=(MCGameplayPlayer *)v((MCObject *)f->player,c);
    f->input=(ItemStack *)v((MCObject *)f->input,c);f->dropped=(ItemStack *)v((MCObject *)f->dropped,c);
    f->entity=(EntityItem *)v((MCObject *)f->entity,c);f->stat=(StatBase *)v((MCObject *)f->stat,c);
}
static const MCObjectClass fixtureClass={"EmptyMapEffects",MCObjectHeap_plainClone,trace,NULL};
static const MCObjectClass fillerClass={"EmptyMapAllocationBoundary",MCObjectHeap_plainClone,NULL,NULL};
static bool enter(Fixture *f) {
    CHECK(MCObjectHeap_hasBorrowers(f->object.heap));CHECK(!MCObjectHeap_collect(f->object.heap));
    ++f->callbacks;MCObjectHeap_touch(f->object.heap);
    if(f->failAt==f->callbacks) {MCObjectHeap_fail(f->object.heap);return false;}return true;
}
static bool unexpected(MCObject *o) {MCObjectHeap_fail(o->heap);return false;}
static ItemStack *match(InventoryCrafting *g,MCObject *w) {return CraftingManager_findMatchingRecipe(((MCGameplayWorld *)w)->manager,g,w,NULL,NULL);}
static ItemStackArray *remaining(InventoryCrafting *g,MCObject *w) {return CraftingManager_func_180303_b(((MCGameplayWorld *)w)->manager,g,w);}
static bool crafted(ItemStack *s,MCObject *w,MCObject *p,int32_t n) {(void)s;(void)w;(void)n;return unexpected(p);}
static bool achievement(MCObject *p,mc_crafting_achievement a) {(void)a;return unexpected(p);}
static bool crafting_drop(MCObject *p,ItemStack *s,bool b) {(void)s;(void)b;return unexpected(p);}
static bool no_item(const Item *i) {(void)i;return false;}
static int32_t armor(const Item *i) {(void)i;return -1;}
static const mc_crafting_dispatch crafting={MCGameplayPlayer_inventory,MCGameplayPlayer_world,match,remaining,crafted,achievement,crafting_drop,no_item,no_item,no_item,no_item,armor,MCGameplayWorld_isRemote,MCGameplayWorld_isCraftingTable,MCGameplayPlayer_getDistanceSq};
static bool log_missing(MCObject *o,int32_t id) {(void)id;return unexpected(o);}
static bool is_remote(MCObject *o,MCObject *w) {(void)o;return MCGameplayWorld_isRemote(w);}
static InventoryPlayer *inventory(MCObject *o,MCObject *p) {(void)o;return MCGameplayPlayer_inventory(p);}
static const NBTString *player_name(MCObject *o,MCObject *p) {(void)o;return ((MCGameplayPlayer *)p)->name;}
static MCObject *find(MCObject *o,MCObject *w,const NBTString *n) {(void)w;(void)n;unexpected(o);return NULL;}
static bool entity_achievement(MCObject *o,MCObject *p,EntityItemAchievement a) {(void)p;(void)a;return unexpected(o);}
static bool silent(MCObject *o,const EntityItem *e) {(void)e;return unexpected(o);}
static float entity_random(MCObject *o,EntityItem *e) {(void)e;unexpected(o);return 0;}
static bool sound(MCObject *o,MCObject *w,MCObject *p,const char *s,float v,float pitch) {(void)w;(void)p;(void)s;(void)v;(void)pitch;return unexpected(o);}
static bool pickup(MCObject *o,MCObject *p,EntityItem *e,int32_t n) {(void)p;(void)e;(void)n;return unexpected(o);}
static bool dead(MCObject *o,EntityItem *e) {(void)e;return unexpected(o);}
static const EntityItemDependencies entityDeps={log_missing,is_remote,inventory,player_name,find,entity_achievement,silent,entity_random,sound,pickup,dead};
static bool base(MCObject *o,EntityItem *e,MCObject *w) {Fixture *f=(Fixture *)o;CHECK(e->health==0&&e->worldObj==w);e->entityId=7;f->entity=e;MCObjectHeap_touch(o->heap);return EntityItem_nativeInitializeDataWatcher(e,NULL,NULL);}
static double math_random(MCObject *o) {CHECK(MCObjectHeap_hasBorrowers(o->heap));return 0.25;}
static bool size(MCObject *o,EntityItem *e,float w,float h) {e->width=w;e->height=h;MCObjectHeap_touch(o->heap);return true;}
static bool position(MCObject *o,EntityItem *e,double x,double y,double z) {e->posX=x;e->posY=y;e->posZ=z;MCObjectHeap_touch(o->heap);return true;}
static const EntityItemConstructorDependencies constructors={base,math_random,size,position};
static float eye(MCObject *o,MCGameplayPlayer *p) {(void)p;CHECK(MCObjectHeap_hasBorrowers(o->heap));return 1.62f;}
static float random_float(MCObject *o,MCGameplayPlayer *p) {(void)p;CHECK(MCObjectHeap_hasBorrowers(o->heap));return 0.25f;}
static NBTString *name(MCObject *o,MCGameplayPlayer *p) {(void)o;return p->name;}
static bool join(MCObject *o,MCGameplayPlayer *p,EntityItem *e) {
    Fixture *f=(Fixture *)o;CHECK(p==f->player&&EntityItem_getEntityItem(e)==f->dropped&&e->delayBeforeCanPickup==40);
    if(!f->spawnRejected) {MCGameplayObjects *owners=p->worldObj->owners;CHECK(owners->itemCount<MC_GAMEPLAY_MAX_ITEMS);owners->items[owners->itemCount++]=(MCObject *)e;}
    MCObjectHeap_touch(o->heap);return true;
}
static bool drop_stat(MCObject *o,MCGameplayPlayer *p) {(void)p;return unexpected(o);}
static double math_sin(MCObject *o,double n) {(void)o;return sin(n);}
static double math_cos(MCObject *o,double n) {(void)o;return cos(n);}
static const EntityPlayerDropsDependencies dropDeps={&entityDeps,&constructors,eye,random_float,name,join,drop_stat,math_sin,math_cos};
static StatBase *lookup(MCObject *o,const Item *item) {
    Fixture *f=(Fixture *)o;CHECK(item==ItemStack_registryItem(395));if(!enter(f))return NULL;
    ++f->lookups;CHECK(f->player->worldObj->maps.count==1&&f->input->stackSize>0);
    mc_map_info *map=&f->player->worldObj->maps.entries[0];CHECK(map->dirty&&map->metadata_known&&map->scale==0);
    return f->nullStat?NULL:f->stat;
}
static bool trigger(MCObject *o,MCGameplayPlayer *p,StatBase *stat) {
    Fixture *f=(Fixture *)o;CHECK(p==f->player&&stat==(f->nullStat?NULL:f->stat));if(!enter(f))return false;
    ++f->triggers;return stat?StatFileWriter_increaseStat(p->stats,(MCObject *)p,stat,1):EntityPlayerMP_addStat(p,NULL,1,NULL,NULL);
}
static bool drop(MCObject *o,MCGameplayPlayer *p,ItemStack *s,bool b,EntityItem **out) {
    Fixture *f=(Fixture *)o;CHECK(p==f->player&&!b&&s->item==ItemStack_registryItem(358)&&s->stackSize==1&&s!=f->input);
    if(!enter(f))return false;
    ++f->drops;f->dropped=s;MCObjectHeap_touch(o->heap);
    *out=f->nullDrop?NULL:EntityPlayer_dropPlayerItemWithRandomChoice(p,s,b,&dropDeps,o);return !MCObjectHeap_failed(o->heap);
}
static const ItemEmptyMapDependencies deps={lookup,trigger,drop};
static MCObject *foreignResult;
static StatBase *bad_stat(MCObject *o,const Item *item) {(void)item;return foreignResult?(StatBase *)foreignResult:(StatBase *)((Fixture *)o)->player;}
static bool bad_drop(MCObject *o,MCGameplayPlayer *p,ItemStack *s,bool b,EntityItem **out) {(void)p;(void)s;(void)b;*out=foreignResult?(EntityItem *)foreignResult:(EntityItem *)o;return true;}
static Fixture *setup(MCGameplay *g,int32_t count,bool full,bool creative,bool remote,int32_t next,double x,double z,int32_t dimension) {
    CHECK(MCGameplay_init(g,4*1024*1024));MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,g->heap));
    CraftingManager *manager=CraftingManager_newEmpty(g->heap);MCGameplayWorld *world=MCGameplayWorld_new(g->heap,MCGameplay_get(g),NULL,manager);CHECK(world&&MCGameplay_setWorld(g,(MCObject *)world));
    StatFileWriter *stats=StatFileWriter_new(g->heap);NBTString *n=NBTString_fromASCII(g->heap,"emptyMap");
    MCGameplayPlayer *p=MCGameplayPlayer_new(world,n,stats,&crafting);CHECK(p&&MCGameplay_setPlayer(g,0,"11111111-1111-1111-1111-111111111111",(MCObject *)p));
    Fixture *f=(Fixture *)MCObjectHeap_alloc(g->heap,sizeof(*f),&fixtureClass);CHECK(f);f->player=p;p->effects=(MCObject *)f;
    f->stat=StatBase_newIdentity(g->heap,NBTString_fromASCII(g->heap,"stat.useItem.minecraft.map"),STAT_BASE_KIND_BASE);CHECK(f->stat);
    p->creative=creative;world->remote=remote;world->dimension=dimension;world->maps.next_id=next;p->posX=x;p->posZ=z;p->posY=64;
    if(full)for(int i=1;i<36;i++)CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,i,ItemStack_new(g->heap,ItemStack_registryItem(1),64,0)));
    f->input=ItemStack_new(g->heap,ItemStack_registryItem(395),count,0);CHECK(f->input&&InventoryPlayer_setInventorySlotContents(p->inventory,0,f->input));
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,38,f->input)); /* Cross-owner source alias. */
    MCObjectHeap_touch(g->heap);MCObjectRootScope_end(&scope);return f;
}
static int32_t decr(int32_t n) {uint32_t u=(uint32_t)n-1;int32_t r;memcpy(&r,&u,sizeof(r));return r;}
static void cases(bool facts) {
    const int32_t counts[]={1,2,0,-1,INT32_MIN,INT32_MAX};const int32_t ids[]={0,32767,32768,65535};
    const double positions[]={0,-64.5,63.999,1.0e100,NAN,-INFINITY};
    for(unsigned ci=0;ci<6;ci++)for(int full=0;full<2;full++)for(int creative=0;creative<2;creative++)for(int remote=0;remote<2;remote++)for(unsigned ii=0;ii<4;ii++)for(unsigned pi=0;pi<6;pi++) {
        MCGameplay g={0};Fixture *f=setup(&g,counts[ci],full!=0,creative!=0,remote!=0,ids[ii],positions[pi],-positions[pi],257);
        ItemStack *out=ItemEmptyMap_onItemRightClick(ItemStack_registryItem(395),f->input,f->player->worldObj,f->player,&deps,(MCObject *)f);
        MCGameplayWorld *w=f->player->worldObj;int32_t remainingCount=decr(counts[ci]);int32_t mapId=ids[ii]<32768?ids[ii]:0;
        CHECK(out&&!MCObjectHeap_failed(g.heap)&&!MCObjectHeap_hasBorrowers(g.heap));CHECK(f->input->stackSize==remainingCount&&InventoryPlayer_getStackInSlot(f->player->inventory,38)==f->input);
        CHECK(w->maps.count==1&&w->maps.next_id==((ids[ii]+1)&65535));mc_map_info *map=&w->maps.entries[0];CHECK(map->id==mapId&&map->scale==0&&map->dimension==1&&map->dirty&&map->metadata_known);
        int inserted=0;ItemStack *stored=NULL;for(int i=1;i<36;i++){ItemStack *s=InventoryPlayer_getStackInSlot(f->player->inventory,i);if(s&&s->item==ItemStack_registryItem(358)){++inserted;stored=s;}}
        if(remainingCount<=0) {CHECK(out!=f->input&&out->stackSize==1&&out->itemDamage==mapId&&f->callbacks==0&&inserted==0&&w->owners->itemCount==0);}
        else {CHECK(out==f->input&&f->lookups==1&&f->triggers==1&&StatFileWriter_readStat(f->player->stats,f->stat)==1);CHECK(f->drops==(unsigned)(full&&!creative));CHECK(inserted==!full);if(stored)CHECK(stored->stackSize==1&&stored->itemDamage==mapId&&stored!=out);if(f->entity)CHECK(EntityItem_getEntityItem(f->entity)==f->dropped&&f->entity->thrower==NULL&&w->owners->itemCount==1);}
        if(facts)printf("%u %d %d %d %u %u %d %d %d %d %d %d %d %u %u\n",ci,full,creative,remote,ii,pi,remainingCount,out==f->input,mapId,map->center_x,map->center_z,(int)map->dimension,inserted,f->drops,f->triggers);
        CHECK(MCGameplay_free(&g));
    }
}
static void failures(void) {
    for(unsigned at=1;at<=3;at++) {
        MCGameplay g={0};Fixture *f=setup(&g,2,true,false,false,0,0,0,0);MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&g,&tx));
        MCGameplayPlayer *p=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];Fixture *c=(Fixture *)p->effects;c->failAt=at;
        CHECK(c!=f&&c->input==InventoryPlayer_getStackInSlot(p->inventory,0)&&c->input==InventoryPlayer_getStackInSlot(p->inventory,38));
        CHECK(!ItemEmptyMap_onItemRightClick(ItemStack_registryItem(395),c->input,p->worldObj,p,&deps,(MCObject *)c));CHECK(MCObjectHeap_failed(tx.working.heap)&&c->callbacks==at&&!MCObjectHeap_hasBorrowers(tx.working.heap));
        CHECK(f->input->stackSize==2&&f->player->worldObj->maps.count==0&&f->player->worldObj->maps.next_id==0&&StatFileWriter_readStat(f->player->stats,f->stat)==0);
        CHECK(MCGameplay_abort(&tx)&&MCGameplay_free(&g));
    }
    for(int which=0;which<3;which++) {
        MCGameplay g={0};Fixture *f=setup(&g,2,which==2,false,false,0,0,0,0);ItemEmptyMapDependencies d=deps;
        if(which==0)d.objectUseStat=NULL;else if(which==1)d.triggerAchievement=NULL;else d.dropPlayerItemWithRandomChoice=NULL;
        CHECK(!ItemEmptyMap_onItemRightClick(ItemStack_registryItem(395),f->input,f->player->worldObj,f->player,&d,(MCObject *)f));CHECK(MCObjectHeap_failed(g.heap)&&f->input->stackSize==1&&f->player->worldObj->maps.count==1);CHECK(MCGameplay_free(&g));
    }
    MCGameplay g={0};Fixture *f=setup(&g,1,false,false,true,0,0,0,0);CHECK(ItemEmptyMap_onItemRightClick(ItemStack_registryItem(395),f->input,f->player->worldObj,f->player,NULL,NULL)&&!MCObjectHeap_failed(g.heap));CHECK(MCGameplay_free(&g));
    f=setup(&g,2,true,false,false,0,0,0,0);f->nullDrop=true;f->nullStat=true;CHECK(ItemEmptyMap_onItemRightClick(ItemStack_registryItem(395),f->input,f->player->worldObj,f->player,&deps,(MCObject *)f)==f->input);CHECK(f->drops==1&&f->triggers==1&&f->player->worldObj->owners->itemCount==0&&!MCObjectHeap_failed(g.heap));CHECK(MCGameplay_free(&g));
    f=setup(&g,2,true,false,false,0,0,0,0);f->spawnRejected=true;CHECK(ItemEmptyMap_onItemRightClick(ItemStack_registryItem(395),f->input,f->player->worldObj,f->player,&deps,(MCObject *)f)==f->input);CHECK(f->entity&&EntityItem_getEntityItem(f->entity)==f->dropped&&f->triggers==1&&f->player->worldObj->owners->itemCount==0&&!MCObjectHeap_failed(g.heap));CHECK(MCGameplay_free(&g));
    for(int foreign=0;foreign<2;foreign++)for(int kind=0;kind<2;kind++) {
        f=setup(&g,2,true,false,false,0,0,0,0);MCObjectHeap *other=MCObjectHeap_new(1024*1024);CHECK(other);
        foreignResult=foreign?(kind?(MCObject *)EntityItem_nativeNew(other,NULL,NULL,&entityDeps):(MCObject *)StatBase_newIdentity(other,NBTString_fromASCII(other,"stat.foreign"),STAT_BASE_KIND_BASE)):NULL;
        ItemEmptyMapDependencies d=deps;if(kind)d.dropPlayerItemWithRandomChoice=bad_drop;else d.objectUseStat=bad_stat;
        CHECK(!ItemEmptyMap_onItemRightClick(ItemStack_registryItem(395),f->input,f->player->worldObj,f->player,&d,(MCObject *)f));CHECK(MCObjectHeap_failed(g.heap)&&f->triggers==0&&!MCObjectHeap_hasBorrowers(g.heap));CHECK(MCGameplay_free(&g));MCObjectHeap_free(other);foreignResult=NULL;
    }
    f=setup(&g,2,false,false,false,0,0,0,0);f->player->inventory=NULL;CHECK(!ItemEmptyMap_onItemRightClick(ItemStack_registryItem(395),f->input,f->player->worldObj,f->player,&deps,(MCObject *)f)&&MCObjectHeap_failed(g.heap));CHECK(f->input->stackSize==1&&f->player->worldObj->maps.count==1&&f->callbacks==0);CHECK(MCGameplay_free(&g));
}
static void maps_and_aliases(void) {
    MCGameplay g={0};Fixture *f=setup(&g,1,false,false,false,32768,1024,-2048,-129);MCGameplayWorld *w=f->player->worldObj;
    mc_map_info old={0};old.id=0;old.scale=3;old.center_x=128;old.colors[0]=42;old.metadata_known=true;CHECK(ItemMapData_nativeSetItemData(w,&old));
    ItemStack *out=ItemEmptyMap_onItemRightClick(ItemStack_registryItem(395),f->input,w,f->player,NULL,NULL);CHECK(out&&out->itemDamage==0&&w->maps.count==1&&w->maps.next_id==32769);CHECK(w->maps.entries[0].scale==0&&w->maps.entries[0].center_x==1024&&w->maps.entries[0].center_z==-2048&&w->maps.entries[0].dimension==127&&w->maps.entries[0].colors[0]==0);CHECK(MCGameplay_free(&g));
    f=setup(&g,2,false,false,false,96,0,0,0);w=f->player->worldObj;for(int i=0;i<96;i++){old.id=i;CHECK(ItemMapData_nativeSetItemData(w,&old));}
    MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&g,&tx));MCGameplayPlayer *p=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];Fixture *c=(Fixture *)p->effects;
    CHECK(!ItemEmptyMap_onItemRightClick(ItemStack_registryItem(395),c->input,p->worldObj,p,&deps,(MCObject *)c)&&MCObjectHeap_failed(tx.working.heap));CHECK(c->input->stackSize==2&&p->worldObj->maps.next_id==97&&p->worldObj->maps.count==96);CHECK(f->input->stackSize==2&&w->maps.next_id==96&&w->maps.count==96);CHECK(MCGameplay_abort(&tx)&&MCGameplay_free(&g));
    f=setup(&g,2,true,false,false,0,0,0,0);w=f->player->worldObj;CHECK(MCGameplay_begin(&g,&tx));p=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];c=(Fixture *)p->effects;
    CHECK(ItemEmptyMap_onItemRightClick(ItemStack_registryItem(395),c->input,p->worldObj,p,&deps,(MCObject *)c)==c->input);CHECK(c->entity&&EntityItem_getEntityItem(c->entity)==c->dropped&&MCGameplay_get(&tx.working)->items[0]==(MCObject *)c->entity);
    CHECK(InventoryPlayer_getStackInSlot(p->inventory,0)==InventoryPlayer_getStackInSlot(p->inventory,38)&&c->input!=f->input&&f->input->stackSize==2&&w!=p->worldObj);CHECK(f->player->worldObj->maps.count==0&&f->player->worldObj->owners->itemCount==0&&StatFileWriter_readStat(f->player->stats,f->stat)==0);
    CHECK(MCGameplay_abort(&tx));CHECK(MCObjectHeap_collect(g.heap)&&InventoryPlayer_getStackInSlot(f->player->inventory,0)==f->input&&f->input->stackSize==2);CHECK(MCGameplay_free(&g));
    /* Exhaust the tracked native heap at each original stack allocation point.
       Native map buffers have a separate bounded-store dependency. */
    for(int allocation=0;allocation<3;allocation++) {
        f=setup(&g,2,false,false,false,0,0,0,0);CHECK(MCGameplay_begin(&g,&tx));p=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];c=(Fixture *)p->effects;
        size_t used=MCObjectHeap_liveBytes(tx.working.heap),spare=(size_t)allocation*sizeof(ItemStack);CHECK(used+spare<4u*1024u*1024u);
        CHECK(MCObjectHeap_alloc(tx.working.heap,4u*1024u*1024u-used-spare,&fillerClass));
        CHECK(!ItemEmptyMap_onItemRightClick(ItemStack_registryItem(395),c->input,p->worldObj,p,&deps,(MCObject *)c)&&MCObjectHeap_failed(tx.working.heap));
        CHECK(p->worldObj->maps.next_id==1&&p->worldObj->maps.count==(size_t)(allocation!=0)&&c->input->stackSize==(allocation?1:2)&&c->callbacks==0);
        CHECK(f->input->stackSize==2&&f->player->worldObj->maps.count==0&&f->player->worldObj->maps.next_id==0&&StatFileWriter_readStat(f->player->stats,f->stat)==0);CHECK(MCGameplay_abort(&tx)&&MCGameplay_free(&g));
    }
}
int main(int argc,char **argv) {bool facts=argc==2&&!strcmp(argv[1],"--facts");cases(facts);if(!facts){failures();maps_and_aliases();}if(!facts)printf("source empty map: %u checks passed\n",checks);return 0;}
