#include "network/play/server/S1CPacketEntityMetadata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "metadata check %u line %d: %s\n", checks, __LINE__, #x);              \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
typedef struct {
    MCObject object;
    S1CPacketEntityMetadata *packet;
    unsigned calls;
    bool fail;
} Handler;
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
    Handler *h = (Handler *)o;
    h->packet = (S1CPacketEntityMetadata *)v((MCObject *)h->packet, c);
}
static const MCObjectClass klass = {"test.entityMetadata.handler", MCObjectHeap_plainClone, trace,
                                    NULL};
static bool handled(MCObject *o, S1CPacketEntityMetadata *p) {
    Handler *h = (Handler *)o;
    CHECK(MCObjectHeap_hasBorrowers(o->heap));
    h->packet = p;
    h->calls++;
    MCObjectHeap_touch(o->heap);
    return !h->fail;
}
static bool update(MCObject *c, MCObject *owner, int32_t id) {
    (void)c;
    (void)owner;
    (void)id;
    return true; /* Source test owner notification only. */
}
static const DataWatcherDependencies deps = {update, NULL};
static const INetHandlerPlayClientMethods methods = {.handleEntityMetadata = handled};
static void bodies(void) {
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    CHECK(h);
    MCObjectRootScope scope = {0};
    CHECK(MCObjectRootScope_begin(&scope, h));
    Handler *handler = (Handler *)MCObjectHeap_alloc(h, sizeof(*handler), &klass);
    CHECK(handler);
    DataWatcher *w = DataWatcher_new(h, (MCObject *)handler, &deps, (MCObject *)handler);
    ItemStack *s = ItemStack_new(h, ItemStack_registryItem(1), 0, 7);
    CHECK(w && s && DataWatcher_addObject(w, 10, (MCObject *)s));
    S1CPacketEntityMetadata *empty = S1CPacketEntityMetadata_new_empty(h),
                            *full = S1CPacketEntityMetadata_new(h, -1, w, true);
    CHECK(empty && full && S1CPacketEntityMetadata_getEntityId(empty) == 0 &&
          !S1CPacketEntityMetadata_func_149376_c(empty));
    WatchableObjectList *list = S1CPacketEntityMetadata_func_149376_c(full);
    CHECK(list && WatchableObject_getObject(WatchableObjectList_get(list, 0)) == (MCObject *)s &&
          !DataWatcher_hasObjectChanged(w));
    CHECK(DataWatcher_setObjectWatched(w, 10));
    S1CPacketEntityMetadata *changed = S1CPacketEntityMetadata_new(h, 0, w, false);
    CHECK(changed && S1CPacketEntityMetadata_func_149376_c(changed) &&
          !DataWatcher_hasObjectChanged(w) &&
          WatchableObjectList_get(list, 0) ==
              WatchableObjectList_get(S1CPacketEntityMetadata_func_149376_c(changed), 0));
    CHECK(DataWatcher_updateObject(w, 10, NULL));
    CHECK(WatchableObject_getObject(WatchableObjectList_get(list, 0)) == NULL);
    mc_buf b;
    mc_buf_init(&b);
    PacketBuffer pb;
    CHECK(PacketBuffer_init(&pb, h, &b) && S1CPacketEntityMetadata_writePacketData(full, &pb));
    static const uint8_t wire[] = {0xff, 0xff, 0xff, 0xff, 0x0f, 0xaa, 0xff, 0xff, 0x7f};
    CHECK(b.len == sizeof(wire) && memcmp(b.data, wire, sizeof(wire)) == 0);
    CHECK(S1CPacketEntityMetadata_readPacketData(empty, &pb) && empty->entityId == -1 &&
          WatchableObjectList_size(empty->field_149378_b) == 1 &&
          WatchableObject_getObject(WatchableObjectList_get(empty->field_149378_b, 0)) == NULL &&
          b.pos == b.len);
    CHECK(S1CPacketEntityMetadata_processPacket(
              empty, (INetHandlerPlayClient){(MCObject *)handler, &methods}) &&
          handler->packet == empty && handler->calls == 1);
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, h, (MCObject *)handler));
    MCObjectRootScope_end(&scope);
    MCObjectHeap *copy = MCObjectHeap_clone(h);
    CHECK(copy);
    MCObjectRoot other = {0};
    CHECK(MCObjectRoot_rebind(&other, copy, &root));
    Handler *ch = (Handler *)MCObjectRoot_get(&other);
    CHECK(ch != handler && ch->packet != empty &&
          ch->packet->field_149378_b != empty->field_149378_b);
    MCObjectHeap_free(copy);
    MCObjectRoot_drop(&root);
    CHECK(!MCObjectHeap_failed(h));
    mc_buf_free(&b);
    MCObjectHeap_free(h);
}
static void failures(void) {
    for (size_t n = 0; n < 9; n++) {
        MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
        CHECK(h);
        S1CPacketEntityMetadata *p = S1CPacketEntityMetadata_new_empty(h);
        CHECK(p);
        p->entityId = 123;
        mc_buf b;
        mc_buf_init(&b);
        static const uint8_t wire[] = {0xff, 0xff, 0xff, 0xff, 0x0f, 0xaa, 0xff, 0xff, 0x7f};
        mc_put_bytes(&b, wire, n);
        PacketBuffer pb;
        CHECK(PacketBuffer_init(&pb, h, &b));
        CHECK(!S1CPacketEntityMetadata_readPacketData(p, &pb) && b.failed && !p->field_149378_b);
        CHECK(p->entityId == (n < 5 ? 123 : -1));
        mc_buf_free(&b);
        MCObjectHeap_free(h);
    }
    for (int scenario = 0; scenario < 3; scenario++) {
        MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
        CHECK(h);
        Handler *o = (Handler *)MCObjectHeap_alloc(h, sizeof(*o), &klass);
        CHECK(o);
        o->fail = scenario == 0;
        S1CPacketEntityMetadata *p = S1CPacketEntityMetadata_new_empty(h);
        CHECK(p);
        INetHandlerPlayClient bad = {(MCObject *)o, scenario == 1 ? NULL : &methods};
        if (scenario == 2)
            bad.instance = NULL;
        CHECK(!S1CPacketEntityMetadata_processPacket(p, bad) && MCObjectHeap_failed(h));
        MCObjectHeap_free(h);
    }
}
int main(void) {
    bodies();
    failures();
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    CHECK(h);
    S1CPacketEntityMetadata *p = S1CPacketEntityMetadata_new_empty(h);
    CHECK(p);
    mc_buf b;
    mc_buf_init(&b);
    static const uint8_t wire[] = {0x80, 0x80, 0x80, 0x80, 0x10, 0x7f};
    mc_put_bytes(&b, wire, sizeof(wire));
    PacketBuffer pb;
    CHECK(PacketBuffer_init(&pb, h, &b));
    CHECK(S1CPacketEntityMetadata_readPacketData(p, &pb) && p->entityId == 0 &&
          !p->field_149378_b && b.pos == b.len);
    mc_buf_free(&b);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    printf("Source S1C metadata: %u checks passed\n", checks);
    return 0;
}
