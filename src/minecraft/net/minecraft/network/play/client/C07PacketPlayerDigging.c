#include "network/play/client/C07PacketPlayerDigging.h"
#include "network/play/client/native_packet.h"
const C07PacketPlayerDiggingAction C07PacketPlayerDigging_ACTIONS[6] = {{0}, {1}, {2},
                                                                        {3}, {4}, {5}};
const MCNativeEnumFacing C07PacketPlayerDigging_FACINGS[6] = {{0}, {1}, {2}, {3}, {4}, {5}};
const C07PacketPlayerDiggingAction *C07PacketPlayerDigging_action(int32_t ordinal) {
    return ordinal >= 0 && ordinal < 6 ? &C07PacketPlayerDigging_ACTIONS[ordinal] : NULL;
}
const MCNativeEnumFacing *C07PacketPlayerDigging_facing(int32_t index) {
    int32_t remainder = index % 6;
    return &C07PacketPlayerDigging_FACINGS[remainder < 0 ? -remainder : remainder];
}
static bool action_valid(const C07PacketPlayerDiggingAction *action) {
    for (unsigned i = 0; i < 6; i++) {
        if (action == &C07PacketPlayerDigging_ACTIONS[i])
            return true;
    }
    return false;
}
static bool facing_valid(const MCNativeEnumFacing *facing) {
    for (unsigned i = 0; i < 6; i++) {
        if (facing == &C07PacketPlayerDigging_FACINGS[i])
            return true;
    }
    return false;
}
static void trace(MCObject *o, MCObjectVisitor v, void *ctx) {
    C07PacketPlayerDigging *p = (C07PacketPlayerDigging *)o;
    p->position = (DataWatcherBlockPos *)v((MCObject *)p->position, ctx);
}
static const MCObjectClass klass = {"net.minecraft.network.play.client.C07PacketPlayerDigging",
                                    MCObjectHeap_plainClone, trace, NULL};
bool C07PacketPlayerDigging_isInstance(const MCObject *o) { return o && o->klass == &klass; }
C07PacketPlayerDigging *C07PacketPlayerDigging_new_empty(MCObjectHeap *h) {
    return (C07PacketPlayerDigging *)MCObjectHeap_alloc(h, sizeof(C07PacketPlayerDigging), &klass);
}
C07PacketPlayerDigging *C07PacketPlayerDigging_new(MCObjectHeap *h,
                                                   const C07PacketPlayerDiggingAction *action,
                                                   DataWatcherBlockPos *pos,
                                                   const MCNativeEnumFacing *facing) {
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return NULL;
    bool valid = MCObjectRootScope_pin(&scope, (MCObject *)pos) &&
                 (!pos || DataWatcher_blockPosIsInstance((MCObject *)pos)) &&
                 (!action || action_valid(action)) && (!facing || facing_valid(facing));
    if (!valid)
        MCObjectHeap_fail(h);
    C07PacketPlayerDigging *p = valid ? C07PacketPlayerDigging_new_empty(h) : NULL;
    if (p) {
        p->status = action;
        p->position = pos;
        p->facing = facing;
    }
    MCObjectRootScope_end(&scope);
    return p;
}
bool C07PacketPlayerDigging_readPacketData(C07PacketPlayerDigging *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    int32_t ordinal = 0;
    if (!PacketBuffer_readVarIntFromBuffer(b, &ordinal))
        goto done;
    const C07PacketPlayerDiggingAction *action = C07PacketPlayerDigging_action(ordinal);
    if (!action) {
        MCObjectHeap_fail(p->object.heap);
        goto done;
    }
    p->status = action;
    int64_t packed = mc_get_i64(b->buffer);
    if (b->buffer->failed)
        goto done;
    DataWatcherBlockPos *pos = BlockPos_fromLong(b->heap, packed);
    if (!pos)
        goto done;
    p->position = pos;
    uint8_t value = mc_get_u8(b->buffer);
    if (b->buffer->failed)
        goto done;
    p->facing = C07PacketPlayerDigging_facing(value);
done:
    return mc_packet_finish((MCObject *)p, b, &scope, !b->buffer->failed);
}
bool C07PacketPlayerDigging_writePacketData(C07PacketPlayerDigging *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    bool ok = action_valid(p->status);
    if (!ok) {
        MCObjectHeap_fail(p->object.heap);
        goto done;
    }
    if (!PacketBuffer_writeVarIntToBuffer(b, p->status->ordinal))
        goto done;
    ok = p->position && MCObjectRootScope_pin(&scope, (MCObject *)p->position) &&
         DataWatcher_blockPosIsInstance((MCObject *)p->position);
    if (!ok) {
        MCObjectHeap_fail(p->object.heap);
        goto done;
    }
    int64_t packed;
    ok = BlockPos_toLong(p->position, &packed) == NATIVE_ARRAY_OK;
    if (!ok) {
        MCObjectHeap_fail(p->object.heap);
        goto done;
    }
    mc_put_i64(b->buffer, packed);
    if (b->buffer->failed)
        goto done;
    ok = facing_valid(p->facing);
    if (!ok) {
        MCObjectHeap_fail(p->object.heap);
        goto done;
    }
    mc_put_u8(b->buffer, (uint8_t)p->facing->index);
done:
    return mc_packet_finish((MCObject *)p, b, &scope, ok);
}
bool C07PacketPlayerDigging_processPacket(C07PacketPlayerDigging *p,
                                          INetHandlerPlayServer handler) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_handler_begin((MCObject *)p, handler, &scope))
        return false;
    bool ok = handler.methods->processPlayerDigging &&
              handler.methods->processPlayerDigging(handler.instance, p);
    return mc_packet_handler_finish((MCObject *)p, &scope, ok);
}
DataWatcherBlockPos *C07PacketPlayerDigging_getPosition(const C07PacketPlayerDigging *p) {
    return p->position;
}
const MCNativeEnumFacing *C07PacketPlayerDigging_getFacing(const C07PacketPlayerDigging *p) {
    return p->facing;
}
const C07PacketPlayerDiggingAction *
C07PacketPlayerDigging_getStatus(const C07PacketPlayerDigging *p) {
    return p->status;
}
