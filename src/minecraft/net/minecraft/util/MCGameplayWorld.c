#include "util/MCGameplayWorld.h"
#include "world/storage/MapData.h"
#include "world/storage/SaveDataMemoryStorage.h"
#include "util/NativeWallClock.h"
#include "block/block.h"
#include <string.h>

static bool fail(MCObjectHeap *heap){MCObjectHeap_fail(heap);return false;}
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    if(MCObjectHeap_objectSize(object)<sizeof(MCGameplayWorld)){fail(object->heap);return;}
    MCGameplayWorld *world=(MCGameplayWorld *)object;
    World_traceFields(world,visitor,context);
    world->owners=(MCGameplayObjects *)visitor((MCObject *)world->owners,context);
    world->manager=(CraftingManager *)visitor((MCObject *)world->manager,context);
    world->furnace=(FurnaceRecipes *)visitor((MCObject *)world->furnace,context);
    world->statList=(StatList *)visitor((MCObject *)world->statList,context);
    world->itemDisplayContext=visitor(world->itemDisplayContext,context);
    world->nativeContext=visitor(world->nativeContext,context);
    world->emptyMapUseStat=(StatBase *)visitor((MCObject *)world->emptyMapUseStat,context);
    world->savedItemFields=(NBTTagCompound *)visitor((MCObject *)world->savedItemFields,context);
    world->savedItemRootName=(NBTString *)visitor((MCObject *)world->savedItemRootName,context);
    MapData_traceReferences(&world->maps,visitor,context);
    for(size_t i=0;i<MC_GAMEPLAY_CRAFT_STAT_COUNT;i++)world->craftStats[i]=(StatBase *)visitor((MCObject *)world->craftStats[i],context);
}
static void destroy(MCObject *object){if(MCObjectHeap_objectSize(object)>=sizeof(MCGameplayWorld))mc_maps_free(&((MCGameplayWorld *)object)->maps);}
static MCObject *clone(MCObjectHeap *destination,const MCObject *source) {
    if(!MCGameplayWorld_isInstance(source)){fail(destination);return NULL;}
    MCGameplayWorld *copy=(MCGameplayWorld *)MCObjectHeap_plainClone(destination,source);
    if(!copy)return NULL;
    mc_maps_init(&copy->maps);
    if(!mc_maps_copy(&copy->maps,&((const MCGameplayWorld *)source)->maps)){fail(destination);return NULL;}
    return (MCObject *)copy;
}
static const MCObjectClass klass={"C919.native.GameplayWorld",clone,trace,destroy};
bool MCGameplayWorld_isInstance(const MCObject *object){return object&&object->klass==&klass&&MCObjectHeap_objectSize(object)>=sizeof(MCGameplayWorld);}

/* Concrete native biome/generator dependency state. These are platform views,
   not Source BiomeGenBase or GenLayer/WorldChunkManager constructor ports. */
typedef struct {MCObject object;int32_t biomeID;float rainfall;} NativeBiome;
typedef struct {
    MCObject object;World *worldObj;WorldType *terrainType;NBTString *generatorOptions;
    NativeBiome *singleBiome;int64_t seed;float rainfall;
} NativeWorldChunkManager;
static const MCObjectClass biomeClass={"native.BiomeFacts",MCObjectHeap_plainClone,NULL,NULL};
static void manager_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    if(MCObjectHeap_objectSize(object)<sizeof(NativeWorldChunkManager)){fail(object->heap);return;}
    NativeWorldChunkManager *manager=(NativeWorldChunkManager *)object;
    manager->worldObj=(World *)visitor((MCObject *)manager->worldObj,context);
    manager->terrainType=(WorldType *)visitor((MCObject *)manager->terrainType,context);
    manager->generatorOptions=(NBTString *)visitor((MCObject *)manager->generatorOptions,context);
    manager->singleBiome=(NativeBiome *)visitor((MCObject *)manager->singleBiome,context);
}
static const MCObjectClass managerClass={"native.DenseWorldChunkManager",MCObjectHeap_plainClone,manager_trace,NULL};
static bool biome_matches(const MCObject *object,void *context){if(MCObjectHeap_objectSize(object)<sizeof(NativeBiome)){fail(object->heap);return false;}return ((const NativeBiome *)object)->biomeID==*(const int32_t *)context;}
static MCObject *biome(World *world,int32_t id,float rainfall) {
    NativeBiome *value=(NativeBiome *)MCObjectHeap_findObject(world->object.heap,&biomeClass,biome_matches,&id);
    if(!value){value=(NativeBiome *)MCObjectHeap_alloc(world->object.heap,sizeof(*value),&biomeClass);if(value){value->biomeID=id;value->rainfall=rainfall;}}
    return (MCObject *)value;
}
static WorldInfo *provider_info(MCObject *context,World *world){(void)context;return World_getWorldInfo(world);}
static WorldType *provider_terrain(MCObject *context,WorldInfo *info){(void)context;return WorldInfo_getTerrainType(info);}
static NBTString *provider_options(MCObject *context,WorldInfo *info){(void)context;return WorldInfo_getGeneratorOptions(info);}
static MCObject *plains(MCObject *context){return biome((World *)context,1,0.4f);}
static MCObject *hell(MCObject *context){return biome((World *)context,8,0.0f);}
static MCObject *sky(MCObject *context){return biome((World *)context,9,0.0f);}
static MCObject *new_manager(MCObject *context,World *world) {
    (void)context;NativeWorldChunkManager *manager=(NativeWorldChunkManager *)MCObjectHeap_alloc(world->object.heap,sizeof(*manager),&managerClass);
    if(!manager)return NULL;
    manager->worldObj=world;manager->seed=World_getSeed(world);
    WorldInfo *info=World_getWorldInfo(world);manager->terrainType=info?WorldInfo_getTerrainType(info):NULL;
    info=World_getWorldInfo(world);manager->generatorOptions=info?WorldInfo_getGeneratorOptions(info):NULL;
    return !MCObjectHeap_failed(world->object.heap)?(MCObject *)manager:NULL;
}
static MCObject *new_single_manager(MCObject *context,MCObject *biomeObject,float rainfall) {
    World *world=(World *)context;
    if(!biomeObject||biomeObject->heap!=context->heap||biomeObject->klass!=&biomeClass||MCObjectHeap_objectSize(biomeObject)<sizeof(NativeBiome)){fail(context->heap);return NULL;}
    NativeWorldChunkManager *manager=(NativeWorldChunkManager *)new_manager(context,world);
    if(manager){manager->singleBiome=(NativeBiome *)biomeObject;manager->rainfall=rainfall;}return (MCObject *)manager;
}
static const WorldBorderDependencies borderDependencies={.currentTimeMillis=NativeWallClock_currentTimeMillis};
static const WorldProviderDependencies providerDependencies={
    .getWorldInfo=provider_info,.getTerrainType=provider_terrain,.getGeneratorOptions=provider_options,
    .getPlainsBiome=plains,.getHellBiome=hell,.getSkyBiome=sky,
    .newWorldChunkManager=new_manager,.newWorldChunkManagerHell=new_single_manager,
    .borderDependencies=&borderDependencies
};

/* Managed captured-chunk views resolve their borrowed store by coordinates on
   each access, so native unload/array moves cannot leave stale chunk pointers. */
typedef struct {MCObject object;World *world;int32_t x,z;} NativeDenseChunk;
static void chunk_trace(MCObject *object,MCObjectVisitor visitor,void *context){if(MCObjectHeap_objectSize(object)<sizeof(NativeDenseChunk)){fail(object->heap);return;}NativeDenseChunk *chunk=(NativeDenseChunk *)object;chunk->world=(World *)visitor((MCObject *)chunk->world,context);}
static const MCObjectClass chunkClass={"native.DenseChunkView",MCObjectHeap_plainClone,chunk_trace,NULL};
static const mc_chunk *find_chunk(World *world,int32_t x,int32_t z) {
    if(!world->terrain){fail(world->object.heap);return NULL;}
    for(int i=0;i<world->terrain->count;i++)if(world->terrain->chunks[i].x==x&&world->terrain->chunks[i].z==z)return &world->terrain->chunks[i];
    return NULL;
}
static bool manager_ready(World *world) {
    MCObject *object=WorldProvider_getWorldChunkManager(world->provider);
    return (object&&object->klass==&managerClass&&object->heap==world->object.heap&&
        MCObjectHeap_objectSize(object)>=sizeof(NativeWorldChunkManager)&&
        ((NativeWorldChunkManager *)object)->worldObj==world)||fail(world->object.heap);
}
static bool loaded(MCObject *context,World *world,int32_t x,int32_t z,bool allowEmpty,bool *out) {
    (void)context;(void)allowEmpty;
    if(!out||!manager_ready(world))return fail(world->object.heap);
    const mc_chunk *chunk=find_chunk(world,x,z);if(MCObjectHeap_failed(world->object.heap))return false;
    *out=chunk!=NULL;return true;
}
static MCObject *get_chunk(MCObject *context,World *world,int32_t x,int32_t z) {
    (void)context;
    if(!manager_ready(world)||!find_chunk(world,x,z)){fail(world->object.heap);return NULL;}
    NativeDenseChunk *view=(NativeDenseChunk *)MCObjectHeap_alloc(world->object.heap,sizeof(*view),&chunkClass);
    if(view){view->world=world;view->x=x;view->z=z;}return (MCObject *)view;
}
static const mc_chunk *chunk_value(MCObject *object) {
    if(!object||object->klass!=&chunkClass||MCObjectHeap_objectSize(object)<sizeof(NativeDenseChunk)){fail(object?object->heap:NULL);return NULL;}
    NativeDenseChunk *view=(NativeDenseChunk *)object;
    if(!MCGameplayWorld_isInstance((MCObject *)view->world)||view->world->object.heap!=object->heap){fail(object->heap);return NULL;}
    const mc_chunk *chunk=find_chunk(view->world,view->x,view->z);
    if(!chunk||!chunk->blocks){fail(object->heap);return NULL;}return chunk;
}
static bool height(MCObject *context,MCObject *object,int32_t x,int32_t z,int32_t *out) {
    (void)context;const mc_chunk *chunk=chunk_value(object);
    if(!chunk||!out||x<0||x>=16||z<0||z>=16)return fail(object?object->heap:NULL);
    *out=chunk->heightMap[(z<<4)|x];return true;
}
static MCObject *block_chunk(MCObject *context,World *world,BlockPos *pos) {
    if(!BlockPos_isInstance((MCObject *)pos)||((MCObject *)pos)->heap!=world->object.heap){fail(world->object.heap);return NULL;}
    return get_chunk(context,world,mc_floor_div16(pos->x),mc_floor_div16(pos->z));
}
static bool top_segment(MCObject *context,MCObject *object,int32_t *out) {
    (void)context;const mc_chunk *chunk=chunk_value(object);if(!chunk||!out)return fail(object?object->heap:NULL);
    *out=0;for(int section=15;section>=0;section--)if(chunk->sectionMask&(1u<<section)){*out=section<<4;break;}return true;
}
typedef struct {MCObject object;uint16_t state;} NativeBlock;
typedef struct {MCObject object;bool movement,leaves;} NativeMaterial;
static const MCObjectClass blockClass={"native.RegisteredBlockView",MCObjectHeap_plainClone,NULL,NULL};
static const MCObjectClass materialClass={"native.MaterialPropertyView",MCObjectHeap_plainClone,NULL,NULL};
static MCObject *chunk_block(MCObject *context,MCObject *object,BlockPos *pos) {
    (void)context;const mc_chunk *chunk=chunk_value(object);
    if(!chunk||!BlockPos_isInstance((MCObject *)pos)||((MCObject *)pos)->heap!=object->heap){fail(object?object->heap:NULL);return NULL;}
    uint16_t state=pos->y<0||pos->y>=256?0:chunk->blocks[((size_t)pos->y<<8)|((size_t)(pos->z&15)<<4)|(size_t)(pos->x&15)];
    if(!mc_block_valid(state)){fail(object->heap);return NULL;}
    NativeBlock *block=(NativeBlock *)MCObjectHeap_alloc(object->heap,sizeof(*block),&blockClass);if(block)block->state=state;return (MCObject *)block;
}
static MCObject *material(MCObject *context,MCObject *object) {
    (void)context;
    if(!object||object->klass!=&blockClass||MCObjectHeap_objectSize(object)<sizeof(NativeBlock)){fail(object?object->heap:NULL);return NULL;}
    bool movement,leaves;
    if(!mc_block_material_flags(((NativeBlock *)object)->state,&movement,&leaves)){fail(object->heap);return NULL;}
    NativeMaterial *value=(NativeMaterial *)MCObjectHeap_alloc(object->heap,sizeof(*value),&materialClass);
    if(value){value->movement=movement;value->leaves=leaves;}return (MCObject *)value;
}
static bool movement(MCObject *context,MCObject *object,bool *out) {
    (void)context;
    if(!object||object->klass!=&materialClass||MCObjectHeap_objectSize(object)<sizeof(NativeMaterial)||!out)return fail(object?object->heap:NULL);
    *out=((NativeMaterial *)object)->movement;return true;
}
static bool leaves(MCObject *context,MCObject *object,bool *out) {
    (void)context;
    if(!object||object->klass!=&materialClass||MCObjectHeap_objectSize(object)<sizeof(NativeMaterial)||!out)return fail(object?object->heap:NULL);
    *out=((NativeMaterial *)object)->leaves;return true;
}
static const WorldDependencies worldDependencies={
    .isChunkLoaded=loaded,.getChunkFromChunkCoords=get_chunk,.chunkGetHeightValue=height,
    .getChunkFromBlockCoords=block_chunk,.chunkGetTopFilledSegment=top_segment,.chunkGetBlock=chunk_block,
    .blockGetMaterial=material,.materialBlocksMovement=movement,.materialIsLeaves=leaves
};
MCGameplayWorld *MCGameplayWorld_new(MCObjectHeap *heap,MCGameplayObjects *owners,const mc_world *terrain,CraftingManager *manager) {
    return MCGameplayWorld_newWithRandomRuntime(heap,owners,terrain,manager,NativeJavaRandomRuntime_process());
}
MCGameplayWorld *MCGameplayWorld_newWithRandomRuntime(MCObjectHeap *heap,MCGameplayObjects *owners,const mc_world *terrain,CraftingManager *manager,NativeJavaRandomRuntime *runtime) {
    return MCGameplayWorld_nativeNewDimension(heap,owners,terrain,manager,runtime,terrain?(int64_t)terrain->seed:0,0,false);
}
MCGameplayWorld *MCGameplayWorld_nativeNewDimension(MCObjectHeap *heap,MCGameplayObjects *owners,const mc_world *terrain,
    CraftingManager *manager,NativeJavaRandomRuntime *runtime,int64_t seed,int32_t dimension,bool client) {
    if(!heap||!owners||owners->object.heap!=heap||!runtime||(manager&&manager->object.heap!=heap)){fail(heap);return NULL;}
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    MCGameplayWorld *world=(MCGameplayWorld *)MCObjectHeap_alloc(heap,sizeof(*world),&klass);bool ok=world!=NULL;
    if(world) {
        world->owners=owners;world->terrain=terrain;world->manager=manager;world->randomRuntime=runtime;mc_maps_init(&world->maps);
        world->dependencies=&worldDependencies;world->dependencyContext=(MCObject *)world;
    }
    WorldTypeStatics *types=ok?WorldType_getStatics(heap):NULL;
    WorldSettings *settings=types?WorldSettings_new(heap,seed,&WorldSettingsGameType_SURVIVAL,true,false,types->DEFAULT):NULL;
    NBTString *name=settings?NBTString_literalASCII(heap,client?"MpServer":"C919"):NULL;
    WorldInfo *info=name?WorldInfo_new(heap,settings,name):NULL;
    WorldProvider *provider=info?WorldProvider_getProviderForDimension(heap,dimension,&providerDependencies,(MCObject *)world):NULL;
    ok=world&&provider&&World_construct(world,NULL,info,provider,NULL,client)&&WorldProvider_registerWorld(provider,world);
    if(ok) {
        world->mapStorage=client?(MapStorage *)SaveDataMemoryStorage_nativeNewCounterProvider(heap):
            MapStorage_nativeNewCounterProvider(heap,NULL,NULL,NULL);
        ok=world->mapStorage!=NULL;
    }
    if(!ok)fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap)?world:NULL;
}
bool MCGameplayWorld_isRemote(const MCObject *object) {
    if(!MCGameplayWorld_isInstance(object)){fail(object?object->heap:NULL);return false;}return ((const MCGameplayWorld *)object)->isRemote;
}
bool MCGameplayWorld_isCraftingTable(const MCObject *object,int32_t x,int32_t y,int32_t z) {
    if(!MCGameplayWorld_isInstance(object)){fail(object?object->heap:NULL);return false;}
    const MCGameplayWorld *world=(const MCGameplayWorld *)object;
    if(!world->terrain){fail(object->heap);return false;}return (mc_world_get(world->terrain,x,y,z)>>4)==58;
}
