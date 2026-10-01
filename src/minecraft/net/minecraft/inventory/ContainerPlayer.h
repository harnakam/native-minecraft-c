#ifndef C919_SOURCE_CONTAINER_PLAYER_H
#define C919_SOURCE_CONTAINER_PLAYER_H
#include "inventory/SlotCrafting.h"
typedef struct ContainerPlayer {
    Container container;
    InventoryCrafting *craftMatrix;
    InventoryCraftResult *craftResult;
    bool isLocalWorld;
    MCObject *thePlayer;
    const mc_crafting_dispatch *dependencies;
} ContainerPlayer;
ContainerPlayer *ContainerPlayer_new(InventoryPlayer *,bool localWorld,MCObject *player,const mc_crafting_dispatch *);
bool ContainerPlayer_onCraftMatrixChanged(ContainerPlayer *,IInventory);
bool ContainerPlayer_onContainerClosed(ContainerPlayer *,InventoryPlayer *);
bool ContainerPlayer_canInteractWith(ContainerPlayer *,InventoryPlayer *);
ItemStack *ContainerPlayer_transferStackInSlot(ContainerPlayer *,InventoryPlayer *,int32_t index);
bool ContainerPlayer_canMergeSlot(ContainerPlayer *,const ItemStack *,const Slot *);
#endif
