#include "network/play/client/C09PacketHeldItemChange.h"
#include "network/play/client/native_packet.h"
static const MCObjectClass klass = {"net.minecraft.network.play.client.C09PacketHeldItemChange",
                                    MCObjectHeap_plainClone, NULL, NULL};
bool C09PacketHeldItemChange_isInstance(const MCObject *o) { return o && o->klass == &klass; }
C09PacketHeldItemChange *C09PacketHeldItemChange_new_empty(MCObjectHeap *h) {
    return (C09PacketHeldItemChange *)MCObjectHeap_alloc(h, sizeof(C09PacketHeldItemChange),
                                                         &klass);
}
C09PacketHeldItemChange *C09PacketHeldItemChange_new(MCObjectHeap *h, int32_t slot) {
    C09PacketHeldItemChange *p = C09PacketHeldItemChange_new_empty(h);
    if (p)
        p->slotId = slot;
    return p;
}
bool C09PacketHeldItemChange_readPacketData(C09PacketHeldItemChange *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    int32_t slot = mc_get_i16(b->buffer);
    if (!b->buffer->failed)
        p->slotId = slot;
    return mc_packet_finish((MCObject *)p, b, &scope, !b->buffer->failed);
}
bool C09PacketHeldItemChange_writePacketData(C09PacketHeldItemChange *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    mc_put_i16(b->buffer, mc_packet_short(p->slotId));
    return mc_packet_finish((MCObject *)p, b, &scope, !b->buffer->failed);
}
bool C09PacketHeldItemChange_processPacket(C09PacketHeldItemChange *p,
                                           INetHandlerPlayServer handler) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_handler_begin((MCObject *)p, handler, &scope))
        return false;
    bool ok = handler.methods->processHeldItemChange &&
              handler.methods->processHeldItemChange(handler.instance, p);
    return mc_packet_handler_finish((MCObject *)p, &scope, ok);
}
int32_t C09PacketHeldItemChange_getSlotId(const C09PacketHeldItemChange *p) { return p->slotId; }
