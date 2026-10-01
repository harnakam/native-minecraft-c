#include "network/NativePacket.h"
#include <string.h>
typedef struct {
    MCObject object;
    int32_t id;
    size_t length;
    uint8_t bytes[];
} NativePacket;
static const MCObjectClass klass = {"C919.native.EncodedPacket", MCObjectHeap_plainClone, NULL,
                                    NULL};
bool NativePacket_isInstance(const MCObject *object) {
    return object && object->klass == &klass;
}
MCObject *NativePacket_new(MCObjectHeap *heap, const mc_buf *message) {
    if (!message || message->failed || !message->data || !message->len || message->len > 2097152u ||
        message->len > message->cap) {
        MCObjectHeap_fail(heap);
        return NULL;
    }
    mc_buf input = *message;
    input.pos = 0;
    int32_t id = mc_get_varint(&input);
    if (input.failed || id < 0) {
        MCObjectHeap_fail(heap);
        return NULL;
    }
    size_t length = input.len - input.pos;
    NativePacket *packet =
        (NativePacket *)MCObjectHeap_alloc(heap, sizeof(*packet) + length, &klass);
    if (!packet)
        return NULL;
    packet->id = id;
    packet->length = length;
    if (length)
        memcpy(packet->bytes, input.data + input.pos, length);
    return (MCObject *)packet;
}
bool NativePacket_writePacketData(MCObject *object, PacketBuffer *buffer) {
    if (!NativePacket_isInstance(object) || !buffer || buffer->heap != object->heap ||
        !buffer->buffer) {
        MCObjectHeap_fail(object ? object->heap : NULL);
        return false;
    }
    NativePacket *packet = (NativePacket *)object;
    mc_buf prefix = *buffer->buffer;
    prefix.pos = 0;
    if (mc_get_varint(&prefix) != packet->id || prefix.failed || prefix.pos != prefix.len) {
        MCObjectHeap_fail(object->heap);
        return false;
    }
    mc_put_bytes(buffer->buffer, packet->bytes, packet->length);
    return !buffer->buffer->failed;
}
