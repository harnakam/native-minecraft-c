#ifndef C919_NATIVE_SERVER_PACKET_H
#define C919_NATIVE_SERVER_PACKET_H
#include "network/play/client/native_packet.h"
#include "network/play/INetHandlerPlayClient.h"
static inline bool mc_client_handler_begin(MCObject *packet, INetHandlerPlayClient handler,
                                           MCObjectRootScope *scope) {
    if (!packet)
        return false;
    if (!handler.instance || !handler.methods || handler.instance->heap != packet->heap) {
        MCObjectHeap_fail(packet->heap);
        return false;
    }
    if (!MCObjectRootScope_begin(scope, packet->heap))
        return false;
    if (MCObjectRootScope_pin(scope, packet) && MCObjectRootScope_pin(scope, handler.instance))
        return true;
    MCObjectRootScope_end(scope);
    return false;
}
#endif
