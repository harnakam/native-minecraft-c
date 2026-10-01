#ifndef C919_NATIVE_GAMEPLAY_WORLD_H
#define C919_NATIVE_GAMEPLAY_WORLD_H
#include "world/World.h"
/* Compatibility view of the same Source World object, never a second owner. */
typedef World MCGameplayWorld;
MCGameplayWorld *MCGameplayWorld_new(MCObjectHeap *,MCGameplayObjects *,const mc_world *,CraftingManager *);
MCGameplayWorld *MCGameplayWorld_newWithRandomRuntime(MCObjectHeap *,MCGameplayObjects *,const mc_world *,CraftingManager *,NativeJavaRandomRuntime *);
/* Explicit native allocation/import boundary for the unported child lifecycle.
   Every World base constructor runs exactly once with the given final provider
   and isRemote arguments. Source WorldServer/WorldClient are separate ports. */
MCGameplayWorld *MCGameplayWorld_nativeNewDimension(MCObjectHeap *,MCGameplayObjects *,const mc_world *,
    CraftingManager *,NativeJavaRandomRuntime *,int64_t seed,int32_t dimension,bool client);
bool MCGameplayWorld_isInstance(const MCObject *);
bool MCGameplayWorld_isRemote(const MCObject *);
bool MCGameplayWorld_isCraftingTable(const MCObject *,int32_t,int32_t,int32_t);
#endif
