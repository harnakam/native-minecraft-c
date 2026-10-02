#include "entity/DataWatcher.h"
/* Reuse the recording fixture's actual translated Entity/Living/Player calls;
   no production superclass, world or effect is replaced by a success stub. */
#define main source_parent_constructor_fixture_main
#include "test_source_player_constructor.c"
#undef main
#include "entity/player/EntityPlayerMP.h"
#include "util/NativeReferenceList.h"
#include <math.h>

enum {MP_LIST=1,MP_CLOCK,MP_SPAWN,MP_PROVIDER,MP_SKY,MP_INFO,MP_MODE,MP_PROTECTION,
    MP_BORDER,MP_DISTANCE,MP_RANDOM,MP_TOP,MP_CONFIG,MP_STATS,MP_MOVE,MP_BOX,MP_COLLISIONS,MP_EMPTY,MP_POSITION};

typedef struct {
    MCObject object;
    Witness *parent;
    ItemInWorldManager *manager;
    MCObject *server,*provider,*info,*border,*configuration;
    MCObject *alternative,*capturedConfiguration;
    NBTString *earlyTranslator;
    NativeReferenceList *earlyList;
    NativeReferenceList *collision;
    StatisticsFile *stats,*earlyStats;
    BlockPos *mpSpawn,*capturedSpawn,*topPosition;
    NativeJavaRandom *replacementRandom,*drawRandom[2];
    MCObjectHeap *markBranch; /* Externally owned fixture snapshot, call lifetime only. */
    const WorldSettingsGameType *mode;
    unsigned events[128],count,failAt,listCalls,randomCalls,queries,positions,markMode,markCalls;
    int32_t protection,observedBound[2],draw[2],topY,collisionRemaining;
    double distance;
    bool noSky,mutate,replaceRandom,mutateSpawn,mutateConfiguration,mutateCollisionY,fillAfterParent;
    bool nullSpawn,nullProvider,nullInfo,nullBorder,nullConfiguration,nullStats,nullCollisions;
} MPFixture;
static void mp_trace(MCObject *object,MCObjectVisitor visit,void *context) {
    MPFixture *f=(MPFixture *)object;
    f->parent=(Witness *)visit((MCObject *)f->parent,context);
    f->manager=(ItemInWorldManager *)visit((MCObject *)f->manager,context);
    f->server=visit(f->server,context);f->provider=visit(f->provider,context);
    f->info=visit(f->info,context);f->border=visit(f->border,context);f->configuration=visit(f->configuration,context);
    f->alternative=visit(f->alternative,context);f->capturedConfiguration=visit(f->capturedConfiguration,context);
    f->earlyTranslator=(NBTString *)visit((MCObject *)f->earlyTranslator,context);
    f->earlyList=(NativeReferenceList *)visit((MCObject *)f->earlyList,context);
    f->stats=(StatisticsFile *)visit((MCObject *)f->stats,context);f->earlyStats=(StatisticsFile *)visit((MCObject *)f->earlyStats,context);
    f->mpSpawn=(BlockPos *)visit((MCObject *)f->mpSpawn,context);f->capturedSpawn=(BlockPos *)visit((MCObject *)f->capturedSpawn,context);
    f->topPosition=(BlockPos *)visit((MCObject *)f->topPosition,context);
    f->replacementRandom=(NativeJavaRandom *)visit((MCObject *)f->replacementRandom,context);
    for(unsigned i=0;i<2;i++)f->drawRandom[i]=(NativeJavaRandom *)visit((MCObject *)f->drawRandom[i],context);
    f->collision=(NativeReferenceList *)visit((MCObject *)f->collision,context);
}
static const MCObjectClass mp_fixture_class={"fixture.Source.MP.dependencies",MCObjectHeap_plainClone,mp_trace,NULL};
static const MCObjectClass mp_leaf_class={"fixture.Source.MP.required-reference",MCObjectHeap_plainClone,NULL,NULL};
static const MCObjectClass mp_padding_class={"fixture.Source.MP.allocation-budget",MCObjectHeap_plainClone,NULL,NULL};
static MPFixture *mp_fixture(MCObject *context) {
    Witness *w=(Witness *)context;CHECK(w&&w->player&&EntityPlayerMP_isInstance((MCObject *)w->player));
    MPFixture *f=(MPFixture *)w->player->effects;CHECK(f&&f->parent==w);return f;
}
static bool mp_record(MPFixture *f,unsigned kind) {
    CHECK(f->count<sizeof f->events/sizeof *f->events);f->events[f->count++]=kind;
    if(f->failAt&&f->count==f->failAt){MCObjectHeap_fail(f->object.heap);return false;}
    return true;
}
static MCObject *mp_list(MCObject *context) {
    MPFixture *f=mp_fixture(context);if(!mp_record(f,MP_LIST))return NULL;
    EntityPlayerMP *p=(EntityPlayerMP *)f->parent->player;
    CHECK(NBTString_equalsASCII(p->translator,"en_US")&&!f->manager->thisPlayerMP);
    f->listCalls++;return (MCObject *)NativeReferenceList_new(context->heap);
}
static bool mp_clock(MCObject *context,int64_t *out) {
    MPFixture *f=mp_fixture(context);if(!mp_record(f,MP_CLOCK))return false;
    EntityPlayerMP *p=(EntityPlayerMP *)f->parent->player;
    CHECK(p->loadedChunks&&p->destroyedItemsNetCache&&p->loadedChunks!=p->destroyedItemsNetCache);
    CHECK(bits(p->combinedHealth)==1&&p->lastHealth==-1.0E8f&&p->respawnInvulnerabilityTicks==60);
    *out=1234567;return true;
}
static BlockPos *mp_spawn(MCObject *context,MCObject *worldObject) {
    MPFixture *f=mp_fixture(context);CHECK(worldObject==context);if(!mp_record(f,MP_SPAWN))return NULL;
    EntityPlayerMP *p=(EntityPlayerMP *)f->parent->player;CHECK(p->theItemInWorldManager==f->manager&&f->manager->thisPlayerMP==p);
    f->capturedSpawn=f->nullSpawn?NULL:f->mpSpawn;
    if(f->mutateSpawn)f->mpSpawn=DataWatcher_blockPos(context->heap,999,111,888);
    return f->capturedSpawn;
}
static MCObject *mp_provider(MCObject *context,MCObject *worldObject) {
    MPFixture *f=mp_fixture(context);CHECK(worldObject==context);return mp_record(f,MP_PROVIDER)&&!f->nullProvider?f->provider:NULL;
}
static bool mp_no_sky(MCObject *context,MCObject *providerObject,bool *out) {
    MPFixture *f=mp_fixture(context);CHECK(providerObject==f->provider);if(!mp_record(f,MP_SKY))return false;*out=f->noSky;return true;
}
static MCObject *mp_world_info(MCObject *context,MCObject *worldObject) {
    MPFixture *f=mp_fixture(context);CHECK(worldObject==context);return mp_record(f,MP_INFO)&&!f->nullInfo?f->info:NULL;
}
static bool mp_game_type(MCObject *context,MCObject *infoObject,const WorldSettingsGameType **out) {
    MPFixture *f=mp_fixture(context);CHECK(infoObject==f->info);if(!mp_record(f,MP_MODE))return false;*out=f->mode;return true;
}
static bool mp_protection(MCObject *context,MCObject *serverObject,int32_t *out) {
    MPFixture *f=mp_fixture(context);CHECK(serverObject==f->server);if(!mp_record(f,MP_PROTECTION))return false;*out=f->protection;return true;
}
static MCObject *mp_border(MCObject *context,MCObject *worldObject) {
    MPFixture *f=mp_fixture(context);CHECK(worldObject==context);return mp_record(f,MP_BORDER)&&!f->nullBorder?f->border:NULL;
}
static bool mp_distance(MCObject *context,MCObject *borderObject,double x,double z,double *out) {
    MPFixture *f=mp_fixture(context);CHECK(borderObject==f->border);if(!mp_record(f,MP_DISTANCE))return false;
    CHECK(f->capturedSpawn&&x==f->capturedSpawn->vec3i.x&&z==f->capturedSpawn->vec3i.z);*out=f->distance;return true;
}
static bool mp_random(MCObject *context,NativeJavaRandom *random,int32_t bound,int32_t *out) {
    MPFixture *f=mp_fixture(context);CHECK(random==((Witness *)context)->player->living.entity.rand);
    if(!mp_record(f,MP_RANDOM))return false;
    CHECK(f->randomCalls<2);
    unsigned index=f->randomCalls++;f->drawRandom[index]=random;f->observedBound[index]=bound;
    bool ok=NativeJavaRandom_nextIntBound(random,bound,out);if(ok)f->draw[index]=*out;
    if(ok&&index==0&&f->replaceRandom)f->parent->player->living.entity.rand=f->replacementRandom;
    return ok;
}
static BlockPos *mp_top(MCObject *context,MCObject *worldObject,BlockPos *pos) {
    MPFixture *f=mp_fixture(context);CHECK(worldObject==context);if(!mp_record(f,MP_TOP))return NULL;
    CHECK(pos&&pos->vec3i.y==f->capturedSpawn->vec3i.y);
    f->topPosition=DataWatcher_blockPos(context->heap,pos->vec3i.x,f->topY,pos->vec3i.z);return f->topPosition;
}
static MCObject *mp_configuration(MCObject *context,MCObject *serverObject) {
    MPFixture *f=mp_fixture(context);CHECK(serverObject==f->server);if(!mp_record(f,MP_CONFIG))return NULL;
    f->capturedConfiguration=f->nullConfiguration?NULL:f->configuration;
    if(f->mutateConfiguration) {
        ((EntityPlayerMP *)f->parent->player)->mcServer=f->alternative;f->configuration=f->alternative;
    }
    return f->capturedConfiguration;
}
static StatisticsFile *mp_stats(MCObject *context,MCObject *configurationObject,EntityPlayerMP *p) {
    MPFixture *f=mp_fixture(context);CHECK(configurationObject==f->capturedConfiguration&&p==(EntityPlayerMP *)f->parent->player);
    if(!mp_record(f,MP_STATS))return NULL;
    return f->nullStats?NULL:f->stats;
}
static bool mp_move(MCObject *context,EntityPlayerMP *p,BlockPos *pos,float yaw,float pitch) {
    MPFixture *f=mp_fixture(context);CHECK(p==(EntityPlayerMP *)f->parent->player);if(!mp_record(f,MP_MOVE))return false;
    CHECK(pos==(f->noSky||f->mode==&WorldSettingsGameType_ADVENTURE?f->capturedSpawn:f->topPosition));
    return Entity_moveToBlockPosAndAngles(&p->player.living.entity,pos,yaw,pitch);
}
static AxisAlignedBB *mp_box(MCObject *context,EntityPlayerMP *p) {
    MPFixture *f=mp_fixture(context);CHECK(p==(EntityPlayerMP *)f->parent->player);return mp_record(f,MP_BOX)?Entity_getEntityBoundingBox(&p->player.living.entity):NULL;
}
static MCObject *mp_collisions(MCObject *context,MCObject *worldObject,EntityPlayerMP *p,AxisAlignedBB *box) {
    MPFixture *f=mp_fixture(context);CHECK(worldObject==context&&p==(EntityPlayerMP *)f->parent->player&&box==p->player.living.entity.boundingBox);
    if(!mp_record(f,MP_COLLISIONS))return NULL;
    f->queries++;
    if(f->nullCollisions)return NULL;
    CHECK(NativeReferenceList_clear(f->collision));
    if(f->collisionRemaining>0){CHECK(NativeReferenceList_add(f->collision,(MCObject *)box));f->collisionRemaining--;}
    if(f->mutateCollisionY)CHECK(Entity_setPosition(&p->player.living.entity,p->player.living.entity.posX,255,p->player.living.entity.posZ));
    return (MCObject *)f->collision;
}
static bool mp_empty(MCObject *context,MCObject *list,bool *out) {
    MPFixture *f=mp_fixture(context);CHECK(list==(MCObject *)f->collision);if(!mp_record(f,MP_EMPTY))return false;
    *out=NativeReferenceList_size((NativeReferenceList *)list)==0;return true;
}
static bool mp_position(MCObject *context,EntityPlayerMP *p,double x,double y,double z) {
    MPFixture *f=mp_fixture(context);CHECK(p==(EntityPlayerMP *)f->parent->player);if(!mp_record(f,MP_POSITION))return false;f->positions++;
    return Entity_setPosition(&p->player.living.entity,x,y,z);
}
static bool mp_entity_location(MCObject *context,Entity *e,double x,double y,double z,float yaw,float pitch) {
    CHECK(e==&((Witness *)context)->player->living.entity);
    return Entity_setLocationAndAngles(e,x,y,z,yaw,pitch);
}
static bool mp_bounds(MCObject *context,Entity *e,AxisAlignedBB *box) {
    if(!bounds(context,e,box))return false;
    MPFixture *f=mp_fixture(context);
    if(f->fillAfterParent&&f->parent->count==sizeof sourceOrder/sizeof *sourceOrder) {
        const size_t budget=32u*1024u*1024u;
        CHECK(MCObjectHeap_alloc(context->heap,budget-MCObjectHeap_liveBytes(context->heap),&mp_padding_class));
    }
    return true;
}
static bool mp_init(MCObject *context,Entity *e) {
    if(!init(context,e))return false;
    MPFixture *f=mp_fixture(context);EntityPlayerMP *p=(EntityPlayerMP *)e;
    if(f->mutate) {
        p->translator=f->earlyTranslator;p->playerNetServerHandler=(NetHandlerPlayServer *)f->alternative;
        p->mcServer=f->alternative;p->theItemInWorldManager=f->manager;
        p->managedPosX=25;p->managedPosZ=-27;p->loadedChunks=(MCObject *)f->earlyList;
        p->destroyedItemsNetCache=(MCObject *)f->earlyList;p->statsFile=f->earlyStats;
        p->combinedHealth=12;p->lastHealth=13;p->lastFoodLevel=14;p->wasHungry=false;
        p->lastExperience=15;p->respawnInvulnerabilityTicks=16;p->chatVisibility=f->alternative;
        p->chatColours=false;p->playerLastActiveTime=17;p->spectatingEntity=e;
        p->currentWindowId=18;p->isChangingQuantityOnly=true;p->ping=19;p->playerConqueredTheEnd=true;
        MCObjectHeap_touch(context->heap);
    }
    return true;
}
static const EntityDependencies mp_entity_dependencies={
    .entityInit=mp_init,.setPosition=position,.setEntityBoundingBox=mp_bounds,.getDimensionId=dimension,
    .watcher=&watcherDependencies,.setLocationAndAngles=mp_entity_location
};
static const EntityPlayerDependencies mp_parent_dependencies={
    .entity=&mp_entity_dependencies,.living=&livingDependencies,.isRemote=remote,.getSpawnPoint=spawn,
    .setLocationAndAngles=location
};
static const EntityPlayerMPConstructorDependencies mp_dependencies={
    .player=&mp_parent_dependencies,.newLinkedList=mp_list,.currentTimeMillis=mp_clock,.getSpawnPoint=mp_spawn,
    .getProvider=mp_provider,.getHasNoSky=mp_no_sky,.getWorldInfo=mp_world_info,.getWorldGameType=mp_game_type,
    .getSpawnProtectionSize=mp_protection,.getWorldBorder=mp_border,.getClosestDistance=mp_distance,.nextInt=mp_random,
    .getTopSolidOrLiquidBlock=mp_top,.getConfigurationManager=mp_configuration,.getPlayerStatsFile=mp_stats,
    .moveToBlockPosAndAngles=mp_move,.getEntityBoundingBox=mp_box,.getCollidingBoundingBoxes=mp_collisions,
    .isCollisionListEmpty=mp_empty,.setPosition=mp_position
};
/* These callbacks are required by the real StatisticsFile constructor's
   immutable dispatch table. Constructor tests never trigger stats effects. */
static bool stats_announcing(MCObject *c,MCObject *s) {(void)c;(void)s;CHECK(false);return false;}
static bool stats_chat(MCObject *c,MCObject *s,MCObject *p,StatBase *b,bool taken) {
    (void)c;(void)s;(void)p;(void)b;(void)taken;CHECK(false);return false;
}
static int32_t stats_ticks(MCObject *c,MCObject *s) {(void)c;(void)s;CHECK(false);return 0;}
static bool stats_send(MCObject *c,MCObject *p,StatisticsFileIntMap *m) {
    (void)c;(void)p;(void)m;CHECK(false);return false;
}
static const StatisticsFileDependencies stats_dependencies={stats_announcing,stats_chat,stats_ticks,stats_send};
static MPFixture *mp_setup(MCObjectHeap *heap) {
    Witness *w=setup(heap);EntityPlayerMP *p=EntityPlayerMP_nativeAllocate(heap);CHECK(p);w->player=&p->player;
    MPFixture *f=(MPFixture *)MCObjectHeap_alloc(heap,sizeof(*f),&mp_fixture_class);CHECK(f);f->parent=w;p->player.effects=(MCObject *)f;
    f->manager=ItemInWorldManager_new(heap,(MCObject *)w);CHECK(f->manager);
    f->server=MCObjectHeap_alloc(heap,sizeof(MCObject),&mp_leaf_class);CHECK(f->server);
    f->provider=MCObjectHeap_alloc(heap,sizeof(MCObject),&mp_leaf_class);CHECK(f->provider);
    f->info=MCObjectHeap_alloc(heap,sizeof(MCObject),&mp_leaf_class);CHECK(f->info);
    f->border=MCObjectHeap_alloc(heap,sizeof(MCObject),&mp_leaf_class);CHECK(f->border);
    f->configuration=MCObjectHeap_alloc(heap,sizeof(MCObject),&mp_leaf_class);CHECK(f->configuration);
    f->alternative=MCObjectHeap_alloc(heap,sizeof(MCObject),&mp_leaf_class);CHECK(f->alternative);
    f->earlyTranslator=NBTString_fromASCII(heap,"early virtual write");CHECK(f->earlyTranslator);
    f->earlyList=NativeReferenceList_new(heap);CHECK(f->earlyList);
    f->collision=NativeReferenceList_new(heap);CHECK(f->collision);
    f->stats=StatisticsFile_nativeNew(heap,f->server,NULL,(MCObject *)w,&stats_dependencies);CHECK(f->stats);
    f->earlyStats=StatisticsFile_nativeNew(heap,f->server,NULL,(MCObject *)w,&stats_dependencies);CHECK(f->earlyStats);
    f->mpSpawn=DataWatcher_blockPos(heap,12,80,-7);CHECK(f->mpSpawn);
    f->replacementRandom=NativeJavaRandom_new(heap,456);CHECK(f->replacementRandom);
    f->noSky=true;f->mode=&WorldSettingsGameType_SURVIVAL;f->protection=16;f->distance=100;f->topY=88;
    return f;
}
static bool mp_construct(MPFixture *f) {
    Witness *w=f->parent;
    return EntityPlayerMP_construct((EntityPlayerMP *)w->player,f->server,(MCObject *)w,w->profile,f->manager,
        &mp_dependencies,&craftingDependencies,(MCObject *)w,w->random,w->ids);
}
static void mp_free(MPFixture *f) {MCObjectHeap *heap=f->object.heap;release(f->parent);MCObjectHeap_free(heap);}
static MPFixture *mp_new_fixture(void) {
    MCObjectHeap *heap=MCObjectHeap_new(32*1024*1024);CHECK(heap);return mp_setup(heap);
}
static void mp_final(MPFixture *f) {
    EntityPlayerMP *p=(EntityPlayerMP *)f->parent->player;Entity *e=&p->player.living.entity;
    CHECK((MCObject *)p==(MCObject *)&p->player&&(MCObject *)p==(MCObject *)e);
    CHECK(EntityPlayerMP_isInstance((MCObject *)p)&&MCGameplayPlayer_isInstance((MCObject *)p));
    CHECK(Entity_isInstance((MCObject *)p)&&EntityLivingBase_isInstance((MCObject *)p));
    CHECK(p->constructorContext==(MCObject *)f->parent&&p->constructorDependencies==&mp_dependencies);
    CHECK(p->translator==NBTString_literalASCII(f->object.heap,"en_US"));
    CHECK(p->loadedChunks&&p->destroyedItemsNetCache&&p->loadedChunks!=p->destroyedItemsNetCache);
    CHECK(NativeReferenceList_size((NativeReferenceList *)p->loadedChunks)==0&&
        NativeReferenceList_size((NativeReferenceList *)p->destroyedItemsNetCache)==0);
    CHECK(p->theItemInWorldManager==f->manager&&f->manager->thisPlayerMP==p&&f->manager->theWorld==(MCObject *)f->parent);
    CHECK(p->mcServer==(f->mutateConfiguration?f->alternative:f->server));
    CHECK(p->statsFile==(f->nullStats?NULL:f->stats)&&EntityPlayerMP_getStatFile(p)==p->statsFile);
    CHECK(bits(p->combinedHealth)==1&&p->lastHealth==-1e8f&&p->lastFoodLevel==-99999999&&p->wasHungry);
    CHECK(p->lastExperience==-99999999&&p->respawnInvulnerabilityTicks==60&&p->chatColours);
    CHECK(p->playerLastActiveTime==1234567&&!p->spectatingEntity);
    if(f->mutate) {
        CHECK(p->playerNetServerHandler==(NetHandlerPlayServer *)f->alternative&&p->managedPosX==25&&p->managedPosZ==-27);
        CHECK(p->chatVisibility==f->alternative&&p->currentWindowId==18&&p->isChangingQuantityOnly);
        CHECK(p->ping==19&&p->playerConqueredTheEnd);
    } else {
        CHECK(!p->playerNetServerHandler&&p->managedPosX==0&&p->managedPosZ==0&&!p->chatVisibility);
        CHECK(p->currentWindowId==0&&!p->isChangingQuantityOnly&&p->ping==0&&!p->playerConqueredTheEnd);
    }
    CHECK(e->stepHeight==0&&e->rotationYaw==0&&e->rotationPitch==0&&e->dimension==42);
    CHECK(p->player.inventory->player==(MCObject *)p&&p->player.living._combatTracker->fighter==&p->player.living);
    CHECK(p->player.gameProfile==f->parent->profile&&e->entityUniqueID==f->parent->profile->id);
    CHECK(p->player.openContainer==p->player.inventoryContainer&&EntityLivingBase_getHealth(&p->player.living)==20);
    CHECK(f->parent->count>=sizeof sourceOrder/sizeof *sourceOrder&&
        memcmp(f->parent->events,sourceOrder,sizeof sourceOrder)==0);
    CHECK(!MCObjectHeap_failed(f->object.heap)&&!MCObjectHeap_hasBorrowers(f->object.heap));
}
static void mp_success(void) {
    static const unsigned skipped[]={MP_LIST,MP_LIST,MP_CLOCK,MP_SPAWN,MP_PROVIDER,MP_SKY,
        MP_CONFIG,MP_STATS,MP_MOVE,MP_BOX,MP_COLLISIONS,MP_EMPTY};
    static const unsigned ordinary[]={MP_LIST,MP_LIST,MP_CLOCK,MP_SPAWN,MP_PROVIDER,MP_SKY,MP_INFO,MP_MODE,
        MP_PROTECTION,MP_BORDER,MP_DISTANCE,MP_RANDOM,MP_RANDOM,MP_TOP,MP_CONFIG,MP_STATS,MP_MOVE,MP_BOX,MP_COLLISIONS,MP_EMPTY};
    for(unsigned mode=0;mode<8;mode++) {
        MPFixture *f=mp_new_fixture();f->mutate=(mode&1)!=0;f->parent->mutate=f->mutate;
        f->noSky=(mode&2)!=0;f->mode=(mode&4)?&WorldSettingsGameType_ADVENTURE:&WorldSettingsGameType_SURVIVAL;
        CHECK(mp_construct(f));mp_final(f);
        bool ordinaryBranch=!f->noSky&&f->mode!=&WorldSettingsGameType_ADVENTURE;
        if(ordinaryBranch) {
            CHECK(f->count==sizeof ordinary/sizeof *ordinary&&memcmp(f->events,ordinary,sizeof ordinary)==0);
            CHECK(f->randomCalls==2&&f->observedBound[0]==20&&f->observedBound[1]==20);
        } else if(f->noSky)CHECK(f->count==sizeof skipped/sizeof *skipped&&memcmp(f->events,skipped,sizeof skipped)==0);
        else {CHECK(f->count==14&&f->events[6]==MP_INFO&&f->events[7]==MP_MODE&&f->randomCalls==0);}
        BlockPos *pos=ordinaryBranch?f->topPosition:f->capturedSpawn;
        Entity *e=&f->parent->player->living.entity;
        CHECK(e->posX==(double)pos->vec3i.x+0.5&&e->posY==(double)pos->vec3i.y&&e->posZ==(double)pos->vec3i.z+0.5);
        CHECK(f->queries==1&&f->positions==0&&f->listCalls==2);
        mp_free(f);
    }
}
static void mp_random_and_capture(void) {
    static const struct {int32_t protection;double distance;int32_t bound;} cases[]={
        {16,100,20},{0,100,10},{16,3.9,6},{16,1.9,2},{16,-0.5,2},{16,NAN,2},
        {INT32_MIN,3.25,6},{INT32_MAX,2.0,4},{16,-INFINITY,20}
    };
    for(unsigned i=0;i<sizeof cases/sizeof *cases;i++) {
        MPFixture *f=mp_new_fixture();f->noSky=false;f->protection=cases[i].protection;f->distance=cases[i].distance;
        CHECK(mp_construct(f));mp_final(f);CHECK(f->observedBound[0]==cases[i].bound&&f->observedBound[1]==cases[i].bound);
        CHECK(f->topPosition->vec3i.x==f->capturedSpawn->vec3i.x+f->draw[0]-cases[i].bound/2);
        CHECK(f->topPosition->vec3i.z==f->capturedSpawn->vec3i.z+f->draw[1]-cases[i].bound/2);
        mp_free(f);
    }
    MPFixture *f=mp_new_fixture();f->noSky=false;f->mode=NULL;f->replaceRandom=true;f->mutateSpawn=true;
    f->mutateConfiguration=true;f->nullStats=true;
    CHECK(mp_construct(f));mp_final(f);
    CHECK(f->drawRandom[0]!=f->drawRandom[1]&&f->drawRandom[1]==f->replacementRandom);
    CHECK(f->capturedSpawn!=f->mpSpawn&&f->capturedSpawn->vec3i.x==12&&f->mpSpawn->vec3i.x==999);
    CHECK(f->capturedConfiguration!=f->configuration&&f->configuration==f->alternative);
    CHECK(f->topPosition->vec3i.x==12+f->draw[0]-10&&f->topPosition->vec3i.z==-7+f->draw[1]-10);
    mp_free(f);
    /* The source wrap creates a negative bound, and Random throws before the
       second draw or top-solid lookup; no native clamp substitutes for it. */
    f=mp_new_fixture();f->noSky=false;f->protection=INT32_MIN;f->distance=INFINITY;
    CHECK(!mp_construct(f)&&MCObjectHeap_failed(f->object.heap));
    CHECK(f->randomCalls==1&&f->observedBound[0]==-12&&f->events[f->count-1]==MP_RANDOM);
    CHECK(!((EntityPlayerMP *)f->parent->player)->mcServer&&!f->topPosition&&!MCObjectHeap_hasBorrowers(f->object.heap));
    mp_free(f);
}
static void mp_collision_order(void) {
    for(unsigned mode=0;mode<4;mode++) {
        MPFixture *f=mp_new_fixture();f->collisionRemaining=2;
        if(mode==1)f->mpSpawn=DataWatcher_blockPos(f->object.heap,12,255,-7);
        if(mode==2)f->mpSpawn=DataWatcher_blockPos(f->object.heap,12,254,-7);
        if(mode==3)f->mutateCollisionY=true;
        CHECK(mp_construct(f));mp_final(f);
        CHECK(f->queries==(mode==0?3u:mode==2?2u:1u));
        CHECK(f->positions==(mode==0?2u:mode==2?1u:0u));
        CHECK(f->parent->player->living.entity.posY==(mode==0?82:255));
        CHECK(f->events[f->count-3]==MP_BOX&&f->events[f->count-2]==MP_COLLISIONS&&f->events[f->count-1]==MP_EMPTY);
        mp_free(f);
    }
}
static void mp_failures(void) {
    unsigned baseline[128],count;
    MPFixture *f=mp_new_fixture();f->noSky=false;f->collisionRemaining=2;CHECK(mp_construct(f));
    count=f->count;memcpy(baseline,f->events,count*sizeof *baseline);mp_free(f);
    for(unsigned fail=1;fail<=count;fail++) {
        f=mp_new_fixture();f->noSky=false;f->collisionRemaining=2;f->mutate=true;f->failAt=fail;
        CHECK(!mp_construct(f)&&MCObjectHeap_failed(f->object.heap)&&f->count==fail);
        CHECK(memcmp(f->events,baseline,fail*sizeof *baseline)==0&&!MCObjectHeap_hasBorrowers(f->object.heap));
        EntityPlayerMP *p=(EntityPlayerMP *)f->parent->player;
        CHECK(NBTString_equalsASCII(p->translator,"en_US"));
        if(fail==1)CHECK(p->loadedChunks==(MCObject *)f->earlyList&&p->destroyedItemsNetCache==(MCObject *)f->earlyList);
        if(fail==2)CHECK(p->loadedChunks!=(MCObject *)f->earlyList&&p->destroyedItemsNetCache==(MCObject *)f->earlyList);
        if(fail==3)CHECK(p->playerLastActiveTime==17&&p->spectatingEntity==&p->player.living.entity&&p->statsFile==f->earlyStats);
        if(fail>3)CHECK(p->playerLastActiveTime==1234567&&!p->spectatingEntity&&f->manager->thisPlayerMP==p);
        if(baseline[fail-1]==MP_STATS)CHECK(p->statsFile==f->earlyStats&&p->player.living.entity.stepHeight==0.6f);
        mp_free(f);
    }
    for(unsigned fail=1;fail<=sizeof sourceOrder/sizeof *sourceOrder;fail++) {
        f=mp_new_fixture();f->parent->failAt=fail;
        CHECK(!mp_construct(f)&&MCObjectHeap_failed(f->object.heap)&&f->parent->count==fail&&f->count==0);
        CHECK(memcmp(f->parent->events,sourceOrder,fail*sizeof *sourceOrder)==0);
        CHECK(!((EntityPlayerMP *)f->parent->player)->translator&&!MCObjectHeap_hasBorrowers(f->object.heap));
        mp_free(f);
    }
}
static void mp_null_dependencies(void) {
    for(unsigned mode=0;mode<10;mode++) {
        MPFixture *f=mp_new_fixture();Witness *w=f->parent;f->noSky=false;
        if(mode==3)f->nullSpawn=true;
        if(mode==4)f->nullProvider=true;
        if(mode==5)f->nullInfo=true;
        if(mode==6)f->nullBorder=true;
        if(mode==7)f->nullConfiguration=true;
        if(mode==8)f->nullCollisions=true;
        if(mode==9){f->noSky=true;f->nullSpawn=true;}
        CHECK(!EntityPlayerMP_construct((EntityPlayerMP *)w->player,f->server,mode==1?NULL:(MCObject *)w,
            mode==0?NULL:w->profile,mode==2?NULL:f->manager,&mp_dependencies,&craftingDependencies,(MCObject *)w,w->random,w->ids));
        CHECK(MCObjectHeap_failed(f->object.heap)&&!MCObjectHeap_hasBorrowers(f->object.heap));
        EntityPlayerMP *p=(EntityPlayerMP *)w->player;
        if(mode==0||mode==1)CHECK(f->count==0&&!p->translator&&w->mathCount==3&&p->player.inventory&&p->player.capabilities);
        if(mode==2)CHECK(f->count==3&&!p->theItemInWorldManager&&p->playerLastActiveTime==1234567);
        if(mode==3)CHECK(f->events[f->count-1]==MP_BORDER&&f->randomCalls==0);
        if(mode==4)CHECK(f->events[f->count-1]==MP_PROVIDER);
        if(mode==5)CHECK(f->events[f->count-1]==MP_INFO);
        if(mode==6)CHECK(f->events[f->count-1]==MP_BORDER);
        if(mode==7)CHECK(f->events[f->count-1]==MP_CONFIG&&!p->statsFile);
        if(mode==8)CHECK(f->events[f->count-1]==MP_COLLISIONS&&p->statsFile==f->stats);
        if(mode==9)CHECK(f->events[f->count-1]==MP_MOVE&&p->statsFile==f->stats&&p->player.living.entity.stepHeight==0);
        mp_free(f);
    }
}
static void mp_lifetime(void) {
    MPFixture *f=mp_new_fixture();f->noSky=false;f->mutate=true;f->parent->mutate=true;f->replaceRandom=true;
    MCObjectHeap *heap=f->object.heap;MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,heap,(MCObject *)f));
    CHECK(mp_construct(f));mp_final(f);CHECK(MCObjectHeap_collect(heap));mp_final(f);
    MCObjectHeap *branch=MCObjectHeap_clone(heap);CHECK(branch);MCObjectRoot other={0};CHECK(MCObjectRoot_rebind(&other,branch,&root));
    MPFixture *copy=(MPFixture *)MCObjectRoot_get(&other);CHECK(copy&&copy!=f&&copy->parent->player!=f->parent->player);
    mp_final(copy);CHECK(copy->manager->field_180240_f==NativeBlockPos_origin(branch));
    CHECK(copy->manager->field_180240_f!=f->manager->field_180240_f&&copy->replacementRandom!=f->replacementRandom);
    CHECK(MCObjectHeap_collect(branch));mp_final(copy);
    CHECK(MCObjectHeap_adopt(heap,branch));f=(MPFixture *)MCObjectRoot_get(&root);CHECK(f);mp_final(f);
    CHECK(MCObjectHeap_collect(heap));mp_final(f);
    MCObjectRoot_drop(&other);MCObjectHeap_free(branch);release(f->parent);MCObjectRoot_drop(&root);MCObjectHeap_free(heap);
}
static bool mark_clock(MCObject *context,int64_t *out) {
    MPFixture *f=mp_fixture(context);EntityPlayerMP *p=(EntityPlayerMP *)f->parent->player;
    f->markCalls++;p->ping=91;MCObjectHeap_touch(context->heap);
    /* These are actual heap operations during the live required clock call.
       The source receiver/context must remain borrowed until its final write. */
    CHECK(!MCObjectHeap_collect(context->heap));
    CHECK(MCObjectHeap_hasBorrowers(context->heap));
    CHECK(!MCObjectHeap_failed(context->heap));
    CHECK(!MCObjectHeap_clone(context->heap)&&f->markBranch);
    CHECK(!MCObjectHeap_adopt(context->heap,f->markBranch)&&!MCObjectHeap_failed(context->heap));
    *out=7654321;
    if(f->markMode==2)MCObjectHeap_fail(context->heap);
    if(f->markMode==3){p->constructorContext=f->alternative;p->constructorDependencies=NULL;}
    return f->markMode!=1;
}
static const EntityPlayerMPConstructorDependencies mark_dependencies={.currentTimeMillis=mark_clock};
static void mp_active_clock_lifetime(void) {
    for(unsigned mode=0;mode<4;mode++) {
        MPFixture *f=mp_new_fixture();MCObjectHeap *heap=f->object.heap;
        MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,heap,(MCObject *)f));CHECK(mp_construct(f));
        EntityPlayerMP *p=(EntityPlayerMP *)f->parent->player;p->constructorDependencies=&mark_dependencies;f->markMode=mode;
        f->markBranch=MCObjectHeap_clone(heap);CHECK(f->markBranch);
        bool result=EntityPlayerMP_markPlayerActive(p);CHECK(result==(mode==0||mode==3));
        CHECK(f->markCalls==1&&p->ping==91&&!MCObjectHeap_hasBorrowers(heap));
        CHECK(p->playerLastActiveTime==(result?7654321:1234567)&&MCObjectHeap_failed(heap)==!result);
        if(mode==3)CHECK(p->constructorContext==f->alternative&&!p->constructorDependencies);
        if(result)CHECK(EntityPlayerMP_getLastActiveTime(p)==7654321&&MCObjectHeap_collect(heap));
        MCObjectHeap_free(f->markBranch);f->markBranch=NULL;
        release(f->parent);MCObjectRoot_drop(&root);MCObjectHeap_free(heap);
    }
    MPFixture *f=mp_new_fixture();CHECK(mp_construct(f));EntityPlayerMP *p=(EntityPlayerMP *)f->parent->player;
    MCObjectHeap *foreign=MCObjectHeap_new(1024);CHECK(foreign);
    p->constructorDependencies=&mark_dependencies;p->constructorContext=MCObjectHeap_alloc(foreign,sizeof(MCObject),&mp_leaf_class);CHECK(p->constructorContext);
    CHECK(!EntityPlayerMP_markPlayerActive(p)&&MCObjectHeap_failed(f->object.heap)&&!MCObjectHeap_failed(foreign));
    CHECK(f->markCalls==0&&p->playerLastActiveTime==1234567&&!MCObjectHeap_hasBorrowers(f->object.heap));
    p->constructorContext=NULL;mp_free(f);MCObjectHeap_free(foreign);
}
static void mp_native_exception_prefix(void) {
    MPFixture *f=mp_new_fixture();f->mutate=true;f->fillAfterParent=true;
    CHECK(!mp_construct(f)&&MCObjectHeap_failed(f->object.heap));
    EntityPlayerMP *p=(EntityPlayerMP *)f->parent->player;
    CHECK(f->parent->count==sizeof sourceOrder/sizeof *sourceOrder&&f->count==0);
    CHECK(p->translator==f->earlyTranslator&&p->loadedChunks==(MCObject *)f->earlyList&&p->playerLastActiveTime==17);
    CHECK(!MCObjectHeap_hasBorrowers(f->object.heap));mp_free(f);
    for(unsigned mode=0;mode<4;mode++) {
        f=mp_new_fixture();CHECK(mp_construct(f));p=(EntityPlayerMP *)f->parent->player;
        MCObjectHeap *foreign=MCObjectHeap_new(4096);CHECK(foreign);
        if(mode==0)f->manager->gameType=&WorldSettingsGameType_SPECTATOR;
        if(mode==2)p->theItemInWorldManager=NULL;
        if(mode==3) {
            p->theItemInWorldManager=ItemInWorldManager_new(foreign,NULL);CHECK(p->theItemInWorldManager);
            p->theItemInWorldManager->gameType=&WorldSettingsGameType_SPECTATOR;
        }
        CHECK(EntityPlayerMP_isSpectator(p)==(mode==0));
        CHECK(MCObjectHeap_failed(f->object.heap)==(mode>=2)&&!MCObjectHeap_failed(foreign));
        CHECK(!MCObjectHeap_hasBorrowers(f->object.heap));
        p->theItemInWorldManager=f->manager;mp_free(f);MCObjectHeap_free(foreign);
    }
}
int main(void) {
    mp_success();mp_random_and_capture();mp_collision_order();mp_failures();mp_null_dependencies();mp_lifetime();mp_active_clock_lifetime();mp_native_exception_prefix();
    printf("Source MP constructor: %u checks GREEN\n",checks);return 0;
}
