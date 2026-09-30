#ifndef C919_CLIENT_H
#define C919_CLIENT_H
#include "../network/protocol.h"
#include "../world/world.h"

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
    mc_player_info player_info[MC_CLIENT_PLAYERS];
    mc_remote_player players[MC_CLIENT_PLAYERS];
    char host[256], name[17], status[256];
    char chat[MC_CLIENT_CHAT_LINES][256];
    int chat_count;
    uint16_t port;
    int state, dimension, gamemode, entity_id, selected;
    bool joined, positioned, failed, disconnected, flying, can_fly, on_ground;
    bool paused, chat_open;
    double x, y, z, velocity_y;
    float yaw, pitch;
    uint64_t last_receive_ms, last_move_ms;
    unsigned chunks_received, block_updates, packets_received;
} mc_client;

typedef struct {
    bool quit, forward, backward, left, right, up, down, sprint;
    bool toggle_flight, break_block, place_block, chat_submit;
    bool paused, chat_open;
    int select_slot;
    float look_x, look_y;
    char chat[301];
} mc_input;

extern const int mc_client_hotbar[9];
extern const char *const mc_client_material_names[9];
bool mc_client_ray(const mc_client *client, int *x, int *y, int *z, int *face);
#endif
