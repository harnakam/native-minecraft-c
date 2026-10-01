#ifndef C919_NATIVE_GAMEPLAY_WORLD_H
#define C919_NATIVE_GAMEPLAY_WORLD_H
#include "util/MCGameplay.h"
#include "item/crafting/CraftingManager.h"
#include "world/map.h"

typedef struct StatBase StatBase;
typedef struct StatList StatList;
typedef struct FurnaceRecipes FurnaceRecipes;
#define MC_GAMEPLAY_CRAFT_STAT_COUNT 2268u
/* Explicit native World owner adapter, not a complete translated World class.
   Terrain is borrowed read-only and must outlive this graph and its snapshots.
   Mutable maps belong to the graph; snapshots deep-copy their native buffers. */
typedef struct MCGameplayWorld {
    MCObject object;
    MCGameplayObjects *owners;
    const mc_world *terrain;
    mc_maps maps;
    CraftingManager *manager;
    FurnaceRecipes *furnace;
    StatList *statList;
    ItemStackDisplayNameDispatch itemDisplayName;
    MCObject *itemDisplayContext;
    NBTTagCompound *savedItemFields;
    NBTString *savedItemRootName;
    StatBase *craftStats[MC_GAMEPLAY_CRAFT_STAT_COUNT];
    bool remote;
    int32_t spawnX,spawnZ,dimension,nextEntityId;
    uint64_t randomState;
} MCGameplayWorld;
MCGameplayWorld *MCGameplayWorld_new(MCObjectHeap *,MCGameplayObjects *,const mc_world *,CraftingManager *);
bool MCGameplayWorld_isInstance(const MCObject *);
/* Callback-compatible native reads for mc_crafting_dispatch. A missing terrain
   dependency fails the heap instead of manufacturing an empty world. */
bool MCGameplayWorld_isRemote(const MCObject *);
bool MCGameplayWorld_isCraftingTable(const MCObject *,int32_t x,int32_t y,int32_t z);
#endif
