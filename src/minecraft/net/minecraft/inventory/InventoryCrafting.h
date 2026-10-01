#ifndef C919_INVENTORY_CRAFTING_H
#define C919_INVENTORY_CRAFTING_H
#include "item/ItemStack.h"
typedef struct { const char *key; bool translated; } InventoryDisplayName;
typedef struct InventoryCrafting InventoryCrafting;
typedef bool (*InventoryCraftingNotify)(MCObject *,InventoryCrafting *);
struct InventoryCrafting {
    MCObject object;
    ItemStackArray *stackList;
    int32_t inventoryWidth,inventoryHeight;
    MCObject *eventHandler;
    InventoryCraftingNotify onCraftMatrixChanged;
};
InventoryCrafting *InventoryCrafting_new(MCObjectHeap *,MCObject *,InventoryCraftingNotify,int32_t width,int32_t height);
int32_t InventoryCrafting_getSizeInventory(const InventoryCrafting *);
ItemStack *InventoryCrafting_getStackInSlot(InventoryCrafting *,int32_t);
ItemStack *InventoryCrafting_getStackInRowAndColumn(InventoryCrafting *,int32_t,int32_t);
const char *InventoryCrafting_getName(const InventoryCrafting *);
bool InventoryCrafting_hasCustomName(const InventoryCrafting *);
InventoryDisplayName InventoryCrafting_getDisplayName(const InventoryCrafting *);
ItemStack *InventoryCrafting_removeStackFromSlot(InventoryCrafting *,int32_t);
ItemStack *InventoryCrafting_decrStackSize(InventoryCrafting *,int32_t,int32_t);
bool InventoryCrafting_setInventorySlotContents(InventoryCrafting *,int32_t,ItemStack *);
int32_t InventoryCrafting_getInventoryStackLimit(const InventoryCrafting *);
void InventoryCrafting_markDirty(InventoryCrafting *);
bool InventoryCrafting_isUseableByPlayer(const InventoryCrafting *,const MCObject *);
void InventoryCrafting_openInventory(InventoryCrafting *,MCObject *);
void InventoryCrafting_closeInventory(InventoryCrafting *,MCObject *);
bool InventoryCrafting_isItemValidForSlot(const InventoryCrafting *,int32_t,const ItemStack *);
int32_t InventoryCrafting_getField(const InventoryCrafting *,int32_t);
void InventoryCrafting_setField(InventoryCrafting *,int32_t,int32_t);
int32_t InventoryCrafting_getFieldCount(const InventoryCrafting *);
void InventoryCrafting_clear(InventoryCrafting *);
int32_t InventoryCrafting_getHeight(const InventoryCrafting *);
int32_t InventoryCrafting_getWidth(const InventoryCrafting *);
/* Direct nullable refs, signed counts and callback order are original bodies.
   MCObject dispatch/heap failure and IChatComponent name tokens are adapters. */
#endif
