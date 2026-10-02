#include "item/ItemStack.h"
#include "network/play/server/S34PacketMaps.h"
#include "world/storage/MapData.h"
#include <string.h>

static WorldSavedDataResult fail(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return WORLD_SAVED_DATA_FAILURE;
}
static WorldSavedDataResult outer(MapInfo *info, MapData **out) {
    MapData *value = info->outer;
    if (!value)
        return WORLD_SAVED_DATA_EXCEPTION;
    if (!MapData_isInstance((MCObject *)value) || value->base.object.heap != info->object.heap)
        return fail(info->object.heap);
    *out = value;
    return WORLD_SAVED_DATA_OK;
}
static int32_t bits(uint32_t value) {
    int32_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}
static WorldSavedDataResult packet_result(NativeArrayResult result) {
    switch (result) {
    case NATIVE_ARRAY_OK:
        return WORLD_SAVED_DATA_OK;
    case NATIVE_ARRAY_EXCEPTION:
        return WORLD_SAVED_DATA_EXCEPTION;
    case NATIVE_ARRAY_FAILURE:
        return WORLD_SAVED_DATA_FAILURE;
    }
    return WORLD_SAVED_DATA_FAILURE;
}
WorldSavedDataResult MapInfo_getPacket(MapInfo *info, ItemStack *stack, S34PacketMaps **out) {
    if (!info)
        return WORLD_SAVED_DATA_EXCEPTION;
    MCObjectHeap *heap = info->object.heap;
    MCObjectRootScope scope = {0};
    if (!out || !MapInfo_isInstance((MCObject *)info) || MCObjectHeap_failed(heap) ||
        !MCObjectRootScope_begin(&scope, heap) ||
        !MCObjectRootScope_pin(&scope, (MCObject *)info)) {
        MCObjectRootScope_end(&scope);
        return fail(heap);
    }
    bool dirty = info->field_176105_d;
    if (dirty) {
        info->field_176105_d = false;
    } else {
        int32_t old = info->field_176109_i;
        info->field_176109_i = bits((uint32_t)old + 1u);
        if (old % 5 != 0) {
            *out = NULL;
            MCObjectRootScope_end(&scope);
            return WORLD_SAVED_DATA_OK;
        }
    }
    S34PacketMaps *packet = S34PacketMaps_nativeAllocate(heap);
    WorldSavedDataResult result = WORLD_SAVED_DATA_FAILURE;
    if (!packet || !MCObjectRootScope_pin(&scope, (MCObject *)packet))
        goto done;
    if (!stack) {
        result = WORLD_SAVED_DATA_EXCEPTION;
        goto done;
    }
    if (!ItemStack_isInstance((MCObject *)stack) || stack->object.heap != heap) {
        result = fail(heap);
        goto done;
    }
    int32_t id = ItemStack_getMetadata(stack);
    if (MCObjectHeap_failed(heap))
        goto done;
    MapData *map;
    result = outer(info, &map);
    if (result != WORLD_SAVED_DATA_OK)
        goto done;
    int8_t scale = map->scale;
    result = outer(info, &map);
    if (result != WORLD_SAVED_DATA_OK)
        goto done;
    NativeLinkedHashMap *decorations = map->mapDecorations;
    if (!decorations) {
        result = WORLD_SAVED_DATA_EXCEPTION;
        goto done;
    }
    if (!NativeLinkedHashMap_isInstance((MCObject *)decorations) ||
        ((MCObject *)decorations)->heap != heap) {
        result = fail(heap);
        goto done;
    }
    NativeLinkedHashMapView *values = NativeLinkedHashMap_values(decorations);
    if (!values) {
        result = fail(heap);
        goto done;
    }
    result = outer(info, &map);
    if (result != WORLD_SAVED_DATA_OK)
        goto done;
    NativeByteArray *colors = map->colors;
    int32_t minX = 0, minY = 0, width = 0, height = 0;
    if (dirty) {
        minX = info->minX;
        minY = info->minY;
        width = bits((uint32_t)info->maxX + 1u - (uint32_t)info->minX);
        height = bits((uint32_t)info->maxY + 1u - (uint32_t)info->minY);
    }
    result = packet_result(S34PacketMaps_construct(packet, id, scale, (MCObject *)values, colors,
                                                   minX, minY, width, height));
    if (result == WORLD_SAVED_DATA_OK)
        *out = packet;
done:
    if (MCObjectHeap_failed(heap))
        result = WORLD_SAVED_DATA_FAILURE;
    MCObjectRootScope_end(&scope);
    return result;
}
