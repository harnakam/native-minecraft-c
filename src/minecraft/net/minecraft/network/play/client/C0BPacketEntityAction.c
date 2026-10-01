#include "network/play/client/C0BPacketEntityAction.h"
#include "network/play/client/native_packet.h"
static const C0BPacketEntityActionAction actions[7] = {{0}, {1}, {2}, {3}, {4}, {5}, {6}};
const C0BPacketEntityActionAction *C0BPacketEntityAction_nativeAction(int32_t ordinal) {
    return ordinal >= 0 && ordinal < 7 ? &actions[ordinal] : NULL;
}
bool C0BPacketEntityAction_nativeOrdinal(const C0BPacketEntityActionAction *a, int32_t *out) {
    for (int32_t i = 0; i < 7; i++)
        if (a == &actions[i]) {
            if (out)
                *out = i;
            return true;
        }
    return false;
}
static const MCObjectClass klass = {"net.minecraft.network.play.client.C0BPacketEntityAction",
                                    MCObjectHeap_plainClone, NULL, NULL};
bool C0BPacketEntityAction_isInstance(const MCObject *o) {
    return o && o->klass == &klass && MCObjectHeap_objectSize(o) >= sizeof(C0BPacketEntityAction);
}
static bool valid(C0BPacketEntityAction *p) {
    if (C0BPacketEntityAction_isInstance((MCObject *)p) && !MCObjectHeap_failed(p->object.heap))
        return true;
    MCObjectHeap_fail(p ? p->object.heap : NULL);
    return false;
}
C0BPacketEntityAction *C0BPacketEntityAction_new_empty(MCObjectHeap *h) {
    return (C0BPacketEntityAction *)MCObjectHeap_alloc(h, sizeof(C0BPacketEntityAction), &klass);
}
C0BPacketEntityAction *C0BPacketEntityAction_new_aux(MCObjectHeap *h, Entity *e,
                                                     const C0BPacketEntityActionAction *a,
                                                     int32_t n) {
    MCObjectRootScope scope = {0};
    if (!Entity_isInstance((MCObject *)e) || e->object.heap != h ||
        !MCObjectRootScope_begin(&scope, h)) {
        MCObjectHeap_fail(h);
        return NULL;
    }
    bool ok = MCObjectRootScope_pin(&scope, (MCObject *)e);
    C0BPacketEntityAction *p = ok ? C0BPacketEntityAction_new_empty(h) : NULL;
    if (p) {
        p->entityID = Entity_getEntityId(e);
        if (MCObjectHeap_failed(h))
            p = NULL;
        else {
            p->action = a;
            p->auxData = n;
            MCObjectHeap_touch(h);
        }
    }
    MCObjectRootScope_end(&scope);
    return p;
}
C0BPacketEntityAction *C0BPacketEntityAction_new(MCObjectHeap *h, Entity *e,
                                                 const C0BPacketEntityActionAction *a) {
    return C0BPacketEntityAction_new_aux(h, e, a, 0);
}
bool C0BPacketEntityAction_readPacketData(C0BPacketEntityAction *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!valid(p) || !mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    int32_t n;
    bool ok = PacketBuffer_readVarIntFromBuffer(b, &n);
    if (ok) {
        p->entityID = n;
        MCObjectHeap_touch(p->object.heap);
        ok = PacketBuffer_readVarIntFromBuffer(b, &n);
    }
    if (ok) {
        const C0BPacketEntityActionAction *a = C0BPacketEntityAction_nativeAction(n);
        if (!a)
            ok = false;
        else {
            p->action = a;
            MCObjectHeap_touch(p->object.heap);
            ok = PacketBuffer_readVarIntFromBuffer(b, &n);
        }
    }
    if (ok) {
        p->auxData = n;
        MCObjectHeap_touch(p->object.heap);
    }
    return mc_packet_finish((MCObject *)p, b, &scope, ok);
}
bool C0BPacketEntityAction_writePacketData(C0BPacketEntityAction *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!valid(p) || !mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    int32_t ordinal;
    bool ok = PacketBuffer_writeVarIntToBuffer(b, p->entityID);
    if (ok)
        ok = C0BPacketEntityAction_nativeOrdinal(p->action, &ordinal);
    if (ok)
        ok = PacketBuffer_writeVarIntToBuffer(b, ordinal);
    if (ok)
        ok = PacketBuffer_writeVarIntToBuffer(b, p->auxData);
    return mc_packet_finish((MCObject *)p, b, &scope, ok);
}
bool C0BPacketEntityAction_processPacket(C0BPacketEntityAction *p, INetHandlerPlayServer h) {
    MCObjectRootScope scope = {0};
    if (!valid(p) || !mc_packet_handler_begin((MCObject *)p, h, &scope))
        return false;
    bool ok = h.methods->processEntityAction && h.methods->processEntityAction(h.instance, p);
    return mc_packet_handler_finish((MCObject *)p, &scope, ok);
}
const C0BPacketEntityActionAction *C0BPacketEntityAction_getAction(const C0BPacketEntityAction *p) {
    return p->action;
}
int32_t C0BPacketEntityAction_getAuxData(const C0BPacketEntityAction *p) { return p->auxData; }
