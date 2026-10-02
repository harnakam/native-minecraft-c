#include "world/World.h"
#include "util/NativeCollection.h"
#include "world/ChunkCoordIntPair.h"
#include <string.h>
#include <math.h>
#include "util/MathHelper.h"

/* Exact native concrete descriptor lives with its allocation/storage adapter. */
extern bool MCGameplayWorld_isInstance(const MCObject *);
static bool fail(World *world){MCObjectHeap_fail(world?world->object.heap:NULL);return false;}
static int32_t signed_bits(uint32_t bits){int32_t out;memcpy(&out,&bits,sizeof(out));return out;}
static bool valid(World *world) {
    return (World_isInstance((MCObject *)world)&&!MCObjectHeap_failed(world->object.heap)&&
        (!world->dependencyContext||world->dependencyContext->heap==world->object.heap))||fail(world);
}
static bool pin(World *world,MCObjectRootScope *scope,MCObject *object) {
    return (!object||object->heap==world->object.heap)&&MCObjectRootScope_pin(scope,object)&&!MCObjectHeap_failed(world->object.heap);
}
static bool begin(World *world,MCObjectRootScope *scope) {
    if(!valid(world)||!MCObjectRootScope_begin(scope,world->object.heap))return fail(world);
    if(pin(world,scope,(MCObject *)world)&&pin(world,scope,world->dependencyContext))return true;
    MCObjectRootScope_end(scope);return fail(world);
}
static bool end(World *world,MCObjectRootScope *scope,bool ok) {
    ok=ok&&!MCObjectHeap_failed(world->object.heap);if(!ok)fail(world);
    MCObjectRootScope_end(scope);return ok;
}
void World_traceFields(World *world,MCObjectVisitor visitor,void *context) {
    if(MCObjectHeap_objectSize((MCObject *)world)<sizeof(*world)){fail(world);return;}
#define TRACE(field,type) world->field=(type)visitor((MCObject *)world->field,context)
    TRACE(loadedEntityList,NativeReferenceList *);TRACE(unloadedEntityList,NativeReferenceList *);
    TRACE(loadedTileEntityList,NativeReferenceList *);TRACE(tickableTileEntities,NativeReferenceList *);
    TRACE(addedTileEntityList,NativeReferenceList *);TRACE(tileEntitiesToBeRemoved,NativeReferenceList *);
    TRACE(playerEntities,NativeReferenceList *);TRACE(weatherEffects,NativeReferenceList *);
    TRACE(entitiesById,IntHashMap *);TRACE(rand,NativeJavaRandom *);TRACE(provider,WorldProvider *);
    TRACE(worldAccesses,NativeReferenceList *);TRACE(chunkProvider,MCObject *);TRACE(saveHandler,MCObject *);
    TRACE(worldInfo,WorldInfo *);TRACE(mapStorage,MapStorage *);TRACE(villageCollectionObj,MCObject *);
    TRACE(theProfiler,MCObject *);TRACE(theCalendar,NativeCalendar *);TRACE(worldScoreboard,Scoreboard *);
    TRACE(activeChunkSet,NativeHashSet *);TRACE(worldBorder,WorldBorder *);TRACE(lightUpdateBlockList,NativeIntArray *);
    TRACE(dependencyContext,MCObject *);
#undef TRACE
}
static void trace(MCObject *object,MCObjectVisitor visitor,void *context){World_traceFields((World *)object,visitor,context);}
static const MCObjectClass klass={"native.AbstractWorldReceiver",MCObjectHeap_plainClone,trace,NULL};
bool World_isInstance(const MCObject *object) {
    return object&&MCObjectHeap_objectSize(object)>=sizeof(World)&&
        (object->klass==&klass||MCGameplayWorld_isInstance(object));
}
World *World_nativeAllocate(MCObjectHeap *heap,const WorldDependencies *deps,MCObject *context) {
    if(context&&context->heap!=heap){MCObjectHeap_fail(heap);return NULL;}
    World *world=(World *)MCObjectHeap_alloc(heap,sizeof(*world),&klass);
    if(world){world->dependencies=deps;world->dependencyContext=context;}return world;
}
static bool chunk_hash(MCObject *context,MCObject *key,int32_t *out) {
    (void)context;
    if(!ChunkCoordIntPair_isInstance(key)||!out){MCObjectHeap_fail(key?key->heap:NULL);return false;}
    *out=ChunkCoordIntPair_hashCode((ChunkCoordIntPair *)key);return !MCObjectHeap_failed(key->heap);
}
static bool chunk_equals(MCObject *context,MCObject *query,MCObject *stored,bool *out) {
    (void)context;
    if(!ChunkCoordIntPair_isInstance(query)||!out){MCObjectHeap_fail(query?query->heap:NULL);return false;}
    *out=ChunkCoordIntPair_equals((ChunkCoordIntPair *)query,stored);return !MCObjectHeap_failed(query->heap);
}
static const NativeHashKeyMethods chunkKeys={chunk_hash,chunk_equals};
static NativeJavaRandom *new_random(World *world) {
    const WorldDependencies *d=world->dependencies;
    return d&&d->newRandom?d->newRandom(world->dependencyContext,world):
        NativeJavaRandomRuntime_newRandom(world->randomRuntime?world->randomRuntime:NativeJavaRandomRuntime_process(),world->object.heap);
}
static bool next_int(World *world,NativeJavaRandom *random,int32_t *out) {
    const WorldDependencies *d=world->dependencies;
    return d&&d->randomNextInt?d->randomNextInt(world->dependencyContext,random,out):NativeJavaRandom_nextInt(random,out);
}
static bool next_bound(World *world,NativeJavaRandom *random,int32_t bound,int32_t *out) {
    const WorldDependencies *d=world->dependencies;
    return d&&d->randomNextIntBound?d->randomNextIntBound(world->dependencyContext,random,bound,out):NativeJavaRandom_nextIntBound(random,bound,out);
}
bool World_construct(World *world,MCObject *save,WorldInfo *info,WorldProvider *provider,MCObject *profiler,bool client) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return false;
    bool ok=false;MCObjectHeap *heap=world->object.heap;const WorldDependencies *d=world->dependencies;
    if(!pin(world,&scope,save)||!pin(world,&scope,(MCObject *)info)||!pin(world,&scope,(MCObject *)provider)||!pin(world,&scope,profiler))goto done;
    world->seaLevel=63;MCObjectHeap_touch(heap);
#define NEW_LIST(field) do { \
    NativeReferenceList *list=d&&d->newArrayList?d->newArrayList(world->dependencyContext,world):NativeReferenceList_new(heap); \
    if(!list||!pin(world,&scope,(MCObject *)list)||!NativeReferenceList_isInstance((MCObject *)list))goto done; \
    world->field=list;MCObjectHeap_touch(heap); \
} while(0)
    NEW_LIST(loadedEntityList);NEW_LIST(unloadedEntityList);NEW_LIST(loadedTileEntityList);NEW_LIST(tickableTileEntities);
    NEW_LIST(addedTileEntityList);NEW_LIST(tileEntitiesToBeRemoved);NEW_LIST(playerEntities);NEW_LIST(weatherEffects);
#undef NEW_LIST
    IntHashMap *index=d&&d->newIntHashMap?d->newIntHashMap(world->dependencyContext,world):IntHashMap_new(heap);
    if(!index||!pin(world,&scope,(MCObject *)index)||!IntHashMap_isInstance((MCObject *)index))goto done;
    world->entitiesById=index;world->cloudColour=16777215;MCObjectHeap_touch(heap);
    NativeJavaRandom *temporary=new_random(world);
    if(!temporary||!pin(world,&scope,(MCObject *)temporary)||!NativeJavaRandom_isInstance((MCObject *)temporary))goto done;
    int32_t draw;if(!next_int(world,temporary,&draw)||MCObjectHeap_failed(heap))goto done;
    world->updateLCG=draw;world->DIST_HASH_MAGIC=1013904223;MCObjectHeap_touch(heap);
    NativeJavaRandom *random=new_random(world);
    if(!random||!pin(world,&scope,(MCObject *)random)||!NativeJavaRandom_isInstance((MCObject *)random))goto done;
    world->rand=random;MCObjectHeap_touch(heap);
    NativeReferenceList *accesses=d&&d->newArrayList?d->newArrayList(world->dependencyContext,world):NativeReferenceList_new(heap);
    if(!accesses||!pin(world,&scope,(MCObject *)accesses)||!NativeReferenceList_isInstance((MCObject *)accesses))goto done;
    world->worldAccesses=accesses;MCObjectHeap_touch(heap);
    NativeCalendar *calendar=d&&d->newCalendar?d->newCalendar(world->dependencyContext,world):NativeCalendar_getInstance(heap);
    if(!calendar||!pin(world,&scope,(MCObject *)calendar)||!NativeCalendar_isInstance((MCObject *)calendar))goto done;
    world->theCalendar=calendar;MCObjectHeap_touch(heap);
    Scoreboard *board=d&&d->newScoreboard?d->newScoreboard(world->dependencyContext,world):Scoreboard_new(heap);
    if(!board||!pin(world,&scope,(MCObject *)board)||!Scoreboard_isInstance((MCObject *)board))goto done;
    world->worldScoreboard=board;MCObjectHeap_touch(heap);
    NativeHashSet *set=d&&d->newActiveChunkSet?d->newActiveChunkSet(world->dependencyContext,world):NativeHashSet_newWithKeys(heap,&chunkKeys,NULL);
    if(!set||!pin(world,&scope,(MCObject *)set)||!NativeHashSet_isInstance((MCObject *)set))goto done;
    world->activeChunkSet=set;MCObjectHeap_touch(heap);
    /* Read rand again after intervening virtual allocations. Default fields
       are allocation-zeroed, never cleared after a callback's earlier write. */
    random=world->rand;if(!random||!pin(world,&scope,(MCObject *)random)||!next_bound(world,random,12000,&draw)||MCObjectHeap_failed(heap))goto done;
    world->ambientTickCountdown=draw;world->spawnHostileMobs=true;world->spawnPeacefulMobs=true;MCObjectHeap_touch(heap);
    NativeIntArray *light=d&&d->newLightUpdateBlockList?d->newLightUpdateBlockList(world->dependencyContext,world,32768):NativeIntArray_new(heap,32768);
    if(!light||!pin(world,&scope,(MCObject *)light)||!NativeIntArray_isInstance((MCObject *)light)||light->length!=32768)goto done;
    world->lightUpdateBlockList=light;world->saveHandler=save;world->theProfiler=profiler;world->worldInfo=info;
    world->provider=provider;world->isRemote=client;MCObjectHeap_touch(heap);
    if(!provider)goto done;
    WorldBorder *border=d&&d->providerGetWorldBorder?d->providerGetWorldBorder(world->dependencyContext,provider):WorldProvider_getWorldBorder(provider);
    if(!pin(world,&scope,(MCObject *)border)||(border&&!WorldBorder_isInstance((MCObject *)border)))goto done;
    world->worldBorder=border;MCObjectHeap_touch(heap);ok=true;
done:return end(world,&scope,ok);
}
World *World_init(World *world){return valid(world)?world:NULL;}
WorldInfo *World_getWorldInfo(World *world) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return NULL;
    const WorldDependencies *d=world->dependencies;
    WorldInfo *info=d&&d->getWorldInfo?d->getWorldInfo(world->dependencyContext,world):world->worldInfo;
    bool ok=pin(world,&scope,(MCObject *)info)&&(!info||WorldInfo_isInstance((MCObject *)info));
    if(!end(world,&scope,ok))info=NULL;
    return info;
}
WorldBorder *World_getWorldBorder(World *world) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return NULL;
    const WorldDependencies *d=world->dependencies;
    WorldBorder *border=d&&d->getWorldBorder?d->getWorldBorder(world->dependencyContext,world):world->worldBorder;
    bool ok=pin(world,&scope,(MCObject *)border)&&(!border||WorldBorder_isInstance((MCObject *)border));
    if(!end(world,&scope,ok))border=NULL;
    return border;
}
MapStorage *World_getMapStorage(World *world){return valid(world)?world->mapStorage:NULL;}
Scoreboard *World_getScoreboard(World *world){return valid(world)?world->worldScoreboard:NULL;}
int32_t World_getSeaLevel(World *world) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return 0;
    int32_t out=world->seaLevel;const WorldDependencies *d=world->dependencies;
    bool ok=!(d&&d->getSeaLevel)||d->getSeaLevel(world->dependencyContext,world,&out);
    return end(world,&scope,ok)?out:0;
}
bool World_setSeaLevel(World *world,int32_t level){if(!valid(world))return false;world->seaLevel=level;MCObjectHeap_touch(world->object.heap);return true;}
static WorldInfo *field_info(World *world) {
    WorldInfo *info=world->worldInfo;
    if(!WorldInfo_isInstance((MCObject *)info)||((MCObject *)info)->heap!=world->object.heap){fail(world);return NULL;}return info;
}
int64_t World_getSeed(World *world){WorldInfo *info=valid(world)?field_info(world):NULL;return info?WorldInfo_getSeed(info):0;}
int64_t World_getWorldTime(World *world){WorldInfo *info=valid(world)?field_info(world):NULL;return info?WorldInfo_getWorldTime(info):0;}
int64_t World_getTotalWorldTime(World *world){WorldInfo *info=valid(world)?field_info(world):NULL;return info?WorldInfo_getWorldTotalTime(info):0;}
bool World_setWorldTime(World *world,int64_t time){WorldInfo *info=valid(world)?field_info(world):NULL;return info&&WorldInfo_setWorldTime(info,time);}
bool World_setTotalWorldTime(World *world,int64_t time){WorldInfo *info=valid(world)?field_info(world):NULL;return info&&WorldInfo_setWorldTotalTime(info,time);}
GameRules *World_getGameRules(World *world){WorldInfo *info=valid(world)?field_info(world):NULL;return info?WorldInfo_getGameRulesInstance(info):NULL;}
WorldType *World_getWorldType(World *world){
    MCObjectRootScope scope={0};if(!begin(world,&scope))return NULL;
    WorldInfo *info=field_info(world);WorldType *out=NULL;
    bool ok=info&&pin(world,&scope,(MCObject *)info);
    if(ok){out=WorldInfo_getTerrainType(info);ok=pin(world,&scope,(MCObject *)out);}
    return end(world,&scope,ok)?out:NULL;
}
Entity *World_getEntityByID(World *world,int32_t id) {
    if(!valid(world))return NULL;
    if(!IntHashMap_isInstance((MCObject *)world->entitiesById)||world->entitiesById->object.heap!=world->object.heap){fail(world);return NULL;}
    MCObject *entity=IntHashMap_lookup(world->entitiesById,id);
    if(entity&&(!Entity_isInstance(entity)||entity->heap!=world->object.heap)){fail(world);return NULL;}
    return (Entity *)entity;
}
bool World_setSpawnPoint(World *world,BlockPos *pos){WorldInfo *info=valid(world)?field_info(world):NULL;return info&&WorldInfo_setSpawn(info,pos);}
static bool coordinate(World *world,BlockPos *pos,int axis,int32_t *out) {
    const WorldDependencies *d=world->dependencies;
    if(!pos||((MCObject *)pos)->heap!=world->object.heap)return fail(world);
    bool (*method)(MCObject *,BlockPos *,int32_t *)=d?(axis==0?d->positionGetX:axis==1?d->positionGetY:d->positionGetZ):NULL;
    if(method)return (method(world->dependencyContext,pos,out)&&!MCObjectHeap_failed(world->object.heap))||fail(world);
    if(!BlockPos_isInstance((MCObject *)pos))return fail(world);
    NativeArrayResult r=axis==0?Vec3i_getX(&pos->vec3i,out):axis==1?Vec3i_getY(&pos->vec3i,out):Vec3i_getZ(&pos->vec3i,out);
    return r==NATIVE_ARRAY_OK||fail(world);
}
static int32_t shift4(int32_t value){uint32_t bits=(uint32_t)value;return signed_bits((bits>>4)|(value<0?UINT32_C(0xf0000000):0));}
BlockPos *World_getHeight_base(World *world,BlockPos *pos) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return NULL;
    bool ok=false;BlockPos *result=NULL;int32_t x,z,height=0;const WorldDependencies *d=world->dependencies;
    if(!pin(world,&scope,(MCObject *)pos))goto done;
    if(!coordinate(world,pos,0,&x))goto done;
    if(x< -30000000)goto outside;
    if(!coordinate(world,pos,2,&z))goto done;
    if(z< -30000000)goto outside;
    if(!coordinate(world,pos,0,&x))goto done;
    if(x>=30000000)goto outside;
    if(!coordinate(world,pos,2,&z))goto done;
    if(z>=30000000)goto outside;
    if(!coordinate(world,pos,0,&x)||!coordinate(world,pos,2,&z))goto done;
    bool loaded;
    if(!d||!d->isChunkLoaded||!d->isChunkLoaded(world->dependencyContext,world,shift4(x),shift4(z),true,&loaded)||MCObjectHeap_failed(world->object.heap))goto done;
    if(loaded) {
        if(!coordinate(world,pos,0,&x)||!coordinate(world,pos,2,&z)||!d->getChunkFromChunkCoords)goto done;
        MCObject *chunk=d->getChunkFromChunkCoords(world->dependencyContext,world,shift4(x),shift4(z));
        if(!pin(world,&scope,chunk))goto done;
        /* Invocation arguments are evaluated before the null receiver throws. */
        if(!coordinate(world,pos,0,&x)||!coordinate(world,pos,2,&z))goto done;
        if(!chunk||!d->chunkGetHeightValue||!d->chunkGetHeightValue(world->dependencyContext,chunk,x&15,z&15,&height)||MCObjectHeap_failed(world->object.heap))goto done;
    }
    goto make;
outside:
    height=World_getSeaLevel(world);if(MCObjectHeap_failed(world->object.heap))goto done;
    height=signed_bits((uint32_t)height+1u);
make:
    result=NativeBlockPos_allocate(world->object.heap);
    if(!result||!pin(world,&scope,(MCObject *)result))goto done;
    if(!coordinate(world,pos,0,&x)||!coordinate(world,pos,2,&z))goto done;
    ok=NativeBlockPos_constructCoordinates(result,x,height,z);
done:if(!end(world,&scope,ok))result=NULL;return result;
}
BlockPos *World_getHeight(World *world,BlockPos *pos) {
    if(!valid(world))return NULL;
    const WorldDependencies *d=world->dependencies;
    if(!d||!d->getHeight)return World_getHeight_base(world,pos);
    MCObjectRootScope scope={0};if(!begin(world,&scope))return NULL;
    BlockPos *result=NULL;
    if(pin(world,&scope,(MCObject *)pos))result=d->getHeight(world->dependencyContext,world,pos);
    bool ok=pin(world,&scope,(MCObject *)result)&&(!result||BlockPos_isInstance((MCObject *)result));
    if(!end(world,&scope,ok))result=NULL;
    return result;
}
BlockPos *World_getSpawnPoint(World *world) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return NULL;
    BlockPos *result=NULL;bool ok=false;int32_t x,y,z;
    result=NativeBlockPos_allocate(world->object.heap);
    if(!result||!pin(world,&scope,(MCObject *)result))goto done;
    WorldInfo *info=field_info(world);if(!info)goto done;
    x=WorldInfo_getSpawnX(info);if(MCObjectHeap_failed(world->object.heap))goto done;
    info=field_info(world);if(!info)goto done;
    y=WorldInfo_getSpawnY(info);if(MCObjectHeap_failed(world->object.heap))goto done;
    info=field_info(world);if(!info)goto done;
    z=WorldInfo_getSpawnZ(info);if(MCObjectHeap_failed(world->object.heap))goto done;
    if(!NativeBlockPos_constructCoordinates(result,x,y,z))goto done;
    WorldBorder *border=World_getWorldBorder(world);bool contains;
    if(!border||!WorldBorder_containsBlockPos(border,result,&contains))goto done;
    if(!contains) {
        BlockPos *center=NativeBlockPos_allocate(world->object.heap);
        if(!center||!pin(world,&scope,(MCObject *)center))goto done;
        border=World_getWorldBorder(world);if(!border)goto done;
        double centerX=WorldBorder_getCenterX(border);if(MCObjectHeap_failed(world->object.heap))goto done;
        border=World_getWorldBorder(world);if(!border)goto done;
        double centerZ=WorldBorder_getCenterZ(border);if(MCObjectHeap_failed(world->object.heap))goto done;
        if(BlockPos_constructDouble(center,centerX,0.0,centerZ)!=NATIVE_ARRAY_OK)goto done;
        result=World_getHeight(world,center);if(MCObjectHeap_failed(world->object.heap))goto done;
    }
    ok=true;
done:if(!end(world,&scope,ok))result=NULL;return result;
}
BlockPos *World_getTopSolidOrLiquidBlock(World *world,BlockPos *pos) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return NULL;
    BlockPos *result=NULL;bool ok=false;int32_t x,z,top;const WorldDependencies *d=world->dependencies;
    if(!pin(world,&scope,(MCObject *)pos)||!pos||!d||!d->getChunkFromBlockCoords)goto done;
    MCObject *chunk=d->getChunkFromBlockCoords(world->dependencyContext,world,pos);
    if(!pin(world,&scope,chunk))goto done;
    result=NativeBlockPos_allocate(world->object.heap);
    if(!result||!pin(world,&scope,(MCObject *)result))goto done;
    if(!coordinate(world,pos,0,&x)||!chunk||!d->chunkGetTopFilledSegment||
       !d->chunkGetTopFilledSegment(world->dependencyContext,chunk,&top)||MCObjectHeap_failed(world->object.heap))goto done;
    if(!coordinate(world,pos,2,&z))goto done;
    if(!NativeBlockPos_constructCoordinates(result,x,signed_bits((uint32_t)top+16u),z))goto done;
    for(;;) {
        int32_t y;
        if(Vec3i_getY(&result->vec3i,&y)!=NATIVE_ARRAY_OK)goto done;
        if(y<0)break;
        BlockPos *below=BlockPos_down(result);if(!below||!d->chunkGetBlock)goto done;
        MCObject *block=d->chunkGetBlock(world->dependencyContext,chunk,below);
        if(!block||!pin(world,&scope,block)||!d->blockGetMaterial)goto done;
        MCObject *material=d->blockGetMaterial(world->dependencyContext,block);
        if(!material||!pin(world,&scope,material)||!d->materialBlocksMovement)goto done;
        bool movement,leaves;
        if(!d->materialBlocksMovement(world->dependencyContext,material,&movement)||MCObjectHeap_failed(world->object.heap))goto done;
        if(movement) {
            if(!d->materialIsLeaves||
               !d->materialIsLeaves(world->dependencyContext,material,&leaves)||MCObjectHeap_failed(world->object.heap))goto done;
            if(!leaves)break;
        }
        result=below;
    }
    ok=true;
done:if(!end(world,&scope,ok))result=NULL;return result;
}

/* Source virtual dispatch is immutable; all transient receivers remain pinned
   through their argument evaluation and invocation. */
typedef bool (*WorldFloatMethod)(MCObject *,World *,float,float *);
static float float_virtual(World *world,float argument,WorldFloatMethod method,float (*base)(World *,float)) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return NAN;
    float result=NAN;bool ok=true;
    if(method)ok=method(world->dependencyContext,world,argument,&result);
    else result=base(world,argument);
    if(!end(world,&scope,ok))result=NAN;
    return result;
}
float World_getCelestialAngle_base(World *world,float partial) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return NAN;
    bool ok=false;float result=NAN;
    WorldProvider *receiver=world->provider;
    if(!pin(world,&scope,(MCObject *)receiver))goto done;
    WorldInfo *info=field_info(world);
    if(!info||!pin(world,&scope,(MCObject *)info))goto done;
    int64_t time=WorldInfo_getWorldTime(info);
    if(MCObjectHeap_failed(world->object.heap)||!receiver)goto done;
    result=WorldProvider_calculateCelestialAngle(receiver,time,partial);ok=true;
done:if(!end(world,&scope,ok))result=NAN;return result;
}
float World_getCelestialAngle(World *world,float partial) {
    if(!valid(world))return NAN;
    const WorldDependencies *d=world->dependencies;
    return float_virtual(world,partial,d?d->getCelestialAngle:NULL,World_getCelestialAngle_base);
}
float World_getRainStrength_base(World *world,float delta) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return NAN;
    float previous=world->prevRainingStrength;
    volatile float difference=world->rainingStrength-world->prevRainingStrength;
    volatile float weighted=difference*delta;
    float result=previous+weighted;
    if(!end(world,&scope,true))result=NAN;
    return result;
}
float World_getRainStrength(World *world,float delta) {
    if(!valid(world))return NAN;
    const WorldDependencies *d=world->dependencies;
    return float_virtual(world,delta,d?d->getRainStrength:NULL,World_getRainStrength_base);
}
float World_getThunderStrength_base(World *world,float delta) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return NAN;
    float previous=world->prevThunderingStrength;
    volatile float difference=world->thunderingStrength-world->prevThunderingStrength;
    volatile float weighted=difference*delta;
    volatile float interpolated=previous+weighted;
    float rain=World_getRainStrength(world,delta);
    volatile float result=interpolated*rain;
    if(!end(world,&scope,true))result=NAN;
    return result;
}
float World_getThunderStrength(World *world,float delta) {
    if(!valid(world))return NAN;
    const WorldDependencies *d=world->dependencies;
    return float_virtual(world,delta,d?d->getThunderStrength:NULL,World_getThunderStrength_base);
}
static int32_t java_float_to_int(float value) {
    if(isnan(value))return 0;
    if(value>=2147483648.0f)return INT32_MAX;
    if(value<=-2147483648.0f)return INT32_MIN;
    return (int32_t)value;
}
int32_t World_calculateSkylightSubtracted_base(World *world,float partial) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return 0;
    int32_t result=0;bool ok=false;
    float angle=World_getCelestialAngle(world,partial);
    if(MCObjectHeap_failed(world->object.heap))goto done;
    volatile float radians=angle*3.1415927f;
    volatile float circle=radians*2.0f;
    float cosine=MathHelper_cos(circle);
    volatile float twice=cosine*2.0f;
    volatile float raised=twice+0.5f;
    float amount=1.0f-raised;
    amount=MathHelper_clamp_float(amount,0.0f,1.0f);
    amount=1.0f-amount;
    float rain=World_getRainStrength(world,partial);
    if(MCObjectHeap_failed(world->object.heap))goto done;
    volatile float rainFive=rain*5.0f;
    volatile double rainDivision=(double)rainFive/16.0;
    volatile double rainFactor=1.0-rainDivision;
    volatile double rained=(double)amount*rainFactor;
    amount=(float)rained;
    float thunder=World_getThunderStrength(world,partial);
    if(MCObjectHeap_failed(world->object.heap))goto done;
    volatile float thunderFive=thunder*5.0f;
    volatile double thunderDivision=(double)thunderFive/16.0;
    volatile double thunderFactor=1.0-thunderDivision;
    volatile double thundered=(double)amount*thunderFactor;
    amount=(float)thundered;
    amount=1.0f-amount;
    volatile float scaled=amount*11.0f;
    result=java_float_to_int(scaled);ok=true;
done:if(!end(world,&scope,ok))result=0;
    return result;
}
int32_t World_calculateSkylightSubtracted(World *world,float partial) {
    if(!valid(world))return 0;
    const WorldDependencies *d=world->dependencies;
    if(!d||!d->calculateSkylightSubtracted)return World_calculateSkylightSubtracted_base(world,partial);
    MCObjectRootScope scope={0};if(!begin(world,&scope))return 0;
    int32_t result=0;bool ok=d->calculateSkylightSubtracted(world->dependencyContext,world,partial,&result);
    if(!end(world,&scope,ok))result=0;
    return result;
}
bool World_calculateInitialSkylight_base(World *world) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return false;
    int32_t amount=World_calculateSkylightSubtracted(world,1.0f);
    bool ok=!MCObjectHeap_failed(world->object.heap);
    if(ok&&amount!=world->skylightSubtracted){world->skylightSubtracted=amount;MCObjectHeap_touch(world->object.heap);}
    return end(world,&scope,ok);
}
bool World_calculateInitialSkylight(World *world) {
    if(!valid(world))return false;
    const WorldDependencies *d=world->dependencies;
    if(!d||!d->calculateInitialSkylight)return World_calculateInitialSkylight_base(world);
    MCObjectRootScope scope={0};if(!begin(world,&scope))return false;
    bool ok=d->calculateInitialSkylight(world->dependencyContext,world);
    return end(world,&scope,ok);
}
bool World_calculateInitialWeather_base(World *world) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return false;
    bool ok=false;WorldInfo *info=field_info(world);
    if(!info||!pin(world,&scope,(MCObject *)info))goto done;
    bool raining=WorldInfo_isRaining(info);
    if(MCObjectHeap_failed(world->object.heap))goto done;
    if(raining) {
        world->rainingStrength=1.0f;MCObjectHeap_touch(world->object.heap);
        info=field_info(world);
        if(!info||!pin(world,&scope,(MCObject *)info))goto done;
        bool thundering=WorldInfo_isThundering(info);
        if(MCObjectHeap_failed(world->object.heap))goto done;
        if(thundering){world->thunderingStrength=1.0f;MCObjectHeap_touch(world->object.heap);}
    }
    ok=true;
done:return end(world,&scope,ok);
}
bool World_calculateInitialWeather(World *world) {
    if(!valid(world))return false;
    const WorldDependencies *d=world->dependencies;
    if(!d||!d->calculateInitialWeather)return World_calculateInitialWeather_base(world);
    MCObjectRootScope scope={0};if(!begin(world,&scope))return false;
    bool ok=d->calculateInitialWeather(world->dependencyContext,world);
    return end(world,&scope,ok);
}
bool World_markTileEntityForRemoval_base(World *world,MCObject *tile) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return false;
    NativeReferenceList *destination=world->tileEntitiesToBeRemoved;
    bool ok=NativeReferenceList_isInstance((MCObject *)destination)&&
        pin(world,&scope,(MCObject *)destination)&&pin(world,&scope,tile)&&
        NativeReferenceList_add(destination,tile);
    return end(world,&scope,ok);
}
bool World_markTileEntityForRemoval(World *world,MCObject *tile) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return false;
    bool ok=false;
    if(!pin(world,&scope,tile)||!pin(world,&scope,world->dependencyContext))goto done;
    const WorldDependencies *d=world->dependencies;
    ok=d&&d->markTileEntityForRemoval?
        d->markTileEntityForRemoval(world->dependencyContext,world,tile):
        World_markTileEntityForRemoval_base(world,tile);
done:return end(world,&scope,ok);
}
bool World_unloadEntities_base(World *world,MCObject *collection) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return false;
    bool ok=false,changed=false;
    /* Java evaluates the list receiver before the variable argument. A NULL
       list fails at addAll invocation, before Collection.toArray is reached. */
    NativeReferenceList *destination=world->unloadedEntityList;
    if(!NativeReferenceList_isInstance((MCObject *)destination)||
       !pin(world,&scope,(MCObject *)destination)||
       !pin(world,&scope,collection)||!pin(world,&scope,world->dependencyContext))goto done;
    const WorldDependencies *d=world->dependencies;
    if(d&&d->collectionAddAll) {
        ok=d->collectionAddAll(world->dependencyContext,destination,collection,&changed);
    } else {
        if(!collection)goto done;
        NativeObjectArray *array=d&&d->collectionToArray?
            d->collectionToArray(world->dependencyContext,collection):
            NativeCollection_toArray(collection);
        if(!NativeObjectArray_isInstance((MCObject *)array)||
           !pin(world,&scope,(MCObject *)array))goto done;
        ok=NativeReferenceList_addAllArray(destination,array,&changed);
    }
done:return end(world,&scope,ok);
}
bool World_unloadEntities(World *world,MCObject *collection) {
    MCObjectRootScope scope={0};if(!begin(world,&scope))return false;
    bool ok=false;
    if(!pin(world,&scope,collection)||!pin(world,&scope,world->dependencyContext))goto done;
    const WorldDependencies *d=world->dependencies;
    ok=d&&d->unloadEntities?
        d->unloadEntities(world->dependencyContext,world,collection):
        World_unloadEntities_base(world,collection);
done:return end(world,&scope,ok);
}
