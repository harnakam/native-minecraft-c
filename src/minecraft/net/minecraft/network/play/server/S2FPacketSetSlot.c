#include "network/play/server/S2FPacketSetSlot.h"
#include "network/play/server/native_packet.h"
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
    S2FPacketSetSlot *p = (S2FPacketSetSlot *)o;
    p->item = (ItemStack *)v((MCObject *)p->item, c);
}
static const MCObjectClass klass = {"net.minecraft.network.play.server.S2FPacketSetSlot",
                                    MCObjectHeap_plainClone, trace, NULL};
S2FPacketSetSlot *S2FPacketSetSlot_new_empty(MCObjectHeap *heap) {
    return (S2FPacketSetSlot *)MCObjectHeap_alloc(heap, sizeof(S2FPacketSetSlot), &klass);
}
S2FPacketSetSlot *S2FPacketSetSlot_new(MCObjectHeap *heap, int32_t windowId, int32_t slot,
                                       ItemStack *item) {
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap))
        return NULL;
    S2FPacketSetSlot *p = S2FPacketSetSlot_new_empty(heap);
    if (p) {
        p->windowId = windowId;
        p->slot = slot;
        p->item = item ? ItemStack_copy(heap, item) : NULL;
        if (item && !p->item)
            p = NULL;
    }
    MCObjectRootScope_end(&scope);
    return p;
}
bool S2FPacketSetSlot_readPacketData(S2FPacketSetSlot *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    int32_t value = mc_packet_byte(b);
    if (b->buffer->failed)
        goto done;
    p->windowId = value;
    value = mc_get_i16(b->buffer);
    if (b->buffer->failed)
        goto done;
    p->slot = value;
    if (!PacketBuffer_readItemStackFromBuffer(b, &p->item))
        goto done;
done:
    return mc_packet_finish((MCObject *)p, b, &scope, !b->buffer->failed);
}
bool S2FPacketSetSlot_writePacketData(S2FPacketSetSlot *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    mc_put_u8(b->buffer, (uint8_t)p->windowId);
    mc_put_i16(b->buffer, mc_packet_short(p->slot));
    return mc_packet_finish((MCObject *)p, b, &scope,
                            PacketBuffer_writeItemStackToBuffer(b, p->item));
}
bool S2FPacketSetSlot_processPacket(S2FPacketSetSlot *p, INetHandlerPlayClient h) {
    MCObjectRootScope scope = {0};
    if (!mc_client_handler_begin((MCObject *)p, h, &scope))
        return false;
    bool ok = h.methods->handleSetSlot && h.methods->handleSetSlot(h.instance, p);
    return mc_packet_handler_finish((MCObject *)p, &scope, ok);
}
int32_t S2FPacketSetSlot_func_149175_c(const S2FPacketSetSlot *p) {
    return p->windowId;
}
int32_t S2FPacketSetSlot_func_149173_d(const S2FPacketSetSlot *p) {
    return p->slot;
}
ItemStack *S2FPacketSetSlot_func_149174_e(const S2FPacketSetSlot *p) {
    return p->item;
}
