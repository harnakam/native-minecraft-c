#include "world/WorldDataStorage.h"
#include <limits.h>
#include <string.h>
static bool valid(const World *world) {
    MCObjectHeap *heap = world ? world->object.heap : NULL;
    if (!World_isInstance((MCObject *)world) ||
        !MapStorage_isInstance((MCObject *)world->mapStorage) ||
        world->mapStorage->object.heap != heap || MCObjectHeap_failed(heap)) {
        MCObjectHeap_fail(heap);
        return false;
    }
    return true;
}
bool World_getUniqueDataId(World *world, NBTString *key, int32_t *output) {
    if (!valid(world) || !output) {
        MCObjectHeap_fail(world ? world->object.heap : NULL);
        return false;
    }
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, world->object.heap))
        return false;
    bool ok = MapStorage_getUniqueDataId(world->mapStorage, key, output);
    MCObjectRootScope_end(&scope);
    return ok;
}
bool World_loadItemData(World *world, NativeJavaClass *clazz, NBTString *key,
                        WorldSavedData **output) {
    if (!valid(world) || !output) {
        MCObjectHeap_fail(world ? world->object.heap : NULL);
        return false;
    }
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, world->object.heap))
        return false;
    MapStorage *storage = world->mapStorage;
    bool ok = MCObjectRootScope_pin(&scope, (MCObject *)world) &&
              MCObjectRootScope_pin(&scope, (MCObject *)storage) &&
              MapStorage_loadData(storage, clazz, key, output);
    MCObjectRootScope_end(&scope);
    return ok;
}
bool World_setItemData(World *world, NBTString *key, WorldSavedData *data) {
    if (!valid(world))
        return false;
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, world->object.heap))
        return false;
    MapStorage *storage = world->mapStorage;
    bool ok = MCObjectRootScope_pin(&scope, (MCObject *)world) &&
              MCObjectRootScope_pin(&scope, (MCObject *)storage) &&
              MapStorage_setData(storage, key, data);
    MCObjectRootScope_end(&scope);
    return ok;
}
bool World_nativeMapNextProjection(const World *world, int32_t *output) {
    return valid(world) && MapStorage_nativeGetMapNextProjection(world->mapStorage, output);
}
bool World_nativeImportMapNextProjection(World *world, int32_t next) {
    if (!valid(world) || next < 0 || next > UINT16_MAX) {
        MCObjectHeap_fail(world ? world->object.heap : NULL);
        return false;
    }
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, world->object.heap))
        return false;
    NBTString *key = NBTString_fromASCII(world->object.heap, "map");
    uint16_t bits = (uint16_t)((uint32_t)next - 1u);
    int16_t last;
    memcpy(&last, &bits, sizeof(last));
    bool ok = key && MapStorage_nativeImportExactShort(world->mapStorage, key, last);
    MCObjectRootScope_end(&scope);
    return ok;
}
