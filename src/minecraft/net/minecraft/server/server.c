#include "server.h"
#include "../network/protocol.h"
#include "../world/world.h"
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
    double x, y, z;
    float yaw, pitch;
    bool grounded;
    int selected;
    uint64_t chunks_sent;
    int16_t inventory[9];
} server_peer;
typedef struct {
    mc_world world;
    server_peer peers[SERVER_CONNECTIONS];
    mc_socket listener;
    const char *save_path;
    int max_players, next_entity;
    bool fatal;
} mc_server;

static void stop_server(int sig) { (void)sig; server_running = 0; }
static void packet_start(mc_buf *packet, int id) { mc_buf_init(packet); mc_put_varint(packet, id); }
static void queue_packet(server_peer *peer, mc_buf *packet) {
    if (peer->used && !peer->conn.closed) mc_conn_send(&peer->conn, packet);
    mc_buf_free(packet);
}
static bool playing(const server_peer *peer) {
    return peer->used && !peer->closing && !peer->conn.closed && peer->state == STATE_PLAY;
}
static int player_count(const mc_server *server) {
    int count = 0;
    for (int i = 0; i < SERVER_CONNECTIONS; ++i) if (playing(&server->peers[i])) ++count;
    return count;
}
static void broadcast(mc_server *server, const mc_buf *packet, const server_peer *except) {
    for (int i = 0; i < SERVER_CONNECTIONS; ++i) {
        server_peer *peer = &server->peers[i];
        if (peer != except && playing(peer)) mc_conn_send(&peer->conn, packet);
    }
}
static void disconnect_peer(server_peer *peer, const char *reason) {
    if (!peer->used || peer->closing || peer->conn.closed) return;
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
static void send_slot(mc_buf *packet, int16_t item) {
    mc_put_i16(packet, item);
    if (item >= 0) {
        mc_put_u8(packet, 1);
        mc_put_i16(packet, 0);
        mc_put_u8(packet, 0); /* absent NBT root is TAG_End, not legacy -1 length */
    }
}
static void inventory_slot(server_peer *peer, int index) {
    mc_buf packet;
    packet_start(&packet, 0x2f);
    mc_put_u8(&packet, 0);
    mc_put_i16(&packet, (int16_t)(36 + index));
    send_slot(&packet, peer->inventory[index]);
    queue_packet(peer, &packet);
}
static void player_info(mc_buf *packet, const server_peer *peer, bool add) {
    packet_start(packet, 0x38);
    mc_put_varint(packet, add ? 0 : 4);
    mc_put_varint(packet, 1);
    mc_put_bytes(packet, peer->uuid, 16);
    if (add) {
        mc_put_string(packet, peer->name);
        mc_put_varint(packet, 0);
        mc_put_varint(packet, 1);
        mc_put_varint(packet, 0);
        mc_put_u8(packet, 0);
    }
}
static uint8_t angle_byte(float angle) {
    float normalized = fmodf(angle, 360.0f);
    if (normalized < 0) normalized += 360.0f;
    return (uint8_t)(normalized * (256.0f / 360.0f));
}
static void spawn_player(mc_buf *packet, const server_peer *peer) {
    packet_start(packet, 0x0c);
    mc_put_varint(packet, peer->entity);
    mc_put_bytes(packet, peer->uuid, 16);
    mc_put_i32(packet, (int32_t)floor(peer->x * 32.0));
    mc_put_i32(packet, (int32_t)floor(peer->y * 32.0));
    mc_put_i32(packet, (int32_t)floor(peer->z * 32.0));
    mc_put_u8(packet, angle_byte(peer->yaw));
    mc_put_u8(packet, angle_byte(peer->pitch));
    mc_put_i16(packet, peer->inventory[peer->selected]);
    mc_put_u8(packet, 0x7f);
}
static void send_position(server_peer *peer) {
    mc_buf packet;
    packet_start(&packet, 8);
    mc_put_f64(&packet, peer->x); mc_put_f64(&packet, peer->y); mc_put_f64(&packet, peer->z);
    mc_put_f32(&packet, peer->yaw); mc_put_f32(&packet, peer->pitch);
    mc_put_u8(&packet, 0);
    queue_packet(peer, &packet);
}
static void send_equipment(mc_server *server, server_peer *peer) {
    mc_buf packet;
    packet_start(&packet, 4);
    mc_put_varint(&packet, peer->entity);
    mc_put_i16(&packet, 0);
    send_slot(&packet, peer->inventory[peer->selected]);
    broadcast(server, &packet, peer);
    mc_buf_free(&packet);
}
static void chat(mc_server *server, const char *name, const char *message) {
    mc_buf packet;
    char text[448], escaped[2800], json[2860];
    if (name) snprintf(text, sizeof text, "<%s> %s", name, message);
    else snprintf(text, sizeof text, "%s", message);
    mc_json_escape(text, escaped, sizeof escaped);
    snprintf(json, sizeof json, "{\"text\":\"%s\"}", escaped);
    packet_start(&packet, 2);
    mc_put_string(&packet, json);
    mc_put_u8(&packet, 0);
    broadcast(server, &packet, NULL);
    mc_buf_free(&packet);
}
static void remove_peer(mc_server *server, server_peer *peer) {
    bool was_player = peer->state == STATE_PLAY;
    int32_t entity = peer->entity;
    if (was_player) {
        mc_buf packet;
        player_info(&packet, peer, false);
        broadcast(server, &packet, peer);
        mc_buf_free(&packet);
        packet_start(&packet, 0x13);
        mc_put_varint(&packet, 1);
        mc_put_varint(&packet, entity);
        broadcast(server, &packet, peer);
        mc_buf_free(&packet);
    }
    mc_conn_close(&peer->conn);
    mc_buf_free(&peer->conn.rx); mc_buf_free(&peer->conn.tx);
    memset(peer, 0, sizeof *peer);
}
static bool valid_name(const char *name) {
    size_t length = strlen(name);
    if (!length || length > 16) return false;
    for (size_t i = 0; i < length; ++i) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}
static bool name_equal(const char *a, const char *b) {
    for (; *a && *b; ++a, ++b) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return false;
    }
    return *a == *b;
}
static bool complete(mc_buf *packet) { return !packet->failed && packet->pos == packet->len; }
static bool get_boolean(mc_buf *packet) {
    uint8_t value = mc_get_u8(packet);
    if (value > 1) packet->failed = true;
    return value != 0;
}
static bool select_spawn(mc_server *server, server_peer *peer) {
    for (int radius = 0; radius <= 64; ++radius) {
        for (int dz = -radius; dz <= radius; ++dz) for (int dx = -radius; dx <= radius; ++dx) {
            if (radius && abs(dx) != radius && abs(dz) != radius) continue;
            int x = 8 + dx, z = 8 + dz;
            if (x < WORLD_MIN || x >= WORLD_MAX || z < WORLD_MIN || z >= WORLD_MAX) continue;
            if (!mc_world_chunk(&server->world, mc_floor_div16(x), mc_floor_div16(z), false)) continue;
            int surface = mc_world_surface(&server->world, x, z);
            if (surface > 252) continue;
            peer->x = x + 0.5; peer->y = surface + 2.0; peer->z = z + 0.5;
            return true;
        }
    }
    return false;
}
static void join_game(mc_server *server, server_peer *peer) {
    mc_buf packet;
    char uuid[37];
    if (!select_spawn(server, peer)) { disconnect_peer(peer, "No clear spawn with player headroom is available."); return; }
    peer->entity = server->next_entity++;
    peer->keepalive_at = mc_time_ms();
    for (int i = 0; i < 9; ++i) peer->inventory[i] = creative_palette[i];
    mc_offline_uuid(peer->name, peer->uuid);
    mc_uuid_string(peer->uuid, uuid);
    packet_start(&packet, 2);
    mc_put_string(&packet, uuid); mc_put_string(&packet, peer->name);
    queue_packet(peer, &packet);
    peer->state = STATE_PLAY;
    packet_start(&packet, 1);
    mc_put_i32(&packet, peer->entity);
    mc_put_u8(&packet, 1); mc_put_u8(&packet, 0); mc_put_u8(&packet, 0);
    mc_put_u8(&packet, (uint8_t)server->max_players);
    mc_put_string(&packet, "flat"); mc_put_u8(&packet, 0);
    queue_packet(peer, &packet);
    packet_start(&packet, 0x39);
    mc_put_u8(&packet, 0x0f); mc_put_f32(&packet, 0.05f); mc_put_f32(&packet, 0.1f);
    queue_packet(peer, &packet);
    packet_start(&packet, 5);
    mc_put_position(&packet, (int)floor(peer->x), (int)peer->y, (int)floor(peer->z));
    queue_packet(peer, &packet);
    packet_start(&packet, 0x30);
    mc_put_u8(&packet, 0); mc_put_i16(&packet, 45);
    for (int i = 0; i < 45; ++i) send_slot(&packet, i >= 36 ? peer->inventory[i - 36] : -1);
    queue_packet(peer, &packet);
    packet_start(&packet, 9); mc_put_u8(&packet, 0); queue_packet(peer, &packet);
    send_position(peer);
    for (int i = 0; i < SERVER_CONNECTIONS; ++i) {
        server_peer *other = &server->peers[i];
        if (!playing(other)) continue;
        player_info(&packet, other, true); queue_packet(peer, &packet);
        if (other != peer) {
            spawn_player(&packet, other); queue_packet(peer, &packet);
            player_info(&packet, peer, true); queue_packet(other, &packet);
            spawn_player(&packet, peer); queue_packet(other, &packet);
        }
    }
    printf("Player %s joined (entity %d).\n", peer->name, peer->entity);
    fflush(stdout);
}
static void send_chunk(mc_server *server, server_peer *peer, int cx, int cz) {
    const mc_chunk *chunk = mc_world_chunk(&server->world, cx, cz, false);
    mc_buf packet, data;
    uint16_t mask = 0;
    if (!chunk) { disconnect_peer(peer, "World chunk is unavailable."); return; }
    for (int section = 0; section < 16; ++section) {
        for (int i = 0; i < 4096; ++i) {
            if (chunk->blocks[section * 4096 + i]) { mask |= (uint16_t)(1u << section); break; }
        }
    }
    mc_buf_init(&data);
    for (int section = 0; section < 16; ++section) if (mask & (1u << section)) {
        for (int i = 0; i < 4096; ++i) {
            uint16_t state = chunk->blocks[section * 4096 + i];
            mc_put_u8(&data, (uint8_t)state); mc_put_u8(&data, (uint8_t)(state >> 8));
        }
    }
    /* Protocol 47 groups data by channel, not by section. */
    for (int section = 0; section < 16; ++section) if (mask & (1u << section))
        for (int i = 0; i < 2048; ++i) mc_put_u8(&data, 0);
    for (int section = 0; section < 16; ++section) if (mask & (1u << section))
        for (int i = 0; i < 2048; ++i) mc_put_u8(&data, 0xff);
    for (int i = 0; i < 256; ++i) mc_put_u8(&data, 1);
    packet_start(&packet, 0x21);
    mc_put_i32(&packet, cx); mc_put_i32(&packet, cz);
    mc_put_u8(&packet, 1); mc_put_i16(&packet, (int16_t)mask);
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
static void block_packet(mc_buf *packet, const mc_world *world, int x, int y, int z) {
    packet_start(packet, 0x23);
    mc_put_position(packet, x, y, z);
    mc_put_varint(packet, mc_world_get(world, x, y, z));
}
static void correct_block(server_peer *peer, const mc_world *world, int x, int y, int z) {
    mc_buf packet;
    block_packet(&packet, world, x, y, z); queue_packet(peer, &packet);
}
static void change_block(mc_server *server, server_peer *peer, int x, int y, int z, uint16_t state) {
    uint16_t previous = mc_world_get(&server->world, x, y, z);
    if (!world_coordinate(x, y, z) || !reachable(peer, x, y, z) || (previous >> 4) == 7) {
        correct_block(peer, &server->world, x, y, z); return;
    }
    if (previous == state) { correct_block(peer, &server->world, x, y, z); return; }
    if (!mc_world_set(&server->world, x, y, z, state)) {
        disconnect_peer(peer, "World edit could not be applied."); return;
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
    broadcast(server, &packet, NULL); mc_buf_free(&packet);
}
/* Accept the exact NBT-free Slot format supported by this palette. */
static bool read_slot(mc_buf *packet, int16_t *item) {
    *item = mc_get_i16(packet);
    if (*item == -1) return !packet->failed;
    int count = mc_get_u8(packet), damage = mc_get_i16(packet), nbt = mc_get_u8(packet);
    if (*item < 0 || count < 1 || count > 64 || damage != 0 || nbt != 0) packet->failed = true;
    return !packet->failed;
}
static bool palette_item(int16_t item) {
    if (item == -1) return true;
    for (int i = 0; i < 9; ++i) if (creative_palette[i] == item) return true;
    return false;
}
static void handle_movement(mc_server *server, server_peer *peer, mc_buf *packet, int id) {
    double x = peer->x, y = peer->y, z = peer->z;
    float yaw = peer->yaw, pitch = peer->pitch;
    if (id == 4 || id == 6) { x = mc_get_f64(packet); y = mc_get_f64(packet); z = mc_get_f64(packet); }
    if (id == 5 || id == 6) { yaw = mc_get_f32(packet); pitch = mc_get_f32(packet); }
    bool grounded = get_boolean(packet);
    if (!complete(packet) || !isfinite(x) || !isfinite(y) || !isfinite(z) ||
        !isfinite(yaw) || !isfinite(pitch) || pitch < -90 || pitch > 90) {
        disconnect_peer(peer, "Invalid movement packet."); return;
    }
    if (x < WORLD_MIN || x >= WORLD_MAX || z < WORLD_MIN || z >= WORLD_MAX || y < 1 || y >= 256) {
        send_position(peer); return;
    }
    peer->x = x; peer->y = y; peer->z = z; peer->yaw = yaw; peer->pitch = pitch; peer->grounded = grounded;
    mc_buf update;
    packet_start(&update, 0x18); mc_put_varint(&update, peer->entity);
    mc_put_i32(&update, (int32_t)floor(x * 32)); mc_put_i32(&update, (int32_t)floor(y * 32));
    mc_put_i32(&update, (int32_t)floor(z * 32));
    mc_put_u8(&update, angle_byte(yaw)); mc_put_u8(&update, angle_byte(pitch)); mc_put_u8(&update, grounded);
    broadcast(server, &update, peer); mc_buf_free(&update);
}
static void handle_play(mc_server *server, server_peer *peer, mc_buf *packet, int id) {
    if (id >= 3 && id <= 6) { handle_movement(server, peer, packet, id); return; }
    if (id == 0) {
        int32_t token = mc_get_varint(packet);
        if (!complete(packet) || !peer->keepalive_pending || token != peer->keepalive_id) {
            disconnect_peer(peer, "Invalid keepalive response."); return;
        }
        peer->keepalive_pending = false; peer->keepalive_at = mc_time_ms(); return;
    }
    if (id == 1) {
        char message[401];
        if (!mc_get_string(packet, message, sizeof message) || !complete(packet) || !message[0]) {
            disconnect_peer(peer, "Invalid chat packet."); return;
        }
        unsigned units = 0;
        for (size_t i = 0; message[i]; ++i) {
            unsigned char byte = (unsigned char)message[i];
            /* mc_get_string has already validated UTF-8. Java's protocol limit
               is 100 UTF-16 units, rather than 100 encoded bytes. */
            if ((byte & 0xc0) != 0x80) units += byte >= 0xf0 ? 2u : 1u;
            if (byte < 32 || byte == 127 || units > 100) {
                disconnect_peer(peer, "Invalid chat text."); return;
            }
        }
        chat(server, peer->name, message); return;
    }
    if (id == 7) {
        int status = mc_get_varint(packet), x, y, z;
        mc_get_position(packet, &x, &y, &z); uint8_t face = mc_get_u8(packet);
        if (!complete(packet) || status < 0 || status > 5 || face > 5) {
            disconnect_peer(peer, "Invalid digging packet."); return;
        }
        if (status == 0 || status == 2) change_block(server, peer, x, y, z, 0);
        return;
    }
    if (id == 8) {
        int x, y, z; int16_t claimed_item;
        mc_get_position(packet, &x, &y, &z); uint8_t face = mc_get_u8(packet);
        read_slot(packet, &claimed_item);
        uint8_t cursor_x = mc_get_u8(packet), cursor_y = mc_get_u8(packet), cursor_z = mc_get_u8(packet);
        if (!complete(packet) || cursor_x > 16 || cursor_y > 16 || cursor_z > 16 ||
            (face > 5 && face != 255)) { disconnect_peer(peer, "Invalid block placement packet."); return; }
        if (face == 255) return; /* right click in air */
        static const int dx[6] = {0, 0, 0, 0, -1, 1};
        static const int dy[6] = {-1, 1, 0, 0, 0, 0};
        static const int dz[6] = {0, 0, -1, 1, 0, 0};
        int tx = x + dx[face], ty = y + dy[face], tz = z + dz[face];
        int16_t held = peer->inventory[peer->selected];
        if (!world_coordinate(x, y, z) || !reachable(peer, x, y, z) || !mc_world_get(&server->world, x, y, z) ||
            mc_world_get(&server->world, tx, ty, tz) || held < 0 || claimed_item != held) {
            correct_block(peer, &server->world, tx, ty, tz); return;
        }
        change_block(server, peer, tx, ty, tz, (uint16_t)(held << 4)); return;
    }
    if (id == 9) {
        int selected = mc_get_i16(packet);
        if (!complete(packet) || selected < 0 || selected > 8) { disconnect_peer(peer, "Invalid hotbar selection."); return; }
        peer->selected = selected; send_equipment(server, peer); return;
    }
    if (id == 0x10) {
        int index = mc_get_i16(packet); int16_t item;
        read_slot(packet, &item);
        if (!complete(packet) || index < 36 || index > 44 || !palette_item(item)) {
            disconnect_peer(peer, "Invalid creative hotbar item."); return;
        }
        peer->inventory[index - 36] = item; inventory_slot(peer, index - 36);
        if (index - 36 == peer->selected) send_equipment(server, peer);
        return;
    }
    if (id == 0x0a) {
        if (!complete(packet)) { disconnect_peer(peer, "Invalid animation packet."); return; }
        mc_buf animation; packet_start(&animation, 0x0b); mc_put_varint(&animation, peer->entity); mc_put_u8(&animation, 0);
        broadcast(server, &animation, peer); mc_buf_free(&animation); return;
    }
    if (id == 0x0b) {
        int entity = mc_get_varint(packet), action = mc_get_varint(packet), parameter = mc_get_varint(packet);
        if (!complete(packet) || entity != peer->entity || action < 0 || action > 6 || parameter < 0)
            disconnect_peer(peer, "Invalid entity action packet.");
        return;
    }
    if (id == 0x13) {
        int flags = mc_get_u8(packet); float fly = mc_get_f32(packet), walk = mc_get_f32(packet);
        if (!complete(packet) || flags > 15 || !isfinite(fly) || !isfinite(walk) || fly < 0 || walk < 0)
            disconnect_peer(peer, "Invalid abilities packet.");
        return;
    }
    if (id == 0x15) {
        char locale[17]; mc_get_string(packet, locale, sizeof locale);
        int distance = mc_get_u8(packet), mode = mc_get_u8(packet); get_boolean(packet); mc_get_u8(packet);
        if (!complete(packet) || distance < 2 || distance > 32 || mode > 2) disconnect_peer(peer, "Invalid client settings.");
        return;
    }
    if (id == 0x16) {
        int action = mc_get_varint(packet);
        if (!complete(packet) || action < 0 || action > 2) disconnect_peer(peer, "Invalid client status.");
        return;
    }
    if (id == 0x17) {
        char channel[21];
        if (!mc_get_string(packet, channel, sizeof channel) || packet->len - packet->pos > 32767)
            disconnect_peer(peer, "Invalid plugin message.");
        return;
    }
    if (id == 0x0d) {
        uint8_t window = mc_get_u8(packet);
        if (!complete(packet) || window != 0) disconnect_peer(peer, "Invalid inventory window.");
        return;
    }
    disconnect_peer(peer, "Unsupported or malformed play packet.");
}
static void handle_packet(mc_server *server, server_peer *peer, mc_buf *packet) {
    int id = mc_get_varint(packet);
    if (packet->failed) { disconnect_peer(peer, "Malformed packet ID."); return; }
    if (peer->state == STATE_HANDSHAKE) {
        char host[256];
        int version = mc_get_varint(packet); mc_get_string(packet, host, sizeof host); mc_get_i16(packet);
        int next = mc_get_varint(packet);
        if (id != 0 || !complete(packet) || (next != 1 && next != 2)) {
            disconnect_peer(peer, "Invalid handshake."); return;
        }
        peer->state = next == 1 ? STATE_STATUS : STATE_LOGIN;
        if (next == 2 && version != MC_PROTOCOL_VERSION) disconnect_peer(peer, "This server requires Minecraft 1.8.9 protocol 47.");
        return;
    }
    if (peer->state == STATE_STATUS) {
        mc_buf reply;
        if (id == 0 && !peer->status_requested && complete(packet)) {
            char json[400];
            snprintf(json, sizeof json, "{\"version\":{\"name\":\"C919 1.8.9\",\"protocol\":47},\"players\":{\"max\":%d,\"online\":%d},\"description\":{\"text\":\"C919 original C creative server (offline)\"}}", server->max_players, player_count(server));
            packet_start(&reply, 0); mc_put_string(&reply, json); queue_packet(peer, &reply);
            peer->status_requested = true;
        } else if (id == 1 && peer->status_requested && !peer->pinged) {
            int64_t token = mc_get_i64(packet);
            if (!complete(packet)) { disconnect_peer(peer, "Invalid status ping."); return; }
            packet_start(&reply, 1); mc_put_i64(&reply, token); queue_packet(peer, &reply);
            peer->pinged = true; peer->closing = true; peer->close_at = mc_time_ms();
        } else disconnect_peer(peer, "Invalid status request.");
        return;
    }
    if (peer->state == STATE_LOGIN) {
        if (id != 0 || !mc_get_string(packet, peer->name, sizeof peer->name) || !complete(packet) || !valid_name(peer->name)) {
            disconnect_peer(peer, "Username must contain 1 to 16 letters, digits, or underscores."); return;
        }
        if (player_count(server) >= server->max_players) { disconnect_peer(peer, "Server is full."); return; }
        for (int i = 0; i < SERVER_CONNECTIONS; ++i) if (&server->peers[i] != peer && playing(&server->peers[i]) && name_equal(peer->name, server->peers[i].name)) {
            disconnect_peer(peer, "That username is already connected."); return;
        }
        join_game(server, peer); return;
    }
    handle_play(server, peer, packet, id);
}
static void tick_peer(mc_server *server, server_peer *peer, uint64_t now) {
    if (!mc_conn_poll(&peer->conn)) { remove_peer(server, peer); return; }
    if (peer->closing) {
        if (!peer->conn.tx.len || now - peer->close_at > 1000) remove_peer(server, peer);
        return;
    }
    for (int i = 0; i < 64 && !peer->closing && !peer->conn.closed; ++i) {
        mc_buf packet; mc_buf_init(&packet);
        int result = mc_conn_next(&peer->conn, &packet);
        if (result == 1) handle_packet(server, peer, &packet);
        mc_buf_free(&packet);
        if (result == -1) { remove_peer(server, peer); return; }
        if (result == 0) break;
    }
    if (peer->conn.closed) { remove_peer(server, peer); return; }
    if (peer->closing) return;
    if (peer->state != STATE_PLAY) {
        if (now - peer->connected_at > LOGIN_TIMEOUT_MS) disconnect_peer(peer, "Handshake timed out.");
        return;
    }
    if (peer->conn.tx.len < 256 * 1024) {
        int cx = mc_floor_div16((int)floor(peer->x)), cz = mc_floor_div16((int)floor(peer->z));
        bool sent = false;
        for (int z = cz - 2; z <= cz + 2 && !sent; ++z) for (int x = cx - 2; x <= cx + 2; ++x) {
            if (x < -3 || x > 3 || z < -3 || z > 3) continue;
            uint64_t bit = UINT64_C(1) << ((z + 3) * 7 + x + 3);
            if (peer->chunks_sent & bit) continue;
            peer->chunks_sent |= bit;
            send_chunk(server, peer, x, z);
            sent = true; break;
        }
    }
    if (peer->keepalive_pending) {
        if (now - peer->keepalive_at > KEEPALIVE_TIMEOUT_MS) disconnect_peer(peer, "Keepalive timed out.");
    } else if (now - peer->keepalive_at > KEEPALIVE_INTERVAL_MS) {
        mc_buf packet;
        peer->keepalive_id = (int32_t)(now & 0x7fffffff);
        peer->keepalive_pending = true; peer->keepalive_at = now;
        packet_start(&packet, 0); mc_put_varint(&packet, peer->keepalive_id); queue_packet(peer, &packet);
    }
}
static bool parse_number(const char *text, uint64_t min, uint64_t max, uint64_t *result) {
    char *end; errno = 0;
    if (!text[0] || text[0] == '-' || text[0] == '+') return false;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno || *end || value < min || value > max) return false;
    *result = (uint64_t)value; return true;
}
static void usage(void) {
    puts("c919-server [--bind ADDRESS] [--port 25565] [--world saves/world.c919]"
         " [--seed 919] [--max-players 16] [--run-seconds N]");
    puts("Offline creative protocol 47 server; use trusted local networks only.");
}
int mc_server_main(int argc, char **argv) {
    const char *bind = "127.0.0.1", *path = "saves/world.c919";
    uint64_t port = 25565, seed = 919, max_players = 16, run_seconds = 0;
    bool default_path = true;
    for (int i = 1; i < argc; ++i) {
        const char *option = argv[i];
        if (!strcmp(option, "--help")) { usage(); return 0; }
        if (i + 1 >= argc) { usage(); return 2; }
        const char *value = argv[++i]; bool valid = true;
        if (!strcmp(option, "--bind")) bind = value;
        else if (!strcmp(option, "--world")) { path = value; default_path = false; }
        else if (!strcmp(option, "--port")) valid = parse_number(value, 1, 65535, &port);
        else if (!strcmp(option, "--seed")) valid = parse_number(value, 0, UINT32_MAX, &seed);
        else if (!strcmp(option, "--max-players")) valid = parse_number(value, 1, SERVER_CONNECTIONS, &max_players);
        else if (!strcmp(option, "--run-seconds")) valid = parse_number(value, 1, 86400, &run_seconds);
        else valid = false;
        if (!valid || !value[0]) { fprintf(stderr, "Invalid option: %s %s\n", option, value); usage(); return 2; }
    }
    if (default_path && mc_mkdir("saves") && errno != EEXIST) { perror("Could not create saves directory"); return 1; }
    mc_server *server = calloc(1, sizeof *server);
    if (!server) { fputs("Not enough memory for server.\n", stderr); return 1; }
    server->save_path = path; server->max_players = (int)max_players; server->next_entity = 1;
    server->listener = MC_INVALID_SOCKET;
    mc_world_init(&server->world, (uint32_t)seed);
    char error[256]; struct stat info;
    if (!stat(path, &info)) {
        if (!mc_world_load(&server->world, path, error, sizeof error)) {
            fprintf(stderr, "Could not load existing world %s: %s\n", path, error);
            mc_world_free(&server->world); free(server); return 1;
        }
    } else if (errno == ENOENT) {
        for (int z = -3; z <= 3; ++z) for (int x = -3; x <= 3; ++x) mc_world_generate(&server->world, x, z);
        if (server->world.count != 49 || !mc_world_save(&server->world, path, error, sizeof error)) {
            fprintf(stderr, "Could not create world %s: %s\n", path, server->world.count != 49 ? "chunk allocation failed" : error);
            mc_world_free(&server->world); free(server); return 1;
        }
    } else {
        fprintf(stderr, "Could not inspect world %s: %s\n", path, strerror(errno));
        mc_world_free(&server->world); free(server); return 1;
    }
    if (!mc_net_init()) { fputs("Network initialization failed.\n", stderr); mc_world_free(&server->world); free(server); return 1; }
    server->listener = mc_net_listen(bind, (uint16_t)port, error, sizeof error);
    if (server->listener == MC_INVALID_SOCKET) {
        fprintf(stderr, "Could not listen: %s\n", error);
        mc_net_shutdown(); mc_world_free(&server->world); free(server); return 1;
    }
    signal(SIGINT, stop_server); signal(SIGTERM, stop_server);
    server_running = 1;
    uint64_t started = mc_time_ms();
    printf("C919 offline creative server listening on %s:%u, world %s, seed %u.\n", bind, (unsigned)port, path, server->world.seed);
    fflush(stdout);
    while (server_running && !server->fatal) {
        uint64_t now = mc_time_ms();
        if (run_seconds && now - started >= run_seconds * 1000) break;
        for (int accepted = 0; accepted < 64; ++accepted) {
            mc_socket socket = mc_net_accept(server->listener);
            if (socket == MC_INVALID_SOCKET) break;
            server_peer *peer = NULL;
            for (int i = 0; i < SERVER_CONNECTIONS; ++i) if (!server->peers[i].used) { peer = &server->peers[i]; break; }
            if (!peer) { mc_socket_close(socket); continue; }
            memset(peer, 0, sizeof *peer);
            mc_conn_init(&peer->conn, socket);
            peer->used = true; peer->connected_at = now;
        }
        for (int i = 0; i < SERVER_CONNECTIONS; ++i) if (server->peers[i].used) tick_peer(server, &server->peers[i], now);
        mc_sleep_ms(2);
    }
    if (!server->fatal && !mc_world_save(&server->world, path, error, sizeof error)) {
        fprintf(stderr, "Final world save failed: %s\n", error); server->fatal = true;
    }
    for (int i = 0; i < SERVER_CONNECTIONS; ++i) {
        server_peer *peer = &server->peers[i];
        if (peer->used) { disconnect_peer(peer, server->fatal ? "World persistence failed." : "Server stopped."); mc_conn_poll(&peer->conn); remove_peer(server, peer); }
    }
    int result = server->fatal ? 1 : 0;
    mc_socket_close(server->listener); mc_net_shutdown(); mc_world_free(&server->world); free(server);
    return result;
}
int main(int argc, char **argv) { return mc_server_main(argc, argv); }
