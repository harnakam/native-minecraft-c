#include "world/World.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"World ctor check %u line %d\n",checks,__LINE__);exit(1);}}while(0)
typedef struct {
    MCObject object;
    World *world;
    WorldInfo *alternateInfo;
    NativeJavaRandom *replacement;
    NativeJavaRandom *boundReceiver;
    unsigned calls,failAt,infoCalls;
    bool nullBorder;
} ConstructionContext;
static void context_trace(MCObject *object,MCObjectVisitor visit,void *opaque) {
    ConstructionContext *c=(ConstructionContext *)object;
    c->world=(World *)visit((MCObject *)c->world,opaque);
    c->alternateInfo=(WorldInfo *)visit((MCObject *)c->alternateInfo,opaque);
    c->replacement=(NativeJavaRandom *)visit((MCObject *)c->replacement,opaque);
    c->boundReceiver=(NativeJavaRandom *)visit((MCObject *)c->boundReceiver,opaque);
}
static const MCObjectClass contextClass={"test.WorldConstruction",MCObjectHeap_plainClone,context_trace,NULL};
static const MCObjectClass opaqueClass={"test.WorldOpaque",MCObjectHeap_plainClone,NULL,NULL};
static bool step(ConstructionContext *c) {
    ++c->calls;
    if(c->calls==1) {
        c->world->scheduledUpdatesAreImmediate=true;
        c->world->prevRainingStrength=0.375f;
        c->world->processingLoadedTiles=true;
        c->world->isRemote=true;
    }
    return c->calls!=c->failAt;
}
static NativeReferenceList *new_list(MCObject *object,World *world) {
    ConstructionContext *c=(ConstructionContext *)object;CHECK(c->world==world);
    return step(c)?NativeReferenceList_new(object->heap):NULL;
}
static IntHashMap *new_index(MCObject *object,World *world) {
    ConstructionContext *c=(ConstructionContext *)object;CHECK(c->world==world);
    return step(c)?IntHashMap_new(object->heap):NULL;
}
static NativeJavaRandom *new_random(MCObject *object,World *world) {
    ConstructionContext *c=(ConstructionContext *)object;CHECK(c->world==world);
    return step(c)?NativeJavaRandom_new(object->heap,(int64_t)c->calls):NULL;
}
static bool draw_int(MCObject *object,NativeJavaRandom *random,int32_t *out) {
    ConstructionContext *c=(ConstructionContext *)object;CHECK(NativeJavaRandom_isInstance((MCObject *)random));
    if(!step(c))return false;
    *out=678;return true;
}
static bool draw_bound(MCObject *object,NativeJavaRandom *random,int32_t bound,int32_t *out) {
    ConstructionContext *c=(ConstructionContext *)object;CHECK(bound==12000);
    c->boundReceiver=random;
    if(!step(c))return false;
    *out=411;return true;
}
static NativeCalendar *new_calendar(MCObject *object,World *world) {
    ConstructionContext *c=(ConstructionContext *)object;CHECK(c->world==world);
    return step(c)?NativeCalendar_getInstance(object->heap):NULL;
}
static Scoreboard *new_board(MCObject *object,World *world) {
    ConstructionContext *c=(ConstructionContext *)object;CHECK(c->world==world);
    return step(c)?Scoreboard_new(object->heap):NULL;
}
static bool identity_hash(MCObject *context,MCObject *key,int32_t *out) {
    (void)context;*out=MCObjectHeap_identityHashCode(key);return true;
}
static bool identity_equal(MCObject *context,MCObject *query,MCObject *stored,bool *out) {
    (void)context;*out=query==stored;return true;
}
static const NativeHashKeyMethods identityKeys={identity_hash,identity_equal};
static NativeHashSet *new_set(MCObject *object,World *world) {
    ConstructionContext *c=(ConstructionContext *)object;CHECK(c->world==world);
    if(!step(c))return NULL;
    c->replacement=NativeJavaRandom_new(object->heap,987);
    world->rand=c->replacement;
    return c->replacement?NativeHashSet_newWithKeys(object->heap,&identityKeys,NULL):NULL;
}
static NativeIntArray *new_light(MCObject *object,World *world,int32_t length) {
    ConstructionContext *c=(ConstructionContext *)object;CHECK(c->world==world&&length==32768);
    return step(c)?NativeIntArray_new(object->heap,length):NULL;
}
static WorldBorder *get_border(MCObject *object,WorldProvider *provider) {
    ConstructionContext *c=(ConstructionContext *)object;CHECK(c->world->provider==provider);
    if(!step(c)){MCObjectHeap_fail(object->heap);return NULL;}
    if(c->nullBorder)return NULL;
    return WorldProvider_getWorldBorder(provider);
}
static WorldInfo *get_info(MCObject *object,World *world) {
    ConstructionContext *c=(ConstructionContext *)object;CHECK(c->world==world);
    ++c->infoCalls;return c->alternateInfo;
}
static const WorldDependencies constructionDependencies={
    .newArrayList=new_list,.newIntHashMap=new_index,.newRandom=new_random,
    .randomNextInt=draw_int,.randomNextIntBound=draw_bound,.newCalendar=new_calendar,
    .newScoreboard=new_board,.newActiveChunkSet=new_set,.newLightUpdateBlockList=new_light,
    .providerGetWorldBorder=get_border,.getWorldInfo=get_info
};
static WorldInfo *new_info(MCObjectHeap *heap) {
    WorldInfo *info=WorldInfo_nativeAllocate(heap,NULL,NULL);CHECK(info);
    CHECK(WorldInfo_construct(info));return info;
}
static void constructor_prefixes(void) {
    /* These are native allocation/dispatch failures, not a claim about JVM
       OOM behavior. Each leaf returns real managed instances on success. */
    for(unsigned failure=0;failure<=19;failure++) {
        MCObjectHeap *heap=MCObjectHeap_new(32u*1024u*1024u);CHECK(heap);
        ConstructionContext *c=(ConstructionContext *)MCObjectHeap_alloc(heap,sizeof(*c),&contextClass);CHECK(c);
        c->failAt=failure;c->world=World_nativeAllocate(heap,&constructionDependencies,(MCObject *)c);CHECK(c->world);
        World *world=c->world;WorldInfo *info=new_info(heap);
        WorldProvider *provider=WorldProvider_getProviderForDimension(heap,0,NULL,NULL);CHECK(provider);
        MCObject *save=MCObjectHeap_alloc(heap,sizeof(MCObject),&opaqueClass);
        MCObject *profiler=MCObjectHeap_alloc(heap,sizeof(MCObject),&opaqueClass);CHECK(save&&profiler);
        bool success=World_construct(world,save,info,provider,profiler,false);
        CHECK(success==(failure==0)&&MCObjectHeap_failed(heap)==(failure!=0));
        unsigned reached=failure?failure:19;CHECK(c->calls==reached);
        CHECK(world->seaLevel==63&&world->scheduledUpdatesAreImmediate&&world->processingLoadedTiles);
        CHECK(world->prevRainingStrength==0.375f);
        CHECK((world->loadedEntityList!=NULL)==(failure==0||failure>1));
        CHECK((world->weatherEffects!=NULL)==(failure==0||failure>8));
        CHECK((world->entitiesById!=NULL)==(failure==0||failure>9));
        CHECK(world->cloudColour==((failure==0||failure>9)?16777215:0));
        CHECK(world->updateLCG==((failure==0||failure>11)?678:0));
        CHECK(world->DIST_HASH_MAGIC==((failure==0||failure>11)?1013904223:0));
        CHECK((world->worldAccesses!=NULL)==(failure==0||failure>13));
        CHECK((world->theCalendar!=NULL)==(failure==0||failure>14));
        CHECK((world->worldScoreboard!=NULL)==(failure==0||failure>15));
        CHECK((world->activeChunkSet!=NULL)==(failure==0||failure>16));
        if(failure==0||failure>16)CHECK(world->rand==c->replacement&&c->boundReceiver==c->replacement);
        CHECK(world->ambientTickCountdown==((failure==0||failure>17)?411:0));
        CHECK(world->spawnHostileMobs==(failure==0||failure>17)&&world->spawnPeacefulMobs==world->spawnHostileMobs);
        CHECK((world->lightUpdateBlockList!=NULL)==(failure==0||failure>18));
        CHECK(world->saveHandler==((failure==0||failure>18)?save:NULL));
        CHECK(world->worldInfo==((failure==0||failure>18)?info:NULL));
        CHECK(world->theProfiler==((failure==0||failure>18)?profiler:NULL));
        CHECK(world->provider==((failure==0||failure>18)?provider:NULL));
        CHECK(world->isRemote==(failure!=0&&failure<=18));
        CHECK(!world->chunkProvider&&!world->mapStorage&&!world->villageCollectionObj);
        MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap=MCObjectHeap_new(32u*1024u*1024u);CHECK(heap);
    World *world=World_nativeAllocate(heap,NULL,NULL);WorldInfo *info=new_info(heap);
    CHECK(!World_construct(world,NULL,info,NULL,NULL,true)&&MCObjectHeap_failed(heap));
    CHECK(world->worldInfo==info&&!world->provider&&!world->worldBorder&&world->isRemote);
    CHECK(world->lightUpdateBlockList&&world->spawnHostileMobs&&world->spawnPeacefulMobs);
    MCObjectHeap_free(heap);
}
static void info_owner(void) {
    MCObjectHeap *heap=MCObjectHeap_new(32u*1024u*1024u);CHECK(heap);
    ConstructionContext *c=(ConstructionContext *)MCObjectHeap_alloc(heap,sizeof(*c),&contextClass);CHECK(c);
    c->world=World_nativeAllocate(heap,&constructionDependencies,(MCObject *)c);CHECK(c->world);
    WorldInfo *info=new_info(heap);c->alternateInfo=new_info(heap);
    WorldProvider *provider=WorldProvider_getProviderForDimension(heap,0,NULL,NULL);CHECK(provider);
    c->nullBorder=true;
    CHECK(World_construct(c->world,NULL,info,provider,NULL,false));
    CHECK(!c->world->worldBorder&&!MCObjectHeap_failed(heap));
    CHECK(World_getWorldInfo(c->world)==c->alternateInfo&&c->infoCalls==1);
    info->randomSeed=INT64_MIN;
    CHECK(World_getSeed(c->world)==INT64_MIN&&c->infoCalls==1);
    CHECK(World_setWorldTime(c->world,INT64_MAX)&&World_setTotalWorldTime(c->world,INT64_MIN));
    CHECK(info->worldTime==INT64_MAX&&info->totalTime==INT64_MIN);
    CHECK(World_getWorldTime(c->world)==INT64_MAX&&World_getTotalWorldTime(c->world)==INT64_MIN);
    CHECK(World_getGameRules(c->world)==info->theGameRules);
    World *second=World_nativeAllocate(heap,NULL,NULL);CHECK(second);
    CHECK(World_construct(second,NULL,info,provider,NULL,false));
    CHECK(World_setWorldTime(second,-987)&&World_getWorldTime(c->world)==-987);
    CHECK(World_setSeaLevel(c->world,INT32_MIN)&&World_getSeaLevel(c->world)==INT32_MIN);
    MCObjectHeap_free(heap);
}
static void entity_lookup_guards(void) {
    MCObjectHeap *heap=MCObjectHeap_new(32u*1024u*1024u);CHECK(heap);
    World *world=World_nativeAllocate(heap,NULL,NULL);CHECK(world);
    CHECK(!World_getEntityByID(world,1)&&MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);

    heap=MCObjectHeap_new(32u*1024u*1024u);CHECK(heap);
    MCObjectHeap *foreign=MCObjectHeap_new(1024u*1024u);CHECK(foreign);
    world=World_nativeAllocate(heap,NULL,NULL);CHECK(world);
    world->entitiesById=IntHashMap_new(foreign);CHECK(world->entitiesById);
    CHECK(!World_getEntityByID(world,1)&&MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(foreign));
    MCObjectHeap_free(heap);MCObjectHeap_free(foreign);

    heap=MCObjectHeap_new(32u*1024u*1024u);CHECK(heap);
    world=World_nativeAllocate(heap,NULL,NULL);CHECK(world);
    WorldProvider *provider=WorldProvider_getProviderForDimension(heap,0,NULL,NULL);CHECK(provider);
    CHECK(World_construct(world,NULL,NULL,provider,NULL,false));
    CHECK(!World_getEntityByID(world,1)&&!MCObjectHeap_failed(heap));
    CHECK(IntHashMap_addKey(world->entitiesById,INT32_MIN,NULL));
    CHECK(!World_getEntityByID(world,INT32_MIN)&&!MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
static void border_lifetime(void) {
    MCObjectHeap *heap=MCObjectHeap_new(32u*1024u*1024u);CHECK(heap);
    WorldProvider *provider=WorldProvider_getProviderForDimension(heap,-1,NULL,NULL);CHECK(provider);
    World *world=World_nativeAllocate(heap,NULL,NULL);CHECK(world);
    CHECK(World_construct(world,NULL,NULL,provider,NULL,false));
    CHECK(WorldBorder_setCenter(world->worldBorder,80.0,-24.0));
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,heap,(MCObject *)world));
    MCObjectHeap *snapshot=MCObjectHeap_clone(heap);CHECK(snapshot);
    MCObjectRoot copied={0};CHECK(MCObjectRoot_rebind(&copied,snapshot,&root));
    World *copy=(World *)MCObjectRoot_get(&copied);CHECK(copy&&copy!=world);
    CHECK(copy->worldBorder&&copy->worldBorder!=world->worldBorder);
    CHECK(copy->worldBorder->object.heap==snapshot);
    CHECK(WorldBorder_getCenterX(copy->worldBorder)==10.0&&WorldBorder_getCenterZ(copy->worldBorder)==-3.0);
    CHECK(MCObjectHeap_collect(snapshot));
    CHECK(World_getWorldBorder(copy)==copy->worldBorder);
    CHECK(WorldBorder_getCenterX(copy->worldBorder)==10.0);
    CHECK(MCObjectHeap_adopt(heap,snapshot));MCObjectHeap_free(snapshot);
    world=(World *)MCObjectRoot_get(&root);CHECK(world&&world->worldBorder->object.heap==heap);
    CHECK(MCObjectHeap_collect(heap));
    CHECK(WorldBorder_getCenterZ(World_getWorldBorder(world))==-3.0);
    MCObjectRoot_drop(&root);MCObjectHeap_free(heap);
}
int main(void) {
    MCObjectHeap *heap=MCObjectHeap_new(32u*1024u*1024u);CHECK(heap);
    WorldProvider *provider=WorldProvider_getProviderForDimension(heap,0,NULL,NULL);CHECK(provider);
    World *world=World_nativeAllocate(heap,NULL,NULL);CHECK(world);
    CHECK(World_construct(world,NULL,NULL,provider,NULL,true));
    CHECK(world->seaLevel==63&&world->DIST_HASH_MAGIC==1013904223&&world->cloudColour==16777215);
    CHECK(world->loadedEntityList&&world->unloadedEntityList&&world->loadedTileEntityList&&world->tickableTileEntities);
    CHECK(world->addedTileEntityList&&world->tileEntitiesToBeRemoved&&world->playerEntities&&world->weatherEffects);
    CHECK(world->entitiesById&&world->worldAccesses&&world->worldScoreboard&&world->theCalendar&&world->activeChunkSet);
    CHECK(world->ambientTickCountdown>=0&&world->ambientTickCountdown<12000&&world->rand);
    CHECK(world->spawnHostileMobs&&world->spawnPeacefulMobs&&world->isRemote&&!world->worldInfo);
    CHECK(world->lightUpdateBlockList&&world->lightUpdateBlockList->length==32768&&world->lightUpdateBlockList->values[32767]==0);
    CHECK(world->provider==provider&&world->worldBorder&&!provider->worldObj&&!provider->worldChunkMgr&&!world->chunkProvider&&!world->mapStorage);
    CHECK(World_init(world)==world);MCObjectHeap_free(heap);
    border_lifetime();
    constructor_prefixes();info_owner();
    entity_lookup_guards();
    printf("Source World constructor: %u checks passed\n",checks);return 0;
}
