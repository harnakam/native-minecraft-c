#ifndef C919_NATIVE_SERVER_GAMEPLAY_H
#define C919_NATIVE_SERVER_GAMEPLAY_H
#include "util/MCGameplayStorage.h"
#include "util/MCGameplayCrafting.h"
#include "network/NetHandlerPlayServer.h"
#include "world/WorldSettingsGameType.h"

/* Single canonical server graph. Source EntityPlayerMP constructs the actual
   player once. MinecraftServer/World providers remain native bindings. No callback
   carries a cached actor/stack pointer from another snapshot or sends sockets. */
bool mc_server_graph_init(MCGameplay *, const mc_world *terrain, int32_t spawnX, int32_t spawnZ,
                          uint64_t seed);
bool mc_server_graph_set_world_game_type(MCGameplay *,const WorldSettingsGameType *);
MCGameplayWorld *mc_server_graph_world(MCGameplay *);
MCGameplayPlayer *mc_server_graph_player(MCGameplay *, size_t index);
const mc_crafting_dispatch *mc_server_graph_crafting(void);
const EntityItemDependencies *mc_server_graph_item_dependencies(void);
const EntityItemConstructorDependencies *mc_server_graph_item_constructors(void);
/* Calls below operate only on disposable working graphs under a RootScope. */
bool mc_server_graph_add_player(MCGameplay *, size_t index, const char *uuid, const char *name,
                                int32_t entityId, double x, double y, double z, bool creative);
/* Native position import adapters run Source MP construction, then set the
   requested position. The explicit-ID overload also imports an Entity ID. */
bool mc_server_graph_add_player_auto(MCGameplay *,size_t,const char *,const char *,double,double,double,bool);
/* Live admission retains both Source MP spawn position and global Entity
   constructor ID (including zero). No independent player spawn is selected. */
bool mc_server_graph_create_player(MCGameplay *,size_t,const char *,const char *,bool);
int32_t mc_server_graph_allocate_entity(MCGameplayWorld *);
bool mc_server_graph_drop(MCGameplayPlayer *, bool all);
bool mc_server_graph_use_item(MCGameplayPlayer *);
/* Calls the original mainInventory update order. The caller owns a RootScope;
   authoritative map effects require the complete disposable durable graph. */
bool mc_server_graph_tick_inventory(MCGameplayPlayer *);
bool mc_server_graph_open_workbench(MCGameplayPlayer *, int32_t x, int32_t y, int32_t z);
bool mc_server_graph_close(MCGameplayPlayer *, bool sendClose);
bool mc_server_graph_detect_changes(MCGameplayObjects *);
/* Explicit native complete-inventory byte budget. Encodes source references
   directly; no authoritative slot mirror or count normalization is created. */
bool mc_server_graph_preflight_inventory(MCGameplayObjects *);
bool mc_server_graph_send_item(MCGameplayPlayer *recipient, EntityItem *);
bool mc_server_graph_send_motion(EntityItem *);
bool mc_server_graph_kill_item(EntityItem *);
bool mc_server_graph_remove_dead(MCGameplayObjects *);
EntityItem *mc_server_graph_find_item(MCGameplayObjects *, int32_t entityId);
#endif
