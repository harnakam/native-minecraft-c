#include "network/play/server/S1CPacketEntityMetadata.h"
#include "network/play/server/native_packet.h"
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
    S1CPacketEntityMetadata *p = (S1CPacketEntityMetadata *)o;
    p->field_149378_b = (WatchableObjectList *)v((MCObject *)p->field_149378_b, c);
}
static const MCObjectClass klass = {"net.minecraft.network.play.server.S1CPacketEntityMetadata",
                                    MCObjectHeap_plainClone, trace, NULL};
S1CPacketEntityMetadata *S1CPacketEntityMetadata_new_empty(MCObjectHeap *h) {
    return (S1CPacketEntityMetadata *)MCObjectHeap_alloc(h, sizeof(S1CPacketEntityMetadata),
                                                         &klass);
}
S1CPacketEntityMetadata *S1CPacketEntityMetadata_new(MCObjectHeap *h, int32_t id, DataWatcher *w,
                                                     bool all) {
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return NULL;
    S1CPacketEntityMetadata *p = S1CPacketEntityMetadata_new_empty(h);
    if (p) {
        p->entityId = id;
        if (!w || !MCObjectRootScope_pin(&scope, (MCObject *)w))
            MCObjectHeap_fail(h);
        else
            p->field_149378_b = all ? DataWatcher_getAllWatched(w) : DataWatcher_getChanged(w);
    }
    if (MCObjectHeap_failed(h))
        p = NULL;
    MCObjectRootScope_end(&scope);
    return p;
}
bool S1CPacketEntityMetadata_readPacketData(S1CPacketEntityMetadata *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    int32_t id = 0;
    bool ok = PacketBuffer_readVarIntFromBuffer(b, &id);
    if (ok) {
        p->entityId = id;
        MCObjectHeap_touch(p->object.heap);
        ok = DataWatcher_readWatchedListFromPacketBuffer(b, &p->field_149378_b);
        if (ok)
            MCObjectHeap_touch(p->object.heap);
    }
    return mc_packet_finish((MCObject *)p, b, &scope, ok);
}
bool S1CPacketEntityMetadata_writePacketData(S1CPacketEntityMetadata *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    bool ok = PacketBuffer_writeVarIntToBuffer(b, p->entityId);
    return mc_packet_finish((MCObject *)p, b, &scope,
                            ok && DataWatcher_writeWatchedListToPacketBuffer(p->field_149378_b, b));
}
bool S1CPacketEntityMetadata_processPacket(S1CPacketEntityMetadata *p, INetHandlerPlayClient h) {
    MCObjectRootScope scope = {0};
    if (!mc_client_handler_begin((MCObject *)p, h, &scope))
        return false;
    bool ok = h.methods->handleEntityMetadata && h.methods->handleEntityMetadata(h.instance, p);
    return mc_packet_handler_finish((MCObject *)p, &scope, ok);
}
WatchableObjectList *S1CPacketEntityMetadata_func_149376_c(const S1CPacketEntityMetadata *p) {
    return p->field_149378_b;
}
int32_t S1CPacketEntityMetadata_getEntityId(const S1CPacketEntityMetadata *p) {
    return p->entityId;
}
