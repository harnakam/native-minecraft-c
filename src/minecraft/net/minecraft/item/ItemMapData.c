#include "item/ItemMapData.h"
#include "world/WorldDataStorage.h"
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int32_t bits(uint32_t n) {
  int32_t out;
  memcpy(&out, &n, sizeof(out));
  return out;
}
static bool identity(const MCObject *o, void *expected) {
  return o == expected;
}
static bool tracked(MCObjectHeap *h, const MCObject *o) {
  return o && o->heap == h && o->klass &&
         MCObjectHeap_findObject(h, o->klass, identity, (void *)o) == o;
}
static WorldSavedDataResult failure(MCObjectHeap *h) {
  MCObjectHeap_fail(h);
  return WORLD_SAVED_DATA_FAILURE;
}
static NBTString *map_name(MCObjectHeap *h, int32_t metadata) {
  char text[32];
  int n = snprintf(text, sizeof(text), "map_%" PRId32, metadata);
  if (n < 0 || (size_t)n >= sizeof(text)) {
    failure(h);
    return NULL;
  }
  /* Explicit native String concatenation: fresh immutable UTF16 storage. */
  return NBTString_fromASCII(h, text);
}

WorldSavedDataResult ItemMap_getMapData(ItemStack *stack, World *world,
                                        MapData **out) {
  if (!stack)
    return out ? WORLD_SAVED_DATA_EXCEPTION : WORLD_SAVED_DATA_FAILURE;
  MCObjectHeap *h = stack->object.heap;
  if (!out)
    return failure(h);
  if (!tracked(h, (MCObject *)stack) ||
      !ItemStack_isInstance((MCObject *)stack) ||
      MCObjectHeap_objectSize((MCObject *)stack) < sizeof(*stack) ||
      MCObjectHeap_failed(h))
    return failure(h);
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, h) ||
      !MCObjectRootScope_pin(&scope, (MCObject *)stack)) {
    MCObjectRootScope_end(&scope);
    return failure(h);
  }
  WorldSavedDataResult r = WORLD_SAVED_DATA_FAILURE;
  MapData *map = NULL;
  NBTString *key = map_name(h, ItemStack_getMetadata(stack));
  if (!key || !MCObjectRootScope_pin(&scope, (MCObject *)key))
    goto done;
  NativeJavaClass *clazz = MapData_nativeClass(h);
  if (!clazz || !MCObjectRootScope_pin(&scope, (MCObject *)clazz))
    goto done;
  /* The reached World invocation follows String and class evaluation. */
  if (!world) {
    r = WORLD_SAVED_DATA_EXCEPTION;
    goto done;
  }
  if (!tracked(h, (MCObject *)world) || !World_isInstance((MCObject *)world) ||
      !MCObjectRootScope_pin(&scope, (MCObject *)world))
    goto done;
  WorldSavedData *loaded = NULL;
  if (!World_loadItemData(world, clazz, key, &loaded))
    goto done;
  if (loaded) {
    if (!tracked(h, (MCObject *)loaded))
      goto done;
    if (!MapData_isInstance((MCObject *)loaded)) {
      r = WORLD_SAVED_DATA_EXCEPTION;
      goto done;
    }
    map = (MapData *)loaded;
    if (!MCObjectRootScope_pin(&scope, (MCObject *)map))
      goto done;
  }
  if (!map && !world->isRemote) {
    NBTString *namespace = NBTString_literalASCII(h, "map");
    int32_t id = 0;
    if (!namespace || !World_getUniqueDataId(world, namespace, &id))
      goto done;
    ItemStack_setItemDamage(stack, id);
    key = map_name(h, ItemStack_getMetadata(stack));
    if (!key || !MCObjectRootScope_pin(&scope, (MCObject *)key))
      goto done;
    map = MapData_new(h, key, NULL, NULL);
    if (!map || !MCObjectRootScope_pin(&scope, (MCObject *)map))
      goto done;
    map->scale = 3;
    MCObjectHeap_touch(h);
    /* Each Source argument independently invokes the live World getter.
       Keep X, then reload WorldInfo for Z; read scale after both calls. */
    WorldInfo *info = World_getWorldInfo(world);
    if (MCObjectHeap_failed(h))
      goto done;
    if (!info) {
      r = WORLD_SAVED_DATA_EXCEPTION;
      goto done;
    }
    int32_t x = WorldInfo_getSpawnX(info);
    if (MCObjectHeap_failed(h))
      goto done;
    info = World_getWorldInfo(world);
    if (MCObjectHeap_failed(h))
      goto done;
    if (!info) {
      r = WORLD_SAVED_DATA_EXCEPTION;
      goto done;
    }
    int32_t z = WorldInfo_getSpawnZ(info);
    if (MCObjectHeap_failed(h))
      goto done;
    r = MapData_calculateMapCenter(map, (double)x, (double)z, map->scale);
    if (r != WORLD_SAVED_DATA_OK)
      goto done;
    WorldProvider *provider = world->provider;
    if (!provider) {
      r = WORLD_SAVED_DATA_EXCEPTION;
      goto done;
    }
    r = WORLD_SAVED_DATA_FAILURE;
    if (!tracked(h, (MCObject *)provider) ||
        !WorldProvider_isInstance((MCObject *)provider) ||
        !MCObjectRootScope_pin(&scope, (MCObject *)provider))
      goto done;
    uint8_t dim = (uint8_t)WorldProvider_getDimensionId(provider);
    if (MCObjectHeap_failed(h))
      goto done;
    memcpy(&map->dimension, &dim, sizeof(dim));
    MCObjectHeap_touch(h);
    r = WorldSavedData_markDirty(&map->base);
    if (r != WORLD_SAVED_DATA_OK)
      goto done;
    r = WORLD_SAVED_DATA_FAILURE;
    if (!World_setItemData(world, key, &map->base))
      goto done;
  }
  r = WORLD_SAVED_DATA_OK;
done:
  if (r == WORLD_SAVED_DATA_FAILURE || MCObjectHeap_failed(h))
    r = failure(h);
  if (r == WORLD_SAVED_DATA_OK)
    *out = map;
  MCObjectRootScope_end(&scope);
  return r;
}
static int32_t java_floor(double n) {
  int32_t i = isnan(n)         ? 0
              : n >= INT32_MAX ? INT32_MAX
              : n <= INT32_MIN ? INT32_MIN
                               : (int32_t)n;
  return n < (double)i ? bits((uint32_t)i - 1) : i;
}
/* Native MapData field adapter for the original calculateMapCenter method.
   Its integer multiplication/addition wrap, including extreme spawn values. */
void NativeItemMapData_calculateMapCenter(mc_map_info *map, double x, double z,
                                          int32_t scale) {
  int32_t size = bits(128u << (uint32_t)(scale & 31));
  int32_t a = java_floor((x + 64.0) / (double)size),
          b = java_floor((z + 64.0) / (double)size);
  map->center_x =
      bits((uint32_t)a * (uint32_t)size + (uint32_t)(size / 2) - 64u);
  map->center_z =
      bits((uint32_t)b * (uint32_t)size + (uint32_t)(size / 2) - 64u);
}
int32_t NativeItemMapData_getUniqueDataId(MCGameplayWorld *world) {
  MCObjectHeap *heap = world ? world->object.heap : NULL;
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, heap))
    return 0;
  NBTString *key = NBTString_literalASCII(heap, "map");
  int32_t value = 0;
  bool ok = key && World_getUniqueDataId(world, key, &value);
  if (!ok)
    MCObjectHeap_fail(heap);
  MCObjectRootScope_end(&scope);
  return value;
}
mc_map_info *NativeItemMapData_setItemData(MCGameplayWorld *world,
                                           const mc_map_info *source) {
  MCObjectHeap *h = world ? world->object.heap : NULL;
  if (!MCGameplayWorld_isInstance((MCObject *)world)) {
    MCObjectHeap_fail(h);
    return NULL;
  }
  mc_maps *maps = &world->maps;
  mc_map_info copy = {0};
  if (maps->count > maps->capacity || maps->capacity > MC_MAX_MAPS ||
      (!maps->capacity ? maps->entries != NULL : maps->entries == NULL) ||
      maps->next_id < 0 || maps->next_id > UINT16_MAX) {
    MCObjectHeap_fail(h);
    return NULL;
  }
  if (!source || !mc_map_info_copy(&copy, source)) {
    MCObjectHeap_fail(h);
    return NULL;
  }
  mc_map_info *old = mc_maps_find(&world->maps, copy.id);
  if (old) {
    mc_map_info_free(old);
    *old = copy;
    MCObjectHeap_touch(h);
    return old;
  }
  if (maps->count >= MC_MAX_MAPS) {
    mc_map_info_free(&copy);
    MCObjectHeap_fail(h);
    return NULL;
  }
  if (maps->count == maps->capacity) {
    size_t capacity = maps->capacity ? maps->capacity * 2 : 4;
    if (capacity > MC_MAX_MAPS)
      capacity = MC_MAX_MAPS;
    mc_map_info *entries = realloc(maps->entries, capacity * sizeof(*entries));
    if (!entries) {
      mc_map_info_free(&copy);
      MCObjectHeap_fail(h);
      return NULL;
    }
    maps->entries = entries;
    maps->capacity = capacity;
  }
  maps->entries[maps->count++] = copy;
  MCObjectHeap_touch(h);
  return &maps->entries[maps->count - 1];
}
mc_map_info *NativeItemMapData_getMapData(ItemStack *stack,
                                          MCGameplayWorld *world) {
  MCObjectHeap *h = stack   ? stack->object.heap
                    : world ? world->object.heap
                            : NULL;
  if (!stack || !MCGameplayWorld_isInstance((MCObject *)world) ||
      world->object.heap != h) {
    MCObjectHeap_fail(h);
    return NULL;
  }
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, h))
    return NULL;
  bool ok = MCObjectRootScope_pin(&scope, (MCObject *)stack) &&
            MCObjectRootScope_pin(&scope, (MCObject *)world);
  mc_map_info *map =
      ok ? mc_maps_find(&world->maps, ItemStack_getMetadata(stack)) : NULL;
  if (ok && !map && !world->isRemote) {
    /* ItemMap source order: allocate ID, mutate the exact stack, construct
       MapData, scale/center/dimension, markDirty, then setItemData. */
    int32_t id = NativeItemMapData_getUniqueDataId(world);
    if (!MCObjectHeap_failed(h)) {
      ItemStack_setItemDamage(stack, id);
      mc_map_info created = {0};
      created.id = ItemStack_getMetadata(stack);
      created.scale = 3;
      /* Each argument independently evaluates the actual World getter;
         a virtual first getter may replace worldInfo before the second. */
      WorldInfo *info = World_getWorldInfo(world);
      if (!info) {
        MCObjectHeap_fail(h);
        goto done;
      }
      int32_t x = WorldInfo_getSpawnX(info);
      if (MCObjectHeap_failed(h))
        goto done;
      info = World_getWorldInfo(world);
      if (!info) {
        MCObjectHeap_fail(h);
        goto done;
      }
      int32_t z = WorldInfo_getSpawnZ(info);
      if (MCObjectHeap_failed(h))
        goto done;
      NativeItemMapData_calculateMapCenter(&created, (double)x, (double)z,
                                           created.scale);
      WorldProvider *provider = world->provider;
      if (!WorldProvider_isInstance((MCObject *)provider) ||
          provider->object.heap != h) {
        MCObjectHeap_fail(h);
        goto done;
      }
      uint8_t dimension = (uint8_t)WorldProvider_getDimensionId(provider);
      if (MCObjectHeap_failed(h))
        goto done;
      memcpy(&created.dimension, &dimension, sizeof(dimension));
      created.metadata_known = true;
      created.dirty = true;
      MCObjectHeap_touch(h);
      map = NativeItemMapData_setItemData(world, &created);
    }
  }
done:
  MCObjectRootScope_end(&scope);
  return MCObjectHeap_failed(h) ? NULL : map;
}
