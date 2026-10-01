#ifndef C919_SOURCE_CONTAINER_WORKBENCH_H
#define C919_SOURCE_CONTAINER_WORKBENCH_H
#include "inventory/SlotCrafting.h"
typedef struct { int32_t x,y,z; } mc_crafting_position;
typedef struct ContainerWorkbench {
    Container container;
    InventoryCrafting *craftMatrix;
    InventoryCraftResult *craftResult;
    MCObject *worldObj;
    mc_crafting_position pos;
    bool hasPosition;
    const mc_crafting_dispatch *dependencies;
} ContainerWorkbench;
ContainerWorkbench *ContainerWorkbench_new(InventoryPlayer *,MCObject *world,const mc_crafting_position *,const mc_crafting_dispatch *);
bool ContainerWorkbench_isInstance(const MCObject *);
bool ContainerWorkbench_onCraftMatrixChanged(ContainerWorkbench *,IInventory);
bool ContainerWorkbench_onContainerClosed(ContainerWorkbench *,InventoryPlayer *);
bool ContainerWorkbench_canInteractWith(ContainerWorkbench *,InventoryPlayer *);
ItemStack *ContainerWorkbench_transferStackInSlot(ContainerWorkbench *,InventoryPlayer *,int32_t index);
bool ContainerWorkbench_canMergeSlot(ContainerWorkbench *,const ItemStack *,const Slot *);
#endif
