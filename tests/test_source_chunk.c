#include "entity/DataWatcher.h"
#include "client/entity/EntityPlayerSP.h"
#include "world/EnumSkyBlock.h"
#include "world/chunk/Chunk.h"
#include "world/chunk/EmptyChunk.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Chunk check %u line %d: %s\n", checks, __LINE__, #x);                 \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static MCObjectHeap *heap(void) {
    MCObjectHeap *h = MCObjectHeap_new(32u * 1024u * 1024u);
    CHECK(h);
    return h;
}
static World *world(MCObjectHeap *h, bool noSky) {
    World *w = World_nativeAllocate(h, NULL, NULL);
    CHECK(w);
    w->provider = WorldProvider_nativeAllocate(h, NULL, NULL);
    CHECK(w->provider && WorldProvider_construct(w->provider));
    w->provider->hasNoSky = noSky;
    w->worldInfo = WorldInfo_nativeAllocate(h, NULL, NULL);
    CHECK(w->worldInfo && WorldInfo_construct(w->worldInfo));
    return w;
}
static void constructor_and_fill(void) {
    MCObjectHeap *h = heap();
    Chunk *c = Chunk_new(h, NULL, -2, 7, NULL);
    CHECK(c);
    CHECK(c->worldObj == NULL && c->xPosition == -2 && c->zPosition == 7 &&
          c->queuedLightChecks == 4096);
    CHECK(c->nativeRuntime == NULL && c->storageArrays->length == 16 &&
          c->entityLists->length == 16);
    CHECK(c->heightMap->length == 256 && c->blockBiomeArray->length == 256 && !c->isModified);
    for (int i = 0; i < 256; i++)
        CHECK(c->precipitationHeightMap->values[i] == -999 && c->blockBiomeArray->values[i] == -1 &&
              c->heightMap->values[i] == 0);
    for (int i = 0; i < 16; i++) {
        ClassInheritanceMultiMap *m = (ClassInheritanceMultiMap *)c->entityLists->values[i];
        CHECK(m && m->values && ClassInheritanceMultiMap_size(m) == 0);
        CHECK(NativeHashMap_get(m->map, (MCObject *)m->baseClass) == (MCObject *)m->values);
        if (i)
            CHECK(c->entityLists->values[i - 1] != (MCObject *)m);
    }
    c->worldObj = world(h, false);
    NativeByteArray *bytes = NativeByteArray_new(h, 8192 + 2048 + 2048 + 256 + 3);
    CHECK(bytes);
    bytes->values[2 * ((5 << 8) | (4 << 4) | 3)] = 16;
    memset(bytes->values + 8192, 0x21, 2048);
    memset(bytes->values + 8192 + 2048, 0x43, 2048);
    for (int i = 0; i < 256; i++)
        bytes->values[12288 + i] = (int8_t)(i & 127);
    CHECK(Chunk_fillChunk(c, bytes, 1, true));
    ExtendedBlockStorage *s = (ExtendedBlockStorage *)c->storageArrays->values[0];
    CHECK(s);
    CHECK(s->blockRefCount == 1 && c->isTerrainPopulated && c->isLightPopulated && c->isModified);
    CHECK(Chunk_getHeightValue(c, 3, 4) == 6 && Chunk_getLowestHeight(c) == 6);
    CHECK(c->heightMap->values[0] == 0 && c->blockBiomeArray->values[255] == 127);
    BlockPos *pos = DataWatcher_blockPos(h, -29, 5, 116);
    CHECK(pos);
    CHECK(Chunk_getBlockState(c, pos) == NativeBlockStateRuntime_state(c->nativeRuntime, 16));
    EnumSkyBlockStatics *e = EnumSkyBlock_getStatics(h);
    CHECK(e);
    CHECK(Chunk_getLightFor(c, e->BLOCK, pos) == 2 && Chunk_getLightFor(c, e->SKY, pos) == 4);
    CHECK(Chunk_getLightSubtracted(c, pos, 1) == 3 && !Chunk_canSeeSky(c, pos));
    NativeCharArray *raw = s->data;
    NativeByteArray *oldBiome = c->blockBiomeArray;
    NativeByteArray *zero = NativeByteArray_new(h, 12288);
    CHECK(zero && Chunk_fillChunk(c, zero, 1, false));
    CHECK(c->storageArrays->values[0] == (MCObject *)s && s->data == raw &&
          c->blockBiomeArray == oldBiome);
    CHECK(s->blockRefCount == 0 && Chunk_getHeightValue(c, 3, 4) == 6 &&
          Chunk_getLowestHeight(c) == INT_MAX);
    NativeByteArray *biome = NativeByteArray_new(h, 256);
    CHECK(biome && Chunk_fillChunk(c, biome, 0, true));
    CHECK(c->storageArrays->values[0] == NULL && Chunk_getHeightValue(c, 3, 4) == 6);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
typedef struct Witness {
    MCObject object;
    Chunk *chunk;
    NativeObjectArray *oldArray, *replacement;
    unsigned calls, mode;
    MCObject *biomes[3];
    int32_t biomeId;
    MCObject *foreignContext;
} Witness;
static void trace_witness(MCObject *o, MCObjectVisitor v, void *ctx) {
    Witness *w = (Witness *)o;
    w->chunk = (Chunk *)v((MCObject *)w->chunk, ctx);
    w->oldArray = (NativeObjectArray *)v((MCObject *)w->oldArray, ctx);
    w->replacement = (NativeObjectArray *)v((MCObject *)w->replacement, ctx);
    for (unsigned i = 0; i < 3; i++)
        w->biomes[i] = v(w->biomes[i], ctx);
    w->foreignContext = v(w->foreignContext, ctx);
}
static const MCObjectClass witnessClass = {"test.ChunkWitness", MCObjectHeap_plainClone,
                                           trace_witness, NULL};
static bool get_y(MCObject *o, BlockPos *p, int32_t *out) {
    Witness *w = (Witness *)o;
    CHECK(MCObjectHeap_hasBorrowers(o->heap));
    if (++w->calls == 3)
        w->chunk->storageArrays = w->replacement;
    *out = p->vec3i.y;
    return true;
}
static const ChunkDependencies readDependencies = {.positionGetY = get_y};
static void captured_array(void) {
    MCObjectHeap *h = heap();
    Witness *w = (Witness *)MCObjectHeap_alloc(h, sizeof *w, &witnessClass);
    CHECK(w);
    w->chunk = Chunk_nativeAllocate(h, &readDependencies, (MCObject *)w, NULL);
    CHECK(w->chunk && Chunk_construct(w->chunk, world(h, false), 0, 0));
    NativeBlockStateRuntime *r = NativeBlockStateRuntime_get(h);
    CHECK(r);
    ExtendedBlockStorage *s = ExtendedBlockStorage_new(h, 0, true, r);
    CHECK(s && ExtendedBlockStorage_set(s, 0, 0, 0, NativeBlockStateRuntime_state(r, 16)));
    CHECK(NativeObjectArray_set(w->chunk->storageArrays, 0, (MCObject *)s));
    w->oldArray = w->chunk->storageArrays;
    w->replacement = NativeObjectArray_new(h, 16);
    CHECK(w->replacement);
    BlockPos *p = DataWatcher_blockPos(h, 0, 0, 0);
    CHECK(p);
    /* The third getY runs after Java captures storageArrays for the aaload. */
    CHECK(Chunk_getBlockState(w->chunk, p) == NativeBlockStateRuntime_state(r, 16));
    CHECK(w->calls == 4 && w->chunk->storageArrays == w->replacement &&
          w->oldArray->values[0] == (MCObject *)s);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static MCObject *biome_plains(MCObject *o, Chunk *c) {
    (void)c;
    Witness *w = (Witness *)o;
    ++w->calls;
    return w->biomes[0];
}
static MCObject *biome_generate(MCObject *o, MCObject *manager, BlockPos *p, MCObject *fallback) {
    Witness *w = (Witness *)o;
    CHECK(manager == w->biomes[0] && fallback == w->biomes[0] && p);
    ++w->calls;
    return w->biomes[2];
}
static bool biome_id(MCObject *o, MCObject *b, int32_t *out) {
    Witness *w = (Witness *)o;
    CHECK(b == w->biomes[2]);
    ++w->calls;
    *out = 257;
    return true;
}
static MCObject *biome_lookup(MCObject *o, Chunk *c, int32_t id) {
    (void)c;
    Witness *w = (Witness *)o;
    ++w->calls;
    w->biomeId = id;
    return id == 257 ? w->biomes[2] : id == 1 ? w->biomes[1] : NULL;
}
static const ChunkDependencies biomeDependencies = {.biomePlains = biome_plains,
                                                    .managerGetBiomeGenerator = biome_generate,
                                                    .biomeGetId = biome_id,
                                                    .biomeById = biome_lookup};
static void biome_cache(void) {
    MCObjectHeap *h = heap();
    Witness *w = (Witness *)MCObjectHeap_alloc(h, sizeof *w, &witnessClass);
    CHECK(w);
    w->chunk = Chunk_nativeAllocate(h, &biomeDependencies, (MCObject *)w, NULL);
    CHECK(w->chunk && Chunk_construct(w->chunk, NULL, 0, 0));
    for (int i = 0; i < 3; i++) {
        w->biomes[i] = (MCObject *)DataWatcher_blockPos(h, i, 0, 0);
        CHECK(w->biomes[i]);
    }
    BlockPos *p = DataWatcher_blockPos(h, 17, 5, -16);
    CHECK(p);
    CHECK(Chunk_getBiome(w->chunk, p, w->biomes[0]) == w->biomes[2] && w->biomeId == 257 &&
          w->calls == 4);
    CHECK(w->chunk->blockBiomeArray->values[1] == 1);
    CHECK(Chunk_getBiome(w->chunk, p, NULL) == w->biomes[1] && w->biomeId == 1 && w->calls == 5);
    w->chunk->blockBiomeArray->values[1] = 7;
    CHECK(Chunk_getBiome(w->chunk, p, NULL) == w->biomes[0] && w->biomeId == 7 && w->calls == 7);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static ClassInheritanceMultiMap *entity_factory(MCObject *o, Chunk *c, NativeJavaClass *clazz) {
    Witness *w = (Witness *)o;
    CHECK(MCObjectHeap_hasBorrowers(o->heap));
    ++w->calls;
    if (w->calls == 1) {
        w->oldArray = c->entityLists;
        c->entityLists = w->replacement;
    }
    return ClassInheritanceMultiMap_new(o->heap, clazz);
}
static const ChunkDependencies entityDependencies = {.newEntityList = entity_factory};
static void constructor_assignment_capture(void) {
    MCObjectHeap *h = heap();
    Witness *w = (Witness *)MCObjectHeap_alloc(h, sizeof *w, &witnessClass);
    CHECK(w);
    w->replacement = NativeObjectArray_new(h, 1);
    CHECK(w->replacement);
    w->chunk = Chunk_nativeAllocate(h, &entityDependencies, (MCObject *)w, NULL);
    CHECK(w->chunk);
    CHECK(Chunk_construct(w->chunk, NULL, 2, 3));
    CHECK(w->calls == 1 && w->oldArray->values[0] != NULL && w->oldArray->values[1] == NULL);
    CHECK(w->chunk->entityLists == w->replacement && w->replacement->values[0] == NULL);
    CHECK(w->chunk->precipitationHeightMap->values[255] == -999 && !MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static NativeObjectArray *swap_context(MCObject *o, Chunk *c, int32_t size) {
    Witness *w = (Witness *)o;
    ++w->calls;
    c->dependencyContext = w->foreignContext;
    return NativeObjectArray_new(o->heap, size);
}
static NativeByteArray *forbidden_foreign_call(MCObject *o, Chunk *c, int32_t size) {
    (void)c;
    Witness *w = (Witness *)o;
    ++w->calls;
    return NativeByteArray_new(o->heap, size);
}
static const ChunkDependencies foreignDependencies = {.newStorageArrays = swap_context,
                                                      .newBiomeArray = forbidden_foreign_call};
static NativeBlock *null_block(MCObject *o, Chunk *c, BlockPos *p) {
    (void)o;
    (void)c;
    (void)p;
    return NULL;
}
static const ChunkDependencies nullBlockDependencies = {.getBlock = null_block};
static void required_boundaries(void) {
    MCObjectHeap *h = heap(), *foreign = heap();
    Witness *w = (Witness *)MCObjectHeap_alloc(h, sizeof *w, &witnessClass),
            *f = (Witness *)MCObjectHeap_alloc(foreign, sizeof *f, &witnessClass);
    CHECK(w && f);
    w->foreignContext = (MCObject *)f;
    w->chunk = Chunk_nativeAllocate(h, &foreignDependencies, (MCObject *)w, NULL);
    CHECK(w->chunk);
    CHECK(!Chunk_construct(w->chunk, NULL, 0, 0) && MCObjectHeap_failed(h));
    CHECK(w->calls == 1 && f->calls == 0 && w->chunk->storageArrays != NULL &&
          w->chunk->blockBiomeArray == NULL);
    CHECK(!MCObjectHeap_failed(foreign));
    MCObjectHeap_free(h);
    MCObjectHeap_free(foreign);
    h = heap();
    Chunk *c = Chunk_nativeAllocate(h, &nullBlockDependencies, NULL, NULL);
    CHECK(c && Chunk_construct(c, NULL, 0, 0));
    CHECK(Chunk_getBlock(c, NULL) == NULL && !MCObjectHeap_failed(h));
    CHECK(Chunk_getBlockLightOpacity(c, NULL) == 0 && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h = heap();
    c = Chunk_new(h, world(h, false), 0, 0, NULL);
    CHECK(c);
    BlockPos *p = DataWatcher_blockPos(h, 0, -1, 0);
    CHECK(p);
    CHECK(Chunk_getBlockXYZ(c, 0, -1, 0) == NativeBlockStateRuntime_get(h)->air &&
          !MCObjectHeap_failed(h));
    CHECK(Chunk_getBlockMetadata(c, p) == 0 && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static const MCObjectClass tileClass = {"test.TileEntity", MCObjectHeap_plainClone, NULL, NULL};
static bool tile_type_fact(MCObject *context, MCObject *value, bool *out) {
    (void)context;
    *out = value && value->klass == &tileClass;
    return true;
}
static const ChunkDependencies tileDependencies = {.tileEntityIsInstance = tile_type_fact};
static bool record_tile_refresh(MCObject *context, MCObject *tile) {
    (void)tile;
    ++((Witness *)context)->calls;
    return true;
}
static const ChunkDependencies refreshDependencies = {
    .tileEntityIsInstance = tile_type_fact, .tileUpdateContainingBlockInfo = record_tile_refresh};
static void tile_cast_failure_order(void) {
    MCObjectHeap *h = heap();
    World *w = world(h, false);
    w->tileEntitiesToBeRemoved = NativeReferenceList_new(h);
    w->unloadedEntityList = NativeReferenceList_new(h);
    Chunk *c = Chunk_nativeAllocate(h, &tileDependencies, NULL, NULL);
    BlockPos *p = DataWatcher_blockPos(h, 0, 0, 0);
    CHECK(c && p && w->tileEntitiesToBeRemoved && w->unloadedEntityList &&
          Chunk_construct(c, w, 0, 0) && Chunk_setChunkLoaded(c, true));
    CHECK(NativeHashMap_put(c->chunkTileEntityMap, (MCObject *)p, (MCObject *)p));
    /* Iterator.next -> checkcast TileEntity precedes the World read/call. */
    CHECK(!Chunk_onChunkUnload(c) && MCObjectHeap_failed(h) && !c->isChunkLoaded);
    CHECK(w->tileEntitiesToBeRemoved->size == 0 && w->unloadedEntityList->size == 0);
    MCObjectHeap_free(h);
    h = heap();
    Witness *observer = (Witness *)MCObjectHeap_alloc(h, sizeof *observer, &witnessClass);
    c = Chunk_nativeAllocate(h, &refreshDependencies, (MCObject *)observer, NULL);
    p = DataWatcher_blockPos(h, 0, 0, 0);
    CHECK(observer && c && p && Chunk_construct(c, world(h, false), 0, 0));
    CHECK(NativeHashMap_put(c->chunkTileEntityMap, (MCObject *)p, (MCObject *)p));
    NativeByteArray *bytes = NativeByteArray_new(h, 0);
    CHECK(bytes && !Chunk_fillChunk(c, bytes, 0, false) && MCObjectHeap_failed(h));
    CHECK(c->isTerrainPopulated && c->isLightPopulated && c->isModified &&
          c->heightMapMinimum == INT_MAX && observer->calls == 0);
    MCObjectHeap_free(h);
}
static void real_unload_lifecycle(void) {
    MCObjectHeap *h = heap();
    World *w = world(h, false);
    w->tileEntitiesToBeRemoved = NativeReferenceList_new(h);
    w->unloadedEntityList = NativeReferenceList_new(h);
    CHECK(w->tileEntitiesToBeRemoved && w->unloadedEntityList);
    Chunk *c = Chunk_nativeAllocate(h, &tileDependencies, NULL, NULL);
    CHECK(c && Chunk_construct(c, w, 0, 0) && Chunk_setChunkLoaded(c, true));
    BlockPos *a = DataWatcher_blockPos(h, 0, 1, 0), *b = DataWatcher_blockPos(h, 0, 0, 1);
    MCObject *tile = MCObjectHeap_alloc(h, sizeof *tile, &tileClass);
    CHECK(a && b && tile);
    CHECK(NativeHashMap_put(c->chunkTileEntityMap, (MCObject *)a, tile) &&
          NativeHashMap_put(c->chunkTileEntityMap, (MCObject *)b, NULL));
    EntityPlayerSP *p = EntityPlayerSP_nativeAllocate(h), *q = EntityPlayerSP_nativeAllocate(h);
    CHECK(p && q);
    ClassInheritanceMultiMap *first = (ClassInheritanceMultiMap *)c->entityLists->values[0],
                             *last = (ClassInheritanceMultiMap *)c->entityLists->values[15];
    CHECK(ClassInheritanceMultiMap_add(first, (MCObject *)p) &&
          ClassInheritanceMultiMap_add(first, (MCObject *)p));
    CHECK(ClassInheritanceMultiMap_add(last, (MCObject *)q) &&
          ClassInheritanceMultiMap_add(last, (MCObject *)p));
    CHECK(Chunk_onChunkUnload(c) && !c->isChunkLoaded);
    CHECK(w->tileEntitiesToBeRemoved->size == 2 &&
          NativeReferenceList_get(w->tileEntitiesToBeRemoved, 0) == NULL &&
          NativeReferenceList_get(w->tileEntitiesToBeRemoved, 1) == tile);
    CHECK(w->unloadedEntityList->size == 4 &&
          NativeReferenceList_get(w->unloadedEntityList, 0) == (MCObject *)p &&
          NativeReferenceList_get(w->unloadedEntityList, 1) == (MCObject *)p &&
          NativeReferenceList_get(w->unloadedEntityList, 2) == (MCObject *)q &&
          NativeReferenceList_get(w->unloadedEntityList, 3) == (MCObject *)p);
    CHECK(first->values->size == 2 && last->values->size == 2 && !MCObjectHeap_failed(h));
    EmptyChunk *empty = EmptyChunk_new(h, w, 0, 0, NULL);
    CHECK(empty && Chunk_setChunkLoaded(&empty->super, true));
    CHECK(Chunk_onChunkUnload(&empty->super) && empty->super.isChunkLoaded &&
          w->unloadedEntityList->size == 4);
    CHECK(Chunk_setChunkModified(&empty->super) && !empty->super.isModified);
    MCObjectHeap_free(h);
    h = heap();
    w = world(h, false);
    w->tileEntitiesToBeRemoved = NativeReferenceList_new(h);
    CHECK(w->tileEntitiesToBeRemoved);
    c = Chunk_nativeAllocate(h, &tileDependencies, NULL, NULL);
    CHECK(c && Chunk_construct(c, w, 0, 0) && Chunk_setChunkLoaded(c, true));
    a = DataWatcher_blockPos(h, 0, 1, 0);
    tile = MCObjectHeap_alloc(h, sizeof *tile, &tileClass);
    CHECK(a && tile && NativeHashMap_put(c->chunkTileEntityMap, (MCObject *)a, tile));
    CHECK(!Chunk_onChunkUnload(c) && MCObjectHeap_failed(h) && !c->isChunkLoaded);
    CHECK(w->tileEntitiesToBeRemoved->size == 1 && w->unloadedEntityList == NULL);
    MCObjectHeap_free(h);
}
static void coordinate_map_order(void) {
    MCObjectHeap *h = heap();
    Chunk *c = Chunk_new(h, NULL, 0, 0, NULL);
    CHECK(c);
    BlockPos *y = DataWatcher_blockPos(h, 0, 1, 0), *z = DataWatcher_blockPos(h, 0, 0, 1);
    CHECK(y && z);
    CHECK(NativeHashMap_put(c->chunkTileEntityMap, (MCObject *)y, (MCObject *)y));
    CHECK(NativeHashMap_put(c->chunkTileEntityMap, (MCObject *)z, (MCObject *)z));
    NativeIterator *it = NativeIterator_fromView(NativeHashMap_values(c->chunkTileEntityMap));
    CHECK(it);
    MCObject *value = NULL;
    /* Vec3i hash: (y + z*31)*31 + x; bucket 1 precedes bucket 15. */
    CHECK(NativeIterator_next(it, &value) && value == (MCObject *)z);
    CHECK(NativeIterator_next(it, &value) && value == (MCObject *)y && !NativeIterator_hasNext(it));
    BlockPos *equal = DataWatcher_blockPos(h, 0, 1, 0);
    CHECK(equal);
    CHECK(NativeHashMap_get(c->chunkTileEntityMap, (MCObject *)equal) == (MCObject *)y);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void partial_failure(void) {
    MCObjectHeap *h = heap();
    Chunk *c = Chunk_new(h, world(h, false), 0, 0, NULL);
    CHECK(c);
    NativeByteArray *bytes = NativeByteArray_new(h, 3);
    CHECK(bytes);
    bytes->values[0] = 16;
    bytes->values[2] = 32;
    CHECK(!Chunk_fillChunk(c, bytes, 1, true) && MCObjectHeap_failed(h));
    ExtendedBlockStorage *s = (ExtendedBlockStorage *)c->storageArrays->values[0];
    CHECK(s && s->data->values[0] == 16 && s->data->values[1] == 0);
    CHECK(!c->isLightPopulated && !c->isTerrainPopulated && !c->isModified &&
          s->blockRefCount == 0);
    CHECK(c->nativeRuntime == NULL && s->nativeRuntime == NULL);
    MCObjectHeap_free(h);
}
static void empty_inherited_storage(void) {
    MCObjectHeap *h = heap();
    EmptyChunk *e = EmptyChunk_new(h, world(h, false), 0, 0, NULL);
    CHECK(e);
    Chunk *c = &e->super;
    NativeByteArray *bytes = NativeByteArray_new(h, 12288 + 256);
    CHECK(bytes);
    bytes->values[0] = 16;
    CHECK(Chunk_fillChunk(c, bytes, 1, true));
    BlockPos *p = DataWatcher_blockPos(h, 0, 0, 0);
    CHECK(p);
    CHECK(Chunk_isInstance((MCObject *)e) && Chunk_isEmpty(c) && c->isTerrainPopulated &&
          c->isLightPopulated && !c->isModified);
    NativeBlockStateRuntime *r = NativeBlockStateRuntime_get(h);
    CHECK(r);
    CHECK(Chunk_getBlockState(c, p) == NativeBlockStateRuntime_state(r, 16));
    CHECK(Chunk_getBlock(c, NULL) == r->air && Chunk_getHeightValue(c, INT_MIN, INT_MAX) == 0);
    CHECK(Chunk_getBlockMetadata(c, NULL) == 0 && Chunk_getBlockLightOpacity(c, NULL) == 255 &&
          !Chunk_canSeeSky(c, NULL));
    CHECK(Chunk_getBlock_base(c, p) == NativeBlockStateRuntime_state(r, 16)->block);
    CHECK(Chunk_generateHeightMap_base(c) && c->isModified && c->heightMap->values[0] == 1);
    CHECK(Chunk_getHeightValue(c, 0, 0) == 0 && Chunk_getHeightValue_base(c, 0, 0) == 1);
    EnumSkyBlockStatics *k = EnumSkyBlock_getStatics(h);
    CHECK(k);
    CHECK(Chunk_getLightFor(c, k->SKY, NULL) == 15 &&
          Chunk_getLightSubtracted(c, NULL, INT_MIN) == 0);
    CHECK(Chunk_getAreLevelsEmpty(c, INT_MIN, INT_MAX));
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void no_sky_and_copy_prefix(void) {
    MCObjectHeap *h = heap();
    Chunk *c = Chunk_new(h, world(h, true), 0, 0, NULL);
    CHECK(c);
    EnumSkyBlockStatics *k = EnumSkyBlock_getStatics(h);
    BlockPos *p = DataWatcher_blockPos(h, 0, 0, 0);
    CHECK(k && p);
    CHECK(Chunk_getLightFor(c, k->SKY, p) == 15 && Chunk_getLightSubtracted(c, p, 0) == 0);
    NativeByteArray *bytes = NativeByteArray_new(h, 8192 + 2048 + 256);
    CHECK(bytes);
    bytes->values[0] = 16;
    bytes->values[8192] = 0x76;
    bytes->values[10240] = 7;
    CHECK(Chunk_fillChunk(c, bytes, 1, true));
    ExtendedBlockStorage *s = (ExtendedBlockStorage *)c->storageArrays->values[0];
    CHECK(s && s->skylightArray == NULL);
    CHECK(Chunk_getLightFor(c, k->SKY, p) == 0 && Chunk_getLightFor(c, k->BLOCK, p) == 6 &&
          c->blockBiomeArray->values[0] == 7);
    CHECK(Chunk_getLightSubtracted(c, p, INT_MAX) == 6);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h = heap();
    c = Chunk_new(h, world(h, false), 0, 0, NULL);
    CHECK(c);
    bytes = NativeByteArray_new(h, 8192 + 19);
    CHECK(bytes);
    bytes->values[0] = 16;
    memset(bytes->values + 8192, 42, 19);
    CHECK(!Chunk_fillChunk(c, bytes, 1, false) && MCObjectHeap_failed(h));
    s = (ExtendedBlockStorage *)c->storageArrays->values[0];
    CHECK(s && s->data->values[0] == 16);
    for (int i = 0; i < 2048; i++)
        CHECK(s->blocklightArray->data->values[i] == 0);
    CHECK(!c->isTerrainPopulated && !c->isLightPopulated && s->blockRefCount == 0);
    MCObjectHeap_free(h);
}
static void graph_aliases(void) {
    MCObjectHeap *h = heap();
    Chunk *c = Chunk_new(h, world(h, false), -1, 9, NULL);
    CHECK(c);
    NativeBlockStateRuntime *r = NativeBlockStateRuntime_get(h);
    CHECK(r);
    c->nativeRuntime = r;
    ExtendedBlockStorage *s = ExtendedBlockStorage_new(h, 48, true, r);
    CHECK(s);
    CHECK(NativeObjectArray_set(c->storageArrays, 0, (MCObject *)s) &&
          NativeObjectArray_set(c->storageArrays, 3, (MCObject *)s));
    CHECK(Chunk_getTopFilledSegment(c) == 48);
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, h, (MCObject *)c));
    CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *copy = MCObjectHeap_clone(h);
    CHECK(copy);
    MCObjectRoot cloned = {0};
    CHECK(MCObjectRoot_rebind(&cloned, copy, &root));
    Chunk *cc = (Chunk *)MCObjectRoot_get(&cloned);
    CHECK(cc && cc != c);
    ExtendedBlockStorage *cs = (ExtendedBlockStorage *)cc->storageArrays->values[0];
    CHECK(cs && cs != s && cc->storageArrays->values[3] == (MCObject *)cs);
    CHECK(cc->nativeRuntime != r && cs->nativeRuntime == cc->nativeRuntime &&
          cc->entityLists->values[0] != c->entityLists->values[0]);
    ClassInheritanceMultiMap *m = (ClassInheritanceMultiMap *)cc->entityLists->values[0];
    CHECK(NativeHashMap_get(m->map, (MCObject *)m->baseClass) == (MCObject *)m->values);
    CHECK(NativeCharArray_set(cs->data, 0, 16) && s->data->values[0] == 0);
    CHECK(MCObjectHeap_adopt(h, copy));
    c = (Chunk *)MCObjectRoot_get(&root);
    CHECK(c && c->storageArrays->values[0] == c->storageArrays->values[3]);
    CHECK(((ExtendedBlockStorage *)c->storageArrays->values[0])->data->values[0] == 16);
    CHECK(MCObjectHeap_collect(h) && !MCObjectHeap_failed(h));
    MCObjectRoot_drop(&root);
    MCObjectHeap_free(copy);
    MCObjectHeap_free(h);
}
int main(void) {
    constructor_and_fill();
    captured_array();
    coordinate_map_order();
    partial_failure();
    empty_inherited_storage();
    no_sky_and_copy_prefix();
    graph_aliases();
    biome_cache();
    constructor_assignment_capture();
    required_boundaries();
    tile_cast_failure_order();
    real_unload_lifecycle();
    printf("Chunk: %u checks passed\n", checks);
    return 0;
}
