#include "network/play/server/S34PacketMaps.h"
#include "network/play/server/native_packet.h"
#include "util/Vec4b.h"
#include "world/storage/MapData.h"
#include <stdio.h>
#include <string.h>

static void trace(MCObject *object, MCObjectVisitor visit, void *context) {
    if (MCObjectHeap_objectSize(object) < sizeof(S34PacketMaps)) {
        MCObjectHeap_fail(object->heap);
        return;
    }
    S34PacketMaps *packet = (S34PacketMaps *)object;
    packet->mapVisiblePlayersVec4b =
        (NativeTypedObjectArray *)visit((MCObject *)packet->mapVisiblePlayersVec4b, context);
    packet->mapDataBytes = (NativeByteArray *)visit((MCObject *)packet->mapDataBytes, context);
    packet->nativeVisiblePlayersContext = visit(packet->nativeVisiblePlayersContext, context);
}
static const MCObjectClass klass = {"net.minecraft.network.play.server.S34PacketMaps",
                                    MCObjectHeap_plainClone, trace, NULL};
static bool pointer_identity(const MCObject *object, void *context) { return object == context; }
static bool tracked(MCObjectHeap *heap, const MCObject *object) {
    return !object ||
           (object->heap == heap && MCObjectHeap_findObject(heap, object->klass, pointer_identity,
                                                            (void *)object) == object);
}
bool S34PacketMaps_isInstance(const MCObject *object) {
    return object && object->klass == &klass && tracked(object->heap, object) &&
           MCObjectHeap_objectSize(object) >= sizeof(S34PacketMaps);
}
S34PacketMaps *S34PacketMaps_nativeAllocate(MCObjectHeap *heap) {
    return (S34PacketMaps *)MCObjectHeap_alloc(heap, sizeof(S34PacketMaps), &klass);
}
S34PacketMaps *S34PacketMaps_new_empty(MCObjectHeap *heap) {
    return S34PacketMaps_nativeAllocate(heap);
}
static NativeArrayResult fail(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return NATIVE_ARRAY_FAILURE;
}
static NativeArrayResult begin(S34PacketMaps *packet, MCObjectRootScope *scope) {
    if (!packet)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectHeap *heap = packet->object.heap;
    if (!S34PacketMaps_isInstance((MCObject *)packet) || MCObjectHeap_failed(heap) ||
        !MCObjectRootScope_begin(scope, heap) ||
        !MCObjectRootScope_pin(scope, (MCObject *)packet)) {
        MCObjectRootScope_end(scope);
        return fail(heap);
    }
    return NATIVE_ARRAY_OK;
}
static NativeArrayResult finish(S34PacketMaps *packet, MCObjectRootScope *scope,
                                NativeArrayResult result) {
    if (MCObjectHeap_failed(packet->object.heap))
        result = NATIVE_ARRAY_FAILURE;
    MCObjectRootScope_end(scope);
    return result;
}
static int32_t bits(uint32_t value) {
    int32_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}
static int8_t byte_bits(uint8_t value) {
    int8_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}
static NativeArrayResult byte_read(MCObjectHeap *heap, NativeByteArray *array, int32_t index,
                                   int8_t *out) {
    if (!array)
        return NATIVE_ARRAY_EXCEPTION;
    if (!tracked(heap, (MCObject *)array) || !NativeByteArray_isInstance((MCObject *)array))
        return fail(heap);
    if (index < 0 || index >= array->length)
        return NATIVE_ARRAY_EXCEPTION;
    *out = array->values[index];
    return NATIVE_ARRAY_OK;
}
static NativeArrayResult byte_store(MCObjectHeap *heap, NativeByteArray *array, int32_t index,
                                    int8_t value) {
    if (!array)
        return NATIVE_ARRAY_EXCEPTION;
    if (!tracked(heap, (MCObject *)array) || !NativeByteArray_isInstance((MCObject *)array))
        return fail(heap);
    if (index < 0 || index >= array->length)
        return NATIVE_ARRAY_EXCEPTION;
    array->values[index] = value;
    MCObjectHeap_touch(heap);
    return NATIVE_ARRAY_OK;
}
static NativeArrayResult array_length(MCObjectHeap *heap, NativeTypedObjectArray *array,
                                      int32_t *out) {
    if (!array)
        return NATIVE_ARRAY_EXCEPTION;
    if (!tracked(heap, (MCObject *)array) ||
        !NativeTypedObjectArray_isInstance((MCObject *)array) || !array->componentType ||
        !tracked(heap, (MCObject *)array->componentType) ||
        !NativeJavaClass_isInstance((MCObject *)array->componentType))
        return fail(heap);
    NativeJavaClass *component = Vec4b_nativeClass(heap);
    bool compatible;
    if (!component ||
        !NativeJavaClass_isAssignableFrom(component, array->componentType, &compatible) ||
        !compatible)
        return fail(heap);
    *out = array->length;
    return NATIVE_ARRAY_OK;
}
NativeArrayResult S34PacketMaps_construct(S34PacketMaps *packet, int32_t mapId, int8_t scale,
                                          MCObject *visiblePlayers, NativeByteArray *colors,
                                          int32_t minX, int32_t minY, int32_t maxX, int32_t maxY) {
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(packet, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    MCObjectHeap *heap = packet->object.heap;
    packet->mapId = mapId;
    packet->mapScale = scale;
    int32_t size;
    result = NativeCollectionTyped_size(heap, visiblePlayers, packet->nativeVisiblePlayersMethods,
                                        &packet->nativeVisiblePlayersContext, &size);
    if (result != NATIVE_ARRAY_OK)
        goto done;
    NativeJavaClass *component = Vec4b_nativeClass(heap);
    if (!component) {
        result = fail(heap);
        goto done;
    }
    NativeTypedObjectArray *requested = NULL;
    result = NativeTypedObjectArray_new(heap, component, size, &requested);
    if (result != NATIVE_ARRAY_OK)
        goto done;
    MCObject *returned = NULL;
    result =
        NativeCollectionTyped_toArray(heap, visiblePlayers, packet->nativeVisiblePlayersMethods,
                                      &packet->nativeVisiblePlayersContext, requested, &returned);
    if (result != NATIVE_ARRAY_OK)
        goto done;
    if (returned) {
        if (!NativeTypedObjectArray_isInstance(returned)) {
            result = NATIVE_ARRAY_EXCEPTION;
            goto done;
        }
        bool compatible;
        if (!NativeTypedObjectArray_isAssignableTo((NativeTypedObjectArray *)returned, component,
                                                   &compatible)) {
            result = fail(heap);
            goto done;
        }
        if (!compatible) {
            result = NATIVE_ARRAY_EXCEPTION;
            goto done;
        }
    }
    packet->mapVisiblePlayersVec4b = (NativeTypedObjectArray *)returned;
    packet->mapMinX = minX;
    packet->mapMinY = minY;
    packet->mapMaxX = maxX;
    packet->mapMaxY = maxY;
    int32_t length = bits((uint32_t)maxX * (uint32_t)maxY);
    if (length < 0) {
        result = NATIVE_ARRAY_EXCEPTION;
        goto done;
    }
    NativeByteArray *data = NativeByteArray_new(heap, length);
    if (!data) {
        result = fail(heap);
        goto done;
    }
    packet->mapDataBytes = data;
    for (int32_t x = 0; x < maxX; ++x) {
        for (int32_t y = 0; y < maxY; ++y) {
            NativeByteArray *destination = packet->mapDataBytes;
            int32_t destinationIndex = bits((uint32_t)x + (uint32_t)y * (uint32_t)maxX);
            int32_t sourceIndex =
                bits((uint32_t)minX + (uint32_t)x + ((uint32_t)minY + (uint32_t)y) * 128u);
            int8_t value;
            result = byte_read(heap, colors, sourceIndex, &value);
            if (result != NATIVE_ARRAY_OK)
                goto done;
            result = byte_store(heap, destination, destinationIndex, value);
            if (result != NATIVE_ARRAY_OK)
                goto done;
        }
    }
done:
    return finish(packet, &scope, result);
}
static NativeArrayResult io_begin(S34PacketMaps *packet, PacketBuffer *io,
                                  MCObjectRootScope *scope) {
    NativeArrayResult result = begin(packet, scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    if (!io)
        return finish(packet, scope, NATIVE_ARRAY_EXCEPTION);
    if (!io->buffer || io->heap != packet->object.heap)
        return finish(packet, scope, fail(packet->object.heap));
    if (io->buffer->failed)
        return finish(packet, scope, NATIVE_ARRAY_EXCEPTION);
    return NATIVE_ARRAY_OK;
}
static NativeArrayResult io_status(S34PacketMaps *packet, PacketBuffer *io) {
    if (MCObjectHeap_failed(packet->object.heap))
        return NATIVE_ARRAY_FAILURE;
    return io->buffer->failed ? NATIVE_ARRAY_EXCEPTION : NATIVE_ARRAY_OK;
}
NativeArrayResult S34PacketMaps_readPacketData(S34PacketMaps *packet, PacketBuffer *io) {
    MCObjectRootScope scope = {0};
    NativeArrayResult result = io_begin(packet, io, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    MCObjectHeap *heap = packet->object.heap;
    int32_t value;
    if (!PacketBuffer_readVarIntFromBuffer(io, &value))
        goto io_done;
    packet->mapId = value;
    uint8_t scale = mc_get_u8(io->buffer);
    if (io->buffer->failed)
        goto io_done;
    packet->mapScale = byte_bits(scale);
    if (!PacketBuffer_readVarIntFromBuffer(io, &value))
        goto io_done;
    if (value < 0) {
        result = NATIVE_ARRAY_EXCEPTION;
        goto done;
    }
    NativeJavaClass *component = Vec4b_nativeClass(heap);
    if (!component) {
        result = fail(heap);
        goto done;
    }
    NativeTypedObjectArray *array = NULL;
    result = NativeTypedObjectArray_new(heap, component, value, &array);
    if (result != NATIVE_ARRAY_OK)
        goto done;
    packet->mapVisiblePlayersVec4b = array;
    for (int32_t i = 0; i < packet->mapVisiblePlayersVec4b->length; ++i) {
        uint8_t packed = mc_get_u8(io->buffer);
        if (io->buffer->failed)
            goto io_done;
        NativeTypedObjectArray *destination = packet->mapVisiblePlayersVec4b;
        Vec4b *icon = Vec4b_nativeAllocate(heap);
        if (!icon) {
            result = fail(heap);
            goto done;
        }
        int8_t type = (int8_t)((packed >> 4) & 15);
        int8_t x = byte_bits(mc_get_u8(io->buffer));
        if (io->buffer->failed)
            goto io_done;
        int8_t y = byte_bits(mc_get_u8(io->buffer));
        if (io->buffer->failed)
            goto io_done;
        int8_t direction = (int8_t)(packed & 15);
        if (!Vec4b_construct(icon, type, x, y, direction)) {
            result = fail(heap);
            goto done;
        }
        result = NativeTypedObjectArray_set(destination, i, (MCObject *)icon);
        if (result != NATIVE_ARRAY_OK)
            goto done;
    }
    uint8_t width = mc_get_u8(io->buffer);
    if (io->buffer->failed)
        goto io_done;
    packet->mapMaxX = width;
    if (packet->mapMaxX > 0) {
        uint8_t height = mc_get_u8(io->buffer);
        if (io->buffer->failed)
            goto io_done;
        packet->mapMaxY = height;
        uint8_t x = mc_get_u8(io->buffer);
        if (io->buffer->failed)
            goto io_done;
        packet->mapMinX = x;
        uint8_t y = mc_get_u8(io->buffer);
        if (io->buffer->failed)
            goto io_done;
        packet->mapMinY = y;
        NativeByteArray *data = NULL;
        result = PacketBuffer_readByteArray(io, &data);
        if (result != NATIVE_ARRAY_OK)
            goto done;
        packet->mapDataBytes = data;
    }
io_done:
    result = io_status(packet, io);
done:
    return finish(packet, &scope, result);
}
NativeArrayResult S34PacketMaps_writePacketData(S34PacketMaps *packet, PacketBuffer *io) {
    MCObjectRootScope scope = {0};
    NativeArrayResult result = io_begin(packet, io, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    MCObjectHeap *heap = packet->object.heap;
    if (!PacketBuffer_writeVarIntToBuffer(io, packet->mapId))
        goto io_done;
    mc_put_u8(io->buffer, (uint8_t)packet->mapScale);
    if (io->buffer->failed)
        goto io_done;
    int32_t count;
    result = array_length(heap, packet->mapVisiblePlayersVec4b, &count);
    if (result != NATIVE_ARRAY_OK)
        goto done;
    if (!PacketBuffer_writeVarIntToBuffer(io, count))
        goto io_done;
    NativeTypedObjectArray *array = packet->mapVisiblePlayersVec4b;
    result = array_length(heap, array, &count);
    if (result != NATIVE_ARRAY_OK)
        goto done;
    for (int32_t i = 0; i < count; ++i) {
        MCObject *element;
        result = NativeTypedObjectArray_get(array, i, &element);
        if (result != NATIVE_ARRAY_OK)
            goto done;
        if (!element) {
            result = NATIVE_ARRAY_EXCEPTION;
            goto done;
        }
        if (!Vec4b_isInstance(element)) {
            result = fail(heap);
            goto done;
        }
        Vec4b *icon = (Vec4b *)element;
        int8_t type = Vec4b_func_176110_a(icon);
        int8_t direction = Vec4b_func_176111_d(icon);
        mc_put_u8(io->buffer, (uint8_t)(((type & 15) << 4) | (direction & 15)));
        if (io->buffer->failed)
            goto io_done;
        int8_t x = Vec4b_func_176112_b(icon);
        mc_put_u8(io->buffer, (uint8_t)x);
        if (io->buffer->failed)
            goto io_done;
        int8_t y = Vec4b_func_176113_c(icon);
        mc_put_u8(io->buffer, (uint8_t)y);
        if (io->buffer->failed)
            goto io_done;
    }
    mc_put_u8(io->buffer, (uint8_t)packet->mapMaxX);
    if (io->buffer->failed)
        goto io_done;
    if (packet->mapMaxX > 0) {
        mc_put_u8(io->buffer, (uint8_t)packet->mapMaxY);
        if (io->buffer->failed)
            goto io_done;
        mc_put_u8(io->buffer, (uint8_t)packet->mapMinX);
        if (io->buffer->failed)
            goto io_done;
        mc_put_u8(io->buffer, (uint8_t)packet->mapMinY);
        if (io->buffer->failed)
            goto io_done;
        result = PacketBuffer_writeByteArray(io, packet->mapDataBytes);
        if (result != NATIVE_ARRAY_OK)
            goto done;
    }
io_done:
    /* This synchronous native writer can fail only on capacity/allocation;
       healthy Source NULL/store exceptions return through done instead. */
    if (io->buffer->failed)
        fail(heap);
    result = io_status(packet, io);
done:
    return finish(packet, &scope, result);
}
NativeArrayResult S34PacketMaps_setMapdataTo(S34PacketMaps *packet, MapData *map) {
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(packet, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    MCObjectHeap *heap = packet->object.heap;
    if (!map) {
        result = NATIVE_ARRAY_EXCEPTION;
        goto done;
    }
    if (!MapData_isInstance((MCObject *)map) || map->base.object.heap != heap ||
        !MCObjectRootScope_pin(&scope, (MCObject *)map)) {
        result = fail(heap);
        goto done;
    }
    map->scale = packet->mapScale;
    NativeLinkedHashMap *decorations = map->mapDecorations;
    if (!decorations) {
        result = NATIVE_ARRAY_EXCEPTION;
        goto done;
    }
    if (!NativeLinkedHashMap_isInstance((MCObject *)decorations) ||
        ((MCObject *)decorations)->heap != heap || !NativeLinkedHashMap_clear(decorations)) {
        result = fail(heap);
        goto done;
    }
    for (int32_t i = 0;; ++i) {
        int32_t count;
        result = array_length(heap, packet->mapVisiblePlayersVec4b, &count);
        if (result != NATIVE_ARRAY_OK)
            goto done;
        if (i >= count)
            break;
        MCObject *element;
        result = NativeTypedObjectArray_get(packet->mapVisiblePlayersVec4b, i, &element);
        if (result != NATIVE_ARRAY_OK)
            goto done;
        NativeLinkedHashMap *destination = map->mapDecorations;
        char identifier[32];
        snprintf(identifier, sizeof(identifier), "icon-%d", i);
        NBTString *key = NBTString_fromASCII(heap, identifier);
        if (!key) {
            result = fail(heap);
            goto done;
        }
        if (!destination) {
            result = NATIVE_ARRAY_EXCEPTION;
            goto done;
        }
        if (!NativeLinkedHashMap_isInstance((MCObject *)destination) ||
            ((MCObject *)destination)->heap != heap ||
            !NativeLinkedHashMap_put(destination, (MCObject *)key, element)) {
            result = fail(heap);
            goto done;
        }
    }
    for (int32_t x = 0; x < packet->mapMaxX; ++x) {
        for (int32_t y = 0; y < packet->mapMaxY; ++y) {
            NativeByteArray *destination = map->colors;
            int32_t destinationIndex = bits((uint32_t)packet->mapMinX + (uint32_t)x +
                                            ((uint32_t)packet->mapMinY + (uint32_t)y) * 128u);
            NativeByteArray *source = packet->mapDataBytes;
            int32_t sourceIndex = bits((uint32_t)x + (uint32_t)y * (uint32_t)packet->mapMaxX);
            int8_t value;
            result = byte_read(heap, source, sourceIndex, &value);
            if (result != NATIVE_ARRAY_OK)
                goto done;
            result = byte_store(heap, destination, destinationIndex, value);
            if (result != NATIVE_ARRAY_OK)
                goto done;
        }
    }
done:
    return finish(packet, &scope, result);
}
bool S34PacketMaps_processPacket(S34PacketMaps *packet, INetHandlerPlayClient handler) {
    MCObjectRootScope scope = {0};
    if (!S34PacketMaps_isInstance((MCObject *)packet)) {
        fail(packet ? packet->object.heap : NULL);
        return false;
    }
    if (!mc_client_handler_begin((MCObject *)packet, handler, &scope))
        return false;
    bool ok = handler.methods->handleMaps && handler.methods->handleMaps(handler.instance, packet);
    return mc_packet_handler_finish((MCObject *)packet, &scope, ok);
}
int32_t S34PacketMaps_getMapId(const S34PacketMaps *packet) {
    if (!S34PacketMaps_isInstance((const MCObject *)packet)) {
        fail(packet ? packet->object.heap : NULL);
        return 0;
    }
    return packet->mapId;
}
