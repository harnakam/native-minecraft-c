#ifndef C919_NATIVE_CLIENT_RUNTIME_H
#define C919_NATIVE_CLIENT_RUNTIME_H
#include "util/MCGameplayClientPackets.h"
#include "client/multiplayer/PlayerControllerMP.h"
#include "client/entity/EntityPlayerSP.h"
#include "inventory/ContainerWorkbench.h"

/* Native Minecraft/screen bindings, not a second inventory. Every retained
   source dependency is traced and remapped with the complete remote graph.
   Win32 input, terrain and rendering remain explicit native adapters. */
typedef struct MCClientBindings {
    MCObject object;
    MCGameplayPlayer *player;
    PlayerControllerMP *controller;
    EntityPlayerSP *sp;
    Container *screenContainer;
    DataWatcherBlockPos *origin;
    bool screenOpen, creativeScreen;
} MCClientBindings;
bool mc_client_graph_init(MCGameplay *, const mc_world *, const char *name);
MCGameplayPlayer *mc_client_graph_player(const MCGameplay *);
MCGameplayWorld *mc_client_graph_world(const MCGameplay *);
MCClientBindings *mc_client_graph_bindings(const MCGameplay *);
EntityItem *mc_client_graph_item(const MCGameplay *, int32_t id);
bool mc_client_graph_spawn_item(MCGameplay *,int32_t id,double x,double y,double z,double vx,double vy,double vz);
bool mc_client_graph_open(MCGameplay *, int32_t window, bool workbench);
bool mc_client_graph_close(MCGameplay *, bool send);
bool mc_client_graph_click(MCGameplay *, int32_t slot, int32_t button, int32_t mode);
bool mc_client_graph_select(MCGameplay *, int32_t index);
bool mc_client_graph_drop(MCGameplay *, bool all);
bool mc_client_graph_creative(MCGameplay *, int32_t id, int32_t damage);
bool mc_client_graph_place(MCGameplay *, int32_t x,int32_t y,int32_t z,int32_t face,bool air);
/* Borrowed source methods are invoked under the caller's initialized scope. */
const mc_crafting_dispatch *mc_client_graph_crafting(void);
void mc_client_graph_slot_name(ItemStack *, char *, size_t);
#endif
