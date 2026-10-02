#include "item/ItemMapLoad.h"
#include "world/WorldDataStorage.h"
#include <inttypes.h>
#include <stdio.h>

static WorldSavedDataResult failure(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return WORLD_SAVED_DATA_FAILURE;
}
static bool identity(const MCObject *object, void *expected) { return object == expected; }
static bool tracked(MCObjectHeap *heap, MCObject *object) {
    return object && object->heap == heap && object->klass &&
           MCObjectHeap_findObject(heap, object->klass, identity, object) == object;
}
WorldSavedDataResult ItemMap_loadMapData(int32_t mapId, World *world, MapData **out) {
    if (!world)
        return out ? WORLD_SAVED_DATA_EXCEPTION : WORLD_SAVED_DATA_FAILURE;
    MCObjectHeap *heap = world->object.heap;
    MCObjectRootScope scope = {0};
    if (!out || !tracked(heap, (MCObject *)world) || !World_isInstance((MCObject *)world) ||
        MCObjectHeap_failed(heap) || !MCObjectRootScope_begin(&scope, heap))
        return failure(heap);
    WorldSavedDataResult result = WORLD_SAVED_DATA_FAILURE;
    MapData *map = NULL;
    if (!MCObjectRootScope_pin(&scope, (MCObject *)world))
        goto done;
    /* Native String-concatenation boundary: a fresh, noninterned UTF16 String
       for Source's map_ + signed int. It precedes the class literal/load call. */
    char text[32];
    int length = snprintf(text, sizeof(text), "map_%" PRId32, mapId);
    if (length < 0 || (size_t)length >= sizeof(text))
        goto done;
    NBTString *key = NBTString_fromASCII(heap, text);
    if (!key || !MCObjectRootScope_pin(&scope, (MCObject *)key))
        goto done;
    NativeJavaClass *clazz = MapData_nativeClass(heap);
    if (!clazz || !MCObjectRootScope_pin(&scope, (MCObject *)clazz))
        goto done;
    WorldSavedData *loaded = NULL;
    if (!World_loadItemData(world, clazz, key, &loaded))
        goto done;
    if (loaded) {
        if (!tracked(heap, (MCObject *)loaded))
            goto done;
        if (!MapData_isInstance((MCObject *)loaded)) {
            result = WORLD_SAVED_DATA_EXCEPTION;
            goto done;
        }
        map = (MapData *)loaded;
    } else {
        map = MapData_new(heap, key, NULL, NULL);
        if (!map || !MCObjectRootScope_pin(&scope, (MCObject *)map))
            goto done;
        if (!World_setItemData(world, key, &map->base))
            goto done;
    }
    result = WORLD_SAVED_DATA_OK;
done:
    if (MCObjectHeap_failed(heap) || result == WORLD_SAVED_DATA_FAILURE)
        result = failure(heap);
    if (result == WORLD_SAVED_DATA_OK)
        *out = map;
    MCObjectRootScope_end(&scope);
    return result;
}
