#include "client.h"
#include "renderer.h"
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int mc_client_hotbar[9] = {1, 2, 3, 4, 5, 20, 12, 17, 45};
const char *const mc_client_material_names[9] = {
    "Stone", "Grass", "Dirt", "Cobblestone", "Planks", "Glass", "Sand", "Timber", "Brick"
};

static void client_error(mc_client *c, const char *message) {
    if (!c->failed) {
        snprintf(c->status, sizeof(c->status), "%s", message);
        fprintf(stderr, "Client error: %s\n", c->status);
    }
    c->failed = true;
}

static bool send_packet(mc_client *c, mc_buf *packet) {
    bool ok = !packet->failed && mc_conn_send(&c->connection, packet);
    if (!ok) client_error(c, c->connection.error[0] ? c->connection.error : "Unable to send packet");
    mc_buf_free(packet);
    return ok;
}

static void start_packet(mc_buf *packet, int id) {
    mc_buf_init(packet);
    mc_put_varint(packet, id);
}

static void skip_string(mc_buf *b) {
    int32_t length = mc_get_varint(b);
    if (length < 0 || length > 32767 * 4 || (size_t)length > b->len - b->pos) b->failed = true;
    else b->pos += (size_t)length;
}

/* The HUD uses the text fields of protocol chat components, including extra[]. */
static void plain_chat(const char *json, char *output, size_t capacity) {
    size_t used = 0;
    const char *cursor = json;
    bool found = false;
    while ((cursor = strstr(cursor, "\"text\"")) != NULL) {
        cursor += 6;
        while (*cursor && *cursor != ':') ++cursor;
        if (*cursor) ++cursor;
        while (isspace((unsigned char)*cursor)) ++cursor;
        if (*cursor != '"') continue;
        ++cursor;
        found = true;
        while (*cursor && *cursor != '"') {
            unsigned char ch = (unsigned char)*cursor++;
            if (ch == '\\' && *cursor) {
                ch = (unsigned char)*cursor++;
                if (ch == 'n' || ch == 'r' || ch == 't') ch = ' ';
                else if (ch == 'u') {
                    unsigned value = 0;
                    bool valid = true;
                    for (int i = 0; i < 4; ++i) {
                        char digit = cursor[i];
                        if (!digit || !isxdigit((unsigned char)digit)) { valid = false; break; }
                        value = value * 16u + (unsigned)(isdigit((unsigned char)digit) ? digit - '0' : tolower((unsigned char)digit) - 'a' + 10);
                    }
                    if (valid) {
                        cursor += 4;
                        if (value < 128) ch = (unsigned char)value;
                        else {
                            if (value >= 0xd800 && value <= 0xdbff && cursor[0] == '\\' && cursor[1] == 'u') {
                                unsigned low = 0; bool paired = true;
                                for (int i = 0; i < 4; ++i) {
                                    char digit = cursor[2 + i];
                                    if (!digit || !isxdigit((unsigned char)digit)) { paired = false; break; }
                                    low = low * 16u + (unsigned)(isdigit((unsigned char)digit) ? digit - '0' : tolower((unsigned char)digit) - 'a' + 10);
                                }
                                if (paired && low >= 0xdc00 && low <= 0xdfff) { value = 0x10000u + ((value - 0xd800u) << 10) + low - 0xdc00u; cursor += 6; }
                            }
                            if (value >= 0xd800 && value <= 0xdfff) value = '?';
                            unsigned char encoded[4];
                            int count;
                            if (value < 128) { encoded[0] = (unsigned char)value; count = 1; }
                            else if (value < 2048) { encoded[0] = (unsigned char)(0xc0u | (value >> 6)); encoded[1] = (unsigned char)(0x80u | (value & 63u)); count = 2; }
                            else if (value < 65536) { encoded[0] = (unsigned char)(0xe0u | (value >> 12)); encoded[1] = (unsigned char)(0x80u | ((value >> 6) & 63u)); encoded[2] = (unsigned char)(0x80u | (value & 63u)); count = 3; }
                            else { encoded[0] = (unsigned char)(0xf0u | (value >> 18)); encoded[1] = (unsigned char)(0x80u | ((value >> 12) & 63u)); encoded[2] = (unsigned char)(0x80u | ((value >> 6) & 63u)); encoded[3] = (unsigned char)(0x80u | (value & 63u)); count = 4; }
                            if (used + (size_t)count < capacity) for (int i = 0; i < count; ++i) output[used++] = (char)encoded[i];
                            continue;
                        }
                    } else ch = '?';
                }
            }
            if (ch >= 128) {
                size_t width = ch < 0xe0 ? 2u : ch < 0xf0 ? 3u : 4u;
                if (strlen(cursor) < width - 1) break;
                if (used + width < capacity) { output[used++] = (char)ch; memcpy(output + used, cursor, width - 1); used += width - 1; }
                cursor += width - 1;
            } else if (ch >= 32 && used + 1 < capacity) output[used++] = (char)ch;
        }
        if (*cursor == '"') ++cursor;
    }
    if (!found) {
        cursor = json;
        if (*cursor == '"') ++cursor;
        while (*cursor && used + 1 < capacity) {
            unsigned char ch = (unsigned char)*cursor++;
            if (ch >= 32 && ch != '"') output[used++] = (char)ch;
        }
    }
    output[used] = '\0';
}

static bool read_chat(mc_buf *b, char *output, size_t capacity) {
    size_t start = b->pos;
    int32_t length = mc_get_varint(b);
    if (b->failed || length < 0 || length > 131068 || (size_t)length > b->len - b->pos) { b->failed = true; return false; }
    char *json = malloc((size_t)length + 1);
    if (!json) { b->failed = true; return false; }
    b->pos = start;
    if (!mc_get_string(b, json, (size_t)length + 1)) { free(json); return false; }
    plain_chat(json, output, capacity);
    free(json);
    return true;
}

static void add_chat(mc_client *c, const char *text) {
    for (int i = 1; i < MC_CLIENT_CHAT_LINES; ++i) memcpy(c->chat[i - 1], c->chat[i], sizeof(c->chat[i]));
    snprintf(c->chat[MC_CLIENT_CHAT_LINES - 1], sizeof(c->chat[0]), "%s", text);
    if (c->chat_count < MC_CLIENT_CHAT_LINES) ++c->chat_count;
    printf("CHAT %s\n", text);
    fflush(stdout);
}

static mc_player_info *player_info(mc_client *c, const uint8_t uuid[16], bool create) {
    mc_player_info *empty = NULL;
    for (int i = 0; i < MC_CLIENT_PLAYERS; ++i) {
        if (c->player_info[i].used && memcmp(c->player_info[i].uuid, uuid, 16) == 0) return &c->player_info[i];
        if (!c->player_info[i].used && !empty) empty = &c->player_info[i];
    }
    if (create && empty) { empty->used = true; memcpy(empty->uuid, uuid, 16); return empty; }
    return NULL;
}

static mc_remote_player *remote_player(mc_client *c, int id, bool create) {
    mc_remote_player *empty = NULL;
    for (int i = 0; i < MC_CLIENT_PLAYERS; ++i) {
        if (c->players[i].active && c->players[i].id == id) return &c->players[i];
        if (!c->players[i].active && !empty) empty = &c->players[i];
    }
    if (create && empty) { memset(empty, 0, sizeof(*empty)); empty->active = true; empty->id = id; return empty; }
    return NULL;
}

static mc_chunk *client_chunk(mc_client *c, int x, int z) {
    mc_chunk *chunk = mc_world_chunk(&c->world, x, z, false);
    if (chunk) return chunk;
    if (c->world.count == MC_MAX_CHUNKS) {
        int farthest = 0;
        double max_distance = -1;
        for (int i = 0; i < c->world.count; ++i) {
            double dx = c->world.chunks[i].x * 16.0 + 8 - c->x;
            double dz = c->world.chunks[i].z * 16.0 + 8 - c->z;
            double distance = dx * dx + dz * dz;
            if (distance > max_distance) { farthest = i; max_distance = distance; }
        }
        mc_world_unload(&c->world, c->world.chunks[farthest].x, c->world.chunks[farthest].z);
    }
    return mc_world_chunk(&c->world, x, z, true);
}

static unsigned section_count(uint16_t mask) {
    unsigned result = 0;
    for (; mask; mask >>= 1) result += mask & 1u;
    return result;
}

static size_t chunk_size(uint16_t mask, bool skylight, bool full) {
    return section_count(mask) * (8192u + 2048u + (skylight ? 2048u : 0u)) + (full ? 256u : 0u);
}

static bool receive_chunk(mc_client *c, int x, int z, uint16_t mask, bool full,
                          bool skylight, const uint8_t *data, size_t length) {
    if (x < -1875000 || x >= 1875000 || z < -1875000 || z >= 1875000) return false;
    if (full && mask == 0) {
        if (length != 0 && length != 256) return false;
        mc_world_unload(&c->world, x, z); return true;
    }
    if (length != chunk_size(mask, skylight, full)) return false;
    mc_chunk *chunk = client_chunk(c, x, z);
    if (!chunk) return false;
    if (full) memset(chunk->blocks, 0, MC_CHUNK_BLOCKS * sizeof(*chunk->blocks));
    size_t offset = 0;
    for (unsigned section = 0; section < 16; ++section) {
        if (!(mask & (1u << section))) continue;
        for (unsigned index = 0; index < 4096; ++index) {
            chunk->blocks[section * 4096u + index] = (uint16_t)(data[offset] | ((uint16_t)data[offset + 1] << 8));
            offset += 2;
        }
    }
    ++chunk->revision;
    ++c->chunks_received;
    return true;
}

static void send_movement(mc_client *c) {
    mc_buf packet;
    start_packet(&packet, 0x06);
    mc_put_f64(&packet, c->x); mc_put_f64(&packet, c->y); mc_put_f64(&packet, c->z);
    mc_put_f32(&packet, c->yaw); mc_put_f32(&packet, c->pitch);
    mc_put_u8(&packet, c->on_ground ? 1 : 0);
    send_packet(c, &packet);
    c->last_move_ms = mc_time_ms();
}

static bool correct_position(mc_client *c, double x, double y, double z, float yaw, float pitch, uint8_t flags) {
    if ((flags & ~31u) || !isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(yaw) || !isfinite(pitch)) return false;
    x += (flags & 1) ? c->x : 0; y += (flags & 2) ? c->y : 0; z += (flags & 4) ? c->z : 0;
    double final_yaw = (double)yaw + ((flags & 8) ? c->yaw : 0);
    double final_pitch = (double)pitch + ((flags & 16) ? c->pitch : 0);
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(final_yaw) || !isfinite(final_pitch) ||
        x < -30000000 || x >= 30000000 || z < -30000000 || z >= 30000000 ||
        y < -1000000 || y > 1000000 || final_pitch < -90 || final_pitch > 90) return false;
    c->x = x; c->y = y; c->z = z; c->yaw = (float)fmod(final_yaw, 360.0); c->pitch = (float)final_pitch;
    c->positioned = true; c->velocity_y = 0; c->on_ground = false;
    return true;
}

static void send_abilities(mc_client *c) {
    mc_buf packet;
    start_packet(&packet, 0x13);
    mc_put_u8(&packet, (uint8_t)((c->gamemode == 1 ? 9 : 0) | (c->can_fly ? 4 : 0) | (c->flying ? 2 : 0)));
    mc_put_f32(&packet, 0.05f); mc_put_f32(&packet, 0.1f);
    send_packet(c, &packet);
}

static void send_slot(mc_client *c, int slot) {
    mc_buf packet;
    c->selected = slot;
    start_packet(&packet, 0x09); mc_put_i16(&packet, (int16_t)slot); send_packet(c, &packet);
}

static void configure_player(mc_client *c) {
    mc_buf packet;
    start_packet(&packet, 0x15);
    mc_put_string(&packet, "en_US"); mc_put_u8(&packet, 6); mc_put_u8(&packet, 0);
    mc_put_u8(&packet, 1); mc_put_u8(&packet, 127); send_packet(c, &packet);
    start_packet(&packet, 0x17);
    mc_put_string(&packet, "MC|Brand"); mc_put_string(&packet, "C919 native"); send_packet(c, &packet);
    if (c->gamemode == 1) {
        for (int i = 0; i < 9; ++i) {
            start_packet(&packet, 0x10);
            mc_put_i16(&packet, (int16_t)(36 + i)); mc_put_i16(&packet, (int16_t)mc_client_hotbar[i]);
            mc_put_u8(&packet, 64); mc_put_i16(&packet, 0); mc_put_u8(&packet, 0);
            send_packet(c, &packet);
        }
        c->can_fly = true;
    }
    send_slot(c, c->selected);
}

static void handle_player_info(mc_client *c, mc_buf *b) {
    int action = mc_get_varint(b), count = mc_get_varint(b);
    if (action < 0 || action > 4 || count < 0 || count > 4096) { b->failed = true; return; }
    for (int i = 0; i < count && !b->failed; ++i) {
        uint8_t uuid[16];
        if (!mc_get_bytes(b, uuid, sizeof(uuid))) break;
        mc_player_info *info = player_info(c, uuid, action == 0);
        if (action == 0) {
            char name[17];
            mc_get_string(b, name, sizeof(name));
            int properties = mc_get_varint(b);
            if (properties < 0 || properties > 64) { b->failed = true; break; }
            for (int p = 0; p < properties && !b->failed; ++p) {
                skip_string(b); skip_string(b); if (mc_get_u8(b)) skip_string(b);
            }
            (void)mc_get_varint(b); (void)mc_get_varint(b);
            if (mc_get_u8(b)) skip_string(b);
            if (info && !b->failed) {
                snprintf(info->name, sizeof(info->name), "%s", name);
                for (int p = 0; p < MC_CLIENT_PLAYERS; ++p)
                    if (c->players[p].active && memcmp(c->players[p].uuid, uuid, 16) == 0) snprintf(c->players[p].name, sizeof(c->players[p].name), "%s", name);
            }
        } else if (action == 1 || action == 2) (void)mc_get_varint(b);
        else if (action == 3) { if (mc_get_u8(b)) skip_string(b); }
        else if (info) info->used = false;
    }
}

static void handle_packet(mc_client *c, mc_buf *b) {
    int id = mc_get_varint(b);
    if (b->failed) return;
    if (c->state == 0) {
        if (id == 0x00) {
            char reason[256];
            if (read_chat(b, reason, sizeof(reason))) client_error(c, reason);
        } else if (id == 0x01) client_error(c, "This server requires online authentication/encryption. Use an offline-mode protocol 47 server.");
        else if (id == 0x02) {
            char uuid[37], name[17];
            mc_get_string(b, uuid, sizeof(uuid)); mc_get_string(b, name, sizeof(name));
            if (!b->failed) { c->state = 1; snprintf(c->status, sizeof(c->status), "Login accepted; waiting for world"); }
        } else if (id == 0x03) {
            int threshold = mc_get_varint(b);
            if (threshold < 0 || threshold > (int)MC_MAX_PACKET) b->failed = true;
            else c->connection.compression_threshold = threshold;
        } else b->failed = true;
        return;
    }

    mc_buf reply;
    switch (id) {
        case 0x00: {
            int keepalive = mc_get_varint(b);
            if (!b->failed) { start_packet(&reply, 0x00); mc_put_varint(&reply, keepalive); send_packet(c, &reply); }
            break;
        }
        case 0x01: {
            c->entity_id = mc_get_i32(b); c->gamemode = mc_get_u8(b) & 7;
            c->dimension = (int8_t)mc_get_u8(b); (void)mc_get_u8(b); (void)mc_get_u8(b);
            skip_string(b); (void)mc_get_u8(b);
            if (!b->failed) {
                c->joined = true;
                snprintf(c->status, sizeof(c->status), "Connected; loading terrain");
                printf("JOIN entity=%d gamemode=%d dimension=%d\n", c->entity_id, c->gamemode, c->dimension);
                configure_player(c);
            }
            break;
        }
        case 0x02: {
            char text[256];
            if (read_chat(b, text, sizeof(text))) { (void)mc_get_u8(b); add_chat(c, text); }
            break;
        }
        case 0x06: {
            float health = mc_get_f32(b); (void)mc_get_varint(b); (void)mc_get_f32(b);
            if (!b->failed && health <= 0) { start_packet(&reply, 0x16); mc_put_varint(&reply, 0); send_packet(c, &reply); }
            break;
        }
        case 0x07:
            c->dimension = mc_get_i32(b); (void)mc_get_u8(b); c->gamemode = mc_get_u8(b); skip_string(b);
            if (!b->failed) { mc_world_free(&c->world); mc_world_init(&c->world, 0); memset(c->players, 0, sizeof(c->players)); c->positioned = false; c->velocity_y = 0; }
            break;
        case 0x08: {
            double x = mc_get_f64(b), y = mc_get_f64(b), z = mc_get_f64(b);
            float yaw = mc_get_f32(b), pitch = mc_get_f32(b); uint8_t flags = mc_get_u8(b);
            if (!b->failed && !correct_position(c, x, y, z, yaw, pitch, flags)) b->failed = true;
            if (!b->failed) send_movement(c);
            break;
        }
        case 0x09: {
            int slot = mc_get_u8(b); if (slot < 9) c->selected = slot;
            break;
        }
        case 0x0c: {
            int entity = mc_get_varint(b); uint8_t uuid[16]; mc_get_bytes(b, uuid, sizeof(uuid));
            double x = mc_get_i32(b) / 32.0, y = mc_get_i32(b) / 32.0, z = mc_get_i32(b) / 32.0;
            float yaw = mc_get_u8(b) * (360.0f / 256.0f), pitch = mc_get_u8(b) * (360.0f / 256.0f); (void)mc_get_i16(b);
            if (!b->failed) {
                mc_remote_player *p = remote_player(c, entity, true);
                if (p) {
                    memcpy(p->uuid, uuid, sizeof(uuid)); p->x = x; p->y = y; p->z = z; p->yaw = yaw; p->pitch = pitch;
                    mc_player_info *info = player_info(c, uuid, false);
                    snprintf(p->name, sizeof(p->name), "%s", info ? info->name : "Player");
                }
            }
            break;
        }
        case 0x13: {
            int count = mc_get_varint(b);
            if (count < 0 || count > 4096) { b->failed = true; break; }
            for (int i = 0; i < count && !b->failed; ++i) { mc_remote_player *p = remote_player(c, mc_get_varint(b), false); if (p) p->active = false; }
            break;
        }
        case 0x15: case 0x16: case 0x17: case 0x18: {
            int entity = mc_get_varint(b); mc_remote_player *p = remote_player(c, entity, false);
            double x = 0, y = 0, z = 0; float yaw = 0, pitch = 0;
            if (id == 0x18) { x = mc_get_i32(b) / 32.0; y = mc_get_i32(b) / 32.0; z = mc_get_i32(b) / 32.0; }
            else if (id != 0x16) { x = (int8_t)mc_get_u8(b) / 32.0; y = (int8_t)mc_get_u8(b) / 32.0; z = (int8_t)mc_get_u8(b) / 32.0; }
            if (id != 0x15) { yaw = mc_get_u8(b) * (360.0f / 256.0f); pitch = mc_get_u8(b) * (360.0f / 256.0f); }
            (void)mc_get_u8(b);
            if (!b->failed && p) {
                if (id == 0x18) { p->x = x; p->y = y; p->z = z; }
                else if (id != 0x16) { p->x += x; p->y += y; p->z += z; }
                if (id != 0x15) { p->yaw = yaw; p->pitch = pitch; }
            }
            break;
        }
        case 0x21: {
            int x = mc_get_i32(b), z = mc_get_i32(b); bool full = mc_get_u8(b) != 0;
            uint16_t mask = (uint16_t)mc_get_i16(b); int length = mc_get_varint(b);
            if (length < 0 || (size_t)length > b->len - b->pos) b->failed = true;
            else if (!b->failed && !receive_chunk(c, x, z, mask, full, c->dimension == 0, b->data + b->pos, (size_t)length)) b->failed = true;
            break;
        }
        case 0x22: {
            int x = mc_get_i32(b), z = mc_get_i32(b), count = mc_get_varint(b);
            if (x < -1875000 || x > 1875000 || z < -1875000 || z > 1875000 || count < 0 || count > 65536) { b->failed = true; break; }
            for (int i = 0; i < count && !b->failed; ++i) {
                uint16_t pos = (uint16_t)mc_get_i16(b); int state = mc_get_varint(b);
                if (state < 0 || state > 65535) { b->failed = true; break; }
                if (!b->failed) { mc_world_set(&c->world, x * 16 + ((pos >> 12) & 15), pos & 255, z * 16 + ((pos >> 8) & 15), (uint16_t)state); ++c->block_updates; }
            }
            break;
        }
        case 0x23: {
            int x, y, z; mc_get_position(b, &x, &y, &z); int state = mc_get_varint(b);
            if (state < 0 || state > 65535) b->failed = true;
            if (!b->failed) { mc_world_set(&c->world, x, y, z, (uint16_t)state); ++c->block_updates; }
            break;
        }
        case 0x26: {
            bool skylight = mc_get_u8(b) != 0; int count = mc_get_varint(b);
            typedef struct { int x, z; uint16_t mask; } chunk_meta;
            if (count < 0 || count > 4096 || (size_t)count > (b->len - b->pos) / 10u) { b->failed = true; break; }
            chunk_meta *metadata = calloc((size_t)(count ? count : 1), sizeof(*metadata));
            if (!metadata) { b->failed = true; break; }
            for (int i = 0; i < count; ++i) { metadata[i].x = mc_get_i32(b); metadata[i].z = mc_get_i32(b); metadata[i].mask = (uint16_t)mc_get_i16(b); }
            for (int i = 0; i < count && !b->failed; ++i) {
                size_t length = chunk_size(metadata[i].mask, skylight, true);
                if (length > b->len - b->pos || !receive_chunk(c, metadata[i].x, metadata[i].z, metadata[i].mask, true, skylight, b->data + b->pos, length)) b->failed = true;
                else b->pos += length;
            }
            free(metadata);
            break;
        }
        case 0x2b: {
            int reason = mc_get_u8(b); float value = mc_get_f32(b);
            if (reason == 3 && isfinite(value) && value >= 0 && value <= 3) c->gamemode = (int)value;
            break;
        }
        case 0x38: handle_player_info(c, b); break;
        case 0x39: {
            int flags = mc_get_u8(b); (void)mc_get_f32(b); (void)mc_get_f32(b);
            c->can_fly = (flags & 4) != 0; c->flying = (flags & 2) != 0;
            break;
        }
        case 0x40: {
            char reason[256];
            if (read_chat(b, reason, sizeof(reason))) { snprintf(c->status, sizeof(c->status), "Disconnected: %.235s", reason); c->disconnected = true; fprintf(stderr, "%s\n", c->status); }
            break;
        }
        case 0x48: {
            skip_string(b); char hash[41]; mc_get_string(b, hash, sizeof(hash));
            if (!b->failed) { start_packet(&reply, 0x19); mc_put_string(&reply, hash); mc_put_varint(&reply, 1); send_packet(c, &reply); }
            break;
        }
        default: break; /* Independent packet frames permit safely ignoring unsupported services. */
    }
}

static void poll_network(mc_client *c) {
    if (!mc_conn_poll(&c->connection)) { client_error(c, c->connection.error[0] ? c->connection.error : "Connection closed by server"); return; }
    for (int i = 0; i < 256 && !c->failed && !c->disconnected; ++i) {
        mc_buf packet;
        mc_buf_init(&packet);
        int result = mc_conn_next(&c->connection, &packet);
        if (result < 0) client_error(c, c->connection.error);
        if (result <= 0) { mc_buf_free(&packet); break; }
        c->last_receive_ms = mc_time_ms(); ++c->packets_received;
        handle_packet(c, &packet);
        if (packet.failed) client_error(c, "Malformed or unsupported protocol 47 packet data");
        mc_buf_free(&packet);
    }
    if (c->joined && c->positioned && c->world.count && !c->failed && !c->disconnected) snprintf(c->status, sizeof(c->status), "Connected");
    if (mc_time_ms() - c->last_receive_ms > 30000) client_error(c, "Timed out waiting for server data");
}

static bool body_loaded(const mc_client *c, double x, double z) {
    int first_x = mc_floor_div16((int)floor(x - 0.299)), last_x = mc_floor_div16((int)floor(x + 0.299));
    int first_z = mc_floor_div16((int)floor(z - 0.299)), last_z = mc_floor_div16((int)floor(z + 0.299));
    for (int cz = first_z; cz <= last_z; ++cz) for (int cx = first_x; cx <= last_x; ++cx) {
        bool found = false;
        for (int i = 0; i < c->world.count; ++i)
            if (c->world.chunks[i].x == cx && c->world.chunks[i].z == cz) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

static bool collides(const mc_client *c, double x, double y, double z) {
    if (!body_loaded(c, x, z)) return true;
    if (y < -1000000 || y + 1.8 > 1000000) return true;
    for (int by = (int)floor(y + 0.001); by <= (int)floor(y + 1.799); ++by)
        for (int bz = (int)floor(z - 0.299); bz <= (int)floor(z + 0.299); ++bz)
            for (int bx = (int)floor(x - 0.299); bx <= (int)floor(x + 0.299); ++bx)
                if (mc_world_solid(mc_world_get(&c->world, bx, by, bz))) return true;
    return false;
}

static void move_axis(mc_client *c, double distance, int axis) {
    int steps = (int)ceil(fabs(distance) / 0.15);
    if (!steps) return;
    double step = distance / steps;
    for (int i = 0; i < steps; ++i) {
        double x = c->x + (axis == 0 ? step : 0), y = c->y + (axis == 1 ? step : 0), z = c->z + (axis == 2 ? step : 0);
        if (collides(c, x, y, z)) {
            if (axis == 1) { c->on_ground = step < 0; c->velocity_y = 0; }
            return;
        }
        c->x = x; c->y = y; c->z = z;
    }
}

bool mc_client_ray(const mc_client *c, int *x, int *y, int *z, int *face) {
    double yaw = c->yaw * 0.0174532925199433, pitch = c->pitch * 0.0174532925199433;
    double dx = -sin(yaw) * cos(pitch), dy = -sin(pitch), dz = cos(yaw) * cos(pitch);
    int previous_x = (int)floor(c->x), previous_y = (int)floor(c->y + 1.62), previous_z = (int)floor(c->z);
    for (double distance = 0; distance <= 6; distance += 0.025) {
        int bx = (int)floor(c->x + dx * distance), by = (int)floor(c->y + 1.62 + dy * distance), bz = (int)floor(c->z + dz * distance);
        if (mc_world_get(&c->world, bx, by, bz) >> 4) {
            *x = bx; *y = by; *z = bz;
            *face = bx > previous_x ? 4 : bx < previous_x ? 5 : by > previous_y ? 0 : by < previous_y ? 1 : bz > previous_z ? 2 : 3;
            return true;
        }
        previous_x = bx; previous_y = by; previous_z = bz;
    }
    return false;
}

static void edit_block(mc_client *c, bool place) {
    int x, y, z, face;
    if (c->gamemode != 1 || !mc_client_ray(c, &x, &y, &z, &face)) return;
    mc_buf packet;
    start_packet(&packet, place ? 0x08 : 0x07);
    if (!place) mc_put_varint(&packet, 0);
    mc_put_position(&packet, x, y, z); mc_put_u8(&packet, (uint8_t)face);
    if (place) {
        mc_put_i16(&packet, (int16_t)mc_client_hotbar[c->selected]); mc_put_u8(&packet, 64); mc_put_i16(&packet, 0); mc_put_u8(&packet, 0);
        mc_put_u8(&packet, 8); mc_put_u8(&packet, 8); mc_put_u8(&packet, 8);
    }
    send_packet(c, &packet);
    start_packet(&packet, 0x0a); send_packet(c, &packet);
}

static void update_player(mc_client *c, const mc_input *input, double dt) {
    c->paused = input->paused; c->chat_open = input->chat_open;
    if (!c->joined || !c->positioned || c->failed || c->disconnected) return;
    if (input->chat_submit && input->chat[0]) {
        mc_buf packet; start_packet(&packet, 0x01); mc_put_string(&packet, input->chat); send_packet(c, &packet);
    }
    if (input->select_slot >= 0 && input->select_slot < 9) send_slot(c, input->select_slot);
    if (!body_loaded(c, c->x, c->z)) {
        c->velocity_y = 0;
        snprintf(c->status, sizeof(c->status), "Waiting for terrain at player position");
        if (mc_time_ms() - c->last_move_ms >= 50) send_movement(c);
        return;
    }
    if (!input->paused && !input->chat_open) {
        c->yaw += input->look_x * 0.12f; c->pitch += input->look_y * 0.12f;
        c->yaw = fmodf(c->yaw, 360.0f);
        if (c->pitch < -89.5f) c->pitch = -89.5f;
        if (c->pitch > 89.5f) c->pitch = 89.5f;
        if (input->toggle_flight && c->can_fly) { c->flying = !c->flying; c->velocity_y = 0; send_abilities(c); }
        double forward = (input->forward ? 1 : 0) - (input->backward ? 1 : 0);
        double strafe = (input->right ? 1 : 0) - (input->left ? 1 : 0);
        double magnitude = sqrt(forward * forward + strafe * strafe);
        if (magnitude > 1) { forward /= magnitude; strafe /= magnitude; }
        double yaw = c->yaw * 0.0174532925199433;
        double speed = (c->flying ? 6.0 : 4.3) * (input->sprint ? 1.8 : 1.0) * dt;
        move_axis(c, (-sin(yaw) * forward - cos(yaw) * strafe) * speed, 0);
        move_axis(c, (cos(yaw) * forward - sin(yaw) * strafe) * speed, 2);
        if (c->flying) {
            c->on_ground = false;
            move_axis(c, ((input->up ? 1 : 0) - (input->down ? 1 : 0)) * speed, 1);
        } else {
            if (input->up && c->on_ground) { c->velocity_y = 7.0; c->on_ground = false; }
            c->velocity_y -= 20.0 * dt;
            if (c->velocity_y < -45) c->velocity_y = -45;
            c->on_ground = false;
            move_axis(c, c->velocity_y * dt, 1);
        }
        if (input->break_block) edit_block(c, false);
        if (input->place_block) edit_block(c, true);
    }
    if (mc_time_ms() - c->last_move_ms >= 50) send_movement(c);
}

static bool connect_client(mc_client *c) {
    char error[160];
    mc_socket socket = mc_net_connect(c->host, c->port, error, sizeof(error));
    if (socket == MC_INVALID_SOCKET) { client_error(c, error); return false; }
    mc_conn_init(&c->connection, socket);
    c->last_receive_ms = mc_time_ms();
    mc_buf packet;
    start_packet(&packet, 0x00);
    mc_put_varint(&packet, MC_PROTOCOL_VERSION); mc_put_string(&packet, c->host); mc_put_i16(&packet, (int16_t)c->port); mc_put_varint(&packet, 2);
    if (!send_packet(c, &packet)) return false;
    start_packet(&packet, 0x00); mc_put_string(&packet, c->name);
    if (!send_packet(c, &packet)) return false;
    snprintf(c->status, sizeof(c->status), "Logging in to %.180s:%u", c->host, (unsigned)c->port);
    return true;
}

static int self_test(void) {
    mc_client *c = calloc(1, sizeof(*c));
    if (!c) return 1;
    mc_world_init(&c->world, 0); c->state = 1;
    size_t length = chunk_size(1, true, true);
    uint8_t *data = calloc(length, 1);
    int errors = 0;
    if (!data) { free(c); return 1; }
#define CHECK(condition, message) do { if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); ++errors; } } while (0)
    data[0] = 0x53; data[1] = 0x12;
    CHECK(receive_chunk(c, -1, 0, 1, true, true, data, length), "full chunk is accepted");
    CHECK(mc_world_get(&c->world, -16, 0, 0) == 0x1253, "chunk block state is little endian at negative coordinate");
    CHECK(!receive_chunk(c, 0, 0, 1, true, true, data, length - 1), "truncated chunk rejected");
    CHECK(!receive_chunk(c, 2000000, 0, 1, true, true, data, length), "out of range chunk coordinate rejected");
    CHECK(receive_chunk(c, -1, 0, 2, false, true, data, chunk_size(2, true, false)), "partial section accepted");
    CHECK(mc_world_get(&c->world, -16, 0, 0) == 0x1253 && mc_world_get(&c->world, -16, 16, 0) == 0x1253, "partial section preserves existing section");
    CHECK(receive_chunk(c, -1, 0, 0, true, true, NULL, 0) && c->world.count == 0, "zero length vanilla unload accepted");
    CHECK(correct_position(c, 4, 70, -3, 720, 20, 0) && c->yaw == 0, "valid position and yaw normalization");
    CHECK(correct_position(c, 2, 1, 5, 15, -10, 31) && c->x == 6 && c->y == 71 && c->z == 2 && c->pitch == 10, "relative correction applied safely");
    CHECK(!correct_position(c, 30000000, 70, 0, 0, 0, 0), "exclusive world position boundary rejected");
    CHECK(!correct_position(c, 0, 70, 0, 0, 0, 128), "unknown correction flags rejected");
    CHECK(!correct_position(c, 0, 70, 0, 0, 91, 0), "invalid pitch rejected");
    c->x = DBL_MAX;
    CHECK(!correct_position(c, DBL_MAX, 0, 0, 0, 0, 1), "relative coordinate overflow rejected before integer casts");
    c->x = 0;
    CHECK(!body_loaded(c, 8.5, 8.5), "unloaded player column blocks physics");
    CHECK(receive_chunk(c, 1, 0, 1, true, true, data, length) && !body_loaded(c, 8.5, 8.5), "remote chunk does not unlock spawn physics");
    CHECK(receive_chunk(c, 0, 0, 1, true, true, data, length) && body_loaded(c, 8.5, 8.5), "spawn chunk unlocks physics");
    CHECK(collides(c, 8.5, 70, -0.1), "unloaded boundary treated as blocked");
    CHECK(!collides(c, 8.5, 300, 8.5), "creative movement above build height remains possible");
    mc_buf packet; start_packet(&packet, 0x38); mc_put_varint(&packet, 0); mc_put_varint(&packet, 1);
    uint8_t uuid[16] = {1, 2, 3}; mc_put_bytes(&packet, uuid, 16); mc_put_string(&packet, "VisiblePlayer");
    mc_put_varint(&packet, 0); mc_put_varint(&packet, 1); mc_put_varint(&packet, 0); mc_put_u8(&packet, 0);
    handle_packet(c, &packet); CHECK(!packet.failed, "player info decoded"); mc_buf_free(&packet);
    start_packet(&packet, 0x0c); mc_put_varint(&packet, 42); mc_put_bytes(&packet, uuid, 16);
    mc_put_i32(&packet, 32); mc_put_i32(&packet, 2048); mc_put_i32(&packet, -32); mc_put_u8(&packet, 0); mc_put_u8(&packet, 0); mc_put_i16(&packet, 0); mc_put_u8(&packet, 127);
    handle_packet(c, &packet); mc_remote_player *player = remote_player(c, 42, false);
    CHECK(!packet.failed && player && strcmp(player->name, "VisiblePlayer") == 0 && player->z == -1, "info before spawn resolves UUID and position"); mc_buf_free(&packet);
    char text[256]; plain_chat("{\"text\":\"Hello \",\"extra\":[{\"text\":\"world\"}]}", text, sizeof(text));
    CHECK(strcmp(text, "Hello world") == 0, "chat extras retain text");
    plain_chat("{\"text\":\"\\u65e5\\u672c \\ud83d\\ude00\"}", text, sizeof(text));
    CHECK(strcmp(text, "\xe6\x97\xa5\xe6\x9c\xac \xf0\x9f\x98\x80") == 0, "Japanese and paired surrogate chat decoded as UTF8");
    free(data); mc_world_free(&c->world); free(c);
    if (!errors) printf("client self-test passed\n");
    return errors ? 1 : 0;
#undef CHECK
}

static void usage(const char *program) {
    printf("Usage: %s [--host HOST] [--port 25565] [--name Player] [--headless]\n"
           "       [--run-seconds SECONDS] [--screenshot FRAME.ppm] [--self-test]\n"
           "Offline protocol 47 (Minecraft 1.8.x). Windows GUI; original procedural materials.\n"
           "WASD move; mouse look; Space jump/ascend; Shift descend; Ctrl sprint; F flight.\n"
           "Left/right mouse remove/place; 1-9 hotbar; T chat, Enter send; Esc pause/cursor.\n", program);
}

int main(int argc, char **argv) {
    mc_client *c = calloc(1, sizeof(*c));
    if (!c) { fprintf(stderr, "Out of memory\n"); return 1; }
    snprintf(c->host, sizeof(c->host), "127.0.0.1"); snprintf(c->name, sizeof(c->name), "C919Player");
    c->port = 25565; c->connection.socket = MC_INVALID_SOCKET;
    bool headless = false; double run_seconds = 0; const char *screenshot = NULL;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) { usage(argv[0]); free(c); return 0; }
        if (strcmp(arg, "--self-test") == 0) { free(c); return self_test(); }
        if (strcmp(arg, "--headless") == 0) { headless = true; continue; }
        if (i + 1 >= argc) { fprintf(stderr, "Missing value for %s\n", arg); free(c); return 2; }
        const char *value = argv[++i];
        if (strcmp(arg, "--host") == 0 && *value && strlen(value) < sizeof(c->host)) snprintf(c->host, sizeof(c->host), "%s", value);
        else if (strcmp(arg, "--name") == 0 && *value && strlen(value) <= 16) {
            for (const char *p = value; *p; ++p) if (!isalnum((unsigned char)*p) && *p != '_') { fprintf(stderr, "Name must contain 1-16 ASCII letters, numbers, or underscores\n"); free(c); return 2; }
            snprintf(c->name, sizeof(c->name), "%s", value);
        } else if (strcmp(arg, "--port") == 0) {
            char *end; errno = 0; long port = strtol(value, &end, 10);
            if (errno || *end || port < 1 || port > 65535) { fprintf(stderr, "Invalid port\n"); free(c); return 2; } c->port = (uint16_t)port;
        } else if (strcmp(arg, "--run-seconds") == 0) {
            char *end; errno = 0; run_seconds = strtod(value, &end);
            if (errno || *end || !isfinite(run_seconds) || run_seconds <= 0 || run_seconds > 86400) { fprintf(stderr, "Invalid run duration\n"); free(c); return 2; }
        } else if (strcmp(arg, "--screenshot") == 0 && *value) screenshot = value;
        else { fprintf(stderr, "Invalid argument: %s\n", arg); usage(argv[0]); free(c); return 2; }
    }
    if (headless && screenshot) { fprintf(stderr, "--screenshot needs the Windows renderer; omit --headless\n"); free(c); return 2; }
    if ((headless || screenshot) && run_seconds == 0) run_seconds = 5;
    if (!mc_net_init()) { fprintf(stderr, "Network initialization failed\n"); free(c); return 1; }
    mc_world_init(&c->world, 0);
    mc_renderer *renderer = NULL;
    if (!headless) {
        char error[160]; renderer = mc_renderer_open(screenshot != NULL, error, sizeof(error));
        if (!renderer) { fprintf(stderr, "Renderer: %s\n", error); mc_net_shutdown(); free(c); return 1; }
    }
    connect_client(c);
    uint64_t started = mc_time_ms(), previous = started;
    bool screenshot_done = false;
    while (true) {
        uint64_t now = mc_time_ms(); double dt = (now - previous) / 1000.0; previous = now;
        if (dt > 0.05) dt = 0.05;
        mc_input input; memset(&input, 0, sizeof(input)); input.select_slot = -1;
        if (renderer) mc_renderer_poll(renderer, &input);
        if (input.quit) break;
        if (!c->failed && !c->disconnected) { poll_network(c); update_player(c, &input, dt); }
        if (renderer) mc_renderer_draw(renderer, c);
        if (screenshot && !screenshot_done && c->joined && c->positioned && c->world.count && now - started >= 500) {
            char error[160]; screenshot_done = mc_renderer_screenshot(renderer, screenshot, error, sizeof(error));
            if (!screenshot_done) client_error(c, error);
            else printf("SCREENSHOT %s\n", screenshot);
        }
        if (headless && (c->failed || c->disconnected)) break;
        if (run_seconds > 0 && now - started >= (uint64_t)(run_seconds * 1000)) break;
        mc_sleep_ms(renderer ? 8 : 5);
    }
    unsigned non_air = 0, players = 0;
    for (int i = 0; i < c->world.count; ++i) for (unsigned b = 0; b < MC_CHUNK_BLOCKS; ++b) if (c->world.chunks[i].blocks[b] >> 4) ++non_air;
    for (int i = 0; i < MC_CLIENT_PLAYERS; ++i) if (c->players[i].active) ++players;
    printf("CLIENT_RESULT joined=%d positioned=%d chunks=%d chunk_packets=%u non_air=%u block_updates=%u players=%u packets=%u position=%.3f,%.3f,%.3f\n",
           c->joined, c->positioned, c->world.count, c->chunks_received, non_air, c->block_updates, players, c->packets_received, c->x, c->y, c->z);
    int result = c->failed || c->disconnected || ((headless || screenshot) && (!c->joined || !c->positioned || !c->world.count)) || (screenshot && !screenshot_done) ? 1 : 0;
    if (renderer) mc_renderer_close(renderer);
    mc_conn_close(&c->connection); mc_world_free(&c->world); mc_net_shutdown(); free(c);
    return result;
}
