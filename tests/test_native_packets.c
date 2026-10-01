#include "network/NativePacket.h"
#include "server/native_gameplay.h"
#include "util/MCPacketQueue.h"
#include "util/MCGameplayPackets.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "native packet %u at %d: %s\n", checks, __LINE__, #x);                 \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static const MCPacketCodec codecs[] = {
    {0x18, NativePacket_isInstance, NativePacket_writePacketData},
    {0x13, NativePacket_isInstance, NativePacket_writePacketData}};
static const MCPacketQueueProfile profile = {codecs, 2, true};
static void rejected(const mc_buf *message) {
    MCObjectHeap *heap = MCObjectHeap_new(4096);
    CHECK(heap);
    size_t before = MCObjectHeap_liveObjects(heap);
    mc_buf saved = message ? *message : (mc_buf){0};
    CHECK(!NativePacket_new(heap, message));
    CHECK(MCObjectHeap_failed(heap) && MCObjectHeap_liveObjects(heap) == before);
    if (message)
        CHECK(saved.data == message->data && saved.len == message->len &&
              saved.cap == message->cap && saved.pos == message->pos &&
              saved.failed == message->failed);
    MCObjectHeap_free(heap);
}
static void invalid_prefixes_and_bounds(void) {
    static const uint8_t truncated[] = {0x80};
    static const uint8_t tooLong[] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
    static const uint8_t invalidHighBits[] = {0x80, 0x80, 0x80, 0x80, 0x10};
    static const uint8_t negative[] = {0xff, 0xff, 0xff, 0xff, 0x0f};
    const uint8_t *bytes[] = {truncated, tooLong, invalidHighBits, negative};
    const size_t lengths[] = {sizeof truncated, sizeof tooLong, sizeof invalidHighBits,
                              sizeof negative};
    for (unsigned i = 0; i < 4; ++i) {
        mc_buf b = {.data = (uint8_t *)bytes[i], .len = lengths[i], .cap = lengths[i]};
        rejected(&b);
    }
    rejected(NULL);
    /* This inaccessible address proves length/cap/failed checks precede reads;
       the test owns neither this sentinel pointer nor any external buffer. */
    mc_buf b = {
        .data = (uint8_t *)(uintptr_t)1, .len = MC_MAX_PACKET + 1u, .cap = MC_MAX_PACKET + 1u};
    rejected(&b);
    b = (mc_buf){.data = (uint8_t *)(uintptr_t)1, .len = 2, .cap = 1};
    rejected(&b);
    b = (mc_buf){.data = (uint8_t *)(uintptr_t)1, .len = 1, .cap = 1, .failed = true};
    rejected(&b);
    b = (mc_buf){.data = (uint8_t *)(uintptr_t)1};
    rejected(&b);
    b = (mc_buf){.len = 1, .cap = 1};
    rejected(&b);
}
static void immutable_message_and_maximum_size(void) {
    MCObjectHeap *heap = MCObjectHeap_new(8u * MC_MAX_PACKET);
    CHECK(heap);
    MCObjectRootScope scope = {0};
    CHECK(MCObjectRootScope_begin(&scope, heap));
    mc_buf input = {0};
    mc_put_varint(&input, INT32_MAX);
    mc_put_u8(&input, 0xaa);
    mc_put_u8(&input, 0x55);
    input.pos = input.len;
    MCObject *packet = NativePacket_new(heap, &input);
    CHECK(packet && NativePacket_isInstance(packet) && MCObjectRootScope_pin(&scope, packet));
    CHECK(input.pos == input.len && !input.failed);
    memset(input.data, 0, input.len);
    mc_buf_free(&input);
    mc_buf out = {0};
    mc_put_varint(&out, INT32_MAX);
    PacketBuffer view;
    CHECK(PacketBuffer_init(&view, heap, &out) && NativePacket_writePacketData(packet, &view));
    static const uint8_t expected[] = {0xff, 0xff, 0xff, 0xff, 0x07, 0xaa, 0x55};
    CHECK(out.len == sizeof expected && !memcmp(out.data, expected, sizeof expected));
    mc_buf_free(&out);
    /* A boundary-sized complete frame is accepted, including its ID byte. */
    uint8_t *storage = (uint8_t *)malloc(MC_MAX_PACKET);
    CHECK(storage);
    memset(storage, 0x5a, MC_MAX_PACKET);
    storage[0] = 0x18;
    input = (mc_buf){.data = storage, .len = MC_MAX_PACKET, .cap = MC_MAX_PACKET};
    packet = NativePacket_new(heap, &input);
    CHECK(packet && MCObjectRootScope_pin(&scope, packet));
    memset(storage, 0, MC_MAX_PACKET);
    mc_buf_free(&input);
    mc_put_varint(&out, 0x18);
    CHECK(PacketBuffer_init(&view, heap, &out) && NativePacket_writePacketData(packet, &view));
    CHECK(out.len == MC_MAX_PACKET && !out.failed && out.data[0] == 0x18);
    for (size_t i = 1; i < out.len; ++i)
        if (out.data[i] != 0x5a)
            CHECK(false);
    CHECK(!MCObjectHeap_failed(heap));
    mc_buf_free(&out);
    MCObjectRootScope_end(&scope);
    MCObjectHeap_free(heap);
}
typedef struct {
    MCGameplay parent;
    MCGameplayTransaction tx;
    mc_world terrain;
} Fixture;
static MCGameplayPlayer *setup(Fixture *f, MCObjectRootScope *scope) {
    mc_world_init(&f->terrain, 919);
    CHECK(mc_server_graph_init(&f->parent, &f->terrain, 0, 0, 919));
    CHECK(MCGameplay_begin(&f->parent, &f->tx));
    CHECK(MCObjectRootScope_begin(scope, f->tx.working.heap));
    /* Real native owner/source actors are constructed in a disposable graph;
       no durable commitment or substitute production hook is claimed. */
    CHECK(mc_server_graph_add_player(&f->tx.working, 0, "11111111-1111-1111-1111-111111111111",
                                     "Packet", 1, 0, 64, 0, false));
    MCGameplayPlayer *p = mc_server_graph_player(&f->tx.working, 0);
    CHECK(p && MCGameplayWorld_isInstance((MCObject *)p->worldObj));
    return p;
}
static void finish(Fixture *f, MCObjectRootScope *scope) {
    MCObjectRootScope_end(scope);
    CHECK(MCGameplay_abort(&f->tx) && MCGameplay_free(&f->parent));
    mc_world_free(&f->terrain);
}
static MCObject *message(MCObjectHeap *heap) {
    mc_buf b = {0};
    mc_put_varint(&b, 0x18);
    mc_put_u8(&b, 0x42);
    MCObject *p = NativePacket_new(heap, &b);
    mc_buf_free(&b);
    CHECK(p);
    return p;
}
static void queue_prefix_failure_preserves_output(void) {
    Fixture f = {0};
    MCObjectRootScope scope = {0};
    MCGameplayPlayer *p = setup(&f, &scope);
    p->pendingPackets =
        MCPacketQueue_new(f.tx.working.heap, MCGameplay_get(&f.tx.working), &profile);
    CHECK(p->pendingPackets);
    MCObject *packet = message(f.tx.working.heap);
    CHECK(MCPacketQueue_append(p->pendingPackets, packet, 0x13));
    mc_buf out = {0};
    mc_put_i16(&out, 0x1234);
    out.pos = 1;
    uint8_t *original = out.data;
    size_t capacity = out.cap;
    CHECK(!MCPacketQueue_encodeAt(p->pendingPackets, 0, &out));
    CHECK(MCObjectHeap_failed(f.tx.working.heap));
    CHECK(out.data == original && out.len == 2 && out.cap == capacity && out.pos == 1 &&
          !out.failed && out.data[0] == 0x12 && out.data[1] == 0x34);
    CHECK(MCPacketQueue_count(p->pendingPackets) == 1 &&
          MCPacketQueue_packetAt(p->pendingPackets, 0, NULL) == packet);
    mc_buf_free(&out);
    finish(&f, &scope);
}
static void borrowed_heap_clone_and_aliases(void) {
    Fixture f = {0};
    MCObjectRootScope scope = {0};
    MCGameplayPlayer *p = setup(&f, &scope);
    p->pendingPackets =
        MCPacketQueue_new(f.tx.working.heap, MCGameplay_get(&f.tx.working), &profile);
    CHECK(p->pendingPackets);
    MCObject *packet = message(f.tx.working.heap);
    CHECK(MCObjectRootScope_pin(&scope, packet));
    CHECK(MCPacketQueue_append(p->pendingPackets, packet, 0x18) &&
          MCPacketQueue_append(p->pendingPackets, packet, 0x18));
    MCObjectRoot alias = {0};
    CHECK(MCObjectRoot_init(&alias, f.tx.working.heap, packet));
    CHECK(MCObjectHeap_hasBorrowers(f.tx.working.heap) && !MCObjectHeap_collect(f.tx.working.heap));
    MCObjectRootScope_end(&scope);
    CHECK(MCObjectHeap_collect(f.tx.working.heap) && MCObjectRoot_get(&alias) == packet);
    MCObjectHeap *clonedHeap = MCObjectHeap_clone(f.tx.working.heap);
    CHECK(clonedHeap);
    MCObjectRoot graphCopyRoot = {0};
    CHECK(MCObjectRoot_rebind(&graphCopyRoot, clonedHeap, &f.tx.working.root));
    CHECK(MCObjectRootScope_begin(&scope, clonedHeap));
    MCGameplayObjects *copiedOwners = (MCGameplayObjects *)MCObjectRoot_get(&graphCopyRoot);
    MCGameplayPlayer *copy = (MCGameplayPlayer *)copiedOwners->players[0];
    CHECK(copy && copy != p && copy->pendingPackets != p->pendingPackets);
    MCObject *copied = MCPacketQueue_packetAt(copy->pendingPackets, 0, NULL);
    CHECK(copied && copied != packet && copied->heap == clonedHeap &&
          MCPacketQueue_packetAt(copy->pendingPackets, 1, NULL) == copied);
    MCObjectRoot copiedAlias = {0};
    CHECK(MCObjectRoot_rebind(&copiedAlias, clonedHeap, &alias) &&
          MCObjectRoot_get(&copiedAlias) == copied);
    CHECK(MCPacketQueue_validateForOwners(copy->pendingPackets, copiedOwners));
    mc_buf out = {0};
    CHECK(MCPacketQueue_encodeAt(copy->pendingPackets, 1, &out));
    CHECK(out.len == 2 && out.data[0] == 0x18 && out.data[1] == 0x42);
    CHECK(!MCObjectHeap_failed(f.tx.working.heap) && !MCObjectHeap_failed(clonedHeap));
    mc_buf_free(&out);
    MCObjectRoot_drop(&copiedAlias);
    MCObjectRootScope_end(&scope);
    MCObjectRoot_drop(&graphCopyRoot);
    MCObjectHeap_free(clonedHeap);
    CHECK(MCObjectRoot_get(&alias) == packet && MCPacketQueue_count(p->pendingPackets) == 2);
    MCObjectRoot_drop(&alias);
    CHECK(MCGameplay_abort(&f.tx) && MCGameplay_free(&f.parent));
    mc_world_free(&f.terrain);
}
static void source_metadata_and_foreign_packet_boundaries(void) {
    Fixture f = {0};
    MCObjectRootScope scope = {0};
    MCGameplayPlayer *p = setup(&f, &scope);
    S1CPacketEntityMetadata *metadata = S1CPacketEntityMetadata_new_empty(f.tx.working.heap);
    CHECK(metadata && MCGameplayPackets_sendMetadata(p, metadata));
    mc_buf encoded = {0};
    CHECK(MCGameplayPackets_encodeAt(p, MCGameplayPackets_count(p) - 1, &encoded));
    CHECK(encoded.len == 3 && encoded.data[0] == 0x1c && encoded.data[1] == 0 &&
          encoded.data[2] == 0x7f);
    mc_buf_free(&encoded);
    int32_t before = MCGameplayPackets_count(p);
    mc_buf native = {0};
    mc_put_varint(&native, 0x1c);
    mc_put_varint(&native, 0);
    mc_put_u8(&native, 0x7f);
    CHECK(!MCGameplayPackets_sendNative(p, &native));
    CHECK(MCObjectHeap_failed(f.tx.working.heap) && MCGameplayPackets_count(p) == before);
    mc_buf_free(&native);
    finish(&f, &scope);

    f = (Fixture){0};
    scope = (MCObjectRootScope){0};
    p = setup(&f, &scope);
    p->pendingPackets =
        MCPacketQueue_new(f.tx.working.heap, MCGameplay_get(&f.tx.working), &profile);
    CHECK(p->pendingPackets);
    MCObjectHeap *foreign = MCObjectHeap_new(4096);
    CHECK(foreign);
    CHECK(!MCPacketQueue_append(p->pendingPackets, message(foreign), 0x18));
    CHECK(MCObjectHeap_failed(f.tx.working.heap) && !MCObjectHeap_failed(foreign) &&
          MCPacketQueue_count(p->pendingPackets) == 0);
    MCObjectHeap_free(foreign);
    finish(&f, &scope);
}
int main(void) {
    invalid_prefixes_and_bounds();
    immutable_message_and_maximum_size();
    queue_prefix_failure_preserves_output();
    borrowed_heap_clone_and_aliases();
    source_metadata_and_foreign_packet_boundaries();
    printf("native packets: %u checks passed\n", checks);
    return 0;
}
