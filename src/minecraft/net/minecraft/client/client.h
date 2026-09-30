#ifndef C919_CLIENT_H
#define C919_CLIENT_H
#include "../network/protocol.h"
#include "../world/world.h"
#include "inventory/inventory.h"
#include "item/item.h"
#include "entity/item/item_entity.h"

#define MC_CLIENT_PLAYERS 128
#define MC_CLIENT_CHAT_LINES 6

typedef struct {
    bool used;
    uint8_t uuid[16];
    char name[17];
} mc_player_info;

typedef struct {
    bool active;
    int32_t id;
    uint8_t uuid[16];
    char name[17];
    double x, y, z;
    float yaw, pitch;
} mc_remote_player;
typedef struct {
    bool active, metadata_ready;
    mc_item_entity entity;
    int32_t server_x, server_y, server_z;
} mc_client_item;

typedef struct {
    mc_conn connection;
    mc_world world;
    mc_inventory inventory, inventory_authoritative;
    mc_player_info player_info[MC_CLIENT_PLAYERS];
    mc_remote_player players[MC_CLIENT_PLAYERS];
    mc_client_item items[MC_MAX_ITEM_ENTITIES];
    size_t item_nbt_bytes;
    unsigned item_spawns, item_metadata, item_collects;
    uint64_t last_item_tick_ms;
    char host[256], name[17], status[256];
    char chat[MC_CLIENT_CHAT_LINES][256];
    int chat_count;
    uint16_t port;
    int state, dimension, gamemode, entity_id, selected;
    bool joined, positioned, failed, disconnected, flying, can_fly, on_ground;
    bool paused, chat_open;
    bool inventory_open, creative_open, inventory_ready, inventory_pending, inventory_sync;
    bool inventory_sync_slots, inventory_sync_cursor;
    bool inventory_close_requested;
    int inventory_queue[46][3];
    unsigned inventory_queue_count, inventory_queue_index;
    int16_t inventory_action, next_inventory_action;
    uint64_t inventory_pending_ms;
    unsigned inventory_packets, inventory_rejections;
    char inventory_status[160];
    double x, y, z, velocity_y;
    float yaw, pitch;
    uint64_t last_receive_ms, last_move_ms;
    unsigned chunks_received, block_updates, packets_received;
} mc_client;

typedef struct {
    bool quit, forward, backward, left, right, up, down, sprint;
    bool toggle_flight, break_block, place_block, chat_submit;
    bool paused, chat_open;
    bool toggle_inventory, toggle_creative, inventory_click, drop_item, drop_all;
    bool inventory_drag;
    unsigned inventory_drag_mode;
    uint64_t inventory_drag_slots;
    int inventory_slot, inventory_button, inventory_mode, creative_pick;
    int select_slot;
    float look_x, look_y;
    char chat[301];
} mc_input;

bool mc_client_ray(const mc_client *client, int *x, int *y, int *z, int *face);
void mc_client_slot_name(const mc_slot *slot, char *output, size_t capacity);
#endif
