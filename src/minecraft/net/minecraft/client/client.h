#ifndef C919_CLIENT_H
#define C919_CLIENT_H
#include "../network/protocol.h"
#include "../world/world.h"
#include "inventory/inventory.h"
#include "item/item.h"

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
    mc_conn connection;
    mc_world world;
    mc_inventory inventory, inventory_authoritative;
    mc_player_info player_info[MC_CLIENT_PLAYERS];
    mc_remote_player players[MC_CLIENT_PLAYERS];
    char host[256], name[17], status[256];
    char chat[MC_CLIENT_CHAT_LINES][256];
    int chat_count;
    uint16_t port;
    int state, dimension, gamemode, entity_id, selected;
    bool joined, positioned, failed, disconnected, flying, can_fly, on_ground;
    bool paused, chat_open;
    bool inventory_open, creative_open, inventory_ready, inventory_pending, inventory_sync;
    bool inventory_sync_slots, inventory_sync_cursor;
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
    bool toggle_inventory, toggle_creative, inventory_click;
    int inventory_slot, inventory_button, inventory_mode, creative_pick;
    int select_slot;
    float look_x, look_y;
    char chat[301];
} mc_input;

bool mc_client_ray(const mc_client *client, int *x, int *y, int *z, int *face);
void mc_client_slot_name(const mc_slot *slot, char *output, size_t capacity);
#endif
