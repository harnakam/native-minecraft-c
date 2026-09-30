#include "server.h"
#include "../network/protocol.h"
#include "../world/world.h"
#include "../inventory/inventory.h"
#include "../item/item.h"
#include "crafting/crafting.h"
#include "entity/item/item_entity.h"
#include "util/transfer.h"
#include "world/map.h"
#include "item/ItemMap.h"
#include "network/play/client/C08PacketPlayerBlockPlacement.h"
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
    bool grounded, sneaking;
    int selected;
    int gamemode;
    bool transaction_pending;
    int16_t rejected_action;
    uint64_t chunks_sent;
    mc_inventory inventory;
    mc_container container;
    uint8_t window_id, next_window_id;
    int table_x, table_y, table_z;
    mc_nbt player_data;
    int creative_drop_threshold;
    int32_t tracked_items[MC_MAX_ITEM_ENTITIES];
    size_t tracked_item_count;
} server_peer;
typedef struct {
    mc_world world;
    server_peer peers[SERVER_CONNECTIONS];
    mc_socket listener;
    const char *save_path;
    int max_players, next_entity, spawn_x, spawn_z;
    bool fatal;
    mc_item_entities items;
    mc_maps maps;
    int64_t map_tick;
    uint64_t last_item_tick, last_item_save;
    uint32_t random_state;
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
static void inventory_slot(server_peer *peer, int index) {
    mc_buf packet;
    packet_start(&packet, 0x2f);
    mc_put_u8(&packet, 0);
    mc_put_i16(&packet, (int16_t)index);
    mc_slot_write(&packet, &peer->inventory.slots[index]);
    queue_packet(peer, &packet);
}
static void inventory_resync(server_peer *peer) {
    mc_buf packet;
    packet_start(&packet, 0x30);
    unsigned count = mc_container_slot_count(&peer->container);
    mc_put_u8(&packet, peer->window_id); mc_put_i16(&packet, (int16_t)count);
    for (unsigned i = 0; i < count; ++i) mc_slot_write(&packet, mc_container_const_get(&peer->inventory, &peer->container, (int)i));
    queue_packet(peer, &packet);
    packet_start(&packet, 0x2f);
    mc_put_u8(&packet, 255); mc_put_i16(&packet, -1);
    mc_slot_write(&packet, &peer->inventory.cursor);
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
        mc_put_varint(packet, peer->gamemode);
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
    mc_put_i16(packet, peer->inventory.slots[MC_HOTBAR_START + peer->selected].item_id);
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
    for (int equipment = 0; equipment < 5; ++equipment) {
        mc_buf packet; packet_start(&packet, 4);
        mc_put_varint(&packet, peer->entity); mc_put_i16(&packet, (int16_t)equipment);
        int index = equipment == 0 ? MC_HOTBAR_START + peer->selected : 9 - equipment;
        mc_slot_write(&packet, &peer->inventory.slots[index]);
        broadcast(server, &packet, peer); mc_buf_free(&packet);
    }
}
static void send_equipment_to(server_peer *recipient, const server_peer *subject) {
    for (int equipment = 0; equipment < 5; ++equipment) {
        mc_buf packet; packet_start(&packet, 4);
        mc_put_varint(&packet, subject->entity); mc_put_i16(&packet, (int16_t)equipment);
        int index = equipment == 0 ? MC_HOTBAR_START + subject->selected : 9 - equipment;
        mc_slot_write(&packet, &subject->inventory.slots[index]); queue_packet(recipient, &packet);
    }
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
static void nbt_name(mc_buf *buffer, int type, const char *name) {
    size_t length = strlen(name);
    mc_put_u8(buffer, (uint8_t)type); mc_put_i16(buffer, (int16_t)length);
    mc_put_bytes(buffer, name, length);
}
static void nbt_int(mc_buf *buffer, int type, const char *name, int value) {
    nbt_name(buffer, type, name);
    if (type == 1) mc_put_u8(buffer, (uint8_t)value);
    else if (type == 2) mc_put_i16(buffer, (int16_t)value);
    else mc_put_i32(buffer, value);
}
static void stored_slot(mc_buf *buffer, const mc_slot *slot, int index) {
    if (index >= 0) nbt_int(buffer, 1, "Slot", index);
    const char *resource = mc_item_resource_name(slot->item_id);
    if (!resource) { buffer->failed = true; return; }
    nbt_name(buffer, 8, "id"); mc_put_i16(buffer, (int16_t)strlen(resource));
    mc_put_bytes(buffer, resource, strlen(resource));
    nbt_int(buffer, 1, "Count", slot->count);
    nbt_int(buffer, 2, "Damage", slot->damage);
    if (slot->nbt.size) {
        mc_nbt_view tag;
        if (!mc_nbt_root(&slot->nbt, &tag) || tag.type != 10) { buffer->failed = true; return; }
        nbt_name(buffer, 10, "tag"); mc_put_bytes(buffer, tag.data, tag.size);
    }
    mc_put_u8(buffer, 0);
}
static int persisted_index(int window_index) {
    if (window_index >= 36) return window_index - 36;
    if (window_index >= 9) return window_index;
    if (window_index >= 5) return 108 - window_index;
    return -1;
}
static int window_index(int stored_index) {
    if (stored_index >= 0 && stored_index <= 8) return stored_index + 36;
    if (stored_index >= 9 && stored_index <= 35) return stored_index;
    if (stored_index >= 100 && stored_index <= 103) return 108 - stored_index;
    return -1;
}
static bool known_player_field(const mc_nbt *field) {
    static const char *const names[] = {"Inventory", "SelectedItemSlot", "C919Crafting", "C919Cursor", "C919Workbench"};
    if (!field->data || field->size < 3) return false;
    size_t length = ((size_t)field->data[1] << 8) | field->data[2];
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i)
        if (strlen(names[i]) == length && field->size >= 3 + length && !memcmp(field->data + 3, names[i], length)) return true;
    return false;
}
static bool player_path(const mc_server *server, const server_peer *peer, char *directory, char *path) {
    if (strlen(server->save_path) > 3900) return false;
    char uuid[37]; mc_uuid_string(peer->uuid, uuid);
    snprintf(directory, 4096, "%s.players", server->save_path);
    snprintf(path, 4096, "%s.players/%s.dat", server->save_path, uuid);
    return true;
}
static bool encode_inventory_all(const server_peer *peer, const mc_inventory *inventory, const mc_container *container, mc_nbt *output, char *error, size_t error_size) {
    mc_buf buffer; mc_buf_init(&buffer);
    if (peer->player_data.size) {
        mc_nbt_view root;
        if (!mc_nbt_root(&peer->player_data, &root) || root.type != 10) {
            snprintf(error, error_size, "Player data root must be a compound"); mc_buf_free(&buffer); return false;
        }
        /* Preserve the original root name and every unknown encoded field. */
        mc_put_bytes(&buffer, peer->player_data.data, peer->player_data.size - root.size);
        mc_buf input = {(uint8_t *)root.data, root.size, root.size, 0, false};
        while (input.pos < input.len && !input.failed) {
            mc_nbt field; mc_nbt_init(&field);
            if (!mc_nbt_read(&input, &field)) input.failed = true;
            if (field.size && !known_player_field(&field)) mc_put_bytes(&buffer, field.data, field.size);
            bool end = !field.size;
            mc_nbt_free(&field);
            if (end) break;
        }
        if (input.failed) buffer.failed = true;
    } else nbt_name(&buffer, 10, "");
    int count = 0;
    for (int i = 5; i < MC_PLAYER_INVENTORY_SIZE; ++i) if (inventory->slots[i].item_id >= 0) ++count;
    nbt_name(&buffer, 9, "Inventory"); mc_put_u8(&buffer, 10); mc_put_i32(&buffer, count);
    for (int i = 5; i < MC_PLAYER_INVENTORY_SIZE; ++i) if (inventory->slots[i].item_id >= 0)
        stored_slot(&buffer, &inventory->slots[i], persisted_index(i));
    count = 0;
    for (int i = 1; i <= 4; ++i) if (inventory->slots[i].item_id >= 0) ++count;
    nbt_name(&buffer, 9, "C919Crafting"); mc_put_u8(&buffer, 10); mc_put_i32(&buffer, count);
    for (int i = 1; i <= 4; ++i) if (inventory->slots[i].item_id >= 0)
        stored_slot(&buffer, &inventory->slots[i], i);
    nbt_name(&buffer, 10, "C919Cursor"); stored_slot(&buffer, &inventory->cursor, -1);
    /* An open table's independent inputs are saved with every player mutation.
       On restart they are released once through the same item-transfer journal. */
    if (container->kind == MC_CONTAINER_WORKBENCH) {
        count = 0;
        for (int i = 1; i <= 9; ++i) if (container->slots[i].item_id >= 0) ++count;
        nbt_name(&buffer, 9, "C919Workbench"); mc_put_u8(&buffer, 10); mc_put_i32(&buffer, count);
        for (int i = 1; i <= 9; ++i) if (container->slots[i].item_id >= 0) stored_slot(&buffer, &container->slots[i], i);
    }
    nbt_int(&buffer, 3, "SelectedItemSlot", peer->selected);
    mc_put_u8(&buffer, 0);
    mc_nbt saved; mc_nbt_init(&saved);
    buffer.pos = 0;
    bool ok = !buffer.failed && mc_nbt_read(&buffer, &saved) && buffer.pos == buffer.len;
    if (ok) { mc_nbt_free(output); *output = saved; }
    else { mc_nbt_free(&saved); if (buffer.failed) snprintf(error, error_size, "Player inventory metadata exceeds its storage limit"); }
    mc_buf_free(&buffer); return ok;
}
static bool encode_inventory(const server_peer *peer, const mc_inventory *inventory, mc_nbt *output, char *error, size_t error_size) {
    return encode_inventory_all(peer, inventory, &peer->container, output, error, error_size);
}
static bool save_inventory(mc_server *server, server_peer *peer, char *error, size_t error_size) {
    char directory[4096], path[4096];
    if (!player_path(server, peer, directory, path)) { snprintf(error, error_size, "Player data path is too long"); return false; }
    if (mc_mkdir(directory) && errno != EEXIST) { snprintf(error, error_size, "Could not create player data directory: %s", strerror(errno)); return false; }
    mc_nbt saved; mc_nbt_init(&saved);
    bool ok = encode_inventory(peer, &peer->inventory, &saved, error, error_size) && mc_nbt_save_gzip(&saved, path, error, error_size);
    if (ok) { mc_nbt_free(&peer->player_data); peer->player_data = saved; }
    else mc_nbt_free(&saved);
    return ok;
}
static bool nbt_integer(const mc_nbt_view *compound, const char *name, int64_t *value) {
    mc_nbt_view field;
    return mc_nbt_find(compound, name, &field) && mc_nbt_get_integer(&field, value);
}
static bool load_stored_slot(const mc_nbt_view *compound, mc_slot *slot) {
    int64_t id, count, damage;
    mc_nbt_view id_field;
    if (compound->type != 10 || !mc_nbt_find(compound, "id", &id_field)) return false;
    if (id_field.type == 8) {
        char resource[128]; int16_t item;
        if (!mc_nbt_get_string(&id_field, resource, sizeof resource) || !mc_item_from_resource_name(resource, &item)) return false;
        id = item;
    } else if (!mc_nbt_get_integer(&id_field, &id)) return false;
    if (!nbt_integer(compound, "Count", &count) ||
        !nbt_integer(compound, "Damage", &damage) || id < -1 || id > INT16_MAX || count < 0 || count > 127 ||
        damage < 0 || damage > INT16_MAX || !mc_slot_set(slot, (int16_t)id, (uint8_t)count, (int16_t)damage)) return false;
    mc_nbt_view tag;
    if (mc_nbt_find(compound, "tag", &tag)) {
        if (tag.type != 10 || id < 0) return false;
        mc_buf encoded; mc_buf_init(&encoded); nbt_name(&encoded, 10, ""); mc_put_bytes(&encoded, tag.data, tag.size);
        bool ok = !encoded.failed && mc_nbt_read(&encoded, &slot->nbt);
        mc_buf_free(&encoded); if (!ok) return false;
    }
    return true;
}
static bool load_inventory(mc_server *server, server_peer *peer, char *error, size_t error_size) {
    char directory[4096], path[4096]; struct stat info;
    if (!player_path(server, peer, directory, path)) { snprintf(error, error_size, "Player data path is too long"); return false; }
    if (stat(path, &info)) {
        if (errno != ENOENT) { snprintf(error, error_size, "Could not inspect player data"); return false; }
        for (int i = 0; i < 9; ++i) mc_slot_set(&peer->inventory.slots[36 + i], creative_palette[i], 64, 0);
        return save_inventory(server, peer, error, error_size);
    }
    if (!mc_nbt_load_gzip(&peer->player_data, path, error, error_size)) return false;
    mc_nbt_view root, list;
    if (!mc_nbt_root(&peer->player_data, &root) || root.type != 10 || !mc_nbt_find(&root, "Inventory", &list) || list.type != 9) goto invalid;
    const char *lists[2] = {"Inventory", "C919Crafting"};
    bool present[MC_PLAYER_INVENTORY_SIZE] = {false};
    for (int which = 0; which < 2; ++which) {
        if (!mc_nbt_find(&root, lists[which], &list)) continue;
        if (list.type != 9 || list.size < 5 || list.data[0] != 10) goto invalid;
        mc_buf list_bytes = {(uint8_t *)list.data, list.size, list.size, 1, false};
        int entries = mc_get_i32(&list_bytes);
        if (entries < 0 || entries > MC_PLAYER_INVENTORY_SIZE) goto invalid;
        for (int i = 0; i < entries; ++i) {
            mc_nbt_view entry; int64_t slot_id;
            if (!mc_nbt_list_get(&list, (size_t)i, &entry) || !nbt_integer(&entry, "Slot", &slot_id)) goto invalid;
            if (slot_id < 0 || slot_id > 103) goto invalid;
            int slot_index = which ? (int)slot_id : window_index((int)slot_id);
            if (slot_index < (which ? 1 : 5) || slot_index >= (which ? 5 : 45) || present[slot_index]) goto invalid;
            mc_slot *slot = &peer->inventory.slots[slot_index];
            if (!load_stored_slot(&entry, slot)) goto invalid;
            present[slot_index] = true;
        }
    }
    mc_nbt_view cursor;
    if (mc_nbt_find(&root, "C919Cursor", &cursor) && !load_stored_slot(&cursor, &peer->inventory.cursor)) goto invalid;
    if (mc_nbt_find(&root, "C919Workbench", &list)) {
        if (list.type != 9 || list.size < 5 || list.data[0] != 10) goto invalid;
        mc_buf bytes = {(uint8_t *)list.data, list.size, list.size, 1, false};
        int count = mc_get_i32(&bytes);
        if (count < 0 || count > 9) goto invalid;
        mc_container_free(&peer->container); mc_container_init(&peer->container, MC_CONTAINER_WORKBENCH);
        bool seen[10] = {false};
        for (int i = 0; i < count; ++i) {
            mc_nbt_view entry; int64_t index;
            if (!mc_nbt_list_get(&list, (size_t)i, &entry) || !nbt_integer(&entry, "Slot", &index) ||
                index < 1 || index > 9 || seen[index] || !load_stored_slot(&entry, &peer->container.slots[index])) goto invalid;
            seen[index] = true;
        }
    }
    int64_t selected;
    if (nbt_integer(&root, "SelectedItemSlot", &selected)) {
        if (selected < 0 || selected > 8) goto invalid;
        peer->selected = (int)selected;
    }
    return true;
invalid:
    snprintf(error, error_size, "Unsupported or invalid player inventory; original player file was preserved"); return false;
}
static bool persist_inventory(mc_server *server, server_peer *peer) {
    char error[256] = "Player data encoding failed";
    if (save_inventory(server, peer, error, sizeof error)) return true;
    fprintf(stderr, "Player %s inventory save failed: %s\n", peer->name, error);
    disconnect_peer(peer, "Player inventory save failed; mutation was not committed.");
    return false;
}
static bool inventory_capacity_all(const server_peer *peer, const mc_inventory *inventory, const mc_container *container) {
    size_t remaining = MC_MAX_PACKET - 8192u;
    for (int i = 0; i <= MC_PLAYER_INVENTORY_SIZE; ++i) {
        const mc_slot *slot = i == MC_PLAYER_INVENTORY_SIZE ? &inventory->cursor : &inventory->slots[i];
        if (remaining < 6 || slot->nbt.size > remaining - 6) return false;
        remaining -= slot->nbt.size + 6;
    }
    if (container->kind == MC_CONTAINER_WORKBENCH) for (unsigned i = 0; i < 10; ++i) {
        if (remaining < 6 || container->slots[i].nbt.size > remaining - 6) return false;
        remaining -= container->slots[i].nbt.size + 6;
    }
    if (peer->player_data.size) {
        mc_nbt_view root;
        if (!mc_nbt_root(&peer->player_data, &root) || root.type != 10) return false;
        size_t prefix = peer->player_data.size - root.size;
        if (prefix > remaining) return false;
        remaining -= prefix;
        mc_buf input = {(uint8_t *)root.data, root.size, root.size, 0, false};
        while (input.pos < input.len) {
            mc_nbt field; mc_nbt_init(&field);
            if (!mc_nbt_read(&input, &field)) { mc_nbt_free(&field); return false; }
            bool end = !field.size;
            if (!known_player_field(&field)) {
                if (field.size > remaining) { mc_nbt_free(&field); return false; }
                remaining -= field.size;
            }
            mc_nbt_free(&field); if (end) break;
        }
    }
    return true;
}
static bool inventory_capacity(const server_peer *peer, const mc_inventory *inventory) {
    return inventory_capacity_all(peer, inventory, &peer->container);
}
static bool commit_state_all(mc_server *server, server_peer *peer, mc_inventory *next, mc_item_entities *next_items,
    mc_container *next_container, mc_maps *next_maps) {
    mc_nbt player_data, items_data, maps_data; mc_nbt_init(&player_data); mc_nbt_init(&items_data); mc_nbt_init(&maps_data);
    char uuid[37], error[256] = "Could not encode item transfer";
    if (peer) mc_uuid_string(peer->uuid, uuid);
    const mc_container *container = peer ? (next_container ? next_container : &peer->container) : NULL;
    bool encoded = (!peer || (next && inventory_capacity_all(peer, next, container) &&
        encode_inventory_all(peer, next, container, &player_data, error, sizeof error))) &&
        mc_item_entities_encode(next_items ? next_items : &server->items, &items_data) &&
        mc_maps_encode(next_maps ? next_maps : &server->maps, &maps_data);
    bool committed = false;
    bool ok = encoded && mc_transfer_commit_all(server->save_path, peer ? uuid : NULL,
        peer ? &player_data : NULL, &items_data, &maps_data, &committed, error, sizeof error);
    /* Once the durable manifest exists, recovery owns the new state. An I/O
       error while checkpointing must stop the server, never restore old items. */
    if (committed) {
        if (peer) {
            mc_inventory_free(&peer->inventory); peer->inventory = *next; mc_inventory_init(next);
            mc_nbt_free(&peer->player_data); peer->player_data = player_data; mc_nbt_init(&player_data);
            if (next_container) {
                mc_container_free(&peer->container); peer->container = *next_container;
                mc_container_init(next_container, MC_CONTAINER_PLAYER);
            }
        }
        if (next_maps) { mc_maps_free(&server->maps); server->maps = *next_maps; mc_maps_init(next_maps); }
        if (next_items) {
            mc_item_entities_free(&server->items); server->items = *next_items; mc_item_entities_init(next_items);
        }
    }
    if (!ok) {
        fprintf(stderr, "Item transfer %s: %s\n", committed ? "requires recovery" : "was not committed", error);
        if (committed || !peer) {
            server->fatal = true;
            for (int i = 0; i < SERVER_CONNECTIONS; ++i)
                disconnect_peer(&server->peers[i], "Item persistence failed; the committed transfer will recover on restart.");
        } else disconnect_peer(peer, "Inventory/item transfer could not be saved; the mutation was not committed.");
    }
    mc_nbt_free(&player_data); mc_nbt_free(&items_data); mc_nbt_free(&maps_data); return ok;
}
static bool commit_state(mc_server *server, server_peer *peer, mc_inventory *next, mc_item_entities *next_items) {
    return commit_state_all(server, peer, next, next_items, NULL, NULL);
}
static int32_t allocate_entity(mc_server *server) {
    for (unsigned tries = 0; tries < MC_MAX_ITEM_ENTITIES + SERVER_CONNECTIONS + 1u; ++tries) {
        int32_t candidate = server->next_entity;
        server->next_entity = candidate == INT32_MAX ? 1 : candidate + 1;
        bool used = mc_item_entities_find(&server->items, candidate) != NULL;
        for (int i = 0; i < SERVER_CONNECTIONS && !used; ++i)
            used = server->peers[i].used && server->peers[i].entity == candidate;
        if (!used) return candidate;
    }
    return 0;
}
static double random_unit(mc_server *server) {
    uint32_t value = server->random_state;
    value ^= value << 13; value ^= value >> 17; value ^= value << 5;
    server->random_state = value; return value / 4294967296.0;
}
static bool add_drops(mc_server *server, const server_peer *peer, mc_item_entities *items,
                      const mc_crafting_effects *effects, bool creative, bool record_thrower) {
    const double radians = 0.017453292519943295;
    for (size_t i = 0; i < effects->count; ++i) {
        mc_item_entity drop; mc_item_entity_init(&drop);
        drop.eid = allocate_entity(server); drop.x = peer->x; drop.y = peer->y + 1.32; drop.z = peer->z;
        drop.pickup_delay = 40; drop.age = creative ? 4800 : 0;
        if (record_thrower) snprintf(drop.thrower, sizeof drop.thrower, "%s", peer->name);
        drop.vx = -sin(peer->yaw * radians) * cos(peer->pitch * radians) * 0.3;
        drop.vz = cos(peer->yaw * radians) * cos(peer->pitch * radians) * 0.3;
        drop.vy = -sin(peer->pitch * radians) * 0.3 + 0.1;
        double angle = random_unit(server) * 6.283185307179586, speed = random_unit(server) * 0.02;
        drop.vx += cos(angle) * speed; drop.vz += sin(angle) * speed;
        drop.vy += (random_unit(server) - random_unit(server)) * 0.1;
        bool ok = drop.eid > 0 && mc_slot_copy(&drop.item, &effects->dropped[i]) && mc_item_entities_add(items, &drop);
        mc_item_entity_free(&drop); if (!ok) return false;
    }
    return true;
}
static void send_item(server_peer *recipient, const mc_item_entity *item) {
    mc_buf packet; mc_buf_init(&packet);
    if (mc_item_entity_spawn(item, &packet)) queue_packet(recipient, &packet); else mc_buf_free(&packet);
    mc_buf_init(&packet);
    if (mc_item_entity_metadata(item, &packet)) queue_packet(recipient, &packet); else mc_buf_free(&packet);
    mc_buf_init(&packet);
    if (mc_item_entity_velocity(item, &packet)) queue_packet(recipient, &packet); else mc_buf_free(&packet);
}
static bool item_visible(const server_peer *peer, const mc_item_entity *item) {
    if (fabs(item->x - peer->x) > 64 || fabs(item->z - peer->z) > 64) return false;
    int cx = mc_floor_div16((int)floor(item->x)), cz = mc_floor_div16((int)floor(item->z));
    if (cx < -3 || cx > 3 || cz < -3 || cz > 3) return false;
    uint64_t bit = UINT64_C(1) << ((cz + 3) * 7 + cx + 3);
    return (peer->chunks_sent & bit) != 0;
}
static bool item_tracked(const server_peer *peer, int32_t eid) {
    for (size_t i = 0; i < peer->tracked_item_count; ++i) if (peer->tracked_items[i] == eid) return true;
    return false;
}
static void forget_item(server_peer *peer, int32_t eid) {
    for (size_t i = 0; i < peer->tracked_item_count; ++i) if (peer->tracked_items[i] == eid) {
        peer->tracked_items[i] = peer->tracked_items[--peer->tracked_item_count]; return;
    }
}
static void destroy_item(mc_server *server, int32_t eid, int32_t collector) {
    for (int i = 0; i < SERVER_CONNECTIONS; ++i) {
        server_peer *peer = &server->peers[i];
        if (!playing(peer) || !item_tracked(peer, eid)) continue;
        mc_buf packet;
        if (collector > 0) { packet_start(&packet, 0x0d); mc_put_varint(&packet, eid); mc_put_varint(&packet, collector); queue_packet(peer, &packet); }
        packet_start(&packet, 0x13); mc_put_varint(&packet, 1); mc_put_varint(&packet, eid); queue_packet(peer, &packet);
        forget_item(peer, eid);
    }
}
static void broadcast_item(mc_server *server, const mc_item_entity *item) {
    for (int i = 0; i < SERVER_CONNECTIONS; ++i) {
        server_peer *peer = &server->peers[i];
        if (playing(peer) && item_visible(peer, item) && !item_tracked(peer, item->eid) && peer->tracked_item_count < MC_MAX_ITEM_ENTITIES) {
            send_item(peer, item); peer->tracked_items[peer->tracked_item_count++] = item->eid;
        }
    }
}
static void sync_items(mc_server *server, server_peer *peer) {
    for (size_t i = 0; i < peer->tracked_item_count;) {
        int32_t eid = peer->tracked_items[i]; mc_item_entity *item = mc_item_entities_find(&server->items, eid);
        if (!item || !item_visible(peer, item)) {
            mc_buf packet; packet_start(&packet, 0x13); mc_put_varint(&packet, 1); mc_put_varint(&packet, eid); queue_packet(peer, &packet);
            forget_item(peer, eid);
        } else ++i;
    }
    for (size_t i = 0; i < server->items.count; ++i) {
        const mc_item_entity *item = &server->items.entries[i];
        if (item_visible(peer, item) && !item_tracked(peer, item->eid) && peer->tracked_item_count < MC_MAX_ITEM_ENTITIES) {
            send_item(peer, item); peer->tracked_items[peer->tracked_item_count++] = item->eid;
        }
    }
}
static bool commit_effects_all(mc_server *server, server_peer *peer, mc_inventory *next,
    mc_container *container, mc_maps *maps, const mc_crafting_effects *effects, bool creative, bool record_thrower) {
    if (!effects->count) return commit_state_all(server, peer, next, NULL, container, maps);
    mc_item_entities items; mc_item_entities_init(&items);
    size_t old_count = server->items.count;
    bool ok = mc_item_entities_copy(&items, &server->items) && add_drops(server, peer, &items, effects, creative, record_thrower);
    if (ok) ok = commit_state_all(server, peer, next, &items, container, maps);
    else disconnect_peer(peer, "The dropped items could not be retained; inventory was preserved.");
    if (ok) for (size_t i = old_count; i < server->items.count; ++i) broadcast_item(server, &server->items.entries[i]);
    mc_item_entities_free(&items); return ok;
}
static bool commit_effects(mc_server *server, server_peer *peer, mc_inventory *next,
    const mc_crafting_effects *effects, bool creative, bool record_thrower) {
    return commit_effects_all(server, peer, next, NULL, NULL, effects, creative, record_thrower);
}
static mc_crafting_context crafting_context(mc_server *server, const server_peer *peer, mc_maps *maps) {
    return (mc_crafting_context){.creative = peer->gamemode == 1, .authoritative = true,
        .maps = maps, .player_x = peer->x, .player_z = peer->z,
        .spawn_x = server->spawn_x, .spawn_z = server->spawn_z, .dimension = 0};
}
static bool close_inventory(mc_server *server, server_peer *peer) {
    mc_inventory next; mc_inventory_init(&next);
    mc_container container; mc_container_init(&container, MC_CONTAINER_PLAYER);
    mc_crafting_effects effects; mc_crafting_effects_init(&effects);
    bool ok = mc_inventory_copy(&next, &peer->inventory) && mc_container_copy(&container, &peer->container) &&
        mc_container_close(&next, &container, &effects);
    if (ok) {
        mc_container_free(&container); mc_container_init(&container, MC_CONTAINER_PLAYER);
        ok = commit_effects_all(server, peer, &next, &container, NULL, &effects, false, false);
    }
    else disconnect_peer(peer, "Inventory close allocation failed; inventory was preserved.");
    if (ok) { peer->window_id = 0; peer->transaction_pending = false; }
    mc_inventory_free(&next); mc_container_free(&container); mc_crafting_effects_free(&effects); return ok;
}
static void remove_peer(mc_server *server, server_peer *peer) {
    bool was_player = peer->state == STATE_PLAY;
    int32_t entity = peer->entity;
    if (was_player) {
        bool external = peer->container.kind == MC_CONTAINER_WORKBENCH;
        if (!server->fatal && close_inventory(server, peer) && external && !server->fatal) (void)close_inventory(server, peer);
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
    mc_inventory_free(&peer->inventory); mc_container_free(&peer->container); mc_nbt_free(&peer->player_data);
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
    peer->entity = allocate_entity(server);
    if (!peer->entity) { disconnect_peer(peer, "No free entity identifier is available."); return; }
    peer->keepalive_at = mc_time_ms();
    peer->gamemode = 1;
    mc_offline_uuid(peer->name, peer->uuid);
    char error[256] = "Could not decode player inventory";
    if (!load_inventory(server, peer, error, sizeof error)) {
        fprintf(stderr, "Player %s data load failed: %s\n", peer->name, error);
        disconnect_peer(peer, "Player data could not be loaded; the existing file was preserved."); return;
    }
    if (peer->container.kind == MC_CONTAINER_WORKBENCH && !close_inventory(server, peer)) return;
    if (!mc_crafting_update(&peer->inventory)) { disconnect_peer(peer, "Crafting output could not be calculated; inventory was preserved."); return; }
    if (!inventory_capacity(peer, &peer->inventory)) {
        disconnect_peer(peer, "Player inventory metadata exceeds the supported size; the existing file was preserved."); return;
    }
    mc_uuid_string(peer->uuid, uuid);
    packet_start(&packet, 2);
    mc_put_string(&packet, uuid); mc_put_string(&packet, peer->name);
    queue_packet(peer, &packet);
    peer->state = STATE_PLAY;
    packet_start(&packet, 1);
    mc_put_i32(&packet, peer->entity);
    mc_put_u8(&packet, (uint8_t)peer->gamemode); mc_put_u8(&packet, 0); mc_put_u8(&packet, 0);
    mc_put_u8(&packet, (uint8_t)server->max_players);
    mc_put_string(&packet, "flat"); mc_put_u8(&packet, 0);
    queue_packet(peer, &packet);
    packet_start(&packet, 0x39);
    mc_put_u8(&packet, 0x0f); mc_put_f32(&packet, 0.05f); mc_put_f32(&packet, 0.1f);
    queue_packet(peer, &packet);
    packet_start(&packet, 5);
    mc_put_position(&packet, (int)floor(peer->x), (int)peer->y, (int)floor(peer->z));
    queue_packet(peer, &packet);
    inventory_resync(peer);
    packet_start(&packet, 9); mc_put_u8(&packet, (uint8_t)peer->selected); queue_packet(peer, &packet);
    send_position(peer);
    for (int i = 0; i < SERVER_CONNECTIONS; ++i) {
        server_peer *other = &server->peers[i];
        if (!playing(other)) continue;
        player_info(&packet, other, true); queue_packet(peer, &packet);
        if (other != peer) {
            spawn_player(&packet, other); queue_packet(peer, &packet); send_equipment_to(peer, other);
            player_info(&packet, peer, true); queue_packet(other, &packet);
            spawn_player(&packet, peer); queue_packet(other, &packet); send_equipment_to(other, peer);
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
static bool table_usable(const mc_server *server, const server_peer *peer) {
    if (peer->container.kind != MC_CONTAINER_WORKBENCH) return true;
    double dx = peer->table_x + 0.5 - peer->x, dy = peer->table_y + 0.5 - peer->y, dz = peer->table_z + 0.5 - peer->z;
    return (mc_world_get(&server->world, peer->table_x, peer->table_y, peer->table_z) >> 4) == 58 &&
        dx * dx + dy * dy + dz * dz <= 64.0;
}
static bool table_reachable(const server_peer *peer, int x, int y, int z) {
    double dx = x + 0.5 - peer->x, dy = y + 0.5 - peer->y, dz = z + 0.5 - peer->z;
    return dx * dx + dy * dy + dz * dz < 64.0;
}
static bool open_workbench(mc_server *server, server_peer *peer, int x, int y, int z) {
    if (peer->container.kind == MC_CONTAINER_WORKBENCH && !close_inventory(server, peer)) return false;
    mc_inventory inventory; mc_inventory_init(&inventory);
    mc_container container; mc_container_init(&container, MC_CONTAINER_WORKBENCH);
    mc_maps maps; mc_maps_init(&maps);
    mc_crafting_context context = crafting_context(server, peer, &maps);
    bool ok = mc_inventory_copy(&inventory, &peer->inventory) && mc_maps_copy(&maps, &server->maps) &&
        mc_container_update(&inventory, &container, &context) &&
        commit_state_all(server, peer, &inventory, NULL, &container, &maps);
    mc_inventory_free(&inventory); mc_container_free(&container); mc_maps_free(&maps);
    if (!ok) return false;
    peer->table_x = x; peer->table_y = y; peer->table_z = z;
    peer->next_window_id = (uint8_t)(peer->next_window_id % 100 + 1);
    peer->window_id = peer->next_window_id; peer->transaction_pending = false;
    mc_buf packet; packet_start(&packet, 0x2d); mc_put_u8(&packet, peer->window_id);
    mc_put_string(&packet, "minecraft:crafting_table");
    mc_put_string(&packet, "{\"translate\":\"tile.workbench.name\"}"); mc_put_u8(&packet, 0);
    queue_packet(peer, &packet); inventory_resync(peer); return true;
}
static void invalidate_workbench(mc_server *server, server_peer *peer) {
    if (table_usable(server, peer)) return;
    uint8_t window = peer->window_id;
    if (close_inventory(server, peer)) {
        mc_buf packet; packet_start(&packet, 0x2e); mc_put_u8(&packet, window); queue_packet(peer, &packet);
        inventory_resync(peer); send_equipment(server, peer);
    }
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
static void confirm_transaction(server_peer *peer, int16_t action, bool accepted) {
    mc_buf packet; packet_start(&packet, 0x32);
    mc_put_u8(&packet, peer->window_id); mc_put_i16(&packet, action); mc_put_u8(&packet, accepted);
    queue_packet(peer, &packet);
}
static void handle_inventory_click(mc_server *server, server_peer *peer, mc_buf *packet) {
    int window = mc_get_u8(packet), index = mc_get_i16(packet), button = mc_get_u8(packet);
    int16_t action = mc_get_i16(packet); int mode = mc_get_u8(packet);
    mc_slot claimed, returned;
    mc_slot_init(&claimed); mc_slot_init(&returned);
    mc_crafting_effects effects; mc_crafting_effects_init(&effects);
    bool parsed = mc_slot_read(packet, &claimed) && complete(packet);
    if (!parsed) { disconnect_peer(peer, "Malformed inventory click packet."); goto done; }
    if (window != peer->window_id || peer->transaction_pending) goto done;
    if (!table_usable(server, peer)) { invalidate_workbench(server, peer); goto done; }
    mc_inventory next; mc_inventory_init(&next);
    mc_container container; mc_container_init(&container, MC_CONTAINER_PLAYER);
    mc_maps maps; mc_maps_init(&maps);
    mc_crafting_context context = crafting_context(server, peer, &maps);
    bool valid =
        !(peer->gamemode != 1 && (mode == 3 || (mode == 5 && button >= 8))) &&
        mc_inventory_copy(&next, &peer->inventory) && mc_container_copy(&container, &peer->container) &&
        mc_maps_copy(&maps, &server->maps) &&
        mc_container_click(&next, &container, &context, index, button, mode, &returned, &effects) &&
        inventory_capacity_all(peer, &next, &container);
    /* Vanilla applies a legitimate action before checking its client return
       stack. A mismatch locks the window and resyncs the resulting state. */
    bool committed = valid && commit_effects_all(server, peer, &next, &container, &maps, &effects, false, false);
    bool accepted = committed && mc_slot_equal(&returned, &claimed);
    mc_inventory_free(&next); mc_container_free(&container); mc_maps_free(&maps);
    confirm_transaction(peer, action, accepted);
    if (!accepted && !peer->transaction_pending) { peer->transaction_pending = true; peer->rejected_action = action; }
    inventory_resync(peer);
    if (committed) send_equipment(server, peer);
done:
    mc_slot_free(&claimed); mc_slot_free(&returned); mc_crafting_effects_free(&effects);
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
static void use_empty_map(mc_server *server, server_peer *peer) {
    int index = MC_HOTBAR_START + peer->selected;
    if (peer->inventory.slots[index].item_id != 395) return;
    mc_inventory next; mc_inventory_init(&next);
    mc_maps maps; mc_maps_init(&maps);
    mc_slot created; mc_slot_init(&created);
    mc_crafting_effects effects; mc_crafting_effects_init(&effects);
    bool ok = mc_inventory_copy(&next, &peer->inventory) && mc_maps_copy(&maps, &server->maps) &&
        mc_maps_create(&maps, &created, peer->x, peer->z, 0, 0);
    if (ok) {
        --next.slots[index].count;
        if (!next.slots[index].count) ok = mc_slot_copy(&next.slots[index], &created);
        else {
            /* Newly allocated IDs have no existing stacks to merge. Vanilla's
               first empty main slot searches hotbar before the three rows. */
            int destination = -1;
            for (int i = 0; i < 36; ++i) {
                int slot = i < 9 ? MC_HOTBAR_START + i : i;
                if (next.slots[slot].item_id < 0) { destination = slot; break; }
            }
            ok = destination >= 0 ? mc_slot_copy(&next.slots[destination], &created) : mc_crafting_effects_append(&effects, &created);
        }
    }
    if (ok) ok = commit_effects_all(server, peer, &next, NULL, &maps, &effects, false, false);
    else disconnect_peer(peer, "Map creation could not be retained; inventory was preserved.");
    if (ok) { inventory_resync(peer); send_equipment(server, peer); }
    mc_inventory_free(&next); mc_maps_free(&maps); mc_slot_free(&created); mc_crafting_effects_free(&effects);
}
typedef struct { mc_server *server; server_peer *peer; } C08ServerHandler;
static void server_processPlayerBlockPlacement(void *opaque, const C08PacketPlayerBlockPlacement *packet) {
    C08ServerHandler *handler = opaque;
    mc_server *server = handler->server; server_peer *peer = handler->peer;
    const C08BlockPos *source_position = C08PacketPlayerBlockPlacement_getPosition(packet);
    if (!source_position) { disconnect_peer(peer, "Invalid block placement position."); return; }
    C08BlockPos position = *source_position;
    int x = position.x, y = position.y, z = position.z;
    int face = C08PacketPlayerBlockPlacement_getPlacedBlockDirection(packet);
    if (face > 5 && face != 255) { disconnect_peer(peer, "Invalid block placement direction."); return; }
    if (face == 255) { use_empty_map(server, peer); return; }
    const mc_slot *held = &peer->inventory.slots[MC_HOTBAR_START + peer->selected];
    if (world_coordinate(x, y, z) && table_reachable(peer, x, y, z) &&
        (mc_world_get(&server->world, x, y, z) >> 4) == 58 && (!peer->sneaking || held->item_id < 0)) {
        (void)open_workbench(server, peer, x, y, z); return;
    }
    static const int dx[6] = {0, 0, 0, 0, -1, 1};
    static const int dy[6] = {-1, 1, 0, 0, 0, 0};
    static const int dz[6] = {0, 0, -1, 1, 0, 0};
    int tx = x + dx[face], ty = y + dy[face], tz = z + dz[face];
    uint16_t placed = 0;
    bool placeable = mc_item_block_state(held->item_id, held->damage, &placed);
    if (!world_coordinate(x, y, z) || !reachable(peer, x, y, z) || !mc_world_get(&server->world, x, y, z) ||
        mc_world_get(&server->world, tx, ty, tz) || held->item_id < 0 || !placeable) {
        correct_block(peer, &server->world, tx, ty, tz); return;
    }
    change_block(server, peer, tx, ty, tz, placed);
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
        if (status == 3 || status == 4) {
            mc_inventory next; mc_inventory_init(&next);
            mc_crafting_effects effects; mc_crafting_effects_init(&effects);
            int index = MC_HOTBAR_START + peer->selected;
            bool ok = mc_inventory_copy(&next, &peer->inventory);
            if (ok && next.slots[index].item_id >= 0) {
                ok = mc_crafting_effects_append(&effects, &next.slots[index]);
                if (ok) {
                    unsigned count = status == 3 ? next.slots[index].count : 1;
                    effects.dropped[0].count = (uint8_t)count;
                    next.slots[index].count -= (uint8_t)count;
                    if (!next.slots[index].count) mc_slot_free(&next.slots[index]);
                    ok = commit_effects(server, peer, &next, &effects, false, true);
                }
                if (ok) { inventory_resync(peer); send_equipment(server, peer); }
            }
            if (!ok && !peer->closing) disconnect_peer(peer, "Drop allocation failed; inventory was preserved.");
            mc_inventory_free(&next); mc_crafting_effects_free(&effects);
        }
        return;
    }
    if (id == 8) {
        C08PacketPlayerBlockPlacement placement; C08PacketPlayerBlockPlacement_init(&placement);
        if (!C08PacketPlayerBlockPlacement_readPacketData(&placement, packet) || !complete(packet))
            disconnect_peer(peer, "Invalid block placement packet.");
        else {
            C08ServerHandler handler = {server, peer};
            C08PacketPlayerBlockPlacement_processPacket(&placement, &handler, server_processPlayerBlockPlacement);
        }
        C08PacketPlayerBlockPlacement_free(&placement); return;
    }
    if (id == 9) {
        int selected = mc_get_i16(packet);
        if (!complete(packet) || selected < 0 || selected > 8) { disconnect_peer(peer, "Invalid hotbar selection."); return; }
        int old = peer->selected; peer->selected = selected;
        if (!persist_inventory(server, peer)) { peer->selected = old; return; }
        send_equipment(server, peer); return;
    }
    if (id == 0x10) {
        int index = mc_get_i16(packet); mc_slot item; mc_slot_init(&item);
        bool parsed = mc_slot_read(packet, &item) && complete(packet);
        if (!parsed) { mc_slot_free(&item); disconnect_peer(peer, "Malformed creative inventory packet."); return; }
        if (peer->gamemode != 1 || index == 0 || index >= MC_PLAYER_INVENTORY_SIZE || item.count > 64 || peer->transaction_pending) {
            mc_slot_free(&item); inventory_resync(peer); return;
        }
        if (index < 0) {
            if (item.item_id >= 0 && peer->creative_drop_threshold < 200) {
                mc_inventory next; mc_inventory_init(&next);
                mc_crafting_effects effects; mc_crafting_effects_init(&effects);
                bool ready = mc_inventory_copy(&next, &peer->inventory) && mc_crafting_effects_append(&effects, &item);
                if (ready) ready = commit_effects(server, peer, &next, &effects, true, false);
                else disconnect_peer(peer, "Drop allocation failed; inventory was preserved.");
                if (ready) peer->creative_drop_threshold += 20;
                mc_inventory_free(&next); mc_crafting_effects_free(&effects);
            }
            mc_slot_free(&item); return;
        }
        mc_inventory next; mc_inventory_init(&next);
        mc_container container; mc_container_init(&container, MC_CONTAINER_PLAYER);
        mc_maps maps; mc_maps_init(&maps);
        mc_crafting_context context = crafting_context(server, peer, &maps);
        bool ready = mc_inventory_copy(&next, &peer->inventory) && mc_container_copy(&container, &peer->container) &&
            mc_maps_copy(&maps, &server->maps) && mc_slot_copy(&next.slots[index], &item) &&
            mc_crafting_update(&next);
        mc_slot_free(&item);
        if (!ready) disconnect_peer(peer, "Inventory allocation failed.");
        else if (!inventory_capacity_all(peer, &next, &container)) inventory_resync(peer);
        else if (!mc_container_update(&next, &container, &context)) disconnect_peer(peer, "Inventory allocation failed.");
        else if (!inventory_capacity_all(peer, &next, &container)) inventory_resync(peer);
        else if (commit_state_all(server, peer, &next, NULL, &container, &maps)) {
            inventory_slot(peer, index); if (index <= 4) inventory_slot(peer, 0); send_equipment(server, peer);
            if (peer->window_id) inventory_resync(peer);
        }
        mc_inventory_free(&next); mc_container_free(&container); mc_maps_free(&maps);
        return;
    }
    if (id == 0x0e) { handle_inventory_click(server, peer, packet); return; }
    if (id == 0x0f) {
        int window = mc_get_u8(packet); int16_t action = mc_get_i16(packet); (void)get_boolean(packet);
        if (!complete(packet)) {
            disconnect_peer(peer, "Invalid inventory transaction acknowledgement."); return;
        }
        if (window == peer->window_id && peer->transaction_pending && action == peer->rejected_action) peer->transaction_pending = false;
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
        else if (action == 0 || action == 1) peer->sneaking = action == 0;
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
        (void)mc_get_u8(packet);
        if (!complete(packet)) disconnect_peer(peer, "Invalid inventory window.");
        else if (close_inventory(server, peer)) { inventory_resync(peer); send_equipment(server, peer); }
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
    invalidate_workbench(server, peer);
    if (peer->closing || server->fatal) return;
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
static void item_metadata(mc_server *server, const mc_item_entity *item) {
    mc_buf packet; mc_buf_init(&packet);
    if (mc_item_entity_metadata(item, &packet)) for (int i = 0; i < SERVER_CONNECTIONS; ++i) {
        server_peer *peer = &server->peers[i];
        if (playing(peer) && item_tracked(peer, item->eid)) mc_conn_send(&peer->conn, &packet);
    }
    mc_buf_free(&packet);
}
static void item_motion(mc_server *server, const mc_item_entity *item) {
    mc_buf position, velocity; mc_buf_init(&velocity);
    packet_start(&position, 0x18); mc_put_varint(&position, item->eid);
    mc_put_i32(&position, (int32_t)floor(item->x * 32)); mc_put_i32(&position, (int32_t)floor(item->y * 32));
    mc_put_i32(&position, (int32_t)floor(item->z * 32)); mc_put_u8(&position, 0); mc_put_u8(&position, 0); mc_put_u8(&position, item->on_ground);
    if (mc_item_entity_velocity(item, &velocity)) for (int i = 0; i < SERVER_CONNECTIONS; ++i) {
        server_peer *peer = &server->peers[i];
        if (playing(peer) && item_tracked(peer, item->eid)) { mc_conn_send(&peer->conn, &position); mc_conn_send(&peer->conn, &velocity); }
    }
    mc_buf_free(&position); mc_buf_free(&velocity);
}
static bool save_items(mc_server *server, char *error, size_t size) {
    char path[4096];
    if (strlen(server->save_path) > 3900) return false;
    snprintf(path, sizeof path, "%s.items.dat", server->save_path);
    mc_nbt data; mc_nbt_init(&data);
    bool ok = mc_item_entities_encode(&server->items, &data) && mc_nbt_save_gzip(&data, path, error, size);
    mc_nbt_free(&data); return ok;
}
static bool load_items(mc_server *server, char *error, size_t size) {
    if (!mc_transfer_recover(server->save_path, error, size)) return false;
    char path[4096];
    if (strlen(server->save_path) > 3900) return false;
    snprintf(path, sizeof path, "%s.items.dat", server->save_path);
    struct stat info;
    if (stat(path, &info)) {
        if (errno == ENOENT) return save_items(server, error, size);
        snprintf(error, size, "Cannot inspect existing item snapshot"); return false;
    }
    mc_nbt data; mc_nbt_init(&data);
    bool ok = mc_nbt_load_gzip(&data, path, error, size) && mc_item_entities_decode(&data, &server->items);
    mc_nbt_free(&data);
    if (ok) for (size_t i = 0; i < server->items.count; ++i) {
        int32_t eid = server->items.entries[i].eid;
        if (eid >= server->next_entity) server->next_entity = eid == INT32_MAX ? 1 : eid + 1;
    }
    return ok;
}
static bool load_maps(mc_server *server, char *error, size_t size) {
    char path[4096]; struct stat info;
    if (strlen(server->save_path) > 3900) return false;
    snprintf(path, sizeof path, "%s.maps.dat", server->save_path);
    mc_nbt data; mc_nbt_init(&data);
    bool ok;
    if (stat(path, &info)) {
        ok = errno == ENOENT && mc_maps_encode(&server->maps, &data) && mc_nbt_save_gzip(&data, path, error, size);
    } else ok = mc_nbt_load_gzip(&data, path, error, size) && mc_maps_decode(&data, &server->maps);
    mc_nbt_free(&data); return ok;
}
static void free_world_state(mc_server *server) {
    mc_item_entities_free(&server->items); mc_maps_free(&server->maps); mc_world_free(&server->world); free(server);
}
/* InventoryPlayer uses hotbar 0..8, main 9..35, then armor 0..3.
   The existing protocol inventory stores hotbar at 36 and armor reversed. */
static int map_inventory_slot(int original_index) {
    if (original_index < 9) return MC_HOTBAR_START + original_index;
    return original_index < 36 ? original_index : 44 - original_index;
}
static void tick_maps(mc_server *server) {
    if (server->map_tick == INT64_MAX) server->map_tick = INT64_MIN;
    else ++server->map_tick;
    mc_map_player players[SERVER_CONNECTIONS]; size_t player_count = 0;
    for (int i = 0; i < SERVER_CONNECTIONS; ++i) {
        server_peer *peer = &server->peers[i];
        if (playing(peer)) players[player_count++] = (mc_map_player){peer->entity, peer->name,
            &peer->inventory, peer->x, peer->z, peer->yaw, 0, true};
    }
    for (int i = 0; i < SERVER_CONNECTIONS && !server->fatal; ++i) {
        server_peer *peer = &server->peers[i];
        if (!playing(peer)) continue;
        bool has_map = false;
        for (int n = 0; n < 40; ++n)
            if (peer->inventory.slots[map_inventory_slot(n)].item_id == 358) { has_map = true; break; }
        if (!has_map) continue;
        mc_maps maps; mc_maps_init(&maps); mc_inventory inventory; mc_inventory_init(&inventory);
        bool ok = mc_maps_copy(&maps, &server->maps) && mc_inventory_copy(&inventory, &peer->inventory);
        bool persistent = false, inventory_changed = false;
        mc_map_player viewer = {peer->entity, peer->name, &inventory,
            peer->x, peer->z, peer->yaw, 0, true};
        for (size_t p = 0; p < player_count; ++p)
            if (players[p].entity_id == peer->entity) players[p].inventory = &inventory;
        /* InventoryPlayer.decrementAnimations -> ItemStack.updateAnimation ->
           ItemMap.onUpdate runs for every main-inventory stack, in this order. */
        for (int n = 0; n < 36 && ok; ++n) {
            int slot = map_inventory_slot(n);
            if (inventory.slots[slot].item_id != 358) continue;
            bool changed = false;
            ok = mc_ItemMap_onUpdate(&maps, &server->world, &inventory.slots[slot], &viewer,
                players, player_count, n == peer->selected, server->spawn_x, server->spawn_z,
                false, server->map_tick, &changed);
            persistent |= changed;
        }
        /* EntityPlayerMP also asks armor maps for a packet. They do not survey,
           but its getMapData dependency still resolves missing saved data. */
        for (int n = 36; n < 40 && ok; ++n) {
            mc_slot *stack = &inventory.slots[map_inventory_slot(n)];
            if (stack->item_id != 358) continue;
            size_t count = maps.count; int16_t damage = stack->damage;
            ok = mc_ItemMap_getMapData(&maps, stack, false, server->spawn_x, server->spawn_z, 0) != NULL;
            persistent |= maps.count != count || stack->damage != damage;
        }
        for (int n = 0; n < 40 && ok; ++n) {
            int slot = map_inventory_slot(n);
            inventory_changed |= !mc_slot_equal(&inventory.slots[slot], &peer->inventory.slots[slot]);
        }
        if (ok && (persistent || inventory_changed)) {
            ok = commit_state_all(server, inventory_changed ? peer : NULL,
                inventory_changed ? &inventory : NULL, NULL, NULL, &maps);
            if (ok && inventory_changed) { inventory_resync(peer); send_equipment(server, peer); }
        } else if (ok) {
            /* MapInfo counters/dirty ranges/decorations are runtime state.
               Keeping this copy is necessary even when no NBT changed. */
            mc_maps_free(&server->maps); server->maps = maps; mc_maps_init(&maps);
        }
        for (size_t p = 0; p < player_count; ++p)
            if (players[p].entity_id == peer->entity) players[p].inventory = &peer->inventory;
        /* Original EntityPlayerMP requests each map every tick; MapInfo itself
           decides full, dirty rectangle, icons-only, or no packet. */
        for (int n = 0; n < 40 && ok; ++n) {
            mc_slot *stack = &peer->inventory.slots[map_inventory_slot(n)];
            if (stack->item_id != 358) continue;
            mc_buf packet; mc_buf_init(&packet);
            int result = mc_ItemMap_createMapDataPacket_at(&server->maps, stack, peer->entity,
                server->spawn_x, server->spawn_z, 0, &packet);
            if (result == 1) queue_packet(peer, &packet);
            else { mc_buf_free(&packet); if (result < 0) ok = false; }
        }
        if (!ok && !server->fatal && !peer->closing)
            disconnect_peer(peer, "Map update could not be retained; saved data was preserved.");
        mc_inventory_free(&inventory); mc_maps_free(&maps);
    }
}
static void tick_items(mc_server *server, uint64_t now) {
    unsigned steps = 0;
    while (!server->fatal && now - server->last_item_tick >= 50 && steps++ < 20) {
        server->last_item_tick += 50;
        tick_maps(server);
        for (int i = 0; i < SERVER_CONNECTIONS; ++i) {
            server_peer *peer = &server->peers[i];
            if (peer->creative_drop_threshold > 0) --peer->creative_drop_threshold;
        }
        for (size_t i = 0; i < server->items.count && !server->fatal;) {
            mc_item_entity *current = &server->items.entries[i];
            int32_t eid = current->eid;
            if (!mc_world_chunk(&server->world, mc_floor_div16((int)floor(current->x)), mc_floor_div16((int)floor(current->z)), false)) { ++i; continue; }
            mc_item_entity moved; mc_item_entity_init(&moved);
            if (!mc_item_entity_copy(&moved, current)) { server->fatal = true; mc_item_entity_free(&moved); break; }
            bool alive = mc_item_entity_tick(&moved, &server->world);
            if (!alive) {
                mc_item_entities next; mc_item_entities_init(&next);
                bool ok = mc_item_entities_copy(&next, &server->items) && mc_item_entities_remove(&next, eid) && commit_state(server, NULL, NULL, &next);
                if (ok) destroy_item(server, eid, 0); else server->fatal = true;
                mc_item_entities_free(&next); mc_item_entity_free(&moved);
                continue;
            }
            bool changed = floor(current->x * 32) != floor(moved.x * 32) || floor(current->y * 32) != floor(moved.y * 32) || floor(current->z * 32) != floor(moved.z * 32) || moved.ticks % 60u == 0;
            mc_item_entity_free(current); *current = moved;
            if (changed) item_motion(server, current);
            for (int p = 0; p < SERVER_CONNECTIONS && !server->fatal; ++p) {
                server_peer *peer = &server->peers[p];
                current = mc_item_entities_find(&server->items, eid);
                if (!current) break;
                if (!playing(peer) || !mc_item_entity_pickup_eligible(current, peer->name) || !mc_item_entity_pickup_near(current, peer->x, peer->y, peer->z)) continue;
                mc_inventory inventory; mc_inventory_init(&inventory);
                mc_item_entities next; mc_item_entities_init(&next);
                unsigned inserted = 0, discarded = 0;
                bool ok = mc_inventory_copy(&inventory, &peer->inventory) && mc_item_entities_copy(&next, &server->items);
                mc_item_entity *picked = ok ? mc_item_entities_find(&next, eid) : NULL;
                if (ok) ok = mc_item_entity_pickup(picked, &inventory, peer->gamemode == 1, &inserted, &discarded);
                bool consumed = ok && picked->item.item_id < 0;
                if (consumed) ok = mc_item_entities_remove(&next, eid);
                if (ok && (inserted || discarded)) {
                    ok = commit_state(server, peer, &inventory, &next);
                    if (ok) {
                        inventory_resync(peer); send_equipment(server, peer);
                        if (consumed) destroy_item(server, eid, peer->entity);
                        else { current = mc_item_entities_find(&server->items, eid); if (current) item_metadata(server, current); }
                    }
                } else if (!ok) { disconnect_peer(peer, "Item pickup could not be retained; inventory was preserved."); }
                mc_inventory_free(&inventory); mc_item_entities_free(&next);
            }
            if (mc_item_entities_find(&server->items, eid)) ++i;
        }
        /* Merging is evaluated on the target's 25-tick interval. Only actual
           merges need a durable replacement and metadata/destroy broadcast. */
        for (size_t a = 0; a < server->items.count && !server->fatal; ++a) {
            if (server->items.entries[a].ticks % 25u) continue;
            for (size_t b = a + 1; b < server->items.count && !server->fatal;) {
                const mc_item_entity *first = &server->items.entries[a], *second = &server->items.entries[b];
                if (fabs(first->x - second->x) > 0.75 || fabs(first->y - second->y) > 0.25 || fabs(first->z - second->z) > 0.75 ||
                    first->item.count + second->item.count > mc_item_stack_limit(first->item.item_id) ||
                    first->item.item_id != second->item.item_id ||
                    (mc_item_has_subtypes(first->item.item_id) && first->item.damage != second->item.damage) ||
                    !mc_nbt_equal(&first->item.nbt, &second->item.nbt)) { ++b; continue; }
                int32_t first_id = first->eid, second_id = second->eid;
                mc_item_entities next; mc_item_entities_init(&next);
                bool merged = mc_item_entities_copy(&next, &server->items) && mc_item_entity_merge(&next.entries[a], &next.entries[b]);
                int32_t removed = merged ? (next.entries[a].item.item_id < 0 ? first_id : second_id) : 0;
                int32_t retained = removed == first_id ? second_id : first_id;
                if (merged) merged = mc_item_entities_remove(&next, removed) && commit_state(server, NULL, NULL, &next);
                if (merged) {
                    destroy_item(server, removed, 0);
                    mc_item_entity *item = mc_item_entities_find(&server->items, retained); if (item) item_metadata(server, item);
                }
                mc_item_entities_free(&next);
                if (!merged || removed == first_id) { ++b; if (merged) break; }
            }
        }
        for (int i = 0; i < SERVER_CONNECTIONS; ++i) if (playing(&server->peers[i])) sync_items(server, &server->peers[i]);
    }
    if (!server->fatal && now - server->last_item_save >= 1000) {
        char error[256] = "Could not encode item snapshot";
        if (!save_items(server, error, sizeof error)) { fprintf(stderr, "Item snapshot save failed: %s\n", error); server->fatal = true; }
        server->last_item_save = now;
    }
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
    server->random_state = (uint32_t)seed ^ (uint32_t)mc_time_ms();
    if (!server->random_state) server->random_state = 919;
    mc_item_entities_init(&server->items);
    mc_maps_init(&server->maps);
    server->listener = MC_INVALID_SOCKET;
    mc_world_init(&server->world, (uint32_t)seed);
    char error[256]; struct stat info;
    if (!stat(path, &info)) {
        if (!mc_world_load(&server->world, path, error, sizeof error)) {
            fprintf(stderr, "Could not load existing world %s: %s\n", path, error);
            free_world_state(server); return 1;
        }
    } else if (errno == ENOENT) {
        for (int z = -3; z <= 3; ++z) for (int x = -3; x <= 3; ++x) mc_world_generate(&server->world, x, z);
        if (server->world.count != 49 || !mc_world_save(&server->world, path, error, sizeof error)) {
            fprintf(stderr, "Could not create world %s: %s\n", path, server->world.count != 49 ? "chunk allocation failed" : error);
            free_world_state(server); return 1;
        }
    } else {
        fprintf(stderr, "Could not inspect world %s: %s\n", path, strerror(errno));
        free_world_state(server); return 1;
    }
    if (!load_items(server, error, sizeof error)) {
        fprintf(stderr, "Could not load/recover item state; existing data was preserved: %s\n", error); free_world_state(server); return 1;
    }
    if (!load_maps(server, error, sizeof error)) {
        fprintf(stderr, "Could not load MapData; existing data was preserved: %s\n", error); free_world_state(server); return 1;
    }
    server_peer spawn = {0};
    if (select_spawn(server, &spawn)) { server->spawn_x = (int)floor(spawn.x); server->spawn_z = (int)floor(spawn.z); }
    if (!mc_net_init()) { fputs("Network initialization failed.\n", stderr); free_world_state(server); return 1; }
    server->listener = mc_net_listen(bind, (uint16_t)port, error, sizeof error);
    if (server->listener == MC_INVALID_SOCKET) {
        fprintf(stderr, "Could not listen: %s\n", error);
        mc_net_shutdown(); free_world_state(server); return 1;
    }
    signal(SIGINT, stop_server); signal(SIGTERM, stop_server);
    server_running = 1;
    uint64_t started = mc_time_ms();
    server->last_item_tick = started; server->last_item_save = started;
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
            mc_inventory_init(&peer->inventory); mc_container_init(&peer->container, MC_CONTAINER_PLAYER); mc_nbt_init(&peer->player_data);
            peer->used = true; peer->connected_at = now;
        }
        for (int i = 0; i < SERVER_CONNECTIONS; ++i) if (server->peers[i].used) tick_peer(server, &server->peers[i], now);
        tick_items(server, now);
        mc_sleep_ms(2);
    }
    if (!server->fatal && !mc_world_save(&server->world, path, error, sizeof error)) {
        fprintf(stderr, "Final world save failed: %s\n", error); server->fatal = true;
    }
    for (int i = 0; i < SERVER_CONNECTIONS; ++i) {
        server_peer *peer = &server->peers[i];
        if (peer->used) { disconnect_peer(peer, server->fatal ? "World persistence failed." : "Server stopped."); mc_conn_poll(&peer->conn); remove_peer(server, peer); }
    }
    if (!server->fatal && !save_items(server, error, sizeof error)) { fprintf(stderr, "Final item save failed: %s\n", error); server->fatal = true; }
    int result = server->fatal ? 1 : 0;
    mc_socket_close(server->listener); mc_net_shutdown(); free_world_state(server);
    return result;
}
int main(int argc, char **argv) { return mc_server_main(argc, argv); }
