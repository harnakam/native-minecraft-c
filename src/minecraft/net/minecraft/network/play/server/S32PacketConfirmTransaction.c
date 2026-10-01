#include "network/play/server/S32PacketConfirmTransaction.h"
#include "network/play/server/native_packet.h"
static const MCObjectClass klass = {"net.minecraft.network.play.server.S32PacketConfirmTransaction",
                                    MCObjectHeap_plainClone, NULL, NULL};
S32PacketConfirmTransaction *S32PacketConfirmTransaction_new_empty(MCObjectHeap *heap) {
    return (S32PacketConfirmTransaction *)MCObjectHeap_alloc(
        heap, sizeof(S32PacketConfirmTransaction), &klass);
}
S32PacketConfirmTransaction *S32PacketConfirmTransaction_new(MCObjectHeap *heap, int32_t windowId,
                                                             int16_t action, bool accepted) {
    S32PacketConfirmTransaction *p = S32PacketConfirmTransaction_new_empty(heap);
    if (p) {
        p->windowId = windowId;
        p->actionNumber = action;
        p->field_148893_c = accepted;
    }
    return p;
}
bool S32PacketConfirmTransaction_readPacketData(S32PacketConfirmTransaction *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    uint8_t window = mc_get_u8(b->buffer);
    if (b->buffer->failed)
        goto done;
    p->windowId = window;
    int16_t action = mc_get_i16(b->buffer);
    if (b->buffer->failed)
        goto done;
    p->actionNumber = action;
    uint8_t accepted = mc_get_u8(b->buffer);
    if (!b->buffer->failed)
        p->field_148893_c = accepted != 0;
done:
    return mc_packet_finish((MCObject *)p, b, &scope, !b->buffer->failed);
}
bool S32PacketConfirmTransaction_writePacketData(S32PacketConfirmTransaction *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    mc_put_u8(b->buffer, (uint8_t)p->windowId);
    mc_put_i16(b->buffer, p->actionNumber);
    mc_put_u8(b->buffer, p->field_148893_c ? 1 : 0);
    return mc_packet_finish((MCObject *)p, b, &scope, !b->buffer->failed);
}
bool S32PacketConfirmTransaction_processPacket(S32PacketConfirmTransaction *p,
                                               INetHandlerPlayClient h) {
    MCObjectRootScope scope = {0};
    if (!mc_client_handler_begin((MCObject *)p, h, &scope))
        return false;
    bool ok =
        h.methods->handleConfirmTransaction && h.methods->handleConfirmTransaction(h.instance, p);
    return mc_packet_handler_finish((MCObject *)p, &scope, ok);
}
int32_t S32PacketConfirmTransaction_getWindowId(const S32PacketConfirmTransaction *p) {
    return p->windowId;
}
int16_t S32PacketConfirmTransaction_getActionNumber(const S32PacketConfirmTransaction *p) {
    return p->actionNumber;
}
bool S32PacketConfirmTransaction_func_148888_e(const S32PacketConfirmTransaction *p) {
    return p->field_148893_c;
}
