#ifndef C919_SOURCE_WORLD_BASE_H
#define C919_SOURCE_WORLD_BASE_H
#include "world/WorldProvider.h"
#include "world/storage/WorldInfo.h"
#include "world/storage/MapStorage.h"
#include "scoreboard/Scoreboard.h"
#include "util/IntHashMap.h"
#include "util/NativeHashSet.h"
#include "util/NativePrimitiveArray.h"
#include "util/NativeJavaRandomRuntime.h"
#include "util/NativeCalendar.h"
#include "util/MCGameplay.h"
#include "item/crafting/CraftingManager.h"
#include "world/map.h"
typedef struct StatBase StatBase;
typedef struct StatList StatList;
typedef struct FurnaceRecipes FurnaceRecipes;
#define MC_GAMEPLAY_CRAFT_STAT_COUNT 2268u
/* Native/JDK allocation and virtual method boundaries. NULL override entries
   inherit the translated body. Unported chunk methods are required leaves and
   cannot return empty success. All contexts and returned references are traced. */
typedef struct WorldDependencies {
    NativeReferenceList *(*newArrayList)(MCObject *,World *);
    IntHashMap *(*newIntHashMap)(MCObject *,World *);
    NativeJavaRandom *(*newRandom)(MCObject *,World *);
    bool (*randomNextInt)(MCObject *,NativeJavaRandom *,int32_t *);
    bool (*randomNextIntBound)(MCObject *,NativeJavaRandom *,int32_t,int32_t *);
    NativeCalendar *(*newCalendar)(MCObject *,World *);
    Scoreboard *(*newScoreboard)(MCObject *,World *);
    NativeHashSet *(*newActiveChunkSet)(MCObject *,World *);
    NativeIntArray *(*newLightUpdateBlockList)(MCObject *,World *,int32_t);
    WorldBorder *(*providerGetWorldBorder)(MCObject *,WorldProvider *);
    WorldInfo *(*getWorldInfo)(MCObject *,World *);
    WorldBorder *(*getWorldBorder)(MCObject *,World *);
    bool (*getSeaLevel)(MCObject *,World *,int32_t *);
    BlockPos *(*getHeight)(MCObject *,World *,BlockPos *);
    bool (*positionGetX)(MCObject *,BlockPos *,int32_t *);
    bool (*positionGetY)(MCObject *,BlockPos *,int32_t *);
    bool (*positionGetZ)(MCObject *,BlockPos *,int32_t *);
    bool (*isChunkLoaded)(MCObject *,World *,int32_t,int32_t,bool,bool *);
    MCObject *(*getChunkFromChunkCoords)(MCObject *,World *,int32_t,int32_t);
    bool (*chunkGetHeightValue)(MCObject *,MCObject *chunk,int32_t,int32_t,int32_t *);
    MCObject *(*getChunkFromBlockCoords)(MCObject *,World *,BlockPos *);
    bool (*chunkGetTopFilledSegment)(MCObject *,MCObject *,int32_t *);
    MCObject *(*chunkGetBlock)(MCObject *,MCObject *,BlockPos *);
    MCObject *(*blockGetMaterial)(MCObject *,MCObject *);
    bool (*materialBlocksMovement)(MCObject *,MCObject *,bool *);
    bool (*materialIsLeaves)(MCObject *,MCObject *,bool *);
    /* Reached Source virtual sky/weather calls. NULL entries inherit the
       named base body; callback false is the native exception boundary. */
    bool (*getCelestialAngle)(MCObject *,World *,float,float *);
    bool (*getRainStrength)(MCObject *,World *,float,float *);
    bool (*getThunderStrength)(MCObject *,World *,float,float *);
    bool (*calculateSkylightSubtracted)(MCObject *,World *,float,int32_t *);
    bool (*calculateInitialSkylight)(MCObject *,World *);
    bool (*calculateInitialWeather)(MCObject *,World *);
    bool (*markTileEntityForRemoval)(MCObject *,World *,MCObject *tile);
    bool (*unloadEntities)(MCObject *,World *,MCObject *collection);
    /* Native JDK collection dispatch. Source captures its destination before
       evaluating Collection.toArray; these callbacks may mutate live fields.
       NULL inherits the named native dependency, unknown collections fail. */
    bool (*collectionAddAll)(MCObject *,NativeReferenceList *,MCObject *,bool *changed);
    NativeObjectArray *(*collectionToArray)(MCObject *,MCObject *collection);
} WorldDependencies;
struct World {
    MCObject object;
    /* All forty original World instance fields, in declaration order. */
    int32_t seaLevel;
    bool scheduledUpdatesAreImmediate;
    NativeReferenceList *loadedEntityList,*unloadedEntityList,*loadedTileEntityList,*tickableTileEntities;
    NativeReferenceList *addedTileEntityList,*tileEntitiesToBeRemoved,*playerEntities,*weatherEffects;
    IntHashMap *entitiesById;
    int64_t cloudColour;
    int32_t skylightSubtracted,updateLCG,DIST_HASH_MAGIC;
    float prevRainingStrength,rainingStrength,prevThunderingStrength,thunderingStrength;
    int32_t lastLightningBolt;
    NativeJavaRandom *rand;
    WorldProvider *provider;
    NativeReferenceList *worldAccesses;
    MCObject *chunkProvider,*saveHandler;
    WorldInfo *worldInfo;
    bool findingSpawnPoint;
    MapStorage *mapStorage;
    MCObject *villageCollectionObj,*theProfiler;
    NativeCalendar *theCalendar;
    Scoreboard *worldScoreboard;
    bool isRemote;
    NativeHashSet *activeChunkSet;
    int32_t ambientTickCountdown;
    bool spawnHostileMobs,spawnPeacefulMobs,processingLoadedTiles;
    WorldBorder *worldBorder;
    NativeIntArray *lightUpdateBlockList;
    /* Native allocation/dispatch, borrowed platform services and storage views.
       These are not additional authorities for Source fields above. */
    const WorldDependencies *dependencies;
    MCObject *dependencyContext;
    MCGameplayObjects *owners;
    const mc_world *terrain;
    mc_maps maps;
    CraftingManager *manager;
    FurnaceRecipes *furnace;
    StatList *statList;
    ItemStackDisplayNameDispatch itemDisplayName;
    MCObject *itemDisplayContext,*nativeContext;
    NBTTagCompound *savedItemFields;
    NBTString *savedItemRootName;
    StatBase *craftStats[MC_GAMEPLAY_CRAFT_STAT_COUNT],*emptyMapUseStat;
    int32_t nextEntityId;
    NativeJavaRandomRuntime *randomRuntime;
};
/* Allocation-only concrete native receiver for original abstract World. This
   does not claim either WorldServer or WorldClient constructors. */
World *World_nativeAllocate(MCObjectHeap *,const WorldDependencies *,MCObject *);
bool World_isInstance(const MCObject *);
void World_traceFields(World *,MCObjectVisitor,void *);
bool World_construct(World *,MCObject *saveHandler,WorldInfo *,WorldProvider *,MCObject *profiler,bool client);
World *World_init(World *);
WorldInfo *World_getWorldInfo(World *);
WorldType *World_getWorldType(World *);
WorldBorder *World_getWorldBorder(World *);
MapStorage *World_getMapStorage(World *);
Scoreboard *World_getScoreboard(World *);
int64_t World_getSeed(World *);
int64_t World_getWorldTime(World *);
int64_t World_getTotalWorldTime(World *);
int32_t World_getSeaLevel(World *);
bool World_setSeaLevel(World *,int32_t);
bool World_setWorldTime(World *,int64_t);
bool World_setTotalWorldTime(World *,int64_t);
GameRules *World_getGameRules(World *);
Entity *World_getEntityByID(World *,int32_t);
bool World_setSpawnPoint(World *,BlockPos *);
BlockPos *World_getSpawnPoint(World *);
BlockPos *World_getHeight(World *,BlockPos *);
BlockPos *World_getHeight_base(World *,BlockPos *);
BlockPos *World_getTopSolidOrLiquidBlock(World *,BlockPos *);
float World_getCelestialAngle(World *,float partialTicks);
float World_getCelestialAngle_base(World *,float partialTicks);
float World_getRainStrength(World *,float delta);
float World_getRainStrength_base(World *,float delta);
float World_getThunderStrength(World *,float delta);
float World_getThunderStrength_base(World *,float delta);
int32_t World_calculateSkylightSubtracted(World *,float partialTicks);
int32_t World_calculateSkylightSubtracted_base(World *,float partialTicks);
bool World_calculateInitialSkylight(World *);
bool World_calculateInitialSkylight_base(World *);
bool World_calculateInitialWeather(World *);
bool World_calculateInitialWeather_base(World *);
bool World_markTileEntityForRemoval(World *,MCObject *tile);
bool World_markTileEntityForRemoval_base(World *,MCObject *tile);
bool World_unloadEntities(World *,MCObject *collection);
bool World_unloadEntities_base(World *,MCObject *collection);
/* Remaining World methods, child constructors, chunk/storage/generation,
   ticking/weather/lighting/entity collision and native JDK leaves are pending. */
#endif
