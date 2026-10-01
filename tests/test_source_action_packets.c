#include "network/play/client/C07PacketPlayerDigging.h"
#include "network/play/client/C09PacketHeldItemChange.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "action packet: %s line %d\n", #x, __LINE__);                          \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static MCObjectHeap *heap_new(void) {
    MCObjectHeap *h = MCObjectHeap_new(4 * 1024 * 1024);
    CHECK(h);
    return h;
}
static void view(PacketBuffer *p, MCObjectHeap *h, mc_buf *b) { CHECK(PacketBuffer_init(p, h, b)); }
static void exact(const mc_buf *b, const char *hex) {
    CHECK(!b->failed && strlen(hex) == b->len * 2);
    for (size_t i = 0; i < b->len; i++) {
        unsigned v = 0;
        CHECK(sscanf(hex + i * 2, "%2x", &v) == 1);
        CHECK(b->data[i] == v);
    }
}
static void digging_vectors(void) {
    MCObjectHeap *h = heap_new();
    DataWatcherBlockPos *pos = DataWatcher_blockPos(h, -33554432, -2048, 33554431);
    CHECK(pos);
    C07PacketPlayerDigging *p = C07PacketPlayerDigging_new(h, C07PacketPlayerDigging_action(3), pos,
                                                           C07PacketPlayerDigging_facing(5));
    CHECK(p);
    CHECK(C07PacketPlayerDigging_getPosition(p) == pos &&
          C07PacketPlayerDigging_getStatus(p) == C07PacketPlayerDigging_action(3));
    CHECK(C07PacketPlayerDigging_getFacing(p) == C07PacketPlayerDigging_facing(5));
    CHECK(C07PacketPlayerDigging_isInstance((MCObject *)p) &&
          !C07PacketPlayerDigging_isInstance((MCObject *)pos));
    mc_buf b = {0};
    PacketBuffer io;
    view(&io, h, &b);
    CHECK(C07PacketPlayerDigging_writePacketData(p, &io));
    exact(&b, "038000002001ffffff05");
    C07PacketPlayerDigging *q = C07PacketPlayerDigging_new_empty(h);
    CHECK(q && !q->position && !q->status && !q->facing);
    CHECK(C07PacketPlayerDigging_readPacketData(q, &io) && b.pos == b.len);
    CHECK(q->position != pos && q->position->x == -33554432 && q->position->y == -2048 &&
          q->position->z == 33554431 && q->facing == p->facing && q->status == p->status);
    mc_buf_free(&b);
    for (int action = 0; action < 6; action++)
        for (unsigned face = 0; face < 256; face++) {
            uint8_t raw[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            raw[0] = (uint8_t)action;
            raw[9] = (uint8_t)face;
            mc_buf input = {raw, sizeof(raw), sizeof(raw), 0, false};
            view(&io, h, &input);
            CHECK(C07PacketPlayerDigging_readPacketData(q, &io));
            CHECK(q->status == &C07PacketPlayerDigging_ACTIONS[action] &&
                  q->facing == &C07PacketPlayerDigging_FACINGS[face % 6]);
        }
    CHECK(C07PacketPlayerDigging_facing(-7) == C07PacketPlayerDigging_facing(1));
    CHECK(C07PacketPlayerDigging_facing(INT32_MIN) == C07PacketPlayerDigging_facing(2));
    CHECK(!C07PacketPlayerDigging_action(-1) && !C07PacketPlayerDigging_action(6));
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, h, (MCObject *)p));
    CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *copy = MCObjectHeap_clone(h);
    CHECK(copy);
    MCObjectRoot cr = {0};
    CHECK(MCObjectRoot_rebind(&cr, copy, &root));
    q = (C07PacketPlayerDigging *)MCObjectRoot_get(&cr);
    CHECK(q->position != p->position && q->facing == p->facing && q->status == p->status);
    MCObjectHeap_free(copy);
    MCObjectHeap_free(h);
}
static void held_vectors(void) {
    static const struct {
        int32_t value, want;
        const char *wire;
    } cases[] = {{0, 0, "0000"},         {8, 8, "0008"},          {-1, -1, "ffff"},
                 {32767, 32767, "7fff"}, {32768, -32768, "8000"}, {65535, -1, "ffff"},
                 {65536, 0, "0000"},     {-65537, -1, "ffff"},    {INT32_MIN, 0, "0000"},
                 {INT32_MAX, -1, "ffff"}};
    MCObjectHeap *h = heap_new();
    mc_buf b = {0};
    PacketBuffer io;
    view(&io, h, &b);
    C09PacketHeldItemChange *q = C09PacketHeldItemChange_new_empty(h);
    CHECK(q && q->slotId == 0);
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); i++) {
        C09PacketHeldItemChange *p = C09PacketHeldItemChange_new(h, cases[i].value);
        CHECK(p && C09PacketHeldItemChange_isInstance((MCObject *)p));
        CHECK(C09PacketHeldItemChange_getSlotId(p) == cases[i].value);
        CHECK(C09PacketHeldItemChange_writePacketData(p, &io));
        exact(&b, cases[i].wire);
        CHECK(C09PacketHeldItemChange_readPacketData(q, &io) && q->slotId == cases[i].want &&
              b.pos == 2);
        mc_buf_clear(&b);
    }
    const uint8_t raw[] = {0xff, 0xff};
    for (size_t length = 0; length < 2; length++) {
        mc_buf input = {(uint8_t *)raw, length, length, 0, false};
        view(&io, h, &input);
        q->slotId = 77;
        CHECK(!C09PacketHeldItemChange_readPacketData(q, &io) && q->slotId == 77 &&
              input.pos == 0 && !MCObjectHeap_failed(h));
    }
    mc_buf_free(&b);
    MCObjectHeap_free(h);
}
static void source_partial_failure(void) {
    const uint8_t raw[] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 255};
    for (size_t length = 0; length < sizeof(raw); length++) {
        MCObjectHeap *h = heap_new();
        DataWatcherBlockPos *old = DataWatcher_blockPos(h, 7, 8, 9);
        CHECK(old);
        C07PacketPlayerDigging *p = C07PacketPlayerDigging_new(
            h, C07PacketPlayerDigging_action(4), old, C07PacketPlayerDigging_facing(5));
        CHECK(p);
        mc_buf input = {(uint8_t *)raw, length, length, 0, false};
        PacketBuffer io;
        view(&io, h, &input);
        CHECK(!C07PacketPlayerDigging_readPacketData(p, &io) && input.failed &&
              !MCObjectHeap_failed(h));
        CHECK(p->status == C07PacketPlayerDigging_action(length ? 1 : 4));
        CHECK(length < 9 ? p->position == old : p->position != old && p->position->x == 0);
        CHECK(p->facing == C07PacketPlayerDigging_facing(5));
        CHECK(input.pos == (length == 0 ? 0 : length < 9 ? 1 : 9));
        MCObjectHeap_free(h);
    }
    const uint8_t invalid[][5] = {{6, 0, 0, 0, 0}, {255, 255, 255, 255, 15}};
    for (unsigned i = 0; i < 2; i++) {
        MCObjectHeap *h = heap_new();
        C07PacketPlayerDigging *p = C07PacketPlayerDigging_new_empty(h);
        CHECK(p);
        mc_buf input = {(uint8_t *)invalid[i], i ? 5 : 1, i ? 5 : 1, 0, false};
        PacketBuffer io;
        view(&io, h, &input);
        CHECK(!C07PacketPlayerDigging_readPacketData(p, &io) && input.failed &&
              MCObjectHeap_failed(h) && !p->status && !p->position);
        CHECK(input.pos == (i ? 5u : 1u));
        MCObjectHeap_free(h);
    }
    for (unsigned field = 0; field < 3; field++) {
        MCObjectHeap *h = heap_new();
        mc_buf b = {0};
        PacketBuffer io;
        view(&io, h, &b);
        DataWatcherBlockPos *pos = DataWatcher_blockPos(h, 0, 0, 0);
        CHECK(pos);
        C07PacketPlayerDigging *p = C07PacketPlayerDigging_new(
            h, field ? C07PacketPlayerDigging_action(3) : NULL, field > 1 ? pos : NULL, NULL);
        CHECK(p);
        CHECK(!C07PacketPlayerDigging_writePacketData(p, &io) && b.failed &&
              MCObjectHeap_failed(h));
        CHECK(b.len == (field == 0 ? 0u : field == 1 ? 1u : 9u));
        mc_buf_free(&b);
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h = heap_new(), *foreign = heap_new();
    DataWatcherBlockPos *pos = DataWatcher_blockPos(foreign, 0, 0, 0);
    CHECK(pos);
    CHECK(!C07PacketPlayerDigging_new(h, C07PacketPlayerDigging_action(3), pos,
                                      C07PacketPlayerDigging_facing(0)) &&
          MCObjectHeap_failed(h));
    CHECK(!MCObjectHeap_failed(foreign));
    MCObjectHeap_free(h);
    MCObjectHeap_free(foreign);
}
typedef struct {
    MCObject object;
    MCObject *last;
    int calls;
    bool refuse;
} Handler;
static void trace(MCObject *o, MCObjectVisitor v, void *ctx) {
    Handler *h = (Handler *)o;
    h->last = v(h->last, ctx);
}
static const MCObjectClass handler_class = {"fixture.source.action-handler",
                                            MCObjectHeap_plainClone, trace, NULL};
static bool record(MCObject *o, MCObject *p) {
    Handler *h = (Handler *)o;
    CHECK(MCObjectHeap_hasBorrowers(o->heap) && !MCObjectHeap_collect(o->heap));
    h->last = p;
    h->calls++;
    return !h->refuse;
}
static bool dig(MCObject *o, C07PacketPlayerDigging *p) { return record(o, (MCObject *)p); }
static bool held(MCObject *o, C09PacketHeldItemChange *p) { return record(o, (MCObject *)p); }
static void dispatch(void) {
    static const INetHandlerPlayServerMethods methods = {.processPlayerDigging = dig,
                                                         .processHeldItemChange = held};
    MCObjectHeap *h = heap_new();
    Handler *handler = (Handler *)MCObjectHeap_alloc(h, sizeof(*handler), &handler_class);
    CHECK(handler);
    C07PacketPlayerDigging *a = C07PacketPlayerDigging_new_empty(h);
    C09PacketHeldItemChange *b = C09PacketHeldItemChange_new(h, 9);
    CHECK(a && b);
    INetHandlerPlayServer out = {(MCObject *)handler, &methods};
    CHECK(C07PacketPlayerDigging_processPacket(a, out) && handler->last == (MCObject *)a);
    CHECK(C09PacketHeldItemChange_processPacket(b, out) && handler->last == (MCObject *)b &&
          handler->calls == 2);
    handler->refuse = true;
    CHECK(!C09PacketHeldItemChange_processPacket(b, out) && MCObjectHeap_failed(h) &&
          !MCObjectHeap_hasBorrowers(h));
    MCObjectHeap_free(h);
    h = heap_new();
    a = C07PacketPlayerDigging_new_empty(h);
    CHECK(a);
    CHECK(!C07PacketPlayerDigging_processPacket(a, (INetHandlerPlayServer){0}) &&
          MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
int main(void) {
    digging_vectors();
    held_vectors();
    source_partial_failure();
    dispatch();
    printf("source action packets: %u checks passed\n", checks);
    return 0;
}
