#ifndef C919_INVENTORY_CRAFT_RESULT_H
#define C919_INVENTORY_CRAFT_RESULT_H
#include "InventoryCrafting.h"
typedef struct { mc_slot *stackResult; mc_slot ownedResult; bool ownsStackResult; } InventoryCraftResult;
void InventoryCraftResult_init(InventoryCraftResult *inventory);
void InventoryCraftResult_attach(InventoryCraftResult *inventory, mc_slot *storage);
void InventoryCraftResult_free(InventoryCraftResult *inventory);
int InventoryCraftResult_getSizeInventory(const InventoryCraftResult *inventory);
mc_slot *InventoryCraftResult_getStackInSlot(InventoryCraftResult *inventory,int index);
const char *InventoryCraftResult_getName(const InventoryCraftResult *inventory);
bool InventoryCraftResult_hasCustomName(const InventoryCraftResult *inventory);
InventoryDisplayName InventoryCraftResult_getDisplayName(const InventoryCraftResult *inventory);
bool InventoryCraftResult_decrStackSize(InventoryCraftResult *inventory,int index,int count,mc_slot *removed);
bool InventoryCraftResult_removeStackFromSlot(InventoryCraftResult *inventory,int index,mc_slot *removed);
bool InventoryCraftResult_setInventorySlotContents(InventoryCraftResult *inventory,int index,const mc_slot *stack);
int InventoryCraftResult_getInventoryStackLimit(const InventoryCraftResult *inventory);
void InventoryCraftResult_markDirty(InventoryCraftResult *inventory);
bool InventoryCraftResult_isUseableByPlayer(const InventoryCraftResult *inventory,const void *player);
void InventoryCraftResult_openInventory(InventoryCraftResult *inventory,const void *player);
void InventoryCraftResult_closeInventory(InventoryCraftResult *inventory,const void *player);
bool InventoryCraftResult_isItemValidForSlot(const InventoryCraftResult *inventory,int index,const mc_slot *stack);
int InventoryCraftResult_getField(const InventoryCraftResult *inventory,int id);
void InventoryCraftResult_setField(InventoryCraftResult *inventory,int id,int value);
int InventoryCraftResult_getFieldCount(const InventoryCraftResult *inventory);
void InventoryCraftResult_clear(InventoryCraftResult *inventory);
#endif
