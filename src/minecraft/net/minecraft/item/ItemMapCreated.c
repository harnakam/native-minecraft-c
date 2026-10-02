#include "item/ItemMapCreated.h"
#include "nbt/NBTTagCompound.h"
#include "world/WorldDataStorage.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

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
  return NBTString_fromASCII(h, text);
}

WorldSavedDataResult ItemMap_onCreated(ItemStack *stack, World *world,
                                       MCObject *player) {
  /* Source never evaluates playerIn or worldIn when the marker is absent. */
  (void)player;
  MCObjectHeap *h = stack ? stack->object.heap : NULL;
  if (!stack)
    return WORLD_SAVED_DATA_EXCEPTION;
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
  if (!ItemStack_hasTagCompound(stack)) {
    r = WORLD_SAVED_DATA_OK;
    goto done;
  }
  NBTTagCompound *tag = ItemStack_getTagCompound(stack);
  if (!tracked(h, (MCObject *)tag) ||
      !NBTTagCompound_isInstance((MCObject *)tag) ||
      !MCObjectRootScope_pin(&scope, (MCObject *)tag))
    goto done;
  bool scaling = NBTTagCompound_getBoolean_ascii(tag, "map_is_scaling");
  if (MCObjectHeap_failed(h))
    goto done;
  if (!scaling) {
    r = WORLD_SAVED_DATA_OK;
    goto done;
  }
  MapData *old = NULL;
  r = ItemMap_getMapData(stack, world, &old);
  if (r != WORLD_SAVED_DATA_OK)
    goto done;
  r = WORLD_SAVED_DATA_FAILURE;
  if (!MCObjectRootScope_pin(&scope, (MCObject *)world) ||
      !MCObjectRootScope_pin(&scope, (MCObject *)old))
    goto done;
  NBTString *namespace = NBTString_literalASCII(h, "map");
  int32_t id = 0;
  if (!namespace || !World_getUniqueDataId(world, namespace, &id))
    goto done;
  ItemStack_setItemDamage(stack, id);
  /* NEW precedes its constructor arguments in the executable Source. Keep
     this allocation before metadata/String evaluation and old dereference. */
  MapData *created = MapData_nativeAllocate(h, NULL, NULL);
  if (!created || !MCObjectRootScope_pin(&scope, (MCObject *)created))
    goto done;
  NBTString *name = map_name(h, ItemStack_getMetadata(stack));
  if (!name || !MCObjectRootScope_pin(&scope, (MCObject *)name) ||
      !MapData_construct(created, name))
    goto done;
  if (!old) {
    r = WORLD_SAVED_DATA_EXCEPTION;
    goto done;
  }
  uint8_t scaleBits = (uint8_t)((int32_t)old->scale + 1);
  memcpy(&created->scale, &scaleBits, sizeof(scaleBits));
  if (created->scale > 4)
    created->scale = 4;
  MCObjectHeap_touch(h);
  r = MapData_calculateMapCenter(created, (double)old->xCenter,
                                 (double)old->zCenter, created->scale);
  if (r != WORLD_SAVED_DATA_OK)
    goto done;
  created->dimension = old->dimension;
  MCObjectHeap_touch(h);
  r = WorldSavedData_markDirty(&created->base);
  if (r != WORLD_SAVED_DATA_OK)
    goto done;
  r = WORLD_SAVED_DATA_FAILURE;
  /* Source constructs a second String for this invocation, from live damage.
     The earlier name remains the new object's distinct immutable name edge. */
  NBTString *key = map_name(h, ItemStack_getMetadata(stack));
  if (!key || !MCObjectRootScope_pin(&scope, (MCObject *)key) ||
      !World_setItemData(world, key, &created->base))
    goto done;
  r = WORLD_SAVED_DATA_OK;
done:
  if (r == WORLD_SAVED_DATA_FAILURE || MCObjectHeap_failed(h))
    r = failure(h);
  MCObjectRootScope_end(&scope);
  return r;
}

bool NativeItemMap_onCreated(ItemStack *stack, MCGameplayWorld *world,
                             MCObject *player) {
  /* The original method does not use playerIn. */
  (void)player;
  if (!stack)
    return false;
  MCObjectHeap *heap = stack->object.heap;
  if (!ItemStack_hasTagCompound(stack) ||
      !NBTTagCompound_getBoolean_ascii(ItemStack_getTagCompound(stack),
                                       "map_is_scaling"))
    return !MCObjectHeap_failed(heap);
  if (!MCGameplayWorld_isInstance((MCObject *)world) ||
      world->object.heap != heap) {
    MCObjectHeap_fail(heap);
    return false;
  }
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, heap))
    return false;
  bool ok = MCObjectRootScope_pin(&scope, (MCObject *)stack) &&
            MCObjectRootScope_pin(&scope, (MCObject *)world);
  mc_map_info *old = ok ? NativeItemMapData_getMapData(stack, world) : NULL;
  ok = ok && !MCObjectHeap_failed(heap);
  if (ok) {
    /* Source order matters even when a remote World has no saved map:
       getMapData, allocate ID, change this stack, construct MapData, then
       dereference the old map. A null old map fails after the ID mutation. */
    int32_t id = NativeItemMapData_getUniqueDataId(world);
    ok = !MCObjectHeap_failed(heap);
    if (ok) {
      ItemStack_setItemDamage(stack, id);
      mc_map_info created = {0};
      created.id = ItemStack_getMetadata(stack);
      if (!old)
        ok = false;
      else {
        uint8_t bits = (uint8_t)(old->scale + 1u);
        int8_t scale;
        memcpy(&scale, &bits, sizeof(scale));
        if (scale > 4)
          scale = 4;
        created.scale = (uint8_t)scale;
        NativeItemMapData_calculateMapCenter(&created, (double)old->center_x,
                                             (double)old->center_z, scale);
        created.dimension = old->dimension;
        created.metadata_known = true;
        created.dirty = true;
        /* The store can replace or move old here. All original reads
           precede setItemData; no borrowed native view survives it. */
        ok = NativeItemMapData_setItemData(world, &created) != NULL;
      }
    }
  }
  if (!ok)
    MCObjectHeap_fail(heap);
  MCObjectRootScope_end(&scope);
  return ok && !MCObjectHeap_failed(heap);
}
