#include "item/ItemMapCreated.h"
#include "item/ItemMapLoad.h"
#include "nbt/NBTInternal.h"
#include "util/MCGameplayWorld.h"
#include "world/WorldDataStorage.h"
#include "world/storage/SaveDataMemoryStorage.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      fprintf(stderr, "Source ItemMap check %u line %d: %s\n", checks,         \
              __LINE__, #x);                                                   \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

typedef struct Fixture {
  MCObjectHeap *heap;
  World *world;
  ItemStack *stack;
} Fixture;
/* Reached WorldSavedData inherits Object.equals: comparison is the query's
   exact object identity. This required native leaf is not a fake cache. */
static MapStorageIOResult saved_identity(MCObject *context, MCObject *query,
                                         MCObject *stored, bool *out) {
  (void)context;
  *out = query == stored;
  return MAP_STORAGE_IO_OK;
}
static const MapStorageSourceDependencies identityDependencies = {
    .savedDataEquals = saved_identity};
static Fixture fixture(size_t budget, bool memory, bool remote) {
  Fixture f = {0};
  f.heap = MCObjectHeap_new(budget);
  CHECK(f.heap);
  f.world = World_nativeAllocate(f.heap, NULL, NULL);
  CHECK(f.world);
  f.world->mapStorage = memory ? (MapStorage *)SaveDataMemoryStorage_new(f.heap)
                               : MapStorage_new(f.heap, (ISaveHandler){0}, NULL,
                                                &identityDependencies);
  CHECK(f.world->mapStorage);
  f.world->worldInfo = WorldInfo_nativeAllocate(f.heap, NULL, NULL);
  CHECK(f.world->worldInfo);
  f.world->worldInfo->spawnX = -65;
  f.world->worldInfo->spawnZ = 100;
  f.world->provider = WorldProvider_nativeAllocate(f.heap, NULL, NULL);
  CHECK(f.world->provider);
  f.world->provider->dimensionId = 255;
  f.world->isRemote = remote;
  f.stack = ItemStack_new(f.heap, ItemStack_registryItem(358), -7, 4);
  CHECK(f.stack);
  return f;
}
static void next_id(Fixture *f, uint16_t next) {
  NBTString *key = NBTString_fromASCII(f->heap, "map");
  CHECK(key);
  uint16_t b = (uint16_t)(next - 1u);
  int16_t last;
  memcpy(&last, &b, sizeof(last));
  CHECK(MapStorage_nativeImportExactShort(f->world->mapStorage, key, last));
}
static int32_t after_next(Fixture *f) {
  int32_t next = -1;
  CHECK(World_nativeMapNextProjection(f->world, &next));
  return next;
}
static MapData *save_map(Fixture *f, const char *keyText, int8_t scale) {
  NBTString *key = NBTString_fromASCII(f->heap, keyText);
  CHECK(key);
  MapData *map = MapData_new(f->heap, key, NULL, NULL);
  CHECK(map);
  map->scale = scale;
  map->xCenter = 100;
  map->zCenter = -50;
  map->dimension = -1;
  map->colors->values[0] = 73;
  CHECK(World_setItemData(f->world, key, &map->base));
  return map;
}
static MapData *lookup(Fixture *f, const char *keyText) {
  NBTString *key = NBTString_fromASCII(f->heap, keyText);
  CHECK(key);
  WorldSavedData *out = NULL;
  CHECK(World_loadItemData(f->world, NULL, key, &out));
  return (MapData *)out;
}
static void marker(ItemStack *stack) {
  NBTTagCompound *tag = NBTTagCompound_new(stack->object.heap);
  CHECK(tag);
  CHECK(NBTTagCompound_setBoolean_ascii(tag, "map_is_scaling", true));
  CHECK(ItemStack_setTagCompound(stack, tag));
}
static bool named_map(const MCObject *o, void *unused) {
  (void)unused;
  return MapData_isInstance(o) &&
         NBTString_equalsASCII(((const MapData *)o)->base.mapName, "map_10");
}

typedef struct GetterContext {
  MCObject object;
  WorldInfo *first, *second;
  const MCObjectClass *mapClass;
  int calls, failAt;
  bool mutateScale;
} GetterContext;
static void getter_trace(MCObject *o, MCObjectVisitor v, void *ctx) {
  GetterContext *c = (GetterContext *)o;
  c->first = (WorldInfo *)v((MCObject *)c->first, ctx);
  c->second = (WorldInfo *)v((MCObject *)c->second, ctx);
}
static const MCObjectClass getterClass = {
    "test.ItemMapGetters", MCObjectHeap_plainClone, getter_trace, NULL};
static WorldInfo *get_info(MCObject *o, World *world) {
  GetterContext *c = (GetterContext *)o;
  ++c->calls;
  CHECK(!MCObjectHeap_collect(o->heap));
  if (c->calls == c->failAt) {
    MCObjectHeap_fail(o->heap);
    return c->second;
  }
  if (c->calls == 2 && c->mutateScale) {
    MapData *m = (MapData *)MCObjectHeap_findObject(o->heap, c->mapClass,
                                                    named_map, NULL);
    CHECK(m && m->scale == 3 && m->colors);
    m->scale = 1;
    MCObjectHeap_touch(o->heap);
    world->provider->dimensionId = 258;
  }
  return c->calls == 1 ? c->first : c->second;
}
static const WorldDependencies getterDependencies = {.getWorldInfo = get_info};
static void getter_evaluation_and_failure_prefix(void) {
  for (int fail = 0; fail <= 2; ++fail) {
    Fixture f = fixture(1u << 20, false, false);
    next_id(&f, 10);
    GetterContext *c =
        (GetterContext *)MCObjectHeap_alloc(f.heap, sizeof(*c), &getterClass);
    CHECK(c);
    c->first = f.world->worldInfo;
    c->first->spawnX = -65;
    c->second = WorldInfo_nativeAllocate(f.heap, NULL, NULL);
    CHECK(c->second);
    c->second->spawnZ = 100;
    c->second->spawnX = 999;
    c->mapClass = save_map(&f, "unrelated", 0)->base.object.klass;
    c->failAt = fail;
    c->mutateScale = true;
    f.world->dependencies = &getterDependencies;
    f.world->dependencyContext = (MCObject *)c;
    MapData *out = (MapData *)(void *)f.stack;
    WorldSavedDataResult r = ItemMap_getMapData(f.stack, f.world, &out);
    CHECK(c->calls == (fail ? fail : 2));
    CHECK(f.stack->itemDamage == 10);
    if (fail) {
      CHECK(r == WORLD_SAVED_DATA_FAILURE && MCObjectHeap_failed(f.heap));
      CHECK(out == (MapData *)(void *)f.stack);
      MapData *partial = (MapData *)MCObjectHeap_findObject(f.heap, c->mapClass,
                                                            named_map, NULL);
      CHECK(partial && partial->scale == 3 && partial->colors &&
            !partial->base.dirty);
    } else {
      CHECK(r == WORLD_SAVED_DATA_OK && out->scale == 1 &&
            out->xCenter == -192 && out->zCenter == 64);
      CHECK(out->dimension == 2 && out->base.dirty &&
            !MCObjectHeap_failed(f.heap));
    }
    CHECK(!MCObjectHeap_hasBorrowers(f.heap));
    MCObjectHeap_free(f.heap);
  }
}

static void wrong_saved_trace(MCObject *o, MCObjectVisitor v, void *ctx) {
  WorldSavedData_traceFields((WorldSavedData *)o, v, ctx);
}
static const MCObjectClass wrongSavedClass = {
    "test.OtherSavedData", MCObjectHeap_plainClone, wrong_saved_trace, NULL};
static const WorldSavedDataVirtualMethods wrongSavedMethods = {0};
static const WorldSavedDataNativeType wrongSavedType = {
    &wrongSavedClass, sizeof(WorldSavedData), &wrongSavedMethods};
static void wrong_type_and_foreign_guards(void) {
  Fixture f = fixture(1u << 20, true, false);
  WorldSavedData *wrong =
      WorldSavedData_nativeAllocate(f.heap, &wrongSavedType, NULL);
  CHECK(wrong);
  CHECK(WorldSavedData_construct(wrong, NULL));
  CHECK(
      World_setItemData(f.world, NBTString_fromASCII(f.heap, "map_4"), wrong));
  MapData *out = (MapData *)(void *)f.stack;
  CHECK(ItemMap_getMapData(f.stack, f.world, &out) ==
        WORLD_SAVED_DATA_EXCEPTION);
  CHECK(out == (MapData *)(void *)f.stack && f.stack->itemDamage == 4 &&
        !MCObjectHeap_failed(f.heap));
  marker(f.stack);
  CHECK(ItemMap_onCreated(f.stack, f.world, NULL) ==
        WORLD_SAVED_DATA_EXCEPTION);
  CHECK(f.stack->itemDamage == 4 && !MCObjectHeap_failed(f.heap));
  MCObjectHeap_free(f.heap);
  f = fixture(1u << 20, true, false);
  MCObjectHeap *other = MCObjectHeap_new(1u << 20);
  CHECK(other);
  World *foreign = World_nativeAllocate(other, NULL, NULL);
  CHECK(foreign);
  out = (MapData *)(void *)f.stack;
  CHECK(ItemMap_getMapData(f.stack, foreign, &out) == WORLD_SAVED_DATA_FAILURE);
  CHECK(MCObjectHeap_failed(f.heap) && !MCObjectHeap_failed(other) &&
        out == (MapData *)(void *)f.stack);
  CHECK(!MCObjectHeap_hasBorrowers(f.heap));
  MCObjectHeap_free(f.heap);
  MCObjectHeap_free(other);
  f = fixture(1u << 20, true, false);
  ItemStack *small = (ItemStack *)MCObjectHeap_alloc(f.heap, sizeof(MCObject),
                                                     f.stack->object.klass);
  CHECK(small);
  out = NULL;
  CHECK(ItemMap_getMapData(small, f.world, &out) == WORLD_SAVED_DATA_FAILURE);
  CHECK(MCObjectHeap_failed(f.heap) && !out);
  MCObjectHeap_free(f.heap);
  f = fixture(1u << 20, false, false);
  next_id(&f, 10);
  f.world->provider = NULL;
  out = (MapData *)(void *)f.stack;
  CHECK(ItemMap_getMapData(f.stack, f.world, &out) ==
        WORLD_SAVED_DATA_EXCEPTION);
  CHECK(f.stack->itemDamage == 10 && after_next(&f) == 11 &&
        !MCObjectHeap_failed(f.heap));
  CHECK(out == (MapData *)(void *)f.stack);
  MCObjectHeap_free(f.heap);
}

static void graph_aliases_clone_and_adopt(void) {
  Fixture f = fixture(1u << 20, true, true);
  MapData *old = save_map(&f, "map_4", 1);
  CHECK(World_setItemData(f.world, NBTString_fromASCII(f.heap, "map_0"),
                          &old->base));
  f.stack->itemDamage = 0;
  marker(f.stack);
  MCObjectRoot wr = {0}, sr = {0}, mr = {0}, cr = {0};
  CHECK(MCObjectRoot_init(&wr, f.heap, (MCObject *)f.world));
  CHECK(MCObjectRoot_init(&sr, f.heap, (MCObject *)f.stack));
  CHECK(MCObjectRoot_init(&mr, f.heap, (MCObject *)old));
  CHECK(MCObjectRoot_init(&cr, f.heap, (MCObject *)old->colors));
  CHECK(MCObjectHeap_collect(f.heap));
  MCObjectHeap *working = MCObjectHeap_clone(f.heap);
  CHECK(working);
  MCObjectRoot wwr = {0}, wsr = {0}, wmr = {0}, wcr = {0};
  CHECK(MCObjectRoot_rebind(&wwr, working, &wr) &&
        MCObjectRoot_rebind(&wsr, working, &sr) &&
        MCObjectRoot_rebind(&wmr, working, &mr) &&
        MCObjectRoot_rebind(&wcr, working, &cr));
  World *w = (World *)MCObjectRoot_get(&wwr);
  ItemStack *s = (ItemStack *)MCObjectRoot_get(&wsr);
  MapData *m = (MapData *)MCObjectRoot_get(&wmr);
  CHECK(m != old && m->colors == (NativeByteArray *)MCObjectRoot_get(&wcr));
  CHECK(ItemMap_onCreated(s, w, NULL) == WORLD_SAVED_DATA_OK);
  NBTString *key = NBTString_fromASCII(working, "map_0");
  CHECK(key);
  MapData *created = (MapData *)NativeHashMap_get(w->mapStorage->loadedDataMap,
                                                  (MCObject *)key);
  CHECK(created && created != m && created->scale == 2 &&
        m->colors->values[0] == 73);
  CHECK(old->scale == 1 && old->colors->values[0] == 73 &&
        NativeHashMap_size(f.world->mapStorage->loadedDataMap) == 2);
  CHECK(MCObjectHeap_collect(working) &&
        MCObjectHeap_canAdopt(f.heap, working));
  CHECK(MCObjectHeap_adopt(f.heap, working));
  f.world = (World *)MCObjectRoot_get(&wr);
  f.stack = (ItemStack *)MCObjectRoot_get(&sr);
  old = (MapData *)MCObjectRoot_get(&mr);
  CHECK(old->colors == (NativeByteArray *)MCObjectRoot_get(&cr) &&
        old->colors->values[0] == 73);
  CHECK(lookup(&f, "map_0") != old && lookup(&f, "map_4") == old);
  CHECK(MCObjectHeap_collect(f.heap) &&
        MCObjectRoot_get(&mr) == (MCObject *)old);
  MCObjectRoot_drop(&wr);
  MCObjectRoot_drop(&sr);
  MCObjectRoot_drop(&mr);
  MCObjectRoot_drop(&cr);
  MCObjectHeap_free(working);
  MCObjectHeap_free(f.heap);
}

static void bounded_allocation_failure_prefix(void) {
  Fixture large = fixture(1u << 20, false, false);
  next_id(&large, 10);
  size_t baseline = MCObjectHeap_liveBytes(large.heap);
  MCObjectHeap_free(large.heap);
  unsigned failed = 0, mutated = 0, passed = 0;
  for (size_t extra = 0; extra <= 24000; extra += 256) {
    Fixture f = fixture(baseline + extra, false, false);
    next_id(&f, 10);
    MapData *out = (MapData *)(void *)f.stack;
    WorldSavedDataResult r = ItemMap_getMapData(f.stack, f.world, &out);
    if (r == WORLD_SAVED_DATA_FAILURE) {
      ++failed;
      CHECK(MCObjectHeap_failed(f.heap) && out == (MapData *)(void *)f.stack);
      CHECK(f.stack->itemDamage == 4 || f.stack->itemDamage == 10);
      if (f.stack->itemDamage == 10)
        ++mutated;
    } else {
      ++passed;
      CHECK(r == WORLD_SAVED_DATA_OK && out && out->base.dirty &&
            f.stack->itemDamage == 10);
    }
    CHECK(!MCObjectHeap_hasBorrowers(f.heap));
    MCObjectHeap_free(f.heap);
  }
  CHECK(failed && mutated && passed);
}

static Fixture created_fixture(size_t budget) {
  Fixture f = fixture(budget, false, true);
  next_id(&f, 10);
  save_map(&f, "map_4", 1);
  marker(f.stack);
  CHECK(MapData_nativeClass(f.heap) && NBTString_literalASCII(f.heap, "map"));
  return f;
}
static bool unnamed_map(const MCObject *o, void *unused) {
  (void)unused;
  return MapData_isInstance(o) && !((const MapData *)o)->base.mapName;
}
static void new_precedes_constructor_arguments(void) {
  Fixture large = created_fixture(1u << 20);
  size_t baseline = MCObjectHeap_liveBytes(large.heap);
  const MCObjectClass *klass = lookup(&large, "map_4")->base.object.klass;
  MCObjectHeap_free(large.heap);
  /* The only pre-NEW managed allocation is the lookup's fresh map_4 String.
     Provide room for that and the MapData object, but not its name argument.
     A String-before-NEW implementation leaves no unnamed MapData prefix. */
  Fixture f =
      created_fixture(baseline + sizeof(NBTString) + 10u + sizeof(MapData));
  CHECK(ItemMap_onCreated(f.stack, f.world, NULL) == WORLD_SAVED_DATA_FAILURE);
  CHECK(MCObjectHeap_failed(f.heap) && f.stack->itemDamage == 10);
  MapData *partial =
      (MapData *)MCObjectHeap_findObject(f.heap, klass, unnamed_map, NULL);
  CHECK(partial && !partial->base.mapName && !partial->colors &&
        !partial->playersArrayList);
  CHECK(!MCObjectHeap_hasBorrowers(f.heap));
  MCObjectHeap_free(f.heap);
}

static void aborted_source_failure_does_not_change_parent(void) {
  Fixture f = fixture(1u << 20, false, true);
  next_id(&f, 10);
  marker(f.stack);
  MapData *old = save_map(&f, "map_4", 1);
  MCObjectRoot wr = {0}, sr = {0};
  CHECK(MCObjectRoot_init(&wr, f.heap, (MCObject *)f.world) &&
        MCObjectRoot_init(&sr, f.heap, (MCObject *)f.stack));
  MCObjectHeap *working = MCObjectHeap_clone(f.heap);
  CHECK(working);
  MCObjectRoot wwr = {0}, wsr = {0};
  CHECK(MCObjectRoot_rebind(&wwr, working, &wr) &&
        MCObjectRoot_rebind(&wsr, working, &sr));
  World *w = (World *)MCObjectRoot_get(&wwr);
  ItemStack *s = (ItemStack *)MCObjectRoot_get(&wsr);
  CHECK(NativeHashMap_clear(w->mapStorage->loadedDataMap));
  CHECK(ItemMap_onCreated(s, w, NULL) == WORLD_SAVED_DATA_EXCEPTION &&
        s->itemDamage == 10);
  CHECK(!MCObjectHeap_failed(working) && !MCObjectHeap_hasBorrowers(working));
  MCObjectHeap_free(working);
  CHECK(f.stack->itemDamage == 4 && after_next(&f) == 10 &&
        lookup(&f, "map_4") == old);
  CHECK(old->colors->values[0] == 73 && old->scale == 1 &&
        !MCObjectHeap_failed(f.heap));
  MCObjectRoot_drop(&wr);
  MCObjectRoot_drop(&sr);
  MCObjectHeap_free(f.heap);
}

static void cache_partial_and_null(void) {
  Fixture f = fixture(1u << 20, true, true);
  MapData *m = save_map(&f, "map_4", 127);
  NativeByteArray *oldColors = m->colors;
  m->colors = NULL;
  m->base.mapName = NULL;
  f.world->worldInfo = NULL;
  f.world->provider = NULL;
  MapData *out = NULL;
  CHECK(ItemMap_getMapData(f.stack, f.world, &out) == WORLD_SAVED_DATA_OK);
  CHECK(out == m && !out->colors && !out->base.mapName && out->scale == 127);
  m->colors = oldColors;
  CHECK(World_setItemData(f.world, NBTString_fromASCII(f.heap, "map_4"), NULL));
  out = m;
  CHECK(ItemMap_getMapData(f.stack, f.world, &out) == WORLD_SAVED_DATA_OK &&
        out == NULL);
  CHECK(!MCObjectHeap_failed(f.heap));
  MCObjectHeap_free(f.heap);
}

static void authoritative_create_and_wrap(void) {
  static const uint16_t ids[] = {0, 10, 32767, 32768, 32769, 65535};
  static const int32_t damage[] = {0, 10, 32767, 0, 0, 0};
  for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
    Fixture f = fixture(1u << 20, false, false);
    next_id(&f, ids[i]);
    MapData *out = NULL;
    CHECK(ItemMap_getMapData(f.stack, f.world, &out) == WORLD_SAVED_DATA_OK);
    CHECK(out && f.stack->itemDamage == damage[i] && f.stack->stackSize == -7);
    CHECK(out->scale == 3 && out->xCenter == -576 && out->zCenter == 448 &&
          out->dimension == -1);
    CHECK(out->base.dirty && out->colors && out->colors->length == 16384);
    char name[32];
    snprintf(name, sizeof(name), "map_%d", damage[i]);
    CHECK(NBTString_equalsASCII(out->base.mapName, name));
    CHECK(lookup(&f, name) == out &&
          after_next(&f) == (int32_t)((ids[i] + 1u) & 65535u));
    CHECK(f.world->maps.count == 0 && !MCObjectHeap_failed(f.heap));
    MCObjectHeap_free(f.heap);
  }
  Fixture f = fixture(1u << 20, true, true);
  size_t count = MCObjectHeap_liveObjects(f.heap);
  MapData *out = (MapData *)(void *)f.stack;
  CHECK(ItemMap_getMapData(f.stack, f.world, &out) == WORLD_SAVED_DATA_OK &&
        out == NULL);
  CHECK(f.stack->itemDamage == 4 &&
        NativeHashMap_size(f.world->mapStorage->loadedDataMap) == 0);
  CHECK(MCObjectHeap_liveObjects(f.heap) > count &&
        !MCObjectHeap_failed(f.heap));
  /* Remote lookup still constructs a fresh key; it never creates a MapData. */
  MCObjectHeap_free(f.heap);
}

static void lookup_miss_replaces_colliding_zero_id(void) {
  Fixture f = fixture(1u << 20, false, false);
  next_id(&f, 32768);
  MapData *old = save_map(&f, "map_0", 4);
  NativeByteArray *colors = old->colors;
  MCObjectRoot oldRoot = {0};
  CHECK(MCObjectRoot_init(&oldRoot, f.heap, (MCObject *)old));
  /* ItemMap's method body does not inspect the Item identity or signed count.
   */
  ItemStack_setItem(f.stack, ItemStack_registryItem(1));
  f.stack->itemDamage = 999;
  MapData *out = NULL;
  CHECK(ItemMap_getMapData(f.stack, f.world, &out) == WORLD_SAVED_DATA_OK);
  CHECK(out && out != old && out->colors != colors &&
        out->colors->values[0] == 0);
  CHECK(old->colors == colors && colors->values[0] == 73 && old->scale == 4);
  CHECK(f.stack->itemDamage == 0 && f.stack->stackSize == -7 &&
        after_next(&f) == 32769);
  CHECK(lookup(&f, "map_0") == out && out->base.dirty);
  MCObjectRoot worldRoot = {0};
  CHECK(MCObjectRoot_init(&worldRoot, f.heap, (MCObject *)f.world));
  CHECK(MCObjectHeap_collect(f.heap) &&
        MCObjectRoot_get(&oldRoot) == (MCObject *)old);
  CHECK(old->colors == colors && lookup(&f, "map_0") == out);
  MCObjectRoot_drop(&worldRoot);
  MCObjectRoot_drop(&oldRoot);
  MCObjectHeap_free(f.heap);
}

static void created_replaces_without_alias_rewrite(void) {
  static const int8_t scale[] = {-128, -1, 0, 3, 4, 127};
  static const int8_t expected[] = {-127, 0, 1, 4, 4, -128};
  for (size_t i = 0; i < sizeof(scale) / sizeof(scale[0]); ++i) {
    Fixture f = fixture(1u << 20, true, true);
    MapData *old = save_map(&f, "map_4", scale[i]);
    CHECK(World_setItemData(f.world, NBTString_fromASCII(f.heap, "map_0"),
                            &old->base));
    f.stack->itemDamage = 0;
    marker(f.stack);
    NBTTagCompound *tag = f.stack->stackTagCompound;
    MCObjectRoot oldRoot = {0};
    CHECK(MCObjectRoot_init(&oldRoot, f.heap, (MCObject *)old));
    NativeByteArray *colors = old->colors;
    CHECK(ItemMap_onCreated(f.stack, f.world, (MCObject *)(void *)&checks) ==
          WORLD_SAVED_DATA_OK);
    MapData *created = lookup(&f, "map_0");
    CHECK(created && created != old && created->colors != colors &&
          created->colors->values[0] == 0);
    CHECK(created->scale == expected[i] && created->dimension == -1 &&
          created->base.dirty);
    CHECK(old->scale == scale[i] && old->colors == colors &&
          colors->values[0] == 73 && old->xCenter == 100);
    CHECK(f.stack->itemDamage == 0 && f.stack->stackTagCompound == tag &&
          NBTTagCompound_getBoolean_ascii(tag, "map_is_scaling"));
    CHECK(NativeHashMap_get(f.world->mapStorage->loadedDataMap,
                            (MCObject *)created->base.mapName) ==
          (MCObject *)created);
    CHECK(created->base.mapName != old->base.mapName);
    CHECK(MCObjectHeap_collect(f.heap) &&
          MCObjectRoot_get(&oldRoot) == (MCObject *)old);
    MCObjectRoot_drop(&oldRoot);
    MCObjectHeap_free(f.heap);
  }
}

static void reached_null_prefix_and_unused_owners(void) {
  Fixture f = fixture(1u << 20, false, true);
  next_id(&f, 10);
  marker(f.stack);
  const MCObjectClass *clazz = save_map(&f, "unrelated", 0)->base.object.klass;
  CHECK(ItemMap_onCreated(f.stack, f.world, NULL) ==
        WORLD_SAVED_DATA_EXCEPTION);
  CHECK(f.stack->itemDamage == 10 && after_next(&f) == 11 &&
        lookup(&f, "map_10") == NULL);
  MapData *partial =
      (MapData *)MCObjectHeap_findObject(f.heap, clazz, named_map, NULL);
  CHECK(partial);
  CHECK(partial->colors && partial->playersArrayList &&
        partial->playersHashMap && partial->mapDecorations);
  CHECK(!partial->base.dirty && partial->scale == 0 &&
        !MCObjectHeap_failed(f.heap));
  MCObjectHeap_free(f.heap);
  f = fixture(1u << 20, false, false);
  next_id(&f, 10);
  f.world->worldInfo = NULL;
  MapData *out = (MapData *)(void *)f.stack;
  CHECK(ItemMap_getMapData(f.stack, f.world, &out) ==
        WORLD_SAVED_DATA_EXCEPTION);
  CHECK(out == (MapData *)(void *)f.stack && f.stack->itemDamage == 10 &&
        after_next(&f) == 11);
  CHECK(!MCObjectHeap_failed(f.heap));
  MCObjectHeap_free(f.heap);
  f = fixture(1u << 20, true, false);
  CHECK(ItemMap_onCreated(f.stack, NULL, (MCObject *)(void *)&checks) ==
        WORLD_SAVED_DATA_OK);
  marker(f.stack);
  CHECK(NBTTagCompound_setBoolean_ascii(f.stack->stackTagCompound,
                                        "map_is_scaling", false));
  CHECK(ItemMap_onCreated(f.stack, NULL, (MCObject *)(void *)&checks) ==
        WORLD_SAVED_DATA_OK);
  out = (MapData *)(void *)f.stack;
  size_t before = MCObjectHeap_liveObjects(f.heap);
  CHECK(ItemMap_getMapData(f.stack, NULL, &out) == WORLD_SAVED_DATA_EXCEPTION);
  CHECK(out == (MapData *)(void *)f.stack &&
        MCObjectHeap_liveObjects(f.heap) > before);
  CHECK(ItemMap_getMapData(NULL, f.world, &out) == WORLD_SAVED_DATA_EXCEPTION);
  CHECK(ItemMap_getMapData(NULL, (World *)(void *)&checks, &out) ==
        WORLD_SAVED_DATA_EXCEPTION);
  CHECK(ItemMap_onCreated(NULL, f.world, NULL) == WORLD_SAVED_DATA_EXCEPTION);
  CHECK(!MCObjectHeap_hasBorrowers(f.heap) && !MCObjectHeap_failed(f.heap));
  MCObjectHeap_free(f.heap);
}

/* This regression catches a cache bypass or value-copy result. It uses the
   actual memory provider and Source MapData; no successful fake lookup exists.
 */
static void cache_is_the_original_object(void) {
#ifdef SOURCE_ITEM_MAP_PRIOR_RED
  MCGameplay game = {0};
  CHECK(MCGameplay_init(&game, 4u * 1024u * 1024u));
  MCObjectHeap *heap = game.heap;
  World *world = MCGameplayWorld_nativeNewDimension(
      heap, MCGameplay_get(&game), NULL, NULL,
      NativeJavaRandomRuntime_process(), 919, 0, true);
  CHECK(world && MCGameplay_setWorld(&game, (MCObject *)world));
#else
  MCObjectHeap *heap = MCObjectHeap_new(4u * 1024u * 1024u);
  CHECK(heap);
  World *world = World_nativeAllocate(heap, NULL, NULL);
  CHECK(world);
#endif
  world->mapStorage = (MapStorage *)SaveDataMemoryStorage_new(heap);
  CHECK(world->mapStorage);
  world->isRemote = true;
  NBTString *key = NBTString_fromASCII(heap, "map_4"),
            *name = NBTString_fromASCII(heap, "kept name");
  MapData *stored = MapData_new(heap, name, NULL, NULL);
  CHECK(key && name && stored);
  stored->colors->values[91] = -7;
  stored->scale = -128;
  stored->base.dirty = true;
  CHECK(World_setItemData(world, key, &stored->base));
  ItemStack *stack = ItemStack_new(heap, ItemStack_registryItem(358), 0, 4);
  CHECK(stack);
  MapData *result = NULL;
#ifdef SOURCE_ITEM_MAP_PRIOR_RED
  mc_map_info *prior = NativeItemMapData_getMapData(stack, world);
  CHECK(!MCObjectHeap_failed(heap));
  result = (MapData *)(void *)prior;
#else
  CHECK(ItemMap_getMapData(stack, world, &result) == WORLD_SAVED_DATA_OK);
#endif
  CHECK(result == stored);
  CHECK(result->base.mapName == name && result->colors == stored->colors &&
        result->colors->values[91] == -7 && result->scale == -128 &&
        result->base.dirty);
  CHECK(stack->stackSize == 0 && stack->itemDamage == 4 &&
        world->maps.count == 0);
  CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
#ifdef SOURCE_ITEM_MAP_PRIOR_RED
  CHECK(MCGameplay_free(&game));
#else
  MCObjectHeap_free(heap);
#endif
}
int main(void) {
  cache_is_the_original_object();
  cache_partial_and_null();
  authoritative_create_and_wrap();
  lookup_miss_replaces_colliding_zero_id();
  created_replaces_without_alias_rewrite();
  reached_null_prefix_and_unused_owners();
  getter_evaluation_and_failure_prefix();
  wrong_type_and_foreign_guards();
  graph_aliases_clone_and_adopt();
  bounded_allocation_failure_prefix();
  new_precedes_constructor_arguments();
  aborted_source_failure_does_not_change_parent();
  printf("Source ItemMap: %u checks passed\n", checks);
  return 0;
}
