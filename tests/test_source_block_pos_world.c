#include "entity/DataWatcher.h"
#include "client/multiplayer/ChunkProviderClient.h"
#include "util/BlockPosMutableBlockPos.h"
#include "world/border/WorldBorder.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
    fprintf(stderr,"position world check %u line %d: %s\n",checks,__LINE__,#expr); \
    exit(1); } } while (0)

static void set(BlockPosMutableBlockPos *pos,int32_t x,int32_t y,int32_t z) {
    BlockPosMutableBlockPos *out=NULL;
    CHECK(BlockPosMutableBlockPos_set(pos,x,y,z,&out)==NATIVE_ARRAY_OK && out==pos);
    CHECK(pos->blockPos.vec3i.x==0 && pos->blockPos.vec3i.y==0 && pos->blockPos.vec3i.z==0);
}

static void chunk_and_world(void) {
    MCObjectHeap *heap=MCObjectHeap_new(32u*1024u*1024u);
    CHECK(heap);
    World *world=World_nativeAllocate(heap,NULL,NULL);
    CHECK(world);
    world->provider=WorldProvider_nativeAllocate(heap,NULL,NULL);
    CHECK(world->provider && WorldProvider_construct(world->provider));
    world->worldInfo=WorldInfo_nativeAllocate(heap,NULL,NULL);
    CHECK(world->worldInfo && WorldInfo_construct(world->worldInfo));
    ChunkProviderClient *provider=ChunkProviderClient_new(heap,world,NULL,NULL);
    CHECK(provider);
    world->chunkProvider=(MCObject *)provider;
    Chunk *chunk=ChunkProviderClient_loadChunk(provider,-2,3);
    CHECK(chunk);
    NativeBlockStateRuntime *runtime=NativeBlockStateRuntime_get(heap);
    CHECK(runtime);
    ExtendedBlockStorage *section=ExtendedBlockStorage_new(heap,0,true,runtime);
    CHECK(section && NativeObjectArray_set(chunk->storageArrays,0,(MCObject *)section));
    NativeBlockState *stone=NativeBlockStateRuntime_state(runtime,16);
    NativeBlockState *dirt=NativeBlockStateRuntime_state(runtime,48);
    NativeBlockState *air=NativeBlock_getDefaultState(runtime->air);
    CHECK(stone && dirt && air);
    CHECK(ExtendedBlockStorage_set(section,3,5,4,stone));
    CHECK(ExtendedBlockStorage_set(section,7,9,12,dirt));
    BlockPosMutableBlockPos *mutable=BlockPosMutableBlockPos_newInt(heap,-29,5,52);
    CHECK(mutable);
    BlockPos *pos=&mutable->blockPos;
    BlockPos *snapshot=BlockPos_add(pos,1,0,0);
    CHECK(snapshot && snapshot!=pos && BlockPos_isRuntimeClass((MCObject *)snapshot));
    CHECK(World_getChunkFromBlockCoords(world,pos)==chunk);
    CHECK(ChunkProviderClient_provideChunkAt(provider,pos)==chunk);
    CHECK(Chunk_getBlockState(chunk,pos)==stone);
    set(mutable,-25,9,60);
    CHECK(World_getChunkFromBlockCoords(world,pos)==chunk);
    CHECK(Chunk_getBlockState(chunk,pos)==dirt);
    CHECK(Chunk_getBlock(chunk,pos)==dirt->block);
    CHECK(WorldInfo_setSpawn(world->worldInfo,pos));
    CHECK(world->worldInfo->spawnX==-25 && world->worldInfo->spawnY==9 && world->worldInfo->spawnZ==60);
    CHECK(snapshot->vec3i.x==-28 && snapshot->vec3i.y==5 && snapshot->vec3i.z==52);
    set(mutable,-25,-1,60);
    CHECK(Chunk_getBlockState(chunk,pos)==air);
    set(mutable,-25,256,60);
    CHECK(Chunk_getBlockState(chunk,pos)==air);
    set(mutable,1000,5,-1000);
    CHECK(World_getChunkFromBlockCoords(world,pos)==provider->blankChunk);
    CHECK(ChunkProviderClient_provideChunkAt(provider,pos)==provider->blankChunk);

    /* The actual position-key collection retains this mutable reference.
       A changed hash cannot locate its old entry; there is no hidden copy or
       automatic rehash. Restoring the original coordinates finds it again. */
    set(mutable,-29,5,52);
    CHECK(NativeHashMap_put(chunk->chunkTileEntityMap,(MCObject *)pos,(MCObject *)world));
    BlockPos *equal=BlockPos_newInt(heap,-29,5,52);
    CHECK(equal && NativeHashMap_get(chunk->chunkTileEntityMap,(MCObject *)equal)==(MCObject *)world);
    set(mutable,-25,9,60);
    CHECK(NativeHashMap_get(chunk->chunkTileEntityMap,(MCObject *)pos)==NULL);
    CHECK(NativeHashMap_get(chunk->chunkTileEntityMap,(MCObject *)equal)==NULL);
    CHECK(NativeHashMap_size(chunk->chunkTileEntityMap)==1);
    set(mutable,-29,5,52);
    CHECK(NativeHashMap_get(chunk->chunkTileEntityMap,(MCObject *)pos)==(MCObject *)world);
    CHECK(NativeHashMap_get(chunk->chunkTileEntityMap,(MCObject *)equal)==(MCObject *)world);

    NativeObjectArray *refs=NativeObjectArray_new(heap,3);
    CHECK(refs && NativeObjectArray_set(refs,0,(MCObject *)world) &&
          NativeObjectArray_set(refs,1,(MCObject *)pos) && NativeObjectArray_set(refs,2,(MCObject *)snapshot));
    MCObjectRoot root={0},copyRoot={0};
    CHECK(MCObjectRoot_init(&root,heap,(MCObject *)refs));
    CHECK(MCObjectHeap_collect(heap));
    MCObjectHeap *copy=MCObjectHeap_clone(heap);
    CHECK(copy && MCObjectRoot_rebind(&copyRoot,copy,&root));
    NativeObjectArray *copies=(NativeObjectArray *)MCObjectRoot_get(&copyRoot);
    World *copyWorld=(World *)copies->values[0];
    BlockPosMutableBlockPos *copyPos=(BlockPosMutableBlockPos *)copies->values[1];
    Chunk *copyChunk=World_getChunkFromBlockCoords(copyWorld,&copyPos->blockPos);
    CHECK(copyChunk && copyChunk!=chunk && copies->values[1]!=(MCObject *)pos);
    CHECK(NativeHashMap_get(copyChunk->chunkTileEntityMap,copies->values[1])==(MCObject *)copyWorld);
    CHECK(Chunk_getBlockState(copyChunk,&copyPos->blockPos)==NativeBlockStateRuntime_state(copyChunk->nativeRuntime,16));
    CHECK(MCObjectHeap_canAdopt(heap,copy) && MCObjectHeap_adopt(heap,copy));
    refs=(NativeObjectArray *)MCObjectRoot_get(&root);
    world=(World *)refs->values[0];
    mutable=(BlockPosMutableBlockPos *)refs->values[1];
    set(mutable,-25,9,60);
    CHECK(Chunk_getBlockState(World_getChunkFromBlockCoords(world,&mutable->blockPos),&mutable->blockPos)==
          NativeBlockStateRuntime_state(NativeBlockStateRuntime_get(heap),48));
    CHECK(((BlockPos *)refs->values[2])->vec3i.x==-28);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(copy);
    MCObjectHeap_free(heap);
}

static bool clock_value(MCObject *ctx,int64_t *out) {
    (void)ctx;
    *out=0;
    return true;
}
static const WorldBorderDependencies clockMethods={.currentTimeMillis=clock_value};
static void border(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024u*1024u);
    CHECK(heap);
    WorldBorder *border=WorldBorder_new(heap,&clockMethods,NULL);
    BlockPosMutableBlockPos *pos=BlockPosMutableBlockPos_newInt(heap,0,INT32_MIN,0);
    CHECK(border && pos && WorldBorder_setTransition(border,10));
    bool inside=false;
    CHECK(WorldBorder_containsBlockPos(border,&pos->blockPos,&inside) && inside);
    set(pos,6,0,0);
    CHECK(WorldBorder_containsBlockPos(border,&pos->blockPos,&inside) && !inside);
    set(pos,0,INT32_MAX,6);
    CHECK(WorldBorder_containsBlockPos(border,&pos->blockPos,&inside) && !inside);
    set(pos,-5,0,-5);
    CHECK(WorldBorder_containsBlockPos(border,&pos->blockPos,&inside) && inside);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

typedef struct { MCObject object; unsigned calls; } AllocationWitness;
static const MCObjectClass witnessClass={"test.PositionAllocationWitness",MCObjectHeap_plainClone,NULL,NULL};
static const MCObjectClass paddingClass={"test.PositionAllocationPadding",MCObjectHeap_plainClone,NULL,NULL};
static bool boundary_x(MCObject *ctx,BlockPos *pos,int32_t *out) {
    AllocationWitness *w=(AllocationWitness *)ctx;
    CHECK(pos && MCObjectHeap_hasBorrowers(ctx->heap));
    ++w->calls;
    *out=-30000001;
    return true;
}
static bool boundary_z(MCObject *ctx,BlockPos *pos,int32_t *out) {
    AllocationWitness *w=(AllocationWitness *)ctx;
    CHECK(pos && MCObjectHeap_hasBorrowers(ctx->heap));
    ++w->calls;
    *out=0;
    return true;
}
static MCObject *boundary_chunk(MCObject *ctx,World *world,BlockPos *pos) {
    AllocationWitness *w=(AllocationWitness *)ctx;
    CHECK(world && pos && MCObjectHeap_hasBorrowers(ctx->heap));
    ++w->calls;
    return ctx; /* Explicit native chunk identity leaf; no Chunk body reached. */
}
static bool boundary_top(MCObject *ctx,MCObject *chunk,int32_t *out) {
    AllocationWitness *w=(AllocationWitness *)ctx;
    CHECK(chunk==ctx && MCObjectHeap_hasBorrowers(ctx->heap));
    ++w->calls;
    *out=0;
    return true;
}
static const WorldDependencies allocationMethods={.positionGetX=boundary_x,.positionGetZ=boundary_z,
    .getChunkFromBlockCoords=boundary_chunk,.chunkGetTopFilledSegment=boundary_top};

static void new_precedes_constructor_arguments(void) {
    for(unsigned top=0;top<2;++top) {
        const size_t budget=65536;
        MCObjectHeap *heap=MCObjectHeap_new(budget);
        CHECK(heap);
        AllocationWitness *w=(AllocationWitness *)MCObjectHeap_alloc(heap,sizeof(*w),&witnessClass);
        World *world=World_nativeAllocate(heap,&allocationMethods,(MCObject *)w);
        BlockPos *pos=DataWatcher_blockPos(heap,0,0,0);
        CHECK(w && world && pos);
        size_t remaining=budget-MCObjectHeap_liveBytes(heap);
        CHECK(remaining>sizeof(BlockPos)+sizeof(MCObject));
        CHECK(MCObjectHeap_alloc(heap,remaining-sizeof(BlockPos)+1,&paddingClass));
        CHECK(!MCObjectHeap_failed(heap));
        CHECK(!(top?World_getTopSolidOrLiquidBlock(world,pos):World_getHeight_base(world,pos)));
        /* Height has already read the range-check X. Top has captured its
           chunk receiver. The failed NEW must precede all constructor args. */
        CHECK(w->calls==1 && MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
}

int main(void) {
    chunk_and_world();
    border();
    new_precedes_constructor_arguments();
    printf("Source BlockPos world: %u checks passed\n",checks);
    return 0;
}
