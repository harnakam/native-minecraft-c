#include "util/MCGameplayClientPackets.h"
#define CODEC(type)                                                                                \
    static bool write_##type(MCObject *o, PacketBuffer *b) {                                       \
        return type##_writePacketData((type *)o, b);                                               \
    }
CODEC(C03PacketPlayer)
CODEC(C04PacketPlayerPosition)
CODEC(C05PacketPlayerLook)
CODEC(C06PacketPlayerPosLook)
CODEC(C0BPacketEntityAction)
CODEC(C08PacketPlayerBlockPlacement)
CODEC(C07PacketPlayerDigging)
CODEC(C09PacketHeldItemChange)
CODEC(C0DPacketCloseWindow)
CODEC(C0EPacketClickWindow)
CODEC(C0FPacketConfirmTransaction)
CODEC(C10PacketCreativeInventoryAction)
CODEC(C13PacketPlayerAbilities)
#undef CODEC
static const MCPacketCodec codecs[] = {
    {0x03, C03PacketPlayer_nativeBaseIsInstance, write_C03PacketPlayer},
    {0x04, C04PacketPlayerPosition_isInstance, write_C04PacketPlayerPosition},
    {0x05, C05PacketPlayerLook_isInstance, write_C05PacketPlayerLook},
    {0x06, C06PacketPlayerPosLook_isInstance, write_C06PacketPlayerPosLook},
    {0x0b, C0BPacketEntityAction_isInstance, write_C0BPacketEntityAction},
    {0x07, C07PacketPlayerDigging_isInstance, write_C07PacketPlayerDigging},
    {0x08, C08PacketPlayerBlockPlacement_isInstance, write_C08PacketPlayerBlockPlacement},
    {0x09, C09PacketHeldItemChange_isInstance, write_C09PacketHeldItemChange},
    {0x0d, C0DPacketCloseWindow_isInstance, write_C0DPacketCloseWindow},
    {0x0e, C0EPacketClickWindow_isInstance, write_C0EPacketClickWindow},
    {0x0f, C0FPacketConfirmTransaction_isInstance, write_C0FPacketConfirmTransaction},
    {0x10, C10PacketCreativeInventoryAction_isInstance, write_C10PacketCreativeInventoryAction},
    {0x13,C13PacketPlayerAbilities_isInstance,write_C13PacketPlayerAbilities}};
static const MCPacketQueueProfile profile = {codecs, sizeof codecs / sizeof *codecs, false};
static MCObject *packets(const MCGameplayPlayer *p) {
    if (!p)
        return NULL;
    if (!MCGameplayPlayer_isInstance((const MCObject *)p) || !p->pendingPackets ||
        p->pendingPackets->heap != p->living.entity.object.heap ||
        !MCPacketQueue_isInstance(p->pendingPackets, &profile)) {
        MCObjectHeap_fail(p->living.entity.object.heap);
        return NULL;
    }
    return p->pendingPackets;
}
bool MCGameplayClientPackets_bind(MCGameplayPlayer *p) {
    if (!p || MCObjectHeap_failed(p->living.entity.object.heap))
        return false;
    MCObjectHeap *heap = p->living.entity.object.heap;
    if (!MCGameplayPlayer_isInstance((MCObject *)p) || !((MCGameplayWorld *)(p->living.entity.worldObj)) ||
        !MCGameplayWorld_isInstance((MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj))) || ((MCGameplayWorld *)(p->living.entity.worldObj))->object.heap != heap ||
        !((MCGameplayWorld *)(p->living.entity.worldObj))->isRemote || !((MCGameplayWorld *)(p->living.entity.worldObj))->owners || ((MCGameplayWorld *)(p->living.entity.worldObj))->owners->object.heap != heap) {
        MCObjectHeap_fail(heap);
        return false;
    }
    if (!p->pendingPackets) {
        p->pendingPackets = MCPacketQueue_new(heap, ((MCGameplayWorld *)(p->living.entity.worldObj))->owners, &profile);
        if (!p->pendingPackets)
            return false;
        MCObjectHeap_touch(heap);
    }
    return packets(p) != NULL;
}
bool MCGameplayClientPackets_addToSendQueue(MCGameplayPlayer *p, MCObject *packet) {
    MCObject *q = packets(p);
    if (!q)
        return false;
    for (size_t i = 0; i < profile.count; i++)
        if (codecs[i].isInstance(packet))
            return MCPacketQueue_append(q, packet, codecs[i].id);
    MCObjectHeap_fail(p->living.entity.object.heap);
    return false;
}
int32_t MCGameplayClientPackets_count(const MCGameplayPlayer *p) {
    MCObject *q = packets(p);
    return q ? MCPacketQueue_count(q) : 0;
}
MCObject *MCGameplayClientPackets_packetAt(MCGameplayPlayer *p, int32_t index, int32_t *id) {
    MCObject *q = packets(p);
    return q ? MCPacketQueue_packetAt(q, index, id) : NULL;
}
bool MCGameplayClientPackets_encodeAt(MCGameplayPlayer *p, int32_t index, mc_buf *out) {
    MCObject *q = packets(p);
    return q && MCPacketQueue_encodeAt(q, index, out);
}
bool MCGameplayClientPackets_validate(MCGameplayPlayer *p) {
    MCObject *q = packets(p);
    if (!q)
        return false;
    if (!MCGameplayWorld_isInstance((MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj))) ||
        ((MCGameplayWorld *)(p->living.entity.worldObj))->object.heap != p->living.entity.object.heap) {
        MCObjectHeap_fail(p->living.entity.object.heap);
        return false;
    }
    return MCPacketQueue_validateForOwners(q, ((MCGameplayWorld *)(p->living.entity.worldObj))->owners);
}
bool MCGameplayClientPackets_validateFrame(MCGameplayObjects *objects, void *context) {
    (void)context;
    if (!objects || !MCGameplayWorld_isInstance(objects->world) ||
        !((MCGameplayWorld *)objects->world)->isRemote)
        return false;
    for (size_t i = 0; i < MC_TRANSFER_MAX_PLAYERS; i++)
        if (objects->players[i]) {
            MCObject *actor = objects->players[i];
            if (!MCGameplayPlayer_isInstance(actor) || actor->heap != objects->object.heap)
                return false;
            MCGameplayPlayer *p = (MCGameplayPlayer *)actor;
            if ((MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj)) != objects->world ||
                (p->pendingPackets && !MCGameplayClientPackets_validate(p)))
                return false;
        }
    return !MCObjectHeap_failed(objects->object.heap);
}
MCPacketQueueResult MCGameplayClientPackets_flush(MCGameplay *game, size_t index,
                                                  MCPacketQueueSink sink, void *context) {
    return MCPacketQueue_flush(game, index, &profile, sink, context);
}
