#ifndef C919_NATIVE_PACKET_H
#define C919_NATIVE_PACKET_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayServer.h"
#include <string.h>
/* Native primitive IO/lifetime adapters. Original bodies retain source field
   assignment order and stop at a failed read, as Java IOException would. */
static inline int32_t mc_packet_byte(PacketBuffer *buffer) {
    uint8_t bits=mc_get_u8(buffer->buffer); int8_t value; memcpy(&value,&bits,sizeof(value)); return value;
}
static inline int16_t mc_packet_short(int32_t value) {
    uint16_t bits=(uint16_t)value; int16_t result; memcpy(&result,&bits,sizeof(result)); return result;
}
static inline bool mc_packet_begin(MCObject *packet,PacketBuffer *buffer,MCObjectRootScope *scope) {
    if (!packet || !buffer || !buffer->buffer || buffer->heap!=packet->heap) {
        if (packet) MCObjectHeap_fail(packet->heap);
        if (buffer && buffer->buffer) buffer->buffer->failed=true;
        return false;
    }
    if (buffer->buffer->failed || !MCObjectRootScope_begin(scope,packet->heap)) {
        buffer->buffer->failed=true; return false;
    }
    if (MCObjectRootScope_pin(scope,packet)) return true;
    MCObjectRootScope_end(scope); buffer->buffer->failed=true; return false;
}
static inline bool mc_packet_finish(MCObject *packet,PacketBuffer *buffer,MCObjectRootScope *scope,bool ok) {
    if (!ok || MCObjectHeap_failed(packet->heap)) buffer->buffer->failed=true;
    MCObjectRootScope_end(scope); return !buffer->buffer->failed;
}
static inline bool mc_packet_handler_begin(MCObject *packet,INetHandlerPlayServer handler,MCObjectRootScope *scope) {
    if (!packet) return false;
    if (!handler.instance || !handler.methods || handler.instance->heap!=packet->heap) {
        MCObjectHeap_fail(packet->heap); return false;
    }
    if (!MCObjectRootScope_begin(scope,packet->heap)) return false;
    if (MCObjectRootScope_pin(scope,packet) && MCObjectRootScope_pin(scope,handler.instance)) return true;
    MCObjectRootScope_end(scope); return false;
}
static inline bool mc_packet_handler_finish(MCObject *packet,MCObjectRootScope *scope,bool ok) {
    if (!ok) MCObjectHeap_fail(packet->heap);
    MCObjectRootScope_end(scope); return !MCObjectHeap_failed(packet->heap);
}
#endif
