#include "entity/player/EntityPlayerDrops.h"
#include "util/MathHelper.h"
#include "stats/StatBase.h"
#include "stats/StatFileWriter.h"
#include <inttypes.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks;if(!(x)){fprintf(stderr,"drop check %u line %d: %s\n",checks,__LINE__,#x);exit(1);} }while(0)
static ItemStack *watched(EntityItem *e) {return DataWatcher_getWatchableObjectItemStack(EntityItem_getDataWatcher(e),10);}
typedef struct {
    MCObject object;
    MCGameplayPlayer *player;
    EntityItem *last;
    StatBase *dropStat;
    uint64_t playerSeed,mathSeed;
    unsigned calls,failAt,randomCalls,mathCalls,marks,joins;
    bool spawnAccepted,omitWatcher;
} Fixture;
static void fixture_trace(MCObject *o,MCObjectVisitor v,void *c) {Fixture *f=(Fixture *)o;f->player=(MCGameplayPlayer *)v((MCObject *)f->player,c);f->last=(EntityItem *)v((MCObject *)f->last,c);f->dropStat=(StatBase *)v((MCObject *)f->dropStat,c);}
static const MCObjectClass fixtureClass={"test.sourceDrop.dependencies",MCObjectHeap_plainClone,fixture_trace,NULL};
static bool enter(Fixture *f) {CHECK(MCObjectHeap_hasBorrowers(f->object.heap));CHECK(!MCObjectHeap_collect(f->object.heap));++f->calls;MCObjectHeap_touch(f->object.heap);if(f->failAt==f->calls){MCObjectHeap_fail(f->object.heap);return false;}return true;}
static uint64_t seed(uint64_t n) {return (n^UINT64_C(25214903917))&((UINT64_C(1)<<48)-1);}
/* Independent deterministic Java Random dependency fixture. Mutable RNG state
   belongs to this managed context and follows transaction graph cloning. */
static uint32_t next_bits(uint64_t *state,unsigned n) {*state=(*state*UINT64_C(25214903917)+11)&((UINT64_C(1)<<48)-1);return (uint32_t)(*state>>(48-n));}
static float random_float(MCObject *o,MCGameplayPlayer *p) {Fixture *f=(Fixture *)o;CHECK(p==f->player);if(!enter(f))return 0;++f->randomCalls;return (float)next_bits(&f->playerSeed,24)/16777216.0f;}
static double math_random(MCObject *o) {Fixture *f=(Fixture *)o;if(!enter(f))return 0;++f->mathCalls;uint64_t a=next_bits(&f->mathSeed,26),b=next_bits(&f->mathSeed,27);return (double)((a<<27)+b)/9007199254740992.0;}
static bool mark(MCObject *o,MCObject *owner,int32_t n) {EntityItem *e=(EntityItem *)owner;Fixture *f=(Fixture *)o;if(!enter(f))return false;CHECK(e==f->last&&n==10);++f->marks;return true;}
static const DataWatcherDependencies watcherMethods={.onDataWatcherUpdate=mark};
static bool base(MCObject *o,EntityItem *e,MCObject *w) {Fixture *f=(Fixture *)o;if(!enter(f))return false;CHECK(e->health==0&&e->hoverStart==0&&EntityItem_getDataWatcher(e)==NULL&&e->worldObj==w);e->entityId=1;e->width=0.6f;e->height=1.8f;f->last=e;if(f->omitWatcher)return true;bool ok=EntityItem_nativeInitializeDataWatcher(e,&watcherMethods,o);CHECK(e->health==0&&e->hoverStart==0&&watched(e)==NULL);return ok;}
static bool size(MCObject *o,EntityItem *e,float width,float height) {Fixture *f=(Fixture *)o;if(!enter(f))return false;CHECK(e==f->last&&e->health==5);e->width=width;e->height=height;MCObjectHeap_touch(e->object.heap);return true;}
static bool position(MCObject *o,EntityItem *e,double x,double y,double z) {Fixture *f=(Fixture *)o;if(!enter(f))return false;CHECK(e==f->last&&e->width==0.25f&&e->height==0.25f);e->posX=x;e->posY=y;e->posZ=z;MCObjectHeap_touch(e->object.heap);return true;}
/* Uncalled pickup dependencies fail explicitly if the tested source path
   unexpectedly reaches them. There are no production fallback callbacks. */
static bool unexpected(MCObject *o) {MCObjectHeap_fail(o->heap);return false;}
static bool log_missing(MCObject *o,int32_t id) {(void)id;return unexpected(o);}
static bool remote(MCObject *o,MCObject *w) {(void)o;return MCGameplayWorld_isRemote(w);}
static InventoryPlayer *inventory(MCObject *o,MCObject *p) {(void)o;return MCGameplayPlayer_inventory(p);}
static const NBTString *entity_name(MCObject *o,MCObject *p) {(void)o;return ((MCGameplayPlayer *)p)->name;}
static MCObject *find(MCObject *o,MCObject *w,const NBTString *n) {(void)w;(void)n;unexpected(o);return NULL;}
static bool entity_achievement(MCObject *o,MCObject *p,EntityItemAchievement a) {(void)p;(void)a;return unexpected(o);}
static bool silent(MCObject *o,const EntityItem *e) {(void)e;return unexpected(o);}
static float entity_random(MCObject *o,EntityItem *e) {(void)e;unexpected(o);return 0;}
static bool sound(MCObject *o,MCObject *w,MCObject *p,const char *s,float volume,float pitch) {(void)w;(void)p;(void)s;(void)volume;(void)pitch;return unexpected(o);}
static bool pickup(MCObject *o,MCObject *p,EntityItem *e,int32_t n) {(void)p;(void)e;(void)n;return unexpected(o);}
static bool dead(MCObject *o,EntityItem *e) {(void)e;return unexpected(o);}
static const EntityItemDependencies entityDependencies={log_missing,remote,inventory,entity_name,find,entity_achievement,silent,entity_random,sound,pickup,dead};
static const EntityItemConstructorDependencies constructorDependencies={base,math_random,size,position};
static float eye(MCObject *o,MCGameplayPlayer *p) {Fixture *f=(Fixture *)o;CHECK(p==f->player);return enter(f)?1.62f:0;}
static NBTString *name(MCObject *o,MCGameplayPlayer *p) {Fixture *f=(Fixture *)o;CHECK(p==f->player);return enter(f)?p->name:NULL;}
static bool join(MCObject *o,MCGameplayPlayer *p,EntityItem *e) {
    Fixture *f=(Fixture *)o;if(!enter(f))return false;CHECK(p==f->player&&e==f->last&&e->delayBeforeCanPickup==40);++f->joins;
    if (f->spawnAccepted) {MCGameplayObjects *owners=p->worldObj->owners;CHECK(owners->itemCount<MC_GAMEPLAY_MAX_ITEMS);owners->items[owners->itemCount++]=(MCObject *)e;MCObjectHeap_touch(o->heap);}
    return true; /* Completion, including the source's ignored spawn rejection. */
}
static bool stat(MCObject *o,MCGameplayPlayer *p) {Fixture *f=(Fixture *)o;if(!enter(f))return false;CHECK(p==f->player&&f->joins==1);return StatFileWriter_increaseStat(p->stats,(MCObject *)p,f->dropStat,1);}
static double math_sin(MCObject *o,double v) {return enter((Fixture *)o)?sin(v):0;}
static double math_cos(MCObject *o,double v) {return enter((Fixture *)o)?cos(v):0;}
static const EntityPlayerDropsDependencies dropDependencies={&entityDependencies,&constructorDependencies,eye,random_float,name,join,stat,math_sin,math_cos};
static ItemStack *match(InventoryCrafting *g,MCObject *w) {return CraftingManager_findMatchingRecipe(((MCGameplayWorld *)w)->manager,g,w,NULL,NULL);}
static ItemStackArray *left(InventoryCrafting *g,MCObject *w) {return CraftingManager_func_180303_b(((MCGameplayWorld *)w)->manager,g,w);}
static bool crafted(ItemStack *s,MCObject *w,MCObject *p,int32_t n) {(void)s;(void)w;(void)n;return unexpected(p);}
static bool achievement(MCObject *p,mc_crafting_achievement a) {(void)a;return unexpected(p);}
static bool drop(MCObject *p,ItemStack *s,bool around) {(void)s;(void)around;return unexpected(p);}
static bool pickaxe(const Item *i) {int n=ItemStack_registryId(i);return n==257||n==270||n==274||n==278||n==285;}
static bool hoe(const Item *i) {int n=ItemStack_registryId(i);return n>=290&&n<=294;}
static bool sword(const Item *i) {int n=ItemStack_registryId(i);return n==267||n==268||n==272||n==276||n==283;}
static bool wood(const Item *i) {return ItemStack_registryId(i)==270;}
static int32_t armor(const Item *i) {int n=ItemStack_registryId(i);return n>=298&&n<=317?(n-298)%4:-1;}
static const mc_crafting_dispatch craftingDependencies={MCGameplayPlayer_inventory,MCGameplayPlayer_world,match,left,crafted,achievement,drop,pickaxe,hoe,sword,wood,armor,MCGameplayWorld_isRemote,MCGameplayWorld_isCraftingTable,MCGameplayPlayer_getDistanceSq};
static Fixture *setup(MCGameplay *game,uint64_t randomSeed) {
    CHECK(MCGameplay_init(game,4*1024*1024));MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,game->heap));
    CraftingManager *manager=CraftingManager_newEmpty(game->heap);MCGameplayWorld *world=MCGameplayWorld_new(game->heap,MCGameplay_get(game),NULL,manager);CHECK(manager&&world&&MCGameplay_setWorld(game,(MCObject *)world));
    StatFileWriter *stats=StatFileWriter_new(game->heap);NBTString *playerName=NBTString_fromASCII(game->heap,"sourceDrop");CHECK(stats&&playerName);
    MCGameplayPlayer *p=MCGameplayPlayer_new(world,playerName,stats,&craftingDependencies);CHECK(p&&MCGameplay_setPlayer(game,0,"11111111-1111-1111-1111-111111111111",(MCObject *)p));
    Fixture *f=(Fixture *)MCObjectHeap_alloc(game->heap,sizeof(*f),&fixtureClass);CHECK(f);f->player=p;f->playerSeed=seed(randomSeed);f->mathSeed=seed(randomSeed+1000);f->spawnAccepted=true;
    f->dropStat=StatBase_newIdentity(game->heap,NBTString_fromASCII(game->heap,"stat.drop"),STAT_BASE_KIND_BASE);CHECK(f->dropStat);p->effects=(MCObject *)f;p->posX=1.25;p->posY=64.5;p->posZ=-9.75;MCObjectHeap_touch(game->heap);MCObjectRootScope_end(&scope);return f;
}
static uint32_t float_bits(float v) {uint32_t out;memcpy(&out,&v,sizeof out);return out;}
static uint64_t double_bits(double v) {uint64_t out;memcpy(&out,&v,sizeof out);return out;}
static void constructors(void) {
    for(int type=0;type<3;type++) {
        MCGameplay game={0};Fixture *f=setup(&game,7);ItemStack *s=ItemStack_new(game.heap,ItemStack_registryItem(387),-2,9);CHECK(s);
        EntityItem *e=type==0?EntityItem_new_world(game.heap,NULL,(MCObject *)f,&entityDependencies,&constructorDependencies):type==1?EntityItem_new_position(game.heap,NULL,(MCObject *)f,&entityDependencies,&constructorDependencies,1,2,3):EntityItem_new_stack(game.heap,NULL,(MCObject *)f,&entityDependencies,&constructorDependencies,1,2,3,s);
        CHECK(e&&e->health==5&&e->width==0.25f&&e->height==0.25f&&e->delayBeforeCanPickup==0&&e->dependencyContext==(MCObject *)f);
        CHECK(f->mathCalls==(type?4u:1u)&&f->marks==(type==1?0u:1u));
        if(type==0){CHECK(watched(e)&&watched(e)->item==NULL&&watched(e)->stackSize==0&&e->motionY==0&&e->rotationYaw==0);}
        else {CHECK(e->posX==1&&e->posY==2&&e->posZ==3&&e->motionY==0.20000000298023224);CHECK(type==1?watched(e)==NULL:watched(e)==s);}
        CHECK(!MCObjectHeap_failed(game.heap)&&!MCObjectHeap_hasBorrowers(game.heap));CHECK(MCGameplay_free(&game));
    }
}
static void source_edges(void) {
    for(unsigned type=0;type<3;type++) {
        MCGameplay missing={0};Fixture *f=setup(&missing,1);f->omitWatcher=true;
        ItemStack *s=ItemStack_new(missing.heap,ItemStack_registryItem(1),1,0);CHECK(s);
        EntityItem *e=type==0?EntityItem_new_world(missing.heap,NULL,(MCObject *)f,&entityDependencies,&constructorDependencies):type==1?EntityItem_new_position(missing.heap,NULL,(MCObject *)f,&entityDependencies,&constructorDependencies,1,2,3):EntityItem_new_stack(missing.heap,NULL,(MCObject *)f,&entityDependencies,&constructorDependencies,1,2,3,s);
        CHECK(!e&&MCObjectHeap_failed(missing.heap)&&f->calls==1&&f->mathCalls==0&&f->marks==0);
        CHECK(f->last&&f->last->health==0&&f->last->hoverStart==0&&EntityItem_getDataWatcher(f->last)==NULL);
        CHECK(MCGameplay_free(&missing));
    }
    MCGameplay game={0};Fixture *f=setup(&game,1);MCGameplayPlayer *p=f->player;
    CHECK(!EntityPlayer_dropItem(p,NULL,true,true,NULL,NULL)&&f->calls==0&&!MCObjectHeap_failed(game.heap));ItemStack *zero=ItemStack_new(game.heap,NULL,0,0);CHECK(zero);CHECK(!EntityPlayer_dropItem(p,zero,true,true,NULL,NULL)&&f->calls==0&&!MCObjectHeap_failed(game.heap));
    ItemStack *s=ItemStack_new(game.heap,ItemStack_registryItem(1),-1,9);CHECK(s);CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,0,s));f->spawnAccepted=false;
    EntityItem *e=EntityPlayer_dropItem(p,s,true,true,&dropDependencies,(MCObject *)f);CHECK(e&&watched(e)==s&&e->delayBeforeCanPickup==40&&e->thrower==p->name&&f->randomCalls==2&&f->mathCalls==4&&f->joins==1);CHECK(StatFileWriter_readStat(p->stats,f->dropStat)==1&&p->worldObj->owners->itemCount==0&&s->stackSize==-1);CHECK(InventoryPlayer_getStackInSlot(p->inventory,0)==s);CHECK(MCGameplay_free(&game));
    f=setup(&game,1);p=f->player;s=ItemStack_new(game.heap,ItemStack_registryItem(1),1,0);CHECK(s);e=EntityPlayer_dropPlayerItemWithRandomChoice(p,s,true,&dropDependencies,(MCObject *)f);CHECK(e&&e->thrower==NULL&&f->randomCalls==4&&f->joins==1&&StatFileWriter_readStat(p->stats,f->dropStat)==0&&p->worldObj->owners->itemCount==1);CHECK(MCGameplay_free(&game));
    for(unsigned fail=1;fail<=18;fail++) {
        f=setup(&game,9);f->failAt=fail;s=ItemStack_new(game.heap,ItemStack_registryItem(1),1,0);CHECK(s);CHECK(!EntityPlayer_dropItem(f->player,s,false,true,&dropDependencies,(MCObject *)f));CHECK(f->calls==fail&&MCObjectHeap_failed(game.heap)&&!MCObjectHeap_hasBorrowers(game.heap));CHECK(MCGameplay_free(&game));
    }
    f=setup(&game,1);MCObjectHeap *foreign=MCObjectHeap_new(1024*1024);CHECK(foreign);s=ItemStack_new(foreign,ItemStack_registryItem(1),1,0);CHECK(s&&!EntityPlayer_dropItem(f->player,s,false,false,&dropDependencies,(MCObject *)f)&&MCObjectHeap_failed(game.heap)&&f->calls==0);MCObjectHeap_free(foreign);CHECK(MCGameplay_free(&game));
}
static void aliases(void) {
    MCGameplay game={0};Fixture *f=setup(&game,1);ItemStack *s=ItemStack_new(game.heap,ItemStack_registryItem(387),2,9);CHECK(s&&InventoryPlayer_setInventorySlotContents(f->player->inventory,0,s));EntityItem *e=EntityPlayer_dropItem(f->player,s,false,true,&dropDependencies,(MCObject *)f);CHECK(e&&watched(e)==s);CHECK(MCObjectHeap_collect(game.heap));
    MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&game,&tx));MCGameplayObjects *owners=MCGameplay_get(&tx.working);MCGameplayPlayer *p=(MCGameplayPlayer *)owners->players[0];Fixture *copy=(Fixture *)p->effects;EntityItem *ce=(EntityItem *)owners->items[0];CHECK(copy!=f&&copy->player==p&&copy->last==ce&&ce!=e&&ce->dependencyContext==(MCObject *)copy&&watched(ce)==InventoryPlayer_getStackInSlot(p->inventory,0)&&watched(ce)!=s&&ce->thrower==p->name);CHECK(double_bits(ce->motionX)==double_bits(e->motionX)&&float_bits(ce->hoverStart)==float_bits(e->hoverStart));CHECK(copy->playerSeed==f->playerSeed&&copy->mathSeed==f->mathSeed);watched(ce)->stackSize=0;CHECK(s->stackSize==2);CHECK(MCGameplay_abort(&tx)&&MCGameplay_free(&game));
}
static void math_edges(void) {
    CHECK(float_bits(MathHelper_sin(0))==0&&MathHelper_cos(0)==1);CHECK(float_bits(MathHelper_sin(-0.0f))==0);CHECK(float_bits(MathHelper_sin(NAN))==0&&float_bits(MathHelper_cos(NAN))==0);CHECK(float_bits(MathHelper_sin(-INFINITY))==0&&float_bits(MathHelper_cos(-INFINITY))==0);CHECK(float_bits(MathHelper_sin(INFINITY))==float_bits(MathHelper_cos(INFINITY)));CHECK(MathHelper_sin(INFINITY)<0);CHECK(float_bits(MathHelper_sin(FLT_MAX))==float_bits(MathHelper_sin(INFINITY)));
}
static void differential(bool emit) {
    const float yaw[]={0,90,-90,179.999f,180,360,-450,12345.75f,1.0e30f,NAN,INFINITY};
    const float pitch[]={0,45,-90,89.999f,1.0e30f,NAN};
    for(int seedIndex=0;seedIndex<8;seedIndex++)for(int yi=0;yi<11;yi++)for(int pi=0;pi<6;pi++)for(int around=0;around<2;around++)for(int trace=0;trace<2;trace++) {
        MCGameplay game={0};Fixture *f=setup(&game,(uint64_t)seedIndex);f->player->rotationYaw=yaw[yi];f->player->rotationPitch=pitch[pi];ItemStack *s=ItemStack_new(game.heap,ItemStack_registryItem(387),seedIndex%3-1,9);CHECK(s);
        EntityItem *e=EntityPlayer_dropItem(f->player,s,around!=0,trace!=0,&dropDependencies,(MCObject *)f);
        if(s->stackSize==0)CHECK(!e&&f->calls==0);else{CHECK(e&&watched(e)==s&&e->thrower==(trace?f->player->name:NULL)&&e->delayBeforeCanPickup==40&&f->marks==1&&f->randomCalls==(around?2u:4u)&&f->mathCalls==4);CHECK(StatFileWriter_readStat(f->player->stats,f->dropStat)==trace);}
        CHECK(!MCObjectHeap_failed(game.heap)&&!MCObjectHeap_hasBorrowers(game.heap));
        if(emit)printf("%d %d %d %d %d %d %08" PRIx32 " %08" PRIx32 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %u %u\n",seedIndex,yi,pi,around,trace,e?1:0,e?float_bits(e->hoverStart):0,e?float_bits(e->rotationYaw):0,e?double_bits(e->posY):0,e?double_bits(e->motionX):0,e?double_bits(e->motionY):0,e?double_bits(e->motionZ):0,f->randomCalls,f->mathCalls);
        CHECK(MCGameplay_free(&game));
    }
}
int main(int argc,char **argv) {bool emit=argc==2&&strcmp(argv[1],"--facts")==0;if(!emit){constructors();source_edges();aliases();math_edges();}differential(emit);if(!emit)printf("source player drops: %u checks\n",checks);return 0;}
