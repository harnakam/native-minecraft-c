#ifndef C919_NATIVE_GAMEPLAY_PLAYER_H
#define C919_NATIVE_GAMEPLAY_PLAYER_H
#include "util/MCGameplayWorld.h"
#include "inventory/ContainerPlayer.h"
#include "nbt/NBTTagCompound.h"
#include "util/NativeJavaUUID.h"
#include "entity/player/PlayerCapabilities.h"
#include "entity/player/EntityPlayer.h"
#include "inventory/InventoryEnderChest.h"
#include "util/FoodStats.h"
#include "util/NativeGameProfile.h"

/* Allocation only, used before the actual source super-constructor. Returned
   Java-default fields are not a constructed Player; constructor callers pin it. */
MCGameplayPlayer *MCGameplayPlayer_nativeAllocate(MCObjectHeap *);
/* Native actor allocation invokes the actual InventoryPlayer/ContainerPlayer
   constructors under a RootScope. Callbacks must be immutable and outlive the
   graph. Stats can be attached later; operations needing it must fail if absent.
   The caller retains the returned actor before the next collection/adoption. */
MCGameplayPlayer *MCGameplayPlayer_new(MCGameplayWorld *,NBTString *,StatFileWriter *,const mc_crafting_dispatch *);
MCGameplayPlayer *MCGameplayPlayer_newWithProfile(MCGameplayWorld *,NativeGameProfile *,StatFileWriter *,const mc_crafting_dispatch *);
bool MCGameplayPlayer_isInstance(const MCObject *);
InventoryPlayer *MCGameplayPlayer_inventory(MCObject *);
MCObject *MCGameplayPlayer_world(MCObject *);
PlayerCapabilities *MCGameplayPlayer_capabilities(const MCObject *);
bool MCGameplayPlayer_isCreativeMode(const MCObject *);
double MCGameplayPlayer_getDistanceSq(const MCObject *,double x,double y,double z);
#endif
