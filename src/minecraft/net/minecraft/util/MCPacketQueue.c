#include "util/MCPacketQueue.h"
#include <limits.h>
#include <string.h>

typedef struct {
    MCObject *packet;
    int32_t id;
    uint64_t queuedAtSerial;
} Entry;
typedef struct {
    MCObject object;
    int32_t capacity;
    Entry entries[];
} Entries;
typedef struct {
    MCObject object;
    MCGameplayObjects *owners;
    const MCPacketQueueProfile *profile;
    Entries *storage;
    int32_t start, end;
    bool flushing;
} Queue;
static void entries_trace(MCObject *o, MCObjectVisitor v, void *c) {
    Entries *e = (Entries *)o;
    for (int32_t i = 0; i < e->capacity; i++)
        e->entries[i].packet = v(e->entries[i].packet, c);
}
static void queue_trace(MCObject *o, MCObjectVisitor v, void *c) {
    Queue *q = (Queue *)o;
    q->owners = (MCGameplayObjects *)v((MCObject *)q->owners, c);
    q->storage = (Entries *)v((MCObject *)q->storage, c);
}
static const MCObjectClass entriesClass = {"native.PacketQueueEntries", MCObjectHeap_plainClone,
                                           entries_trace, NULL};
static const MCObjectClass queueClass = {"native.PacketQueue", MCObjectHeap_plainClone, queue_trace,
                                         NULL};
static Queue *queue(MCObject *object) {
    if (!object)
        return NULL;
    if (object->klass != &queueClass) {
        MCObjectHeap_fail(object->heap);
        return NULL;
    }
    return (Queue *)object;
}
static const MCPacketCodec *codec(const Queue *q, int32_t id) {
    for (size_t i = 0; i < q->profile->count; i++)
        if (q->profile->codecs[i].id == id)
            return &q->profile->codecs[i];
    return NULL;
}
MCObject *MCPacketQueue_new(MCObjectHeap *heap, MCGameplayObjects *owners,
                            const MCPacketQueueProfile *profile) {
    if (!heap || !owners || owners->object.heap != heap || !profile || !profile->codecs ||
        !profile->count) {
        MCObjectHeap_fail(heap);
        return NULL;
    }
    for (size_t i = 0; i < profile->count; i++) {
        const MCPacketCodec *c = &profile->codecs[i];
        if (c->id < 0 || !c->isInstance || !c->write) {
            MCObjectHeap_fail(heap);
            return NULL;
        }
        for (size_t j = 0; j < i; j++)
            if (profile->codecs[j].id == c->id) {
                MCObjectHeap_fail(heap);
                return NULL;
            }
    }
    Queue *q = (Queue *)MCObjectHeap_alloc(heap, sizeof(*q), &queueClass);
    if (q) {
        q->owners = owners;
        q->profile = profile;
    }
    return (MCObject *)q;
}
bool MCPacketQueue_isInstance(const MCObject *o, const MCPacketQueueProfile *profile) {
    return o && o->klass == &queueClass && ((const Queue *)o)->profile == profile;
}
bool MCPacketQueue_append(MCObject *object, MCObject *packet, int32_t id) {
    Queue *q = queue(object);
    if (!q)
        return false;
    MCObjectHeap *heap = object->heap;
    const MCPacketCodec *c = codec(q, id);
    if (q->flushing || !packet || packet->heap != heap || !q->owners ||
        q->owners->object.heap != heap || !c || !c->isInstance(packet)) {
        MCObjectHeap_fail(heap);
        return false;
    }
    if (!q->storage || q->end == q->storage->capacity) {
        int32_t count = q->end - q->start;
        int64_t capacity = q->storage ? (int64_t)q->storage->capacity * 2 : 16;
        if (capacity > INT32_MAX ||
            (size_t)capacity > (SIZE_MAX - sizeof(Entries)) / sizeof(Entry)) {
            MCObjectHeap_fail(heap);
            return false;
        }
        Entries *next = (Entries *)MCObjectHeap_alloc(
            heap, sizeof(*next) + (size_t)capacity * sizeof(Entry), &entriesClass);
        if (!next)
            return false;
        next->capacity = (int32_t)capacity;
        if (count)
            memcpy(next->entries, q->storage->entries + q->start, (size_t)count * sizeof(Entry));
        q->storage = next;
        q->start = 0;
        q->end = count;
    }
    q->storage->entries[q->end++] = (Entry){packet, id, q->owners->commitSerial};
    MCObjectHeap_touch(heap);
    return true;
}
int32_t MCPacketQueue_count(const MCObject *o) {
    Queue *q = queue((MCObject *)o);
    return q ? q->end - q->start : 0;
}
MCObject *MCPacketQueue_packetAt(MCObject *o, int32_t index, int32_t *id) {
    Queue *q = queue(o);
    if (!q)
        return NULL;
    if (index < 0 || index >= q->end - q->start) {
        MCObjectHeap_fail(o->heap);
        return NULL;
    }
    Entry *e = &q->storage->entries[q->start + index];
    if (id)
        *id = e->id;
    return e->packet;
}
bool MCPacketQueue_encodeAt(MCObject *o, int32_t index, mc_buf *out) {
    Queue *q = queue(o);
    if (!q || !out)
        return false;
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, o->heap))
        return false;
    int32_t id = 0;
    MCObject *packet = MCPacketQueue_packetAt(o, index, &id);
    const MCPacketCodec *c = codec(q, id);
    mc_buf buffer = {0};
    PacketBuffer view;
    bool ok = packet && c && c->isInstance(packet) && packet->heap == o->heap &&
              PacketBuffer_init(&view, o->heap, &buffer);
    if (ok) {
        mc_put_varint(&buffer, id);
        ok = c->write(packet, &view);
    }
    ok = ok && !buffer.failed && buffer.len <= MC_MAX_PACKET && !MCObjectHeap_failed(o->heap);
    if (ok) {
        mc_buf_free(out);
        *out = buffer;
        mc_buf_init(&buffer);
    } else
        MCObjectHeap_fail(o->heap);
    mc_buf_free(&buffer);
    MCObjectRootScope_end(&scope);
    return ok;
}
bool MCPacketQueue_validate(MCObject *o) {
    if (!queue(o))
        return false;
    int32_t count = MCPacketQueue_count(o);
    for (int32_t i = 0; i < count; i++) {
        mc_buf b = {0};
        bool ok = MCPacketQueue_encodeAt(o, i, &b);
        mc_buf_free(&b);
        if (!ok)
            return false;
    }
    return !MCObjectHeap_failed(o->heap);
}
bool MCPacketQueue_validateForOwners(MCObject *o, const MCGameplayObjects *owners) {
    Queue *q = queue(o);
    if (!q)
        return false;
    if (!owners || owners->object.heap != o->heap || q->owners != owners) {
        MCObjectHeap_fail(o->heap);
        return false;
    }
    return MCPacketQueue_validate(o);
}
MCPacketQueueResult MCPacketQueue_flush(MCGameplay *game, size_t index,
                                        const MCPacketQueueProfile *profile, MCPacketQueueSink sink,
                                        void *context) {
    if (!game || game->snapshot || game->fatal || index >= MC_TRANSFER_MAX_PLAYERS || !profile ||
        !sink)
        return MC_PACKET_QUEUE_FAILED;
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, game->heap))
        return MC_PACKET_QUEUE_FAILED;
    MCGameplayObjects *owners = MCGameplay_get(game);
    MCObject *actor = owners ? owners->players[index] : NULL;
    if (!MCGameplayPlayer_isInstance(actor)) {
        MCObjectRootScope_end(&scope);
        return MC_PACKET_QUEUE_FAILED;
    }
    MCGameplayPlayer *player = (MCGameplayPlayer *)actor;
    if (!profile->durable && (!MCGameplayWorld_isInstance(owners->world) ||
                              !((MCGameplayWorld *)owners->world)->remote ||
                              (MCObject *)player->worldObj != owners->world)) {
        MCObjectRootScope_end(&scope);
        return MC_PACKET_QUEUE_FAILED;
    }
    MCObject *object = player->pendingPackets;
    if (!MCPacketQueue_isInstance(object, profile) || object->heap != game->heap) {
        MCObjectRootScope_end(&scope);
        return MC_PACKET_QUEUE_FAILED;
    }
    Queue *q = (Queue *)object;
    if (q->flushing || q->owners != owners) {
        MCObjectRootScope_end(&scope);
        return MC_PACKET_QUEUE_FAILED;
    }
    MCPacketQueueResult result = MC_PACKET_QUEUE_SENT;
    q->flushing = true;
    while (q->start < q->end) {
        Entry *e = &q->storage->entries[q->start];
        uint64_t fence = profile->durable ? owners->durableSerial : owners->commitSerial;
        if (e->queuedAtSerial >= fence) {
            result = MC_PACKET_QUEUE_UNCOMMITTED;
            break;
        }
        mc_buf b = {0};
        if (!MCPacketQueue_encodeAt(object, 0, &b)) {
            game->fatal = true;
            result = MC_PACKET_QUEUE_FAILED;
            mc_buf_free(&b);
            break;
        }
        bool sent = sink(&b, context);
        mc_buf_free(&b);
        if (sent) {
            q->storage->entries[q->start].packet = NULL;
            ++q->start;
            MCObjectHeap_touch(game->heap);
        }
        if (MCObjectHeap_failed(game->heap)) {
            game->fatal = true;
            result = MC_PACKET_QUEUE_FAILED;
            break;
        }
        if (!sent) {
            result = MC_PACKET_QUEUE_FAILED;
            break;
        }
    }
    if (q->start == q->end)
        q->start = q->end = 0;
    q->flushing = false;
    MCObjectRootScope_end(&scope);
    return result;
}
