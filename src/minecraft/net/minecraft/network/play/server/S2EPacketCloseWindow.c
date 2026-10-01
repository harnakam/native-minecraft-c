#include "network/play/server/S2EPacketCloseWindow.h"
#include "network/play/server/native_packet.h"
static const MCObjectClass klass = {"net.minecraft.network.play.server.S2EPacketCloseWindow",
                                    MCObjectHeap_plainClone, NULL, NULL};
S2EPacketCloseWindow *S2EPacketCloseWindow_new_empty(MCObjectHeap *heap) {
    return (S2EPacketCloseWindow *)MCObjectHeap_alloc(heap, sizeof(S2EPacketCloseWindow), &klass);
}
S2EPacketCloseWindow *S2EPacketCloseWindow_new(MCObjectHeap *heap, int32_t windowId) {
    S2EPacketCloseWindow *p = S2EPacketCloseWindow_new_empty(heap);
    if (p)
        p->windowId = windowId;
    return p;
}
bool S2EPacketCloseWindow_readPacketData(S2EPacketCloseWindow *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    uint8_t value = mc_get_u8(b->buffer);
    if (!b->buffer->failed)
        p->windowId = value;
    return mc_packet_finish((MCObject *)p, b, &scope, !b->buffer->failed);
}
bool S2EPacketCloseWindow_writePacketData(S2EPacketCloseWindow *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    mc_put_u8(b->buffer, (uint8_t)p->windowId);
    return mc_packet_finish((MCObject *)p, b, &scope, !b->buffer->failed);
}
bool S2EPacketCloseWindow_processPacket(S2EPacketCloseWindow *p, INetHandlerPlayClient h) {
    MCObjectRootScope scope = {0};
    if (!mc_client_handler_begin((MCObject *)p, h, &scope))
        return false;
    bool ok = h.methods->handleCloseWindow && h.methods->handleCloseWindow(h.instance, p);
    return mc_packet_handler_finish((MCObject *)p, &scope, ok);
}
