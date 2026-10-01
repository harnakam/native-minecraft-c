#include "network/play/client/C03PacketPlayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Movement packet check %u line %d: %s\n", checks, __LINE__, #x);       \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static double from64(uint64_t bits) {
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
static float from32(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
static uint64_t bits64(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}
static uint32_t bits32(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}
static bool position(unsigned type) { return type == 1 || type == 3; }
static bool look(unsigned type) { return type == 2 || type == 3; }

static C03PacketPlayer *packet(MCObjectHeap *heap, unsigned type, bool empty, double x, double y,
                               double z, float yaw, float pitch, bool ground) {
    switch (type) {
    case 0:
        return empty ? C03PacketPlayer_new_empty(heap) : C03PacketPlayer_new(heap, ground);
    case 1:
        return empty ? C04PacketPlayerPosition_new_empty(heap)
                     : C04PacketPlayerPosition_new(heap, x, y, z, ground);
    case 2:
        return empty ? C05PacketPlayerLook_new_empty(heap)
                     : C05PacketPlayerLook_new(heap, yaw, pitch, ground);
    case 3:
        return empty ? C06PacketPlayerPosLook_new_empty(heap)
                     : C06PacketPlayerPosLook_new(heap, x, y, z, yaw, pitch, ground);
    default:
        return NULL;
    }
}
static void classification(C03PacketPlayer *p, unsigned type) {
    CHECK(C03PacketPlayer_isInstance((MCObject *)p));
    CHECK(C03PacketPlayer_nativeBaseIsInstance((MCObject *)p) == (type == 0));
    CHECK(C04PacketPlayerPosition_isInstance((MCObject *)p) == (type == 1));
    CHECK(C05PacketPlayerLook_isInstance((MCObject *)p) == (type == 2));
    CHECK(C06PacketPlayerPosLook_isInstance((MCObject *)p) == (type == 3));
}
static void write_packet(C03PacketPlayer *p, mc_buf *bytes) {
    mc_buf_init(bytes);
    PacketBuffer buffer;
    CHECK(PacketBuffer_init(&buffer, p->object.heap, bytes));
    CHECK(C03PacketPlayer_writePacketData(p, &buffer));
    CHECK(!MCObjectHeap_hasBorrowers(p->object.heap));
}
/* Independently assembled numeric wire facts, rather than a production writer
   used to generate the expected bytes. */
static size_t big_endian(uint8_t *out, size_t at, uint64_t bits, unsigned count) {
    for (unsigned i = 0; i < count; i++)
        out[at + i] = (uint8_t)(bits >> ((count - 1 - i) * 8));
    return at + count;
}
static size_t expected_wire(uint8_t *out, unsigned type, uint64_t x, uint64_t y, uint64_t z,
                            uint32_t yaw, uint32_t pitch, uint8_t ground) {
    size_t at = 0;
    if (position(type)) {
        at = big_endian(out, at, x, 8);
        at = big_endian(out, at, y, 8);
        at = big_endian(out, at, z, 8);
    }
    if (look(type)) {
        at = big_endian(out, at, yaw, 4);
        at = big_endian(out, at, pitch, 4);
    }
    out[at++] = ground;
    return at;
}
static void constructors_and_literal_wire(void) {
    /* PosLook: .25, -123.5, NaN with payload, NaN yaw with payload, -0 pitch. */
    static const uint8_t literal[] = {0x3f, 0xd0, 0,    0,    0,    0,    0,    0, 0xc0, 0x5e, 0xe0,
                                      0,    0,    0,    0,    0,    0x7f, 0xf8, 0, 0,    0xde, 0xad,
                                      0xbe, 0xef, 0x7f, 0xc1, 0x23, 0x45, 0x80, 0, 0,    0,    1};
    const size_t lengths[] = {1, 25, 9, 33};
    for (unsigned type = 0; type < 4; type++) {
        MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
        CHECK(heap);
        C03PacketPlayer *p = packet(heap, type, true, 0, 0, 0, 0, 0, false);
        CHECK(p);
        classification(p, type);
        CHECK(bits64(p->x) == 0 && bits64(p->y) == 0 && bits64(p->z) == 0);
        CHECK(bits32(p->yaw) == 0 && bits32(p->pitch) == 0 && !p->onGround);
        CHECK(C03PacketPlayer_isMoving(p) == position(type));
        CHECK(C03PacketPlayer_getRotating(p) == look(type));
        mc_buf bytes;
        write_packet(p, &bytes);
        CHECK(bytes.len == lengths[type]);
        for (size_t i = 0; i < bytes.len; i++)
            CHECK(bytes.data[i] == 0);
        mc_buf_free(&bytes);
        p = packet(heap, type, false, .25, -123.5, from64(UINT64_C(0x7ff80000deadbeef)),
                   from32(UINT32_C(0x7fc12345)), from32(UINT32_C(0x80000000)), true);
        CHECK(p);
        classification(p, type);
        CHECK(bits64(C03PacketPlayer_getPositionX(p)) ==
              (position(type) ? UINT64_C(0x3fd0000000000000) : 0));
        CHECK(bits64(C03PacketPlayer_getPositionY(p)) ==
              (position(type) ? UINT64_C(0xc05ee00000000000) : 0));
        CHECK(bits64(C03PacketPlayer_getPositionZ(p)) ==
              (position(type) ? UINT64_C(0x7ff80000deadbeef) : 0));
        CHECK(bits32(C03PacketPlayer_getYaw(p)) == (look(type) ? UINT32_C(0x7fc12345) : 0));
        CHECK(bits32(C03PacketPlayer_getPitch(p)) == (look(type) ? UINT32_C(0x80000000) : 0));
        CHECK(C03PacketPlayer_isOnGround(p));
        CHECK(C03PacketPlayer_setMoving(p, false));
        CHECK(!C03PacketPlayer_isMoving(p));
        /* Virtual IO depends on runtime class, not the mutable moving field. */
        write_packet(p, &bytes);
        CHECK(bytes.len == lengths[type]);
        if (type == 0)
            CHECK(bytes.data[0] == 1);
        if (type == 1) {
            CHECK(!memcmp(bytes.data, literal, 24));
            CHECK(bytes.data[24] == 1);
        }
        if (type == 2)
            CHECK(!memcmp(bytes.data, literal + 24, 9));
        if (type == 3)
            CHECK(!memcmp(bytes.data, literal, sizeof(literal)));
        mc_buf_free(&bytes);
        MCObjectHeap_free(heap);
    }
}
static void raw_ieee_roundtrips(void) {
    static const uint64_t d[] = {0,
                                 UINT64_C(0x8000000000000000),
                                 1,
                                 UINT64_C(0x8000000000000001),
                                 UINT64_C(0x0010000000000000),
                                 UINT64_C(0x8010000000000000),
                                 UINT64_C(0x7fefffffffffffff),
                                 UINT64_C(0xffefffffffffffff),
                                 UINT64_C(0x7ff0000000000000),
                                 UINT64_C(0xfff0000000000000),
                                 UINT64_C(0x7ff80000deadbeef),
                                 UINT64_C(0xfff80000deadbeef),
                                 UINT64_C(0x7ff0000000000001),
                                 UINT64_C(0xfff0000000000001)};
    static const uint32_t f[] = {0,
                                 UINT32_C(0x80000000),
                                 1,
                                 UINT32_C(0x80000001),
                                 UINT32_C(0x00800000),
                                 UINT32_C(0x80800000),
                                 UINT32_C(0x7f7fffff),
                                 UINT32_C(0xff7fffff),
                                 UINT32_C(0x7f800000),
                                 UINT32_C(0xff800000),
                                 UINT32_C(0x7fc12345),
                                 UINT32_C(0xffc12345),
                                 UINT32_C(0x7f800001),
                                 UINT32_C(0xff800001)};
    for (unsigned i = 0; i < 14; i++)
        for (unsigned type = 0; type < 4; type++)
            for (unsigned ground = 0; ground < 2; ground++) {
                MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
                CHECK(heap);
                uint64_t x = d[i], y = d[(i + 3) % 14], z = d[(i + 7) % 14];
                uint32_t yaw = f[i], pitch = f[(i + 5) % 14];
                C03PacketPlayer *p = packet(heap, type, false, from64(x), from64(y), from64(z),
                                            from32(yaw), from32(pitch), ground != 0);
                CHECK(p);
                uint8_t expected[33];
                size_t length = expected_wire(expected, type, x, y, z, yaw, pitch, (uint8_t)ground);
                mc_buf bytes;
                write_packet(p, &bytes);
                CHECK(bytes.len == length && !memcmp(bytes.data, expected, length));
                C03PacketPlayer *q = packet(heap, type, true, 0, 0, 0, 0, 0, false);
                CHECK(q);
                PacketBuffer buffer;
                CHECK(PacketBuffer_init(&buffer, heap, &bytes));
                CHECK(C03PacketPlayer_readPacketData(q, &buffer));
                CHECK(bytes.pos == length);
                CHECK(bits64(q->x) == (position(type) ? x : 0));
                CHECK(bits64(q->y) == (position(type) ? y : 0));
                CHECK(bits64(q->z) == (position(type) ? z : 0));
                CHECK(bits32(q->yaw) == (look(type) ? yaw : 0));
                CHECK(bits32(q->pitch) == (look(type) ? pitch : 0));
                CHECK(q->onGround == (ground != 0));
                CHECK(q->moving == position(type) && q->rotating == look(type));
                CHECK(!MCObjectHeap_hasBorrowers(heap));
                mc_buf_free(&bytes);
                MCObjectHeap_free(heap);
            }
}
static void read_prefixes_and_ground_bytes(void) {
    for (unsigned type = 0; type < 4; type++) {
        uint8_t wire[33];
        size_t length = expected_wire(wire, type, UINT64_C(0x3fd0000000000000),
                                      UINT64_C(0xc05ee00000000000), UINT64_C(0x7ff80000deadbeef),
                                      UINT32_C(0x7fc12345), UINT32_C(0x80000000), 0);
        for (size_t prefix = 0; prefix <= length; prefix++) {
            MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
            CHECK(heap);
            C03PacketPlayer *p = packet(heap, type, true, 0, 0, 0, 0, 0, false);
            CHECK(p);
            p->x = 13;
            p->y = 17;
            p->z = 19;
            p->yaw = 23;
            p->pitch = 29;
            p->onGround = true;
            p->moving = false;
            p->rotating = false;
            mc_buf bytes;
            mc_buf_init(&bytes);
            mc_put_bytes(&bytes, wire, prefix);
            PacketBuffer buffer;
            CHECK(PacketBuffer_init(&buffer, heap, &bytes));
            CHECK(C03PacketPlayer_readPacketData(p, &buffer) == (prefix == length));
            CHECK(bytes.failed == (prefix != length));
            CHECK(bits64(p->x) == (position(type) && prefix >= 8 ? UINT64_C(0x3fd0000000000000)
                                                                 : UINT64_C(0x402a000000000000)));
            CHECK(bits64(p->y) == (position(type) && prefix >= 16 ? UINT64_C(0xc05ee00000000000)
                                                                  : UINT64_C(0x4031000000000000)));
            CHECK(bits64(p->z) == (position(type) && prefix >= 24 ? UINT64_C(0x7ff80000deadbeef)
                                                                  : UINT64_C(0x4033000000000000)));
            size_t start = position(type) ? 24 : 0;
            CHECK(bits32(p->yaw) == (look(type) && prefix >= start + 4 ? UINT32_C(0x7fc12345)
                                                                       : UINT32_C(0x41b80000)));
            CHECK(bits32(p->pitch) == (look(type) && prefix >= start + 8 ? UINT32_C(0x80000000)
                                                                         : UINT32_C(0x41e80000)));
            CHECK(p->onGround == (prefix != length));
            CHECK(!p->moving && !p->rotating);
            size_t consumed = 0;
            if (position(type)) {
                if (prefix >= 8)
                    consumed = 8;
                if (prefix >= 16)
                    consumed = 16;
                if (prefix >= 24)
                    consumed = 24;
            }
            if (look(type)) {
                if (prefix >= start + 4)
                    consumed = start + 4;
                if (prefix >= start + 8)
                    consumed = start + 8;
            }
            if (prefix == length)
                consumed = length;
            CHECK(bytes.pos == consumed);
            CHECK(!MCObjectHeap_hasBorrowers(heap));
            mc_buf_free(&bytes);
            MCObjectHeap_free(heap);
        }
        for (unsigned ground = 0; ground < 256; ground++) {
            MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
            CHECK(heap);
            C03PacketPlayer *p = packet(heap, type, true, 0, 0, 0, 0, 0, false);
            CHECK(p);
            wire[length - 1] = (uint8_t)ground;
            mc_buf bytes;
            mc_buf_init(&bytes);
            mc_put_bytes(&bytes, wire, length);
            PacketBuffer buffer;
            CHECK(PacketBuffer_init(&buffer, heap, &bytes));
            CHECK(C03PacketPlayer_readPacketData(p, &buffer));
            CHECK(C03PacketPlayer_isOnGround(p) == (ground != 0));
            CHECK(bytes.pos == length);
            CHECK(p->moving == position(type) && p->rotating == look(type));
            CHECK(!MCObjectHeap_hasBorrowers(heap));
            mc_buf_free(&bytes);
            MCObjectHeap_free(heap);
        }
    }
}
typedef struct {
    MCObject object;
    C03PacketPlayer *received, *sameAlias;
    unsigned calls;
    bool fail;
} Handler;
static void trace_handler(MCObject *object, MCObjectVisitor visit, void *context) {
    Handler *h = (Handler *)object;
    h->received = (C03PacketPlayer *)visit((MCObject *)h->received, context);
    h->sameAlias = (C03PacketPlayer *)visit((MCObject *)h->sameAlias, context);
}
static const MCObjectClass handler_class = {"test.MovementHandler", MCObjectHeap_plainClone,
                                            trace_handler, NULL};
static bool process_player(MCObject *object, C03PacketPlayer *p) {
    Handler *h = (Handler *)object;
    CHECK(p->object.heap == object->heap);
    CHECK(MCObjectHeap_hasBorrowers(object->heap));
    CHECK(!MCObjectHeap_collect(object->heap));
    CHECK(!MCObjectHeap_failed(object->heap));
    h->received = p;
    h->sameAlias = p;
    h->calls++;
    MCObjectHeap_touch(object->heap);
    return !h->fail;
}
static const INetHandlerPlayServerMethods methods = {.processPlayer = process_player};
static void dispatch_and_managed_lifetime(void) {
    for (unsigned type = 0; type < 4; type++) {
        MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
        CHECK(heap);
        Handler *h = (Handler *)MCObjectHeap_alloc(heap, sizeof(*h), &handler_class);
        CHECK(h);
        C03PacketPlayer *p = packet(heap, type, false, 1, 2, 3, 4, 5, true);
        CHECK(p);
        MCObjectRoot root = {0};
        CHECK(MCObjectRoot_init(&root, heap, (MCObject *)h));
        CHECK(C03PacketPlayer_processPacket(p, (INetHandlerPlayServer){(MCObject *)h, &methods}));
        CHECK(h->calls == 1 && h->received == p && h->sameAlias == p);
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        CHECK(MCObjectHeap_collect(heap));
        CHECK(MCObjectHeap_liveObjects(heap) == 2);
        MCObjectHeap *clone = MCObjectHeap_clone(heap);
        CHECK(clone);
        MCObjectRoot cloneRoot = {0};
        CHECK(MCObjectRoot_rebind(&cloneRoot, clone, &root));
        Handler *copy = (Handler *)MCObjectRoot_get(&cloneRoot);
        CHECK(copy && copy != h);
        CHECK(copy->received != p && copy->received == copy->sameAlias);
        classification(copy->received, type);
        CHECK(C03PacketPlayer_setMoving(copy->received, !p->moving));
        CHECK(copy->received->moving != p->moving && p->moving == position(type));
        MCObjectReadScope guard = {0};
        CHECK(MCObjectReadScope_begin(&guard, heap));
        CHECK(!MCObjectHeap_adopt(heap, clone));
        CHECK(!MCObjectHeap_failed(heap));
        MCObjectReadScope_end(&guard);
        CHECK(MCObjectHeap_adopt(heap, clone));
        MCObjectHeap_free(clone);
        h = (Handler *)MCObjectRoot_get(&root);
        CHECK(h && h->received == h->sameAlias);
        CHECK(h->received->moving == !position(type));
        CHECK(C03PacketPlayer_processPacket(h->received,
                                            (INetHandlerPlayServer){(MCObject *)h, &methods}));
        CHECK(h->calls == 2);
        CHECK(MCObjectHeap_collect(heap));
        CHECK(MCObjectHeap_liveObjects(heap) == 2 && !MCObjectHeap_hasBorrowers(heap));
        MCObjectRoot_drop(&root);
        MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
    CHECK(heap);
    Handler *h = (Handler *)MCObjectHeap_alloc(heap, sizeof(*h), &handler_class);
    CHECK(h);
    C03PacketPlayer *p = C06PacketPlayerPosLook_new_empty(heap);
    CHECK(p);
    h->fail = true;
    CHECK(!C03PacketPlayer_processPacket(p, (INetHandlerPlayServer){(MCObject *)h, &methods}));
    CHECK(h->calls == 1 && h->received == p && h->sameAlias == p);
    CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}
static void native_invalid_boundaries(void) {
    for (unsigned type = 0; type < 4; type++) {
        MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024),
                     *foreign = MCObjectHeap_new(1024 * 1024);
        CHECK(heap && foreign);
        C03PacketPlayer *p = packet(heap, type, true, 0, 0, 0, 0, 0, false);
        CHECK(p);
        mc_buf bytes;
        mc_buf_init(&bytes);
        mc_put_u8(&bytes, 1);
        PacketBuffer buffer;
        CHECK(PacketBuffer_init(&buffer, foreign, &bytes));
        CHECK(!C03PacketPlayer_readPacketData(p, &buffer));
        CHECK(bytes.pos == 0 && bytes.failed && MCObjectHeap_failed(heap));
        CHECK(!MCObjectHeap_hasBorrowers(heap) && !MCObjectHeap_hasBorrowers(foreign));
        mc_buf_free(&bytes);
        MCObjectHeap_free(heap);
        MCObjectHeap_free(foreign);
        heap = MCObjectHeap_new(1024 * 1024);
        CHECK(heap);
        p = packet(heap, type, true, 0, 0, 0, 0, 0, false);
        CHECK(p);
        C03PacketPlayer *shortObject =
            (C03PacketPlayer *)MCObjectHeap_alloc(heap, sizeof(MCObject), p->object.klass);
        CHECK(shortObject);
        CHECK(!C03PacketPlayer_setMoving(shortObject, true));
        CHECK(MCObjectHeap_failed(heap));
        MCObjectHeap_free(heap);
    }
    for (unsigned foreign = 0; foreign < 2; foreign++) {
        MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024), *other = MCObjectHeap_new(1024 * 1024);
        CHECK(heap && other);
        C03PacketPlayer *p = C03PacketPlayer_new_empty(heap);
        CHECK(p);
        Handler *h =
            (Handler *)MCObjectHeap_alloc(foreign ? other : heap, sizeof(*h), &handler_class);
        CHECK(h);
        const INetHandlerPlayServerMethods empty = {0};
        CHECK(!C03PacketPlayer_processPacket(
            p, (INetHandlerPlayServer){(MCObject *)h, foreign ? &methods : &empty}));
        CHECK(!h->calls && MCObjectHeap_failed(heap));
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
        MCObjectHeap_free(other);
    }
}
int main(void) {
    constructors_and_literal_wire();
    raw_ieee_roundtrips();
    read_prefixes_and_ground_bytes();
    dispatch_and_managed_lifetime();
    native_invalid_boundaries();
    printf("Source movement packets: %u checks passed\n", checks);
    return 0;
}
