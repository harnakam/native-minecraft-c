#include "item/ItemMapData.h"
#include "item/ItemMapLoad.h"
#include "util/MCGameplayWorld.h"
#include "world/WorldDataStorage.h"
#include "world/storage/SaveDataMemoryStorage.h"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "source map load check %u failed line %d: %s\n", checks, __LINE__,     \
                    #x);                                                                           \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

/* Concrete abstract-World allocation with only the reached saved-data field
   attached. This tests the real cache method, not a fabricated full World ctor. */
#ifndef SOURCE_MAP_LOAD_PRIOR_RED
static World *memory_world(MCObjectHeap *heap) {
    World *world = World_nativeAllocate(heap, NULL, NULL);
    SaveDataMemoryStorage *memory = SaveDataMemoryStorage_new(heap);
    CHECK(world && memory);
    world->mapStorage = &memory->base;
    world->isRemote = true;
    MCObjectHeap_touch(heap);
    return world;
}
#endif
static WorldSavedDataResult load(int32_t id, World *world, MapData **out) {
#ifdef SOURCE_MAP_LOAD_PRIOR_RED
    ItemStack *stack = ItemStack_new(world->object.heap, ItemStack_registryItem(358), 1, id);
    CHECK(stack);
    /* The existing real native path consults its value store, even when the
       Source saved-data provider has the actual MapData. */
    mc_map_info *prior = NativeItemMapData_getMapData(stack, world);
    if (MCObjectHeap_failed(world->object.heap))
        return WORLD_SAVED_DATA_FAILURE;
    *out = (MapData *)(void *)prior;
    return WORLD_SAVED_DATA_OK;
#else
    return ItemMap_loadMapData(id, world, out);
#endif
}
static void real_memory_cache_alias(void) {
#ifdef SOURCE_MAP_LOAD_PRIOR_RED
    MCGameplay game = {0};
    CHECK(MCGameplay_init(&game, 1024 * 1024));
    MCObjectHeap *heap = game.heap;
    World *world = MCGameplayWorld_nativeNewDimension(
        heap, MCGameplay_get(&game), NULL, NULL, NativeJavaRandomRuntime_process(), 919, 0, true);
    CHECK(world && MCGameplay_setWorld(&game, (MCObject *)world));
#else
    MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
    World *world = memory_world(heap);
#endif
    NBTString *key = NBTString_fromASCII(heap, "map_7"),
              *name = NBTString_fromASCII(heap, "retained map name");
    MapData *stored = MapData_new(heap, name, NULL, NULL);
    CHECK(key && name && stored && World_setItemData(world, key, &stored->base));
    stored->colors->values[123] = -1;
    stored->scale = 4;
    MapData *result = NULL;
    CHECK(load(7, world, &result) == WORLD_SAVED_DATA_OK);
    CHECK(result == stored);
    CHECK(result->base.mapName == name && result->colors->values[123] == -1 && result->scale == 4);
    CHECK(NativeHashMap_size(world->mapStorage->loadedDataMap) == 1 && world->maps.count == 0);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
#ifdef SOURCE_MAP_LOAD_PRIOR_RED
    CHECK(MCGameplay_free(&game));
#else
    MCObjectHeap_free(heap);
#endif
}

#ifndef SOURCE_MAP_LOAD_PRIOR_RED
static void creation_signed_keys_and_cache(void) {
    static const int32_t ids[] = {0, 1, -1, INT32_MIN, INT32_MAX, 65535, -65536};
    for (int remote = 0; remote < 2; ++remote) {
        MCObjectHeap *heap = MCObjectHeap_new(2 * 1024 * 1024);
        World *world = memory_world(heap);
        world->isRemote = remote != 0;
        CHECK(MapStorage_nativeImportExactShort(world->mapStorage, NBTString_fromASCII(heap, "map"),
                                                73));
        for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
            MapData *created = NULL;
            CHECK(load(ids[i], world, &created) == WORLD_SAVED_DATA_OK && created);
            char key[32];
            snprintf(key, sizeof(key), "map_%" PRId32, ids[i]);
            CHECK(MapData_isInstance((MCObject *)created) &&
                  NBTString_equalsASCII(created->base.mapName, key));
            CHECK(created->xCenter == 0 && created->zCenter == 0 && created->scale == 0 &&
                  created->dimension == 0 && !created->base.dirty);
            CHECK(created->colors->length == 16384 && created->colors->values[0] == 0 &&
                  created->colors->values[16383] == 0);
            CHECK(NativeReferenceList_size(created->playersArrayList) == 0 &&
                  NativeHashMap_size(created->playersHashMap) == 0 &&
                  NativeLinkedHashMap_size(created->mapDecorations) == 0);
            CHECK(NativeHashMap_get(world->mapStorage->loadedDataMap,
                                    (MCObject *)created->base.mapName) == (MCObject *)created);
            size_t before = MCObjectHeap_liveObjects(heap);
            MapData *same = NULL;
            CHECK(load(ids[i], world, &same) == WORLD_SAVED_DATA_OK && same == created);
            CHECK(MCObjectHeap_liveObjects(heap) ==
                  before + 1); /* Fresh concatenated key, existing class literal/cache. */
            CHECK(NativeReferenceList_size(world->mapStorage->loadedDataList) == 0 &&
                  world->maps.count == 0);
        }
        CHECK(NativeHashMap_size(world->mapStorage->loadedDataMap) == 7);
        bool present = false;
        int16_t counter = 0;
        CHECK(MapStorage_nativeFindExactShort(world->mapStorage, NBTString_fromASCII(heap, "map"),
                                              &present, &counter) &&
              present && counter == 73);
        CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
}

static void null_cache_entry_and_key_alias(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
    World *world = memory_world(heap);
    NBTString *oldKey = NBTString_fromASCII(heap, "map_-7"),
              *nullKeyName = NBTString_fromASCII(heap, "NULL-key map");
    MapData *nullKeyMap = MapData_new(heap, nullKeyName, NULL, NULL);
    CHECK(oldKey && nullKeyMap && World_setItemData(world, oldKey, NULL) &&
          World_setItemData(world, NULL, &nullKeyMap->base));
    MapData *result = NULL;
    CHECK(load(-7, world, &result) == WORLD_SAVED_DATA_OK && result);
    CHECK(result != nullKeyMap && result->base.mapName != oldKey &&
          NBTString_equals(result->base.mapName, oldKey));
    CHECK(NativeHashMap_get(world->mapStorage->loadedDataMap, NULL) == (MCObject *)nullKeyMap);
    NativeIterator *iterator =
        NativeIterator_fromView(NativeHashMap_keys(world->mapStorage->loadedDataMap));
    CHECK(iterator);
    unsigned actualKey = 0;
    while (NativeIterator_hasNext(iterator)) {
        MCObject *key = NULL;
        CHECK(NativeIterator_next(iterator, &key));
        if (key) {
            CHECK(key == (MCObject *)oldKey);
            ++actualKey;
        }
    }
    CHECK(actualKey == 1 && NativeHashMap_size(world->mapStorage->loadedDataMap) == 2);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

static void saved_trace(MCObject *object, MCObjectVisitor visit, void *context) {
    WorldSavedData_traceFields((WorldSavedData *)object, visit, context);
}
static const MCObjectClass otherSavedClass = {"test.OtherSavedData", MCObjectHeap_plainClone,
                                              saved_trace, NULL};
static const WorldSavedDataNativeType otherSavedType = {&otherSavedClass, sizeof(WorldSavedData),
                                                        NULL};

static void healthy_wrong_saved_data_cast(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
    World *world = memory_world(heap);
    NBTString *key = NBTString_fromASCII(heap, "map_9");
    /* A real registered abstract subtype instance; its uncalled abstract
       methods remain missing dependencies, never empty-success handlers. */
    WorldSavedData *wrong = WorldSavedData_nativeAllocate(heap, &otherSavedType, NULL);
    CHECK(wrong && WorldSavedData_construct(wrong, key) && World_setItemData(world, key, wrong));
    MapData *sentinel = MapData_new(heap, NULL, NULL, NULL), *out = sentinel;
    CHECK(sentinel && ItemMap_loadMapData(9, world, &out) == WORLD_SAVED_DATA_EXCEPTION);
    CHECK(out == sentinel && NativeHashMap_get(world->mapStorage->loadedDataMap, (MCObject *)key) ==
                                 (MCObject *)wrong);
    CHECK(NativeHashMap_size(world->mapStorage->loadedDataMap) == 1 && !MCObjectHeap_failed(heap));
    CHECK(!MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

static void cached_partial_source_fields_are_not_normalized(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
    World *world = memory_world(heap);
    NBTString *key = NBTString_fromASCII(heap, "map_5");
    MapData *map = MapData_new(heap, NULL, NULL, NULL);
    CHECK(map && key);
    /* A caught saved-data read may retain the real object after a field write
       and later exception. Static load only checkcasts the cached reference. */
    map->colors = NULL;
    map->base.dirty = true;
    CHECK(World_setItemData(world, key, &map->base));
    MapData *out = NULL;
    CHECK(ItemMap_loadMapData(5, world, &out) == WORLD_SAVED_DATA_OK && out == map);
    CHECK(out->colors == NULL && out->base.mapName == NULL && out->base.dirty);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

static void clone_gc_adopt_exact_graph(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
    World *world = memory_world(heap);
    MapData *map = NULL;
    CHECK(ItemMap_loadMapData(-19, world, &map) == WORLD_SAVED_DATA_OK);
    map->colors->values[100] = -128;
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)world) && MCObjectHeap_collect(heap));
    size_t retained = MCObjectHeap_liveObjects(heap);
    for (int i = 0; i < 20; ++i) {
        CHECK(ItemMap_loadMapData(-19, world, &map) == WORLD_SAVED_DATA_OK &&
              map->colors->values[100] == -128);
        CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == retained);
    }
    MCObjectHeap *branch = MCObjectHeap_clone(heap);
    MCObjectRoot branchRoot = {0};
    CHECK(branch && MCObjectRoot_rebind(&branchRoot, branch, &root));
    World *copy = (World *)MCObjectRoot_get(&branchRoot);
    MapData *branchMap = NULL;
    CHECK(copy != world && copy->mapStorage != world->mapStorage &&
          ItemMap_loadMapData(-19, copy, &branchMap) == WORLD_SAVED_DATA_OK);
    CHECK(branchMap != map && branchMap->colors != map->colors &&
          branchMap->base.mapName != map->base.mapName);
    CHECK(branchMap->colors->values[100] == -128 &&
          NativeHashMap_get(copy->mapStorage->loadedDataMap, (MCObject *)branchMap->base.mapName) ==
              (MCObject *)branchMap);
    branchMap->colors->values[100] = 127;
    MCObjectHeap_touch(branch);
    CHECK(map->colors->values[100] == -128 && MCObjectHeap_collect(branch));
    CHECK(MCObjectHeap_canAdopt(heap, branch) && MCObjectHeap_adopt(heap, branch));
    world = (World *)MCObjectRoot_get(&root);
    CHECK(ItemMap_loadMapData(-19, world, &map) == WORLD_SAVED_DATA_OK &&
          map->colors->values[100] == 127);
    CHECK(map->base.object.heap == heap && map->colors->object.heap == heap &&
          world->mapStorage->object.heap == heap);
    MCObjectHeap_free(branch);
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(heap));
    CHECK(MCObjectHeap_liveObjects(heap) < retained && !MCObjectHeap_failed(heap) &&
          !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

static void native_failure_boundaries(void) {
    MapData *out = NULL;
    CHECK(ItemMap_loadMapData(0, NULL, &out) == WORLD_SAVED_DATA_EXCEPTION && out == NULL);
    for (int which = 0; which < 4; ++which) {
        MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024),
                     *foreign = MCObjectHeap_new(1024 * 1024);
        World *world = memory_world(heap);
        MapData *sentinel = MapData_new(heap, NULL, NULL, NULL);
        out = sentinel;
        CHECK(sentinel);
        if (which == 0)
            world->mapStorage = NULL;
        if (which == 1)
            world->mapStorage = (MapStorage *)SaveDataMemoryStorage_new(foreign);
        if (which == 2) {
            MapData *foreignMap = MapData_new(foreign, NULL, NULL, NULL);
            CHECK(foreignMap && NativeHashMap_put(world->mapStorage->loadedDataMap,
                                                  (MCObject *)NBTString_fromASCII(heap, "map_0"),
                                                  (MCObject *)sentinel));
            /* Direct malformed managed cache state, without calling a checked
               setter to claim a legitimate foreign-edge write. */
            NativeHashMap *replacement = NativeHashMap_new(foreign, NATIVE_HASH_KEY_STRING);
            CHECK(replacement &&
                  NativeHashMap_put(replacement, (MCObject *)NBTString_fromASCII(foreign, "map_0"),
                                    (MCObject *)foreignMap));
            world->mapStorage->loadedDataMap = replacement;
        }
        WorldSavedDataResult result = ItemMap_loadMapData(0, world, which == 3 ? NULL : &out);
        CHECK(result == WORLD_SAVED_DATA_FAILURE && out == sentinel && MCObjectHeap_failed(heap) &&
              !MCObjectHeap_failed(foreign));
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
        MCObjectHeap_free(foreign);
    }
}

static bool different_object(const MCObject *object, void *exclude) { return object != exclude; }
static World *budget_fixture(MCObjectHeap *heap, MapData **model) {
    World *world = memory_world(heap);
    *model = MapData_new(heap, NULL, NULL, NULL);
    CHECK(*model && MapData_nativeClass(heap));
    return world;
}
static void allocation_failure_prefixes(void) {
    MCObjectHeap *probe = MCObjectHeap_new(1024 * 1024);
    MapData *model = NULL, *result = NULL;
    World *world = budget_fixture(probe, &model);
    size_t setupBytes = MCObjectHeap_liveBytes(probe),
           setupObjects = MCObjectHeap_liveObjects(probe);
    CHECK(NBTString_fromASCII(probe, "map_-2147483648"));
    size_t keyBytes = MCObjectHeap_liveBytes(probe) - setupBytes;
    size_t before = MCObjectHeap_liveBytes(probe);
    CHECK(ItemMap_loadMapData(INT32_MIN, world, &result) == WORLD_SAVED_DATA_OK);
    size_t completeBytes = MCObjectHeap_liveBytes(probe) - before;
    size_t colorBytes = sizeof(*model->colors) + 16384 * sizeof(model->colors->values[0]);
    const MCObjectClass *mapClass = model->base.object.klass;
    size_t offsets[] = {0,
                        keyBytes - 1,
                        keyBytes,
                        keyBytes + sizeof(MapData) - 1,
                        keyBytes + sizeof(MapData),
                        keyBytes + sizeof(MapData) + colorBytes - 1,
                        keyBytes + sizeof(MapData) + colorBytes,
                        completeBytes - 1,
                        completeBytes};
    CHECK(completeBytes > keyBytes + sizeof(MapData) + colorBytes);
    MCObjectHeap_free(probe);
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        MCObjectHeap *heap = MCObjectHeap_new(setupBytes + offsets[i]);
        world = budget_fixture(heap, &model);
        CHECK(MCObjectHeap_liveBytes(heap) == setupBytes &&
              MCObjectHeap_liveObjects(heap) == setupObjects);
        result = model;
        WorldSavedDataResult status = ItemMap_loadMapData(INT32_MIN, world, &result);
        if (i + 1 == sizeof(offsets) / sizeof(offsets[0])) {
            CHECK(status == WORLD_SAVED_DATA_OK && result != model && !MCObjectHeap_failed(heap));
            CHECK(NBTString_equalsASCII(result->base.mapName, "map_-2147483648"));
        } else {
            CHECK(status == WORLD_SAVED_DATA_FAILURE && result == model &&
                  MCObjectHeap_failed(heap));
            MapData *partial =
                (MapData *)MCObjectHeap_findObject(heap, mapClass, different_object, model);
            if (i < 2)
                CHECK(MCObjectHeap_liveObjects(heap) == setupObjects);
            if (i < 4)
                CHECK(partial == NULL);
            else {
                CHECK(partial && NBTString_equalsASCII(partial->base.mapName, "map_-2147483648") &&
                      !partial->base.dirty);
                if (i < 6)
                    CHECK(partial->colors == NULL);
                else
                    CHECK(partial->colors && partial->colors->length == 16384 &&
                          partial->colors->values[0] == 0);
            }
        }
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
}
#endif

int main(void) {
    real_memory_cache_alias();
#ifndef SOURCE_MAP_LOAD_PRIOR_RED
    creation_signed_keys_and_cache();
    null_cache_entry_and_key_alias();
    healthy_wrong_saved_data_cast();
    cached_partial_source_fields_are_not_normalized();
    clone_gc_adopt_exact_graph();
    native_failure_boundaries();
    allocation_failure_prefixes();
#endif
    printf("source map load: %u checks passed\n", checks);
    return 0;
}
