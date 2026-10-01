#include "util/MCGameplayPackets.h"
#include <limits.h>
#include <string.h>
typedef struct {
    MCObject *packet;
    MCGameplayPacketKind kind;
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
    Entries *storage;
    int32_t start, end;
    bool flushing;
} Packets;
static void entries_trace(MCObject *o, MCObjectVisitor v, void *c) {
    Entries *e = (Entries *)o;
    for (int32_t i = 0; i < e->capacity; i++)
        e->entries[i].packet = v(e->entries[i].packet, c);
}
static void packets_trace(MCObject *o, MCObjectVisitor v, void *c) {
    Packets *p = (Packets *)o;
    p->owners = (MCGameplayObjects *)v((MCObject *)p->owners, c);
    p->storage = (Entries *)v((MCObject *)p->storage, c);
}
static const MCObjectClass entriesClass = {"native.GameplayPacketEntries", MCObjectHeap_plainClone,
                                           entries_trace, NULL};
static const MCObjectClass packetsClass = {"native.GameplayPackets", MCObjectHeap_plainClone,
                                           packets_trace, NULL};
static Packets *packets(const MCGameplayPlayer *player) {
    if (!player)
        return NULL;
    if (!MCGameplayPlayer_isInstance((const MCObject *)player)) {
        MCObjectHeap_fail(player->object.heap);
        return NULL;
    }
    MCObject *o = player->pendingPackets;
    if (!o || o->heap != player->object.heap || o->klass != &packetsClass) {
        MCObjectHeap_fail(player->object.heap);
        return NULL;
    }
    return (Packets *)o;
}
bool MCGameplayPackets_bind(MCGameplayPlayer *player) {
    if (!player || MCObjectHeap_failed(player->object.heap))
        return false;
    if (!MCGameplayPlayer_isInstance((const MCObject *)player)) {
        MCObjectHeap_fail(player->object.heap);
        return false;
    }
    if (!player->pendingPackets) {
        MCObjectHeap *heap = player->object.heap;
        if (!player->worldObj || player->worldObj->object.heap != heap ||
            !player->worldObj->owners || player->worldObj->owners->object.heap != heap) {
            MCObjectHeap_fail(heap);
            return false;
        }
        Packets *p = (Packets *)MCObjectHeap_alloc(heap, sizeof(*p), &packetsClass);
        if (!p)
            return false;
        p->owners = player->worldObj->owners;
        player->pendingPackets = (MCObject *)p;
        MCObjectHeap_touch(heap);
    }
    if (!packets(player))
        return false;
    static const EntityPlayerMPWindowsDependencies dependencies = {
        MCGameplayPackets_sendWindowItems, MCGameplayPackets_sendSetSlot,
        MCGameplayPackets_sendCloseWindow};
    return EntityPlayerMPWindows_bind(player, &dependencies);
}
static bool append(MCGameplayPlayer *player, MCObject *packet, MCGameplayPacketKind kind) {
    MCObjectHeap *heap = player->object.heap;
    Packets *p = packets(player);
    if (!p || p->flushing || !packet || packet->heap != heap || !p->owners ||
        p->owners->object.heap != heap) {
        MCObjectHeap_fail(heap);
        return false;
    }
    if (!p->storage || p->end == p->storage->capacity) {
        int32_t count = p->end - p->start;
        int64_t capacity = p->storage ? (int64_t)p->storage->capacity * 2 : 16;
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
            memcpy(next->entries, p->storage->entries + p->start, (size_t)count * sizeof(Entry));
        p->storage = next;
        p->start = 0;
        p->end = count;
    }
    p->storage->entries[p->end++] = (Entry){packet, kind, p->owners->commitSerial};
    MCObjectHeap_touch(heap);
    return true;
}
bool MCGameplayPackets_sendWindowItems(MCGameplayPlayer *p, int32_t w, ContainerList *l) {
    if (!p)
        return false;
    return append(p, (MCObject *)S30PacketWindowItems_new(p->object.heap, w, l),
                  MC_GAMEPLAY_PACKET_ITEMS);
}
bool MCGameplayPackets_sendSetSlot(MCGameplayPlayer *p, int32_t w, int32_t s, ItemStack *i) {
    if (!p)
        return false;
    return append(p, (MCObject *)S2FPacketSetSlot_new(p->object.heap, w, s, i),
                  MC_GAMEPLAY_PACKET_SLOT);
}
bool MCGameplayPackets_sendCloseWindow(MCGameplayPlayer *p, int32_t w) {
    if (!p)
        return false;
    return append(p, (MCObject *)S2EPacketCloseWindow_new(p->object.heap, w),
                  MC_GAMEPLAY_PACKET_CLOSE);
}
bool MCGameplayPackets_sendConfirmTransaction(MCGameplayPlayer *p, int32_t w, int16_t a,
                                              bool accepted) {
    if (!p)
        return false;
    return append(p, (MCObject *)S32PacketConfirmTransaction_new(p->object.heap, w, a, accepted),
                  MC_GAMEPLAY_PACKET_CONFIRM);
}
int32_t MCGameplayPackets_count(const MCGameplayPlayer *player) {
    Packets *p = packets(player);
    return p ? p->end - p->start : 0;
}
MCObject *MCGameplayPackets_packetAt(MCGameplayPlayer *player, int32_t index,
                                     MCGameplayPacketKind *kind) {
    Packets *p = packets(player);
    if (!p)
        return NULL;
    if (index < 0 || index >= p->end - p->start) {
        MCObjectHeap_fail(player->object.heap);
        return NULL;
    }
    Entry *entry = &p->storage->entries[p->start + index];
    if (kind)
        *kind = entry->kind;
    return entry->packet;
}
bool MCGameplayPackets_encodeAt(MCGameplayPlayer *player, int32_t index, mc_buf *out) {
    if (!player || !out)
        return false;
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, player->object.heap))
        return false;
    MCGameplayPacketKind kind = MC_GAMEPLAY_PACKET_CLOSE;
    MCObject *o = MCGameplayPackets_packetAt(player, index, &kind);
    mc_buf buffer = {0};
    PacketBuffer view;
    bool ok = o && PacketBuffer_init(&view, player->object.heap, &buffer);
    if (ok) {
        mc_put_varint(&buffer, kind);
        switch (kind) {
        case MC_GAMEPLAY_PACKET_CLOSE:
            ok = S2EPacketCloseWindow_writePacketData((S2EPacketCloseWindow *)o, &view);
            break;
        case MC_GAMEPLAY_PACKET_SLOT:
            ok = S2FPacketSetSlot_writePacketData((S2FPacketSetSlot *)o, &view);
            break;
        case MC_GAMEPLAY_PACKET_ITEMS:
            ok = S30PacketWindowItems_writePacketData((S30PacketWindowItems *)o, &view);
            break;
        case MC_GAMEPLAY_PACKET_CONFIRM:
            ok = S32PacketConfirmTransaction_writePacketData((S32PacketConfirmTransaction *)o,
                                                             &view);
            break;
        default:
            ok = false;
            MCObjectHeap_fail(player->object.heap);
            break;
        }
    }
    ok = ok && !buffer.failed && buffer.len <= MC_MAX_PACKET &&
         !MCObjectHeap_failed(player->object.heap);
    if (ok) {
        mc_buf_free(out);
        *out = buffer;
        mc_buf_init(&buffer);
    } else
        MCObjectHeap_fail(player->object.heap);
    mc_buf_free(&buffer);
    MCObjectRootScope_end(&scope);
    return ok;
}
bool MCGameplayPackets_validate(MCGameplayPlayer *player) {
    if (!player || !packets(player))
        return false;
    int32_t count = MCGameplayPackets_count(player);
    for (int32_t i = 0; i < count; i++) {
        mc_buf buffer = {0};
        bool ok = MCGameplayPackets_encodeAt(player, i, &buffer);
        mc_buf_free(&buffer);
        if (!ok)
            return false;
    }
    return !MCObjectHeap_failed(player->object.heap);
}
MCGameplayPacketsResult MCGameplayPackets_flush(MCGameplay *game, size_t index,
                                                MCGameplayPacketSink sink, void *context) {
    if (!game || game->snapshot || game->fatal || index >= MC_TRANSFER_MAX_PLAYERS || !sink)
        return MC_GAMEPLAY_PACKETS_FAILED;
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, game->heap))
        return MC_GAMEPLAY_PACKETS_FAILED;
    MCGameplayObjects *owners = MCGameplay_get(game);
    MCObject *actor = owners ? owners->players[index] : NULL;
    if (!MCGameplayPlayer_isInstance(actor)) {
        MCObjectRootScope_end(&scope);
        return MC_GAMEPLAY_PACKETS_FAILED;
    }
    MCGameplayPlayer *player = (MCGameplayPlayer *)actor;
    Packets *p = packets(player);
    MCGameplayPacketsResult result = MC_GAMEPLAY_PACKETS_SENT;
    if (!p || p->flushing || p->owners != owners || player->object.heap != game->heap) {
        MCObjectRootScope_end(&scope);
        return MC_GAMEPLAY_PACKETS_FAILED;
    }
    p->flushing = true;
    while (p->start < p->end) {
        Entry *entry = &p->storage->entries[p->start];
        if (entry->queuedAtSerial >= owners->commitSerial) {
            result = MC_GAMEPLAY_PACKETS_UNCOMMITTED;
            break;
        }
        mc_buf buffer = {0};
        if (!MCGameplayPackets_encodeAt(player, 0, &buffer)) {
            game->fatal = true;
            result = MC_GAMEPLAY_PACKETS_FAILED;
            mc_buf_free(&buffer);
            break;
        }
        bool sent = sink(&buffer, context);
        mc_buf_free(&buffer);
        if (sent) {
            p->storage->entries[p->start].packet = NULL;
            ++p->start;
            MCObjectHeap_touch(game->heap);
        }
        if (MCObjectHeap_failed(game->heap)) {
            game->fatal = true;
            result = MC_GAMEPLAY_PACKETS_FAILED;
            break;
        }
        if (!sent) {
            result = MC_GAMEPLAY_PACKETS_FAILED;
            break;
        }
    }
    if (p->start == p->end)
        p->start = p->end = 0;
    p->flushing = false;
    MCObjectRootScope_end(&scope);
    return result;
}
