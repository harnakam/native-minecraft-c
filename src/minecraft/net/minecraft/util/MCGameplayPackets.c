#include "util/MCGameplayPackets.h"
#include "util/MCPacketQueue.h"
#include "network/NativePacket.h"
#define CODEC(type, id)                                                                            \
    static bool write_##type(MCObject *o, PacketBuffer *b) {                                       \
        return type##_writePacketData((type *)o, b);                                               \
    }
CODEC(S2EPacketCloseWindow, 0x2e)
CODEC(S2FPacketSetSlot, 0x2f)
CODEC(S30PacketWindowItems, 0x30)
CODEC(S32PacketConfirmTransaction, 0x32)
CODEC(S1CPacketEntityMetadata, 0x1c)
#undef CODEC
static const MCPacketCodec codecs[] = {
    {0x04, NativePacket_isInstance, NativePacket_writePacketData},
    {0x0d, NativePacket_isInstance, NativePacket_writePacketData},
    {0x0e, NativePacket_isInstance, NativePacket_writePacketData},
    {0x12, NativePacket_isInstance, NativePacket_writePacketData},
    {0x13, NativePacket_isInstance, NativePacket_writePacketData},
    {0x18, NativePacket_isInstance, NativePacket_writePacketData},
    {0x1c, S1CPacketEntityMetadata_isInstance, write_S1CPacketEntityMetadata},
    {0x29, NativePacket_isInstance, NativePacket_writePacketData},
    {0x2d, NativePacket_isInstance, NativePacket_writePacketData},
    {0x2e, S2EPacketCloseWindow_isInstance, write_S2EPacketCloseWindow},
    {0x2f, S2FPacketSetSlot_isInstance, write_S2FPacketSetSlot},
    {0x30, S30PacketWindowItems_isInstance, write_S30PacketWindowItems},
    {0x32, S32PacketConfirmTransaction_isInstance, write_S32PacketConfirmTransaction},
    {0x34, NativePacket_isInstance, NativePacket_writePacketData},
    {0x37, NativePacket_isInstance, NativePacket_writePacketData}};
static const MCPacketQueueProfile profile = {codecs, sizeof codecs / sizeof *codecs, true};
static MCObject *packets(const MCGameplayPlayer *p) {
    if (!p)
        return NULL;
    if (!MCGameplayPlayer_isInstance((const MCObject *)p) || !p->pendingPackets ||
        p->pendingPackets->heap != p->object.heap ||
        !MCPacketQueue_isInstance(p->pendingPackets, &profile)) {
        MCObjectHeap_fail(p->object.heap);
        return NULL;
    }
    return p->pendingPackets;
}
bool MCGameplayPackets_bind(MCGameplayPlayer *p) {
    if (!p || MCObjectHeap_failed(p->object.heap))
        return false;
    if (!MCGameplayPlayer_isInstance((const MCObject *)p)) {
        MCObjectHeap_fail(p->object.heap);
        return false;
    }
    if (!p->pendingPackets) {
        MCObjectHeap *heap = p->object.heap;
        if (!p->worldObj || !MCGameplayWorld_isInstance((MCObject *)p->worldObj) ||
            p->worldObj->object.heap != heap || !p->worldObj->owners ||
            p->worldObj->owners->object.heap != heap) {
            MCObjectHeap_fail(heap);
            return false;
        }
        p->pendingPackets = MCPacketQueue_new(heap, p->worldObj->owners, &profile);
        if (!p->pendingPackets)
            return false;
        MCObjectHeap_touch(heap);
    }
    if (!packets(p))
        return false;
    static const EntityPlayerMPWindowsDependencies dependencies = {
        MCGameplayPackets_sendWindowItems, MCGameplayPackets_sendSetSlot,
        MCGameplayPackets_sendCloseWindow};
    return EntityPlayerMPWindows_bind(p, &dependencies);
}
static bool append(MCGameplayPlayer *p, MCObject *packet, int32_t id) {
    MCObject *q = packets(p);
    return q && MCPacketQueue_append(q, packet, id);
}
bool MCGameplayPackets_sendWindowItems(MCGameplayPlayer *p, int32_t w, ContainerList *l) {
    return p && append(p, (MCObject *)S30PacketWindowItems_new(p->object.heap, w, l), 0x30);
}
bool MCGameplayPackets_sendSetSlot(MCGameplayPlayer *p, int32_t w, int32_t s, ItemStack *i) {
    return p && append(p, (MCObject *)S2FPacketSetSlot_new(p->object.heap, w, s, i), 0x2f);
}
bool MCGameplayPackets_sendCloseWindow(MCGameplayPlayer *p, int32_t w) {
    return p && append(p, (MCObject *)S2EPacketCloseWindow_new(p->object.heap, w), 0x2e);
}
bool MCGameplayPackets_sendConfirmTransaction(MCGameplayPlayer *p, int32_t w, int16_t a, bool b) {
    return p &&
           append(p, (MCObject *)S32PacketConfirmTransaction_new(p->object.heap, w, a, b), 0x32);
}
bool MCGameplayPackets_sendMetadata(MCGameplayPlayer *p,S1CPacketEntityMetadata *packet) {
    return p&&append(p,(MCObject *)packet,0x1c);
}
bool MCGameplayPackets_sendNative(MCGameplayPlayer *p,const mc_buf *message) {
    if (!p||!message||message->failed||!message->data||message->len>message->cap)return false;
    mc_buf input=*message;input.pos=0;int32_t id=mc_get_varint(&input);
    if (input.failed)return false;
    return append(p,NativePacket_new(p->object.heap,message),id);
}
int32_t MCGameplayPackets_count(const MCGameplayPlayer *p) {
    MCObject *q = packets(p);
    return q ? MCPacketQueue_count(q) : 0;
}
MCObject *MCGameplayPackets_packetAt(MCGameplayPlayer *p, int32_t index,
                                     MCGameplayPacketKind *kind) {
    MCObject *q = packets(p);
    int32_t id = 0;
    MCObject *packet = q ? MCPacketQueue_packetAt(q, index, &id) : NULL;
    if (packet && kind)
        *kind = (MCGameplayPacketKind)id;
    return packet;
}
bool MCGameplayPackets_encodeAt(MCGameplayPlayer *p, int32_t index, mc_buf *out) {
    MCObject *q = packets(p);
    return q && MCPacketQueue_encodeAt(q, index, out);
}
bool MCGameplayPackets_validate(MCGameplayPlayer *p) {
    MCObject *q = packets(p);
    if (!q)
        return false;
    if (!MCGameplayWorld_isInstance((MCObject *)p->worldObj) ||
        p->worldObj->object.heap != p->object.heap) {
        MCObjectHeap_fail(p->object.heap);
        return false;
    }
    return MCPacketQueue_validateForOwners(q, p->worldObj->owners);
}
MCGameplayPacketsResult MCGameplayPackets_flush(MCGameplay *game, size_t index,
                                                MCGameplayPacketSink sink, void *context) {
    MCPacketQueueResult result = MCPacketQueue_flush(game, index, &profile, sink, context);
    switch (result) {
    case MC_PACKET_QUEUE_SENT:
        return MC_GAMEPLAY_PACKETS_SENT;
    case MC_PACKET_QUEUE_UNCOMMITTED:
        return MC_GAMEPLAY_PACKETS_UNCOMMITTED;
    default:
        return MC_GAMEPLAY_PACKETS_FAILED;
    }
}
