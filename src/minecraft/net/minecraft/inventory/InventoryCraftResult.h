#ifndef C919_INVENTORY_CRAFT_RESULT_H
#define C919_INVENTORY_CRAFT_RESULT_H
#include "inventory/InventoryCrafting.h"
typedef struct InventoryCraftResult { MCObject object; ItemStackArray *stackResult; } InventoryCraftResult;
InventoryCraftResult *InventoryCraftResult_new(MCObjectHeap *);
int32_t InventoryCraftResult_getSizeInventory(const InventoryCraftResult *);
ItemStack *InventoryCraftResult_getStackInSlot(const InventoryCraftResult *,int32_t);
const char *InventoryCraftResult_getName(const InventoryCraftResult *);
bool InventoryCraftResult_hasCustomName(const InventoryCraftResult *);
InventoryDisplayName InventoryCraftResult_getDisplayName(const InventoryCraftResult *);
ItemStack *InventoryCraftResult_decrStackSize(InventoryCraftResult *,int32_t,int32_t);
ItemStack *InventoryCraftResult_removeStackFromSlot(InventoryCraftResult *,int32_t);
bool InventoryCraftResult_setInventorySlotContents(InventoryCraftResult *,int32_t,ItemStack *);
int32_t InventoryCraftResult_getInventoryStackLimit(const InventoryCraftResult *);
void InventoryCraftResult_markDirty(InventoryCraftResult *);
bool InventoryCraftResult_isUseableByPlayer(const InventoryCraftResult *,const MCObject *);
void InventoryCraftResult_openInventory(InventoryCraftResult *,MCObject *);
void InventoryCraftResult_closeInventory(InventoryCraftResult *,MCObject *);
bool InventoryCraftResult_isItemValidForSlot(const InventoryCraftResult *,int32_t,const ItemStack *);
int32_t InventoryCraftResult_getField(const InventoryCraftResult *,int32_t);
void InventoryCraftResult_setField(InventoryCraftResult *,int32_t,int32_t);
int32_t InventoryCraftResult_getFieldCount(const InventoryCraftResult *);
void InventoryCraftResult_clear(InventoryCraftResult *);
#endif
