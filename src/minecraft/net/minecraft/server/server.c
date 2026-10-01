#include "server.h"
#include "../network/protocol.h"
#include "../world/world.h"
#include "../item/item.h"
#include "util/transfer.h"
#include "world/map.h"
#include "item/ItemMap.h"
#include "server/native_gameplay.h"
#include "network/GameplayPacketRouter.h"
#include "network/play/client/C08PacketPlayerBlockPlacement.h"
#include "network/play/client/C07PacketPlayerDigging.h"
#include "network/play/client/C09PacketHeldItemChange.h"
#include "entity/item/NativeItemMotion.h"
#include "util/MCGameplayPackets.h"
#include "inventory/ContainerWorkbench.h"
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mc_mkdir(path) _mkdir(path)
#else
#include <sys/types.h>
#define mc_mkdir(path) mkdir(path, 0755)
#endif

#define SERVER_CONNECTIONS 64
#define WORLD_MIN (-48)
#define WORLD_MAX 64
#define LOGIN_TIMEOUT_MS 10000u
#define KEEPALIVE_INTERVAL_MS 10000u
#define KEEPALIVE_TIMEOUT_MS 30000u
static const int16_t creative_palette[9] = {1, 2, 3, 4, 5, 20, 12, 17, 45};
static volatile sig_atomic_t server_running = 1;

typedef enum { STATE_HANDSHAKE, STATE_STATUS, STATE_LOGIN, STATE_PLAY } peer_state;
typedef struct {
    bool used, closing, status_requested, pinged, keepalive_pending;
    peer_state state;
    mc_conn conn;
    uint64_t connected_at, keepalive_at, close_at;
    int32_t entity, keepalive_id;
    uint8_t uuid[16];
    char name[17];
    double x, y, z; /* Native transport/camera position, synchronized into actor on movement. */
    float yaw, pitch;
    bool grounded, sneaking;
    MCGameplay *game;
    size_t ownerIndex;
    int gamemode;
    uint64_t chunks_sent;
    int32_t tracked_items[MC_GAMEPLAY_MAX_ITEMS];
    size_t tracked_item_count;
} server_peer;
typedef struct {
    mc_world world;
    server_peer peers[SERVER_CONNECTIONS];
    mc_socket listener;
    const char *save_path;
    int max_players, spawn_x, spawn_z;
    bool fatal;
    MCGameplay gameplay;
    int defaultGamemode;
    int64_t map_tick;
    uint64_t last_item_tick;
} mc_server;

static MCGameplayPlayer *actor(const server_peer *);
static void stop_server(int sig) {
    (void)sig;
    server_running = 0;
}
static void packet_start(mc_buf *packet, int id) {
    mc_buf_init(packet);
    mc_put_varint(packet, id);
}
static void queue_packet(server_peer *peer, mc_buf *packet) {
    if (peer->used && !peer->conn.closed)
        mc_conn_send(&peer->conn, packet);
    mc_buf_free(packet);
}
static bool playing(const server_peer *peer) {
    return peer->used && !peer->closing && !peer->conn.closed && peer->state == STATE_PLAY;
}
static int player_count(const mc_server *server) {
    int count = 0;
    for (int i = 0; i < SERVER_CONNECTIONS; ++i)
        if (playing(&server->peers[i]))
            ++count;
    return count;
}
static void broadcast(mc_server *server, const mc_buf *packet, const server_peer *except) {
    for (int i = 0; i < SERVER_CONNECTIONS; ++i) {
        server_peer *peer = &server->peers[i];
        if (peer != except && playing(peer))
            mc_conn_send(&peer->conn, packet);
    }
}
static void disconnect_peer(server_peer *peer, const char *reason) {
    if (!peer->used || peer->closing || peer->conn.closed)
        return;
    if (peer->state == STATE_LOGIN || peer->state == STATE_PLAY) {
        mc_buf packet;
        char escaped[512], json[560];
        mc_json_escape(reason, escaped, sizeof escaped);
        snprintf(json, sizeof json, "{\"text\":\"%s\"}", escaped);
        packet_start(&packet, peer->state == STATE_PLAY ? 0x40 : 0);
        mc_put_string(&packet, json);
        queue_packet(peer, &packet);
    }
    peer->closing = true;
    peer->close_at = mc_time_ms();
}

static void player_info(mc_buf *packet, const server_peer *peer, bool add) {
    packet_start(packet, 0x38);
    mc_put_varint(packet, add ? 0 : 4);
    mc_put_varint(packet, 1);
    mc_put_bytes(packet, peer->uuid, 16);
    if (add) {
        mc_put_string(packet, peer->name);
        mc_put_varint(packet, 0);
        mc_put_varint(packet, peer->gamemode);
        mc_put_varint(packet, 0);
        mc_put_u8(packet, 0);
    }
}
static uint8_t angle_byte(float angle) {
    float normalized = fmodf(angle, 360.0f);
    if (normalized < 0)
        normalized += 360.0f;
    return (uint8_t)(normalized * (256.0f / 360.0f));
}
static void spawn_player(mc_buf *packet, const server_peer *peer) {
    MCObjectReadScope scope = {0};
    MCObjectReadScope_begin(&scope, peer->game->heap);
    MCGameplayPlayer *p = actor(peer);
    packet_start(packet, 0x0c);
    mc_put_varint(packet, p->entityId);
    mc_put_bytes(packet, peer->uuid, 16);
    mc_put_i32(packet, (int32_t)floor(p->posX * 32));
    mc_put_i32(packet, (int32_t)floor(p->posY * 32));
    mc_put_i32(packet, (int32_t)floor(p->posZ * 32));
    mc_put_u8(packet, angle_byte(p->rotationYaw));
    mc_put_u8(packet, angle_byte(p->rotationPitch));
    ItemStack *held = InventoryPlayer_getCurrentItem(p->inventory);
    mc_put_i16(packet, held ? (int16_t)ItemStack_registryId(held->item) : 0);
    mc_put_u8(packet, 0x7f);
    MCObjectReadScope_end(&scope);
}

static void send_position(server_peer *peer) {
    mc_buf packet;
    packet_start(&packet, 8);
    mc_put_f64(&packet, peer->x);
    mc_put_f64(&packet, peer->y);
    mc_put_f64(&packet, peer->z);
    mc_put_f32(&packet, peer->yaw);
    mc_put_f32(&packet, peer->pitch);
    mc_put_u8(&packet, 0);
    queue_packet(peer, &packet);
}
static void send_equipment(mc_server *server, server_peer *peer) {
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, peer->game->heap))
        return;
    MCGameplayPlayer *p = actor(peer);
    for (int equipment = 0; equipment < 5; equipment++) {
        mc_buf out;
        packet_start(&out, 4);
        mc_put_varint(&out, p->entityId);
        mc_put_i16(&out, (int16_t)equipment);
        ItemStack *stack = equipment == 0 ? InventoryPlayer_getCurrentItem(p->inventory)
                                          : p->inventory->armorInventory->items[equipment - 1];
        PacketBuffer b;
        if (PacketBuffer_init(&b, peer->game->heap, &out) &&
            PacketBuffer_writeItemStackToBuffer(&b, stack))
            broadcast(server, &out, peer);
        mc_buf_free(&out);
    }
    MCObjectRootScope_end(&scope);
}

static void send_equipment_to(server_peer *recipient, const server_peer *subject) {
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, subject->game->heap))
        return;
    MCGameplayPlayer *p = actor(subject);
    for (int equipment = 0; equipment < 5; equipment++) {
        mc_buf out;
        packet_start(&out, 4);
        mc_put_varint(&out, p->entityId);
        mc_put_i16(&out, (int16_t)equipment);
        ItemStack *stack = equipment == 0 ? InventoryPlayer_getCurrentItem(p->inventory)
                                          : p->inventory->armorInventory->items[equipment - 1];
        PacketBuffer b;
        if (PacketBuffer_init(&b, subject->game->heap, &out) &&
            PacketBuffer_writeItemStackToBuffer(&b, stack))
            queue_packet(recipient, &out);
        else
            mc_buf_free(&out);
    }
    MCObjectRootScope_end(&scope);
}

static void chat(mc_server *server, const char *name, const char *message) {
    mc_buf packet;
    char text[448], escaped[2800], json[2860];
    if (name)
        snprintf(text, sizeof text, "<%s> %s", name, message);
    else
        snprintf(text, sizeof text, "%s", message);
    mc_json_escape(text, escaped, sizeof escaped);
    snprintf(json, sizeof json, "{\"text\":\"%s\"}", escaped);
    packet_start(&packet, 2);
    mc_put_string(&packet, json);
    mc_put_u8(&packet, 0);
    broadcast(server, &packet, NULL);
    mc_buf_free(&packet);
}
static MCGameplayPlayer *actor(const server_peer *peer) {
    return mc_server_graph_player(peer->game, peer->ownerIndex);
}
static bool source_sink(const mc_buf *packet, void *context) {
    server_peer *peer = context;
    if (!mc_conn_send(&peer->conn, packet))
        return false;
    /* Native transport subscriptions advance only after the whole message has
       been accepted. They retain scalar entity IDs, never ItemStack owners. */
    mc_buf input = *packet;
    input.pos = 0;
    int32_t id = mc_get_varint(&input);
    if (id == 0x0e) {
        int32_t entity = mc_get_varint(&input);
        bool found = false;
        for (size_t i = 0; i < peer->tracked_item_count; i++)
            if (peer->tracked_items[i] == entity)
                found = true;
        if (!input.failed && !found && peer->tracked_item_count < MC_GAMEPLAY_MAX_ITEMS)
            peer->tracked_items[peer->tracked_item_count++] = entity;
    } else if (id == 0x13) {
        int32_t count = mc_get_varint(&input);
        for (int32_t n = 0; n < count && !input.failed; n++) {
            int32_t entity = mc_get_varint(&input);
            for (size_t i = 0; i < peer->tracked_item_count; i++)
                if (peer->tracked_items[i] == entity) {
                    peer->tracked_items[i] = peer->tracked_items[--peer->tracked_item_count];
                    break;
                }
        }
    }
    return true;
}
static bool flush_source(mc_server *server) {
    for (int i = 0; i < SERVER_CONNECTIONS; i++) {
        server_peer *p = &server->peers[i];
        if (!p->used || p->state != STATE_PLAY || p->conn.closed || !actor(p))
            continue;
        MCGameplayPacketsResult result =
            MCGameplayPackets_flush(&server->gameplay, p->ownerIndex, source_sink, p);
        if (result == MC_GAMEPLAY_PACKETS_FAILED) {
            disconnect_peer(p, "Unable to queue committed gameplay packets.");
            return false;
        }
    }
    return true;
}
static bool commit_graph(mc_server *server, server_peer *peer, MCGameplayTransaction *tx) {
    char error[256] = "Source gameplay graph could not be committed";
    MCGameplayCommit result = MCGameplay_commit(tx, server->save_path, MCGameplayStorage_encoders(),
                                                NULL, error, sizeof error);
    if (result != MC_GAMEPLAY_COMMITTED) {
        fprintf(stderr, "Gameplay commit: %s\n", error);
        if (result == MC_GAMEPLAY_COMMITTED_NEEDS_RECOVERY || !peer)
            server->fatal = true;
        if (peer)
            disconnect_peer(peer, result == MC_GAMEPLAY_COMMITTED_NEEDS_RECOVERY
                                      ? "Gameplay commitment requires recovery on restart."
                                      : "Inventory/gameplay mutation was not committed.");
        return false;
    }
    return flush_source(server);
}
static bool apply_source_packet(mc_server *server, server_peer *peer, int32_t id, mc_buf *packet) {
    MCGameplayTransaction tx = {0};
    if (!MCGameplay_begin(&server->gameplay, &tx))
        return false;
    GameplayPacketResult result =
        GameplayPacketRouter_server(&tx.working, peer->ownerIndex, id, packet);
    MCObjectRootScope scope = {0};
    bool ok =
        result == MC_GAMEPLAY_PACKET_APPLIED && MCObjectRootScope_begin(&scope, tx.working.heap);
    if (ok)
        ok = mc_server_graph_detect_changes(MCGameplay_get(&tx.working));
    bool budgetOk = ok && mc_server_graph_preflight_inventory(MCGameplay_get(&tx.working));
    MCObjectRootScope_end(&scope);
    if (!ok) {
        MCGameplay_abort(&tx);
        disconnect_peer(peer, "Malformed source gameplay packet or failed dependency.");
        return false;
    }
    if (!budgetOk) {
        MCGameplay_abort(&tx);
        /* Explicit native C10 size-rejection policy. Source mutations/effects
           are all discarded, then the existing real Source owner is resent. */
        if (id != 0x10 || !MCGameplay_begin(&server->gameplay, &tx)) {
            disconnect_peer(peer, "Inventory exceeded the native storage/packet budget.");
            return false;
        }
        ok = MCObjectRootScope_begin(&scope, tx.working.heap);
        MCGameplayPlayer *p = ok ? mc_server_graph_player(&tx.working, peer->ownerIndex) : NULL;
        ok = p && EntityPlayerMPWindows_sendContainerToPlayer(p, p->openContainer);
        MCObjectRootScope_end(&scope);
        if (!ok) {
            MCGameplay_abort(&tx);
            disconnect_peer(peer, "Inventory resynchronization failed.");
            return false;
        }
    }
    return commit_graph(server, peer, &tx);
}
static bool player_path(const mc_server *server, const server_peer *peer, char *directory,
                        char *path) {
    if (strlen(server->save_path) > 3900)
        return false;
    char uuid[37];
    mc_uuid_string(peer->uuid, uuid);
    snprintf(directory, 4096, "%s.players", server->save_path);
    snprintf(path, 4096, "%s.players/%s.dat", server->save_path, uuid);
    return true;
}
static bool item_visible(const server_peer *peer, const EntityItem *item) {
    if (fabs(item->posX - peer->x) > 64 || fabs(item->posZ - peer->z) > 64)
        return false;
    int cx = mc_floor_div16((int)floor(item->posX)), cz = mc_floor_div16((int)floor(item->posZ));
    if (cx < -3 || cx > 3 || cz < -3 || cz > 3)
        return false;
    return (peer->chunks_sent & (UINT64_C(1) << ((cz + 3) * 7 + cx + 3))) != 0;
}

static bool item_tracked(const server_peer *peer, int32_t eid) {
    for (size_t i = 0; i < peer->tracked_item_count; ++i)
        if (peer->tracked_items[i] == eid)
            return true;
    return false;
}

static void sync_items(mc_server *server, server_peer *peer) {
    MCGameplayTransaction tx = {0};
    if (!MCGameplay_begin(&server->gameplay, &tx))
        return;
    MCObjectRootScope scope = {0};
    bool ok = MCObjectRootScope_begin(&scope, tx.working.heap);
    MCGameplayObjects *o = MCGameplay_get(&tx.working);
    bool queued = false;
    size_t count = 0;
    for (size_t i = 0; i < o->itemCount && ok; i++) {
        EntityItem *e = (EntityItem *)o->items[i];
        if (!e->isDead && item_visible(peer, e) && !item_tracked(peer, e->entityId) &&
            peer->tracked_item_count + count < MC_GAMEPLAY_MAX_ITEMS) {
            ok =
                mc_server_graph_send_item(mc_server_graph_player(&tx.working, peer->ownerIndex), e);
            count++;
            queued = true;
        }
    }
    MCObjectRootScope_end(&scope);
    if (ok && queued)
        (void)commit_graph(server, peer, &tx);
    else
        MCGameplay_abort(&tx);
}

static bool close_inventory(mc_server *server, server_peer *peer) {
    MCGameplayTransaction tx = {0};
    if (!MCGameplay_begin(&server->gameplay, &tx))
        return false;
    MCObjectRootScope scope = {0};
    bool ok = MCObjectRootScope_begin(&scope, tx.working.heap);
    MCGameplayPlayer *p = ok ? mc_server_graph_player(&tx.working, peer->ownerIndex) : NULL;
    ok = p && mc_server_graph_close(p, false) &&
         mc_server_graph_detect_changes(MCGameplay_get(&tx.working));
    MCObjectRootScope_end(&scope);
    if (!ok) {
        MCGameplay_abort(&tx);
        return false;
    }
    return commit_graph(server, peer, &tx);
}

static void remove_peer(mc_server *server, server_peer *peer) {
    bool was_player = peer->state == STATE_PLAY;
    if (actor(peer)) {
        if (!server->fatal && !close_inventory(server, peer))
            server->fatal = true;
        /* Removing the native session owner never frees a shared stack. */
        MCObjectRootScope scope = {0};
        if (MCObjectRootScope_begin(&scope, server->gameplay.heap)) {
            MCGameplayPlayer *p = actor(peer);
            p->isDead = true;
            MCGameplay_setPlayer(&server->gameplay, peer->ownerIndex, NULL, NULL);
            MCObjectRootScope_end(&scope);
        }
    }
    if (was_player) {
        mc_buf out;
        player_info(&out, peer, false);
        broadcast(server, &out, peer);
        mc_buf_free(&out);
        packet_start(&out, 0x13);
        mc_put_varint(&out, 1);
        mc_put_varint(&out, peer->entity);
        broadcast(server, &out, peer);
        mc_buf_free(&out);
    }
    mc_conn_close(&peer->conn);
    mc_buf_free(&peer->conn.rx);
    mc_buf_free(&peer->conn.tx);
    memset(peer, 0, sizeof *peer);
    MCObjectHeap_collect(server->gameplay.heap);
}

static bool valid_name(const char *name) {
    size_t length = strlen(name);
    if (!length || length > 16)
        return false;
    for (size_t i = 0; i < length; ++i) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_'))
            return false;
    }
    return true;
}
static bool name_equal(const char *a, const char *b) {
    for (; *a && *b; ++a, ++b) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z')
            x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z')
            y += 'a' - 'A';
        if (x != y)
            return false;
    }
    return *a == *b;
}
static bool complete(mc_buf *packet) {
    return !packet->failed && packet->pos == packet->len;
}
static bool get_boolean(mc_buf *packet) {
    uint8_t value = mc_get_u8(packet);
    if (value > 1)
        packet->failed = true;
    return value != 0;
}
static bool select_spawn(mc_server *server, server_peer *peer) {
    for (int radius = 0; radius <= 64; ++radius) {
        for (int dz = -radius; dz <= radius; ++dz)
            for (int dx = -radius; dx <= radius; ++dx) {
                if (radius && abs(dx) != radius && abs(dz) != radius)
                    continue;
                int x = 8 + dx, z = 8 + dz;
                if (x < WORLD_MIN || x >= WORLD_MAX || z < WORLD_MIN || z >= WORLD_MAX)
                    continue;
                if (!mc_world_chunk(&server->world, mc_floor_div16(x), mc_floor_div16(z), false))
                    continue;
                int surface = mc_world_surface(&server->world, x, z);
                if (surface > 252)
                    continue;
                peer->x = x + 0.5;
                peer->y = surface + 2.0;
                peer->z = z + 0.5;
                return true;
            }
    }
    return false;
}
static void join_game(mc_server *server, server_peer *peer) {
    if (!select_spawn(server, peer)) {
        disconnect_peer(peer, "No clear spawn is available.");
        return;
    }
    peer->gamemode = server->defaultGamemode;
    mc_offline_uuid(peer->name, peer->uuid);
    char uuid[37];
    mc_uuid_string(peer->uuid, uuid);
    MCGameplayTransaction tx = {0};
    if (!MCGameplay_begin(&server->gameplay, &tx)) {
        disconnect_peer(peer, "Unable to allocate source player graph.");
        return;
    }
    MCObjectRootScope scope = {0};
    bool ok = MCObjectRootScope_begin(&scope, tx.working.heap);
    MCGameplayWorld *w = mc_server_graph_world(&tx.working);
    int32_t id = ok ? mc_server_graph_allocate_entity(w) : 0;
    ok = id && mc_server_graph_add_player(&tx.working, peer->ownerIndex, uuid, peer->name, id,
                                          peer->x, peer->y, peer->z, peer->gamemode == 1);
    MCGameplayPlayer *p = ok ? mc_server_graph_player(&tx.working, peer->ownerIndex) : NULL;
    char directory[4096], path[4096], error[256] = "Could not load source player";
    struct stat info;
    mc_nbt input = {0};
    if (ok)
        ok = player_path(server, peer, directory, path);
    if (ok && !stat(path, &info))
        ok = mc_nbt_load_gzip(&input, path, error, sizeof error) &&
             MCGameplayStorage_loadPlayer(p, &input, mc_server_graph_crafting());
    else if (ok && errno == ENOENT) {
        if (peer->gamemode == 1)
            for (int i = 0; i < 9 && ok; i++)
                ok = InventoryPlayer_setInventorySlotContents(
                    p->inventory, i,
                    ItemStack_new(p->object.heap, ItemStack_registryItem(creative_palette[i]), 64,
                                  0));
    } else if (ok)
        ok = false;
    mc_nbt_free(&input);
    if (ok && p->openContainer != &p->inventoryContainer->container)
        ok = mc_server_graph_close(p, false);
    if (ok)
        ok = Container_onCraftGuiOpened(&p->inventoryContainer->container,
                                        EntityPlayerMPWindows_listener(p));
    MCObjectRootScope_end(&scope);
    if (!ok) {
        MCGameplay_abort(&tx);
        disconnect_peer(peer,
                        "Player data or source dependency failed; existing data was preserved.");
        return;
    }
    if (!commit_graph(server, peer, &tx))
        return;
    peer->entity = id;
    peer->keepalive_at = mc_time_ms();
    mc_buf packet;
    packet_start(&packet, 2);
    mc_put_string(&packet, uuid);
    mc_put_string(&packet, peer->name);
    queue_packet(peer, &packet);
    peer->state = STATE_PLAY;
    packet_start(&packet, 1);
    mc_put_i32(&packet, id);
    mc_put_u8(&packet, (uint8_t)peer->gamemode);
    mc_put_u8(&packet, 0);
    mc_put_u8(&packet, 0);
    mc_put_u8(&packet, (uint8_t)server->max_players);
    mc_put_string(&packet, "flat");
    mc_put_u8(&packet, 0);
    queue_packet(peer, &packet);
    packet_start(&packet, 0x39);
    mc_put_u8(&packet, peer->gamemode == 1 ? 0x0f : 0);
    mc_put_f32(&packet, 0.05f);
    mc_put_f32(&packet, 0.1f);
    queue_packet(peer, &packet);
    packet_start(&packet, 5);
    mc_put_position(&packet, (int)floor(peer->x), (int)peer->y, (int)floor(peer->z));
    queue_packet(peer, &packet);
    flush_source(server);
    packet_start(&packet, 9);
    mc_put_u8(&packet, (uint8_t)actor(peer)->inventory->currentItem);
    queue_packet(peer, &packet);
    send_position(peer);
    for (int i = 0; i < SERVER_CONNECTIONS; i++) {
        server_peer *other = &server->peers[i];
        if (!playing(other))
            continue;
        player_info(&packet, other, true);
        queue_packet(peer, &packet);
        if (other != peer) {
            spawn_player(&packet, other);
            queue_packet(peer, &packet);
            send_equipment_to(peer, other);
            player_info(&packet, peer, true);
            queue_packet(other, &packet);
            spawn_player(&packet, peer);
            queue_packet(other, &packet);
            send_equipment_to(other, peer);
        }
    }
    printf("Player %s joined (entity %d).\n", peer->name, id);
    fflush(stdout);
}

static void send_chunk(mc_server *server, server_peer *peer, int cx, int cz) {
    const mc_chunk *chunk = mc_world_chunk(&server->world, cx, cz, false);
    mc_buf packet, data;
    uint16_t mask = 0;
    if (!chunk) {
        disconnect_peer(peer, "World chunk is unavailable.");
        return;
    }
    for (int section = 0; section < 16; ++section) {
        for (int i = 0; i < 4096; ++i) {
            if (chunk->blocks[section * 4096 + i]) {
                mask |= (uint16_t)(1u << section);
                break;
            }
        }
    }
    mc_buf_init(&data);
    for (int section = 0; section < 16; ++section)
        if (mask & (1u << section)) {
            for (int i = 0; i < 4096; ++i) {
                uint16_t state = chunk->blocks[section * 4096 + i];
                mc_put_u8(&data, (uint8_t)state);
                mc_put_u8(&data, (uint8_t)(state >> 8));
            }
        }
    /* Protocol 47 groups data by channel, not by section. */
    for (int section = 0; section < 16; ++section)
        if (mask & (1u << section))
            for (int i = 0; i < 2048; ++i)
                mc_put_u8(&data, 0);
    for (int section = 0; section < 16; ++section)
        if (mask & (1u << section))
            for (int i = 0; i < 2048; ++i)
                mc_put_u8(&data, 0xff);
    for (int i = 0; i < 256; ++i)
        mc_put_u8(&data, 1);
    packet_start(&packet, 0x21);
    mc_put_i32(&packet, cx);
    mc_put_i32(&packet, cz);
    mc_put_u8(&packet, 1);
    mc_put_i16(&packet, (int16_t)mask);
    mc_put_varint(&packet, (int32_t)data.len);
    mc_put_bytes(&packet, data.data, data.len);
    mc_buf_free(&data);
    queue_packet(peer, &packet);
}
static bool world_coordinate(int x, int y, int z) {
    return x >= WORLD_MIN && x < WORLD_MAX && z >= WORLD_MIN && z < WORLD_MAX && y >= 1 && y < 256;
}
static bool reachable(const server_peer *peer, int x, int y, int z) {
    double dx = x + 0.5 - peer->x, dy = y + 0.5 - (peer->y + 1.62), dz = z + 0.5 - peer->z;
    return dx * dx + dy * dy + dz * dz <= 36.0;
}
static bool table_usable(const mc_server *server, const server_peer *peer) {
    (void)server;
    MCGameplayPlayer *p = actor(peer);
    if (!p)
        return false;
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, p->object.heap))
        return false;
    bool ok = Container_canInteractWith(p->openContainer, p->inventory);
    MCObjectRootScope_end(&scope);
    return ok;
}

static bool table_reachable(const server_peer *peer, int x, int y, int z) {
    double dx = x + 0.5 - peer->x, dy = y + 0.5 - peer->y, dz = z + 0.5 - peer->z;
    return dx * dx + dy * dy + dz * dz < 64.0;
}
static bool open_workbench(mc_server *server, server_peer *peer, int x, int y, int z) {
    MCGameplayTransaction tx = {0};
    if (!MCGameplay_begin(&server->gameplay, &tx))
        return false;
    MCObjectRootScope scope = {0};
    bool ok = MCObjectRootScope_begin(&scope, tx.working.heap);
    MCGameplayPlayer *p = ok ? mc_server_graph_player(&tx.working, peer->ownerIndex) : NULL;
    ok = p && mc_server_graph_open_workbench(p, x, y, z) &&
         mc_server_graph_detect_changes(MCGameplay_get(&tx.working));
    MCObjectRootScope_end(&scope);
    if (!ok) {
        MCGameplay_abort(&tx);
        return false;
    }
    return commit_graph(server, peer, &tx);
}

static void invalidate_workbench(mc_server *server, server_peer *peer) {
    if (table_usable(server, peer))
        return;
    MCGameplayTransaction tx = {0};
    if (!MCGameplay_begin(&server->gameplay, &tx))
        return;
    MCObjectRootScope scope = {0};
    bool ok = MCObjectRootScope_begin(&scope, tx.working.heap);
    MCGameplayPlayer *p = ok ? mc_server_graph_player(&tx.working, peer->ownerIndex) : NULL;
    ok = p && mc_server_graph_close(p, true) &&
         mc_server_graph_detect_changes(MCGameplay_get(&tx.working));
    MCObjectRootScope_end(&scope);
    if (ok)
        commit_graph(server, peer, &tx);
    else
        MCGameplay_abort(&tx);
}

static void block_packet(mc_buf *packet, const mc_world *world, int x, int y, int z) {
    packet_start(packet, 0x23);
    mc_put_position(packet, x, y, z);
    mc_put_varint(packet, mc_world_get(world, x, y, z));
}
static void correct_block(server_peer *peer, const mc_world *world, int x, int y, int z) {
    mc_buf packet;
    block_packet(&packet, world, x, y, z);
    queue_packet(peer, &packet);
}
static void change_block(mc_server *server, server_peer *peer, int x, int y, int z,
                         uint16_t state) {
    uint16_t previous = mc_world_get(&server->world, x, y, z);
    if (!world_coordinate(x, y, z) || !reachable(peer, x, y, z) || (previous >> 4) == 7) {
        correct_block(peer, &server->world, x, y, z);
        return;
    }
    if (previous == state) {
        correct_block(peer, &server->world, x, y, z);
        return;
    }
    if (!mc_world_set(&server->world, x, y, z, state)) {
        disconnect_peer(peer, "World edit could not be applied.");
        return;
    }
    char error[256];
    if (!mc_world_save(&server->world, server->save_path, error, sizeof error)) {
        mc_world_set(&server->world, x, y, z, previous);
        fprintf(stderr, "World save failed; edit rolled back: %s\n", error);
        for (int i = 0; i < SERVER_CONNECTIONS; ++i)
            disconnect_peer(&server->peers[i], "World save failed; the edit was rolled back.");
        server->fatal = true;
        return;
    }
    mc_buf packet;
    block_packet(&packet, &server->world, x, y, z);
    broadcast(server, &packet, NULL);
    mc_buf_free(&packet);
}

static void handle_movement(mc_server *server, server_peer *peer, mc_buf *packet, int id) {
    double x = peer->x, y = peer->y, z = peer->z;
    float yaw = peer->yaw, pitch = peer->pitch;
    if (id == 4 || id == 6) {
        x = mc_get_f64(packet);
        y = mc_get_f64(packet);
        z = mc_get_f64(packet);
    }
    if (id == 5 || id == 6) {
        yaw = mc_get_f32(packet);
        pitch = mc_get_f32(packet);
    }
    bool grounded = get_boolean(packet);
    if (!complete(packet) || !isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(yaw) ||
        !isfinite(pitch) || pitch < -90 || pitch > 90) {
        disconnect_peer(peer, "Invalid movement packet.");
        return;
    }
    if (x < WORLD_MIN || x >= WORLD_MAX || z < WORLD_MIN || z >= WORLD_MAX || y < 1 || y >= 256) {
        send_position(peer);
        return;
    }
    peer->x = x;
    peer->y = y;
    peer->z = z;
    peer->yaw = yaw;
    peer->pitch = pitch;
    peer->grounded = grounded;
    MCObjectRootScope scope = {0};
    if (MCObjectRootScope_begin(&scope, server->gameplay.heap)) {
        MCGameplayPlayer *p = actor(peer);
        p->posX = x;
        p->posY = y;
        p->posZ = z;
        p->rotationYaw = yaw;
        p->rotationPitch = pitch;
        MCObjectRootScope_end(&scope);
    }

    mc_buf update;
    packet_start(&update, 0x18);
    mc_put_varint(&update, peer->entity);
    mc_put_i32(&update, (int32_t)floor(x * 32));
    mc_put_i32(&update, (int32_t)floor(y * 32));
    mc_put_i32(&update, (int32_t)floor(z * 32));
    mc_put_u8(&update, angle_byte(yaw));
    mc_put_u8(&update, angle_byte(pitch));
    mc_put_u8(&update, grounded);
    broadcast(server, &update, peer);
    mc_buf_free(&update);
}

typedef struct {
    mc_server *server;
    server_peer *peer;
} C08ServerHandler;
static void server_processPlayerBlockPlacement(void *opaque,
                                               const C08PacketPlayerBlockPlacement *packet) {
    C08ServerHandler *handler = opaque;
    mc_server *server = handler->server;
    server_peer *peer = handler->peer;
    const DataWatcherBlockPos *source_position = C08PacketPlayerBlockPlacement_getPosition(packet);
    if (!source_position) {
        disconnect_peer(peer, "Invalid block placement position.");
        return;
    }
    DataWatcherBlockPos position = *source_position;
    int x = position.x, y = position.y, z = position.z;
    int face = C08PacketPlayerBlockPlacement_getPlacedBlockDirection(packet);
    if (face > 5 && face != 255) {
        disconnect_peer(peer, "Invalid block placement direction.");
        return;
    }
    if (face == 255) {
        MCGameplayTransaction tx = {0};
        if (!MCGameplay_begin(&server->gameplay, &tx))
            return;
        MCObjectRootScope scope = {0};
        bool ok = MCObjectRootScope_begin(&scope, tx.working.heap);
        MCGameplayPlayer *p = ok ? mc_server_graph_player(&tx.working, peer->ownerIndex) : NULL;
        ok = p && mc_server_graph_use_item(p) &&
             mc_server_graph_detect_changes(MCGameplay_get(&tx.working));
        MCObjectRootScope_end(&scope);
        if (ok)
            commit_graph(server, peer, &tx);
        else
            MCGameplay_abort(&tx);
        return;
    }
    MCObjectRootScope read_scope = {0};
    if (!MCObjectRootScope_begin(&read_scope, server->gameplay.heap)) {
        disconnect_peer(peer, "Cannot read the canonical held item.");
        return;
    }
    ItemStack *held = InventoryPlayer_getCurrentItem(actor(peer)->inventory);
    bool has_held = held != NULL;
    uint16_t placed = 0;
    bool placeable = held && mc_item_block_state((int16_t)ItemStack_registryId(held->item),
                                                 (int16_t)held->itemDamage, &placed);
    MCObjectRootScope_end(&read_scope);
    if (world_coordinate(x, y, z) && table_reachable(peer, x, y, z) &&
        (mc_world_get(&server->world, x, y, z) >> 4) == 58 && (!peer->sneaking || !has_held)) {
        (void)open_workbench(server, peer, x, y, z);
        return;
    }
    static const int dx[6] = {0, 0, 0, 0, -1, 1};
    static const int dy[6] = {-1, 1, 0, 0, 0, 0};
    static const int dz[6] = {0, 0, -1, 1, 0, 0};
    int tx = x + dx[face], ty = y + dy[face], tz = z + dz[face];
    if (!world_coordinate(x, y, z) || !reachable(peer, x, y, z) ||
        !mc_world_get(&server->world, x, y, z) || mc_world_get(&server->world, tx, ty, tz) ||
        !has_held || !placeable) {
        correct_block(peer, &server->world, tx, ty, tz);
        return;
    }
    change_block(server, peer, tx, ty, tz, placed);
}
static void handle_play(mc_server *server, server_peer *peer, mc_buf *packet, int id) {
    if (id >= 3 && id <= 6) {
        handle_movement(server, peer, packet, id);
        return;
    }
    if (id == 0) {
        int32_t token = mc_get_varint(packet);
        if (!complete(packet) || !peer->keepalive_pending || token != peer->keepalive_id) {
            disconnect_peer(peer, "Invalid keepalive response.");
            return;
        }
        peer->keepalive_pending = false;
        peer->keepalive_at = mc_time_ms();
        return;
    }
    if (id == 1) {
        char message[401];
        if (!mc_get_string(packet, message, sizeof message) || !complete(packet) || !message[0]) {
            disconnect_peer(peer, "Invalid chat packet.");
            return;
        }
        unsigned units = 0;
        for (size_t i = 0; message[i]; ++i) {
            unsigned char byte = (unsigned char)message[i];
            /* mc_get_string has already validated UTF-8. Java's protocol limit
               is 100 UTF-16 units, rather than 100 encoded bytes. */
            if ((byte & 0xc0) != 0x80)
                units += byte >= 0xf0 ? 2u : 1u;
            if (byte < 32 || byte == 127 || units > 100) {
                disconnect_peer(peer, "Invalid chat text.");
                return;
            }
        }
        chat(server, peer->name, message);
        return;
    }
    if (id == 7) {
        MCObjectHeap *heap = MCObjectHeap_new(64u * 1024u);
        MCObjectRootScope decode_scope = {0};
        bool decoded = MCObjectRootScope_begin(&decode_scope, heap);
        PacketBuffer buffer;
        C07PacketPlayerDigging *source = decoded ? C07PacketPlayerDigging_new_empty(heap) : NULL;
        decoded = source && PacketBuffer_init(&buffer, heap, packet) &&
                  C07PacketPlayerDigging_readPacketData(source, &buffer) && complete(packet);
        int status = -1, x = 0, y = 0, z = 0;
        if (decoded) {
            const DataWatcherBlockPos *position = C07PacketPlayerDigging_getPosition(source);
            status = C07PacketPlayerDigging_getStatus(source)->ordinal;
            x = position->x;
            y = position->y;
            z = position->z;
        }
        MCObjectRootScope_end(&decode_scope);
        MCObjectHeap_free(heap);
        if (!decoded) {
            disconnect_peer(peer, "Invalid digging packet.");
            return;
        }
        if (status == 0 || status == 2)
            change_block(server, peer, x, y, z, 0);
        if (status == 3 || status == 4) {
            MCGameplayTransaction tx = {0};
            if (!MCGameplay_begin(&server->gameplay, &tx))
                return;
            MCObjectRootScope scope = {0};
            bool ok = MCObjectRootScope_begin(&scope, tx.working.heap);
            MCGameplayPlayer *p = ok ? mc_server_graph_player(&tx.working, peer->ownerIndex) : NULL;
            ok = p && mc_server_graph_drop(p, status == 3) &&
                 mc_server_graph_detect_changes(MCGameplay_get(&tx.working));
            MCObjectRootScope_end(&scope);
            if (ok) {
                if (commit_graph(server, peer, &tx))
                    send_equipment(server, peer);
            } else
                MCGameplay_abort(&tx);
        }
        return;
    }
    if (id == 8) {
        MCObjectHeap *heap = MCObjectHeap_new(4u * 1024u * 1024u);
        MCObjectRootScope scope = {0};
        bool ok = MCObjectRootScope_begin(&scope, heap);
        PacketBuffer buffer;
        C08PacketPlayerBlockPlacement *p =
            ok ? C08PacketPlayerBlockPlacement_new_empty(heap) : NULL;
        ok = p && PacketBuffer_init(&buffer, heap, packet) &&
             C08PacketPlayerBlockPlacement_readPacketData(p, &buffer) && complete(packet);
        if (ok) {
            C08ServerHandler handler = {server, peer};
            server_processPlayerBlockPlacement(&handler, p);
        } else
            disconnect_peer(peer, "Malformed source placement packet.");
        MCObjectRootScope_end(&scope);
        MCObjectHeap_free(heap);
        return;
    }
    if (id == 9) {
        MCObjectHeap *heap = MCObjectHeap_new(64u * 1024u);
        MCObjectRootScope decode_scope = {0};
        bool decoded = MCObjectRootScope_begin(&decode_scope, heap);
        PacketBuffer buffer;
        C09PacketHeldItemChange *source = decoded ? C09PacketHeldItemChange_new_empty(heap) : NULL;
        decoded = source && PacketBuffer_init(&buffer, heap, packet) &&
                  C09PacketHeldItemChange_readPacketData(source, &buffer) && complete(packet);
        int selected = decoded ? C09PacketHeldItemChange_getSlotId(source) : -1;
        MCObjectRootScope_end(&decode_scope);
        MCObjectHeap_free(heap);
        if (!decoded || selected < 0 || selected > 8) {
            disconnect_peer(peer, "Invalid hotbar selection.");
            return;
        }
        MCGameplayTransaction tx = {0};
        if (!MCGameplay_begin(&server->gameplay, &tx))
            return;
        MCObjectRootScope scope = {0};
        bool ok = MCObjectRootScope_begin(&scope, tx.working.heap);
        MCGameplayPlayer *p = ok ? mc_server_graph_player(&tx.working, peer->ownerIndex) : NULL;
        if (p)
            p->inventory->currentItem = selected;
        ok = p && mc_server_graph_detect_changes(MCGameplay_get(&tx.working));
        MCObjectRootScope_end(&scope);
        if (ok) {
            if (commit_graph(server, peer, &tx))
                send_equipment(server, peer);
        } else
            MCGameplay_abort(&tx);
        return;
    }
    if (id == 0x10 || id == 0x0d || id == 0x0e || id == 0x0f) {
        if (!table_usable(server, peer))
            invalidate_workbench(server, peer);
        if (apply_source_packet(server, peer, id, packet))
            send_equipment(server, peer);
        return;
    }

    if (id == 0x0a) {
        if (!complete(packet)) {
            disconnect_peer(peer, "Invalid animation packet.");
            return;
        }
        mc_buf animation;
        packet_start(&animation, 0x0b);
        mc_put_varint(&animation, peer->entity);
        mc_put_u8(&animation, 0);
        broadcast(server, &animation, peer);
        mc_buf_free(&animation);
        return;
    }
    if (id == 0x0b) {
        int entity = mc_get_varint(packet), action = mc_get_varint(packet),
            parameter = mc_get_varint(packet);
        if (!complete(packet) || entity != peer->entity || action < 0 || action > 6 ||
            parameter < 0)
            disconnect_peer(peer, "Invalid entity action packet.");
        else if (action == 0 || action == 1) {
            peer->sneaking = action == 0;
            MCObjectRootScope scope = {0};
            if (MCObjectRootScope_begin(&scope, server->gameplay.heap)) {
                actor(peer)->sneaking = peer->sneaking;
                MCObjectRootScope_end(&scope);
            }
        }
        return;
    }
    if (id == 0x13) {
        int flags = mc_get_u8(packet);
        float fly = mc_get_f32(packet), walk = mc_get_f32(packet);
        if (!complete(packet) || flags > 15 || !isfinite(fly) || !isfinite(walk) || fly < 0 ||
            walk < 0)
            disconnect_peer(peer, "Invalid abilities packet.");
        return;
    }
    if (id == 0x15) {
        char locale[17];
        mc_get_string(packet, locale, sizeof locale);
        int distance = mc_get_u8(packet), mode = mc_get_u8(packet);
        get_boolean(packet);
        mc_get_u8(packet);
        if (!complete(packet) || distance < 2 || distance > 32 || mode > 2)
            disconnect_peer(peer, "Invalid client settings.");
        return;
    }
    if (id == 0x16) {
        int action = mc_get_varint(packet);
        if (!complete(packet) || action < 0 || action > 2)
            disconnect_peer(peer, "Invalid client status.");
        return;
    }
    if (id == 0x17) {
        char channel[21];
        if (!mc_get_string(packet, channel, sizeof channel) || packet->len - packet->pos > 32767)
            disconnect_peer(peer, "Invalid plugin message.");
        return;
    }

    disconnect_peer(peer, "Unsupported or malformed play packet.");
}
static void handle_packet(mc_server *server, server_peer *peer, mc_buf *packet) {
    int id = mc_get_varint(packet);
    if (packet->failed) {
        disconnect_peer(peer, "Malformed packet ID.");
        return;
    }
    if (peer->state == STATE_HANDSHAKE) {
        char host[256];
        int version = mc_get_varint(packet);
        mc_get_string(packet, host, sizeof host);
        mc_get_i16(packet);
        int next = mc_get_varint(packet);
        if (id != 0 || !complete(packet) || (next != 1 && next != 2)) {
            disconnect_peer(peer, "Invalid handshake.");
            return;
        }
        peer->state = next == 1 ? STATE_STATUS : STATE_LOGIN;
        if (next == 2 && version != MC_PROTOCOL_VERSION)
            disconnect_peer(peer, "This server requires Minecraft 1.8.9 protocol 47.");
        return;
    }
    if (peer->state == STATE_STATUS) {
        mc_buf reply;
        if (id == 0 && !peer->status_requested && complete(packet)) {
            char json[400];
            snprintf(json, sizeof json,
                     "{\"version\":{\"name\":\"C919 "
                     "1.8.9\",\"protocol\":47},\"players\":{\"max\":%d,\"online\":%d},"
                     "\"description\":{\"text\":\"C919 original C creative server (offline)\"}}",
                     server->max_players, player_count(server));
            packet_start(&reply, 0);
            mc_put_string(&reply, json);
            queue_packet(peer, &reply);
            peer->status_requested = true;
        } else if (id == 1 && peer->status_requested && !peer->pinged) {
            int64_t token = mc_get_i64(packet);
            if (!complete(packet)) {
                disconnect_peer(peer, "Invalid status ping.");
                return;
            }
            packet_start(&reply, 1);
            mc_put_i64(&reply, token);
            queue_packet(peer, &reply);
            peer->pinged = true;
            peer->closing = true;
            peer->close_at = mc_time_ms();
        } else
            disconnect_peer(peer, "Invalid status request.");
        return;
    }
    if (peer->state == STATE_LOGIN) {
        if (id != 0 || !mc_get_string(packet, peer->name, sizeof peer->name) || !complete(packet) ||
            !valid_name(peer->name)) {
            disconnect_peer(peer, "Username must contain 1 to 16 letters, digits, or underscores.");
            return;
        }
        if (player_count(server) >= server->max_players) {
            disconnect_peer(peer, "Server is full.");
            return;
        }
        for (int i = 0; i < SERVER_CONNECTIONS; ++i)
            if (&server->peers[i] != peer && playing(&server->peers[i]) &&
                name_equal(peer->name, server->peers[i].name)) {
                disconnect_peer(peer, "That username is already connected.");
                return;
            }
        join_game(server, peer);
        return;
    }
    handle_play(server, peer, packet, id);
}
static void tick_peer(mc_server *server, server_peer *peer, uint64_t now) {
    if (!mc_conn_poll(&peer->conn)) {
        remove_peer(server, peer);
        return;
    }
    if (peer->closing) {
        if (!peer->conn.tx.len || now - peer->close_at > 1000)
            remove_peer(server, peer);
        return;
    }
    for (int i = 0; i < 64 && !peer->closing && !peer->conn.closed; ++i) {
        mc_buf packet;
        mc_buf_init(&packet);
        int result = mc_conn_next(&peer->conn, &packet);
        if (result == 1)
            handle_packet(server, peer, &packet);
        mc_buf_free(&packet);
        if (result == -1) {
            remove_peer(server, peer);
            return;
        }
        if (result == 0)
            break;
    }
    if (peer->conn.closed) {
        remove_peer(server, peer);
        return;
    }
    if (peer->closing)
        return;
    if (peer->state != STATE_PLAY) {
        if (now - peer->connected_at > LOGIN_TIMEOUT_MS)
            disconnect_peer(peer, "Handshake timed out.");
        return;
    }
    invalidate_workbench(server, peer);
    if (peer->closing || server->fatal)
        return;
    if (peer->conn.tx.len < 256 * 1024) {
        int cx = mc_floor_div16((int)floor(peer->x)), cz = mc_floor_div16((int)floor(peer->z));
        bool sent = false;
        for (int z = cz - 2; z <= cz + 2 && !sent; ++z)
            for (int x = cx - 2; x <= cx + 2; ++x) {
                if (x < -3 || x > 3 || z < -3 || z > 3)
                    continue;
                uint64_t bit = UINT64_C(1) << ((z + 3) * 7 + x + 3);
                if (peer->chunks_sent & bit)
                    continue;
                peer->chunks_sent |= bit;
                send_chunk(server, peer, x, z);
                sent = true;
                break;
            }
    }
    if (peer->keepalive_pending) {
        if (now - peer->keepalive_at > KEEPALIVE_TIMEOUT_MS)
            disconnect_peer(peer, "Keepalive timed out.");
    } else if (now - peer->keepalive_at > KEEPALIVE_INTERVAL_MS) {
        mc_buf packet;
        peer->keepalive_id = (int32_t)(now & 0x7fffffff);
        peer->keepalive_pending = true;
        peer->keepalive_at = now;
        packet_start(&packet, 0);
        mc_put_varint(&packet, peer->keepalive_id);
        queue_packet(peer, &packet);
    }
}
static bool parse_number(const char *text, uint64_t min, uint64_t max, uint64_t *result) {
    char *end;
    errno = 0;
    if (!text[0] || text[0] == '-' || text[0] == '+')
        return false;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno || *end || value < min || value > max)
        return false;
    *result = (uint64_t)value;
    return true;
}

static void free_world_state(mc_server *server) {
    MCGameplay_free(&server->gameplay);
    mc_world_free(&server->world);
    free(server);
}

/* World time and the handler's drop throttle are ephemeral scalar state. An
   idle tick has no stack/map/entity effects to encode or journal. Borrow the
   actual owners and advance only those scalars; source gameplay mutations
   still use the complete disposable graph and durable effect queue below. */
static int advance_idle_tick(mc_server *server) {
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, server->gameplay.heap))
        return -1;
    MCGameplayObjects *objects = MCGameplay_get(&server->gameplay);
    bool idle = objects && objects->itemCount == 0;
    for (size_t i = 0; i < MC_TRANSFER_MAX_PLAYERS && idle; i++) {
        MCGameplayPlayer *p = (MCGameplayPlayer *)objects->players[i];
        if (!p)
            continue;
        for (int n = 0; n < 40; n++) {
            ItemStack *stack = n < 36 ? p->inventory->mainInventory->items[n]
                                      : p->inventory->armorInventory->items[n - 36];
            if (stack && stack->item == ItemStack_registryItem(358)) {
                idle = false;
                break;
            }
        }
    }
    if (idle) {
        MCGameplayWorld *world = mc_server_graph_world(&server->gameplay);
        world->worldTime = world->worldTime == INT64_MAX ? INT64_MIN : world->worldTime + 1;
        for (size_t i = 0; i < MC_TRANSFER_MAX_PLAYERS; i++) {
            MCGameplayPlayer *p = (MCGameplayPlayer *)objects->players[i];
            if (p) {
                NetHandlerPlayServer *handler = (NetHandlerPlayServer *)p->handler;
                if (handler->itemDropThreshold > 0)
                    --handler->itemDropThreshold;
            }
        }
        MCObjectHeap_touch(server->gameplay.heap);
    }
    MCObjectRootScope_end(&scope);
    return objects ? (idle ? 1 : 0) : -1;
}

static void tick_items(mc_server *server, uint64_t now) {
    unsigned steps = 0;
    while (!server->fatal && now - server->last_item_tick >= 50 && steps++ < 20) {
        server->last_item_tick += 50;
        int idle = advance_idle_tick(server);
        if (idle < 0) {
            server->fatal = true;
            break;
        }
        if (idle > 0)
            continue;
        MCGameplayTransaction tx = {0};
        if (!MCGameplay_begin(&server->gameplay, &tx)) {
            server->fatal = true;
            break;
        }
        MCObjectRootScope scope = {0};
        bool ok = MCObjectRootScope_begin(&scope, tx.working.heap);
        MCGameplayObjects *o = MCGameplay_get(&tx.working);
        MCGameplayWorld *w = mc_server_graph_world(&tx.working);
        w->worldTime = w->worldTime == INT64_MAX ? INT64_MIN : w->worldTime + 1;
        for (size_t i = 0; i < MC_TRANSFER_MAX_PLAYERS && ok; i++) {
            MCGameplayPlayer *p = (MCGameplayPlayer *)o->players[i];
            if (!p)
                continue;
            NetHandlerPlayServer *h = (NetHandlerPlayServer *)p->handler;
            if (h->itemDropThreshold > 0)
                --h->itemDropThreshold;
            /* Original InventoryPlayer.decrementAnimations updates main36
               before EntityPlayerMP.onUpdateEntity reads all40 map packets. */
            for (int n = 0; n < 36 && ok; n++) {
                ItemStack *stack = p->inventory->mainInventory->items[n];
                if (!stack || stack->item != ItemStack_registryItem(358))
                    continue;
                bool changed = false;
                ok = ItemMap_onUpdate(stack, w, (MCObject *)p, n, n == p->inventory->currentItem,
                                      &changed);
            }
            for (int n = 0; n < 40 && ok; n++) {
                ItemStack *stack = n < 36 ? p->inventory->mainInventory->items[n]
                                          : p->inventory->armorInventory->items[n - 36];
                if (!stack || stack->item != ItemStack_registryItem(358))
                    continue;
                mc_buf packet;
                mc_buf_init(&packet);
                if (ok) {
                    int result = ItemMap_createMapDataPacket(stack, w, p, &packet);
                    if (result > 0)
                        ok = MCGameplayPackets_sendNative(p, &packet);
                    else if (result < 0)
                        ok = false;
                }
                mc_buf_free(&packet);
            }
        }
        for (size_t i = 0; i < o->itemCount && ok; i++) {
            EntityItem *e = (EntityItem *)o->items[i];
            if (e->isDead)
                continue;
            if (!NativeItemMotion_validate(e)) {
                ok = false;
                break;
            }
            if (!mc_world_chunk(&server->world, mc_floor_div16((int)floor(e->posX)),
                                mc_floor_div16((int)floor(e->posZ)), false))
                continue;
            double x = e->posX, y = e->posY, z = e->posZ;
            if (!NativeItemMotion_tick(e, &server->world)) {
                ok = !MCObjectHeap_failed(e->object.heap) && mc_server_graph_kill_item(e);
                continue;
            }
            if (floor(x * 32) != floor(e->posX * 32) || floor(y * 32) != floor(e->posY * 32) ||
                floor(z * 32) != floor(e->posZ * 32) || e->ticksExisted % 60 == 0)
                ok = mc_server_graph_send_motion(e);
            for (size_t n = 0; n < MC_TRANSFER_MAX_PLAYERS && ok && !e->isDead; n++) {
                MCGameplayPlayer *p = (MCGameplayPlayer *)o->players[n];
                if (!p || p->isDead)
                    continue;
                if (e->posX + e->width * 0.5 > p->posX - 1.3 &&
                    e->posX - e->width * 0.5 < p->posX + 1.3 &&
                    e->posZ + e->width * 0.5 > p->posZ - 1.3 &&
                    e->posZ - e->width * 0.5 < p->posZ + 1.3 &&
                    e->posY + e->height > p->posY - 0.5 && e->posY < p->posY + 2.3)
                    ok = EntityItem_onCollideWithPlayer(e, (MCObject *)p);
            }
        }
        for (size_t a = 0; a < o->itemCount && ok; a++) {
            EntityItem *first = (EntityItem *)o->items[a];
            if (first->isDead || first->ticksExisted % 25)
                continue;
            for (size_t b = a + 1; b < o->itemCount && ok; b++) {
                EntityItem *other = (EntityItem *)o->items[b];
                if (other->isDead)
                    continue;
                if (fabs(first->posX - other->posX) <= 0.75 &&
                    fabs(first->posY - other->posY) <= 0.25 &&
                    fabs(first->posZ - other->posZ) <= 0.75) {
                    EntityItem_combineItems(first, other);
                    ok = !MCObjectHeap_failed(tx.working.heap);
                }
            }
        }
        for (size_t i = 0; i < o->itemCount && ok; i++) {
            EntityItem *e = (EntityItem *)o->items[i];
            if (e->isDead || !DataWatcher_hasObjectChanged(e->dataWatcher))
                continue;
            S1CPacketEntityMetadata *packet =
                S1CPacketEntityMetadata_new(e->object.heap, e->entityId, e->dataWatcher, false);
            ok = packet != NULL;
            for (size_t n = 0; n < MC_TRANSFER_MAX_PLAYERS && ok; n++) {
                MCGameplayPlayer *p = (MCGameplayPlayer *)o->players[n];
                if (p && !p->isDead)
                    ok = MCGameplayPackets_sendMetadata(p, packet);
            }
        }
        if (ok)
            ok = mc_server_graph_remove_dead(o) && mc_server_graph_detect_changes(o);
        MCObjectRootScope_end(&scope);
        if (ok)
            ok = commit_graph(server, NULL, &tx);
        else
            MCGameplay_abort(&tx);
        if (!ok)
            server->fatal = true;
        for (int i = 0; i < SERVER_CONNECTIONS && !server->fatal; i++)
            if (playing(&server->peers[i]))
                sync_items(server, &server->peers[i]);
    }
}

static void usage(void) {
    puts("c919-server [--bind ADDRESS] [--port 25565] [--world saves/world.c919]"
         " [--seed 919] [--gamemode 0|1] [--max-players 16] [--run-seconds N]");
    puts("Offline protocol 47 server (default creative); use trusted local networks only.");
}
int mc_server_main(int argc, char **argv) {
    const char *bind = "127.0.0.1", *path = "saves/world.c919";
    uint64_t port = 25565, seed = 919, max_players = 16, run_seconds = 0, gamemode = 1;
    bool default_path = true;
    for (int i = 1; i < argc; ++i) {
        const char *option = argv[i];
        if (!strcmp(option, "--help")) {
            usage();
            return 0;
        }
        if (i + 1 >= argc) {
            usage();
            return 2;
        }
        const char *value = argv[++i];
        bool valid = true;
        if (!strcmp(option, "--bind"))
            bind = value;
        else if (!strcmp(option, "--world")) {
            path = value;
            default_path = false;
        } else if (!strcmp(option, "--port"))
            valid = parse_number(value, 1, 65535, &port);
        else if (!strcmp(option, "--seed"))
            valid = parse_number(value, 0, UINT32_MAX, &seed);
        else if (!strcmp(option, "--max-players"))
            valid = parse_number(value, 1, SERVER_CONNECTIONS, &max_players);
        else if (!strcmp(option, "--gamemode"))
            valid = parse_number(value, 0, 1, &gamemode);
        else if (!strcmp(option, "--run-seconds"))
            valid = parse_number(value, 1, 86400, &run_seconds);
        else
            valid = false;
        if (!valid || !value[0]) {
            fprintf(stderr, "Invalid option: %s %s\n", option, value);
            usage();
            return 2;
        }
    }
    if (default_path && mc_mkdir("saves") && errno != EEXIST) {
        perror("Could not create saves directory");
        return 1;
    }
    mc_server *server = calloc(1, sizeof *server);
    if (!server) {
        fputs("Not enough memory for server.\n", stderr);
        return 1;
    }
    server->save_path = path;
    server->max_players = (int)max_players;
    server->defaultGamemode = (int)gamemode;
    mc_world_init(&server->world, (uint32_t)seed);
    char error[256];
    struct stat info;
    if (!stat(path, &info)) {
        if (!mc_world_load(&server->world, path, error, sizeof error)) {
            fprintf(stderr, "Could not load existing world %s: %s\n", path, error);
            free_world_state(server);
            return 1;
        }
    } else if (errno == ENOENT) {
        for (int z = -3; z <= 3; ++z)
            for (int x = -3; x <= 3; ++x)
                mc_world_generate(&server->world, x, z);
        if (server->world.count != 49 ||
            !mc_world_save(&server->world, path, error, sizeof error)) {
            fprintf(stderr, "Could not create world %s: %s\n", path,
                    server->world.count != 49 ? "chunk allocation failed" : error);
            free_world_state(server);
            return 1;
        }
    } else {
        fprintf(stderr, "Could not inspect world %s: %s\n", path, strerror(errno));
        free_world_state(server);
        return 1;
    }
    server_peer spawn = {0};
    if (select_spawn(server, &spawn)) {
        server->spawn_x = (int)floor(spawn.x);
        server->spawn_z = (int)floor(spawn.z);
    }
    if (!mc_transfer_recover(path, error, sizeof error) ||
        !mc_server_graph_init(&server->gameplay, &server->world, server->spawn_x, server->spawn_z,
                              (uint64_t)seed ^ mc_time_ms())) {
        fprintf(stderr, "Source world initialization/recovery failed: %s\n", error);
        free_world_state(server);
        return 1;
    }
    MCGameplayTransaction initial = {0};
    bool loaded = MCGameplay_begin(&server->gameplay, &initial);
    MCObjectRootScope loadScope = {0};
    if (loaded)
        loaded = MCObjectRootScope_begin(&loadScope, initial.working.heap);
    MCGameplayWorld *initialWorld = loaded ? mc_server_graph_world(&initial.working) : NULL;
    const char *suffixes[] = {".items.dat", ".maps.dat"};
    for (unsigned i = 0; i < 2 && loaded; i++) {
        char file[4096];
        snprintf(file, sizeof file, "%s%s", path, suffixes[i]);
        struct stat snapshotInfo;
        mc_nbt data = {0};
        if (!stat(file, &snapshotInfo)) {
            loaded = mc_nbt_load_gzip(&data, file, error, sizeof error);
            if (loaded)
                loaded = i == 0 ? MCGameplayStorage_loadItems(initialWorld, &data,
                                                              (MCObject *)initialWorld,
                                                              mc_server_graph_item_dependencies(),
                                                              mc_server_graph_item_constructors())
                                : MCGameplayStorage_loadMaps(initialWorld, &data);
        } else
            loaded = errno == ENOENT;
        mc_nbt_free(&data);
    }
    MCObjectRootScope_end(&loadScope);
    if (!loaded || !commit_graph(server, NULL, &initial)) {
        MCGameplay_abort(&initial);
        fprintf(stderr, "Source saved graph load failed; existing data preserved.\n");
        free_world_state(server);
        return 1;
    }
    if (!mc_net_init()) {
        fputs("Network initialization failed.\n", stderr);
        free_world_state(server);
        return 1;
    }
    server->listener = mc_net_listen(bind, (uint16_t)port, error, sizeof error);
    if (server->listener == MC_INVALID_SOCKET) {
        fprintf(stderr, "Could not listen: %s\n", error);
        mc_net_shutdown();
        free_world_state(server);
        return 1;
    }
    signal(SIGINT, stop_server);
    signal(SIGTERM, stop_server);
    server_running = 1;
    uint64_t started = mc_time_ms();
    server->last_item_tick = started;
    printf("C919 offline server listening on %s:%u, world %s, seed %u, gamemode %u.\n", bind,
           (unsigned)port, path, server->world.seed, (unsigned)gamemode);
    fflush(stdout);
    while (server_running && !server->fatal) {
        uint64_t now = mc_time_ms();
        if (run_seconds && now - started >= run_seconds * 1000)
            break;
        for (int accepted = 0; accepted < 64; ++accepted) {
            mc_socket socket = mc_net_accept(server->listener);
            if (socket == MC_INVALID_SOCKET)
                break;
            server_peer *peer = NULL;
            for (int i = 0; i < SERVER_CONNECTIONS; ++i)
                if (!server->peers[i].used) {
                    peer = &server->peers[i];
                    break;
                }
            if (!peer) {
                mc_socket_close(socket);
                continue;
            }
            memset(peer, 0, sizeof *peer);
            mc_conn_init(&peer->conn, socket);
            peer->game = &server->gameplay;
            peer->ownerIndex = (size_t)(peer - server->peers);
            peer->used = true;
            peer->connected_at = now;
        }
        for (int i = 0; i < SERVER_CONNECTIONS; ++i)
            if (server->peers[i].used)
                tick_peer(server, &server->peers[i], now);
        tick_items(server, now);
        mc_sleep_ms(2);
    }
    if (!server->fatal && !mc_world_save(&server->world, path, error, sizeof error)) {
        fprintf(stderr, "Final world save failed: %s\n", error);
        server->fatal = true;
    }
    for (int i = 0; i < SERVER_CONNECTIONS; ++i) {
        server_peer *peer = &server->peers[i];
        if (peer->used) {
            disconnect_peer(peer, server->fatal ? "World persistence failed." : "Server stopped.");
            mc_conn_poll(&peer->conn);
            remove_peer(server, peer);
        }
    }
    int result = server->fatal ? 1 : 0;
    mc_socket_close(server->listener);
    mc_net_shutdown();
    free_world_state(server);
    return result;
}
int main(int argc, char **argv) {
    return mc_server_main(argc, argv);
}
