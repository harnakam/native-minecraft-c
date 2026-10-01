#ifndef C919_SOURCE_CHUNK_H
#define C919_SOURCE_CHUNK_H
#include "util/ClassInheritanceMultiMap.h"
#include "util/NativeConcurrentQueue.h"
#include "world/World.h"
#include "world/chunk/ChunkPrimer.h"
#include "world/chunk/storage/ExtendedBlockStorage.h"

typedef struct EnumSkyBlock EnumSkyBlock;
typedef struct Chunk Chunk;
/* Explicit native allocation and reached virtual-method dependencies. NULL
   entries inherit the translated body. Biome, tile, debug and exception
   classes are still required boundaries, rather than fabricated air results. */
typedef struct ChunkDependencies {
    NativeObjectArray *(*newStorageArrays)(MCObject *, Chunk *, int32_t);
    NativeByteArray *(*newBiomeArray)(MCObject *, Chunk *, int32_t);
    NativeIntArray *(*newPrecipitationHeightMap)(MCObject *, Chunk *, int32_t);
    NativeBooleanArray *(*newSkylightColumns)(MCObject *, Chunk *, int32_t);
    NativeHashMap *(*newTileEntityMap)(MCObject *, Chunk *);
    NativeConcurrentQueue *(*newTileEntityQueue)(MCObject *, Chunk *);
    NativeObjectArray *(*newEntityLists)(MCObject *, Chunk *, int32_t);
    NativeIntArray *(*newHeightMap)(MCObject *, Chunk *, int32_t);
    ClassInheritanceMultiMap *(*newEntityList)(MCObject *, Chunk *, NativeJavaClass *);
    ExtendedBlockStorage *(*newSection)(MCObject *, Chunk *, int32_t, bool);
    bool (*positionGetX)(MCObject *, BlockPos *, int32_t *);
    bool (*positionGetY)(MCObject *, BlockPos *, int32_t *);
    bool (*positionGetZ)(MCObject *, BlockPos *, int32_t *);
    WorldType *(*worldGetWorldType)(MCObject *, World *);
    bool (*providerGetHasNoSky)(MCObject *, WorldProvider *, bool *);
    bool (*getHeightValue)(MCObject *, Chunk *, int32_t, int32_t, int32_t *);
    bool (*getTopFilledSegment)(MCObject *, Chunk *, int32_t *);
    bool (*generateHeightMap)(MCObject *, Chunk *);
    NativeBlock *(*getBlock)(MCObject *, Chunk *, BlockPos *);
    bool (*canSeeSky)(MCObject *, Chunk *, BlockPos *, bool *);
    NativeBlockState *(*debugGetState)(MCObject *, Chunk *, int32_t, int32_t);
    NativeCharArray *(*sectionGetData)(MCObject *, ExtendedBlockStorage *);
    NibbleArray *(*sectionGetBlocklightArray)(MCObject *, ExtendedBlockStorage *);
    NibbleArray *(*sectionGetSkylightArray)(MCObject *, ExtendedBlockStorage *);
    NativeByteArray *(*nibbleGetData)(MCObject *, NibbleArray *);
    bool (*sectionRemoveInvalidBlocks)(MCObject *, ExtendedBlockStorage *);
    bool (*tileUpdateContainingBlockInfo)(MCObject *, MCObject *);
    MCObject *(*biomePlains)(MCObject *, Chunk *);
    MCObject *(*managerGetBiomeGenerator)(MCObject *, MCObject *, BlockPos *, MCObject *);
    bool (*biomeGetId)(MCObject *, MCObject *, int32_t *);
    MCObject *(*biomeById)(MCObject *, Chunk *, int32_t);
    bool (*warnArrayLength)(MCObject *, Chunk *, const char *, int32_t, int32_t);
    bool (*onChunkUnload)(MCObject *, Chunk *);
    bool (*worldMarkTileEntityForRemoval)(MCObject *, World *, MCObject *);
    bool (*worldUnloadEntities)(MCObject *, World *, MCObject *);
    /* Required runtime type fact for non-NULL values at Java's TileEntity
       checkcast. NULL passes the cast; full TileEntity classes are unported. */
    bool (*tileEntityIsInstance)(MCObject *, MCObject *, bool *);
} ChunkDependencies;
struct Chunk {
    MCObject object;
    /* All twenty-two original instance fields, in declaration order. */
    NativeObjectArray *storageArrays;
    NativeByteArray *blockBiomeArray;
    NativeIntArray *precipitationHeightMap;
    NativeBooleanArray *updateSkylightColumns;
    bool isChunkLoaded;
    World *worldObj;
    NativeIntArray *heightMap;
    int32_t xPosition, zPosition;
    bool isGapLightingUpdated;
    NativeHashMap *chunkTileEntityMap;
    NativeObjectArray *entityLists;
    bool isTerrainPopulated, isLightPopulated, field_150815_m, isModified, hasEntities;
    int64_t lastSaveTime;
    int32_t heightMapMinimum;
    int64_t inhabitedTime;
    int32_t queuedLightChecks;
    NativeConcurrentQueue *tileEntityPosQueue;
    /* Native dependencies are traced, and own no second block store. */
    const ChunkDependencies *dependencies;
    MCObject *dependencyContext;
    NativeBlockStateRuntime *nativeRuntime;
};
Chunk *Chunk_nativeAllocate(MCObjectHeap *, const ChunkDependencies *, MCObject *,
                            NativeBlockStateRuntime *);
bool Chunk_construct(Chunk *, World *, int32_t x, int32_t z);
bool Chunk_constructPrimer(Chunk *, World *, ChunkPrimer *, int32_t x, int32_t z);
Chunk *Chunk_new(MCObjectHeap *, World *, int32_t x, int32_t z, NativeBlockStateRuntime *);
bool Chunk_isInstance(const MCObject *);
void Chunk_traceFields(Chunk *, MCObjectVisitor, void *);
bool Chunk_isAtLocation(Chunk *, int32_t x, int32_t z);
int32_t Chunk_getHeight(Chunk *, BlockPos *);
int32_t Chunk_getHeightValue(Chunk *, int32_t x, int32_t z);
int32_t Chunk_getHeightValue_base(Chunk *, int32_t x, int32_t z);
int32_t Chunk_getTopFilledSegment(Chunk *);
int32_t Chunk_getTopFilledSegment_base(Chunk *);
NativeObjectArray *Chunk_getBlockStorageArray(Chunk *);
bool Chunk_generateHeightMap(Chunk *);
bool Chunk_generateHeightMap_base(Chunk *);
NativeBlock *Chunk_getBlock0(Chunk *, int32_t x, int32_t y, int32_t z);
NativeBlock *Chunk_getBlockXYZ(Chunk *, int32_t x, int32_t y, int32_t z);
NativeBlock *Chunk_getBlock(Chunk *, BlockPos *);
NativeBlock *Chunk_getBlock_base(Chunk *, BlockPos *);
NativeBlockState *Chunk_getBlockState(Chunk *, BlockPos *);
int32_t Chunk_getBlockMetadataXYZ(Chunk *, int32_t x, int32_t y, int32_t z);
int32_t Chunk_getBlockMetadata(Chunk *, BlockPos *);
int32_t Chunk_getBlockLightOpacity(Chunk *, BlockPos *);
int32_t Chunk_getLightFor(Chunk *, EnumSkyBlock *, BlockPos *);
int32_t Chunk_getLightSubtracted(Chunk *, BlockPos *, int32_t amount);
bool Chunk_canSeeSky(Chunk *, BlockPos *);
bool Chunk_canSeeSky_base(Chunk *, BlockPos *);
bool Chunk_fillChunk(Chunk *, NativeByteArray *, int32_t mask, bool full);
bool Chunk_onChunkUnload(Chunk *);
bool Chunk_onChunkUnload_base(Chunk *);
bool Chunk_setChunkModified(Chunk *);
MCObject *Chunk_getBiome(Chunk *, BlockPos *, MCObject *chunkManager);
NativeByteArray *Chunk_getBiomeArray(Chunk *);
bool Chunk_setStorageArrays(Chunk *, NativeObjectArray *);
bool Chunk_setBiomeArray(Chunk *, NativeByteArray *);
bool Chunk_setHeightMap(Chunk *, NativeIntArray *);
bool Chunk_resetRelightChecks(Chunk *);
bool Chunk_isLoaded(Chunk *);
bool Chunk_setChunkLoaded(Chunk *, bool);
World *Chunk_getWorld(Chunk *);
NativeIntArray *Chunk_getHeightMap(Chunk *);
NativeHashMap *Chunk_getTileEntityMap(Chunk *);
NativeObjectArray *Chunk_getEntityLists(Chunk *);
bool Chunk_isTerrainPopulated(Chunk *);
bool Chunk_setTerrainPopulated(Chunk *, bool);
bool Chunk_isLightPopulated(Chunk *);
bool Chunk_setLightPopulated(Chunk *, bool);
bool Chunk_setModified(Chunk *, bool);
bool Chunk_setHasEntities(Chunk *, bool);
bool Chunk_setLastSaveTime(Chunk *, int64_t);
int32_t Chunk_getLowestHeight(Chunk *);
int64_t Chunk_getInhabitedTime(Chunk *);
bool Chunk_setInhabitedTime(Chunk *, int64_t);
bool Chunk_isEmpty(Chunk *);
bool Chunk_getAreLevelsEmpty(Chunk *, int32_t startY, int32_t endY);
/* Lighting, block mutation, entity/tile lifecycle, save and generation methods
   not declared here remain unported. Throwable/CrashReport conversion is a
   native failing-heap boundary, not a translated Java exception graph. */
#endif
