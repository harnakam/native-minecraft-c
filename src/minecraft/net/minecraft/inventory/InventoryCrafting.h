#ifndef C919_INVENTORY_CRAFTING_H
#define C919_INVENTORY_CRAFTING_H
#include "inventory.h"

/* First source-method port of InventoryCrafting. ItemStack remains the legacy
   mc_slot value adapter: Java nullable object identity, aliased remainders and
   signed int stackSize are not yet ported (negative split counts are rejected).
   Attached storage belongs to the existing player/container; no duplicate
   crafting grid is created. C failure returns replace Java exceptions, and
   DisplayName is a translation token until IChatComponent is ported. */
typedef struct { const char *key; bool translated; } InventoryDisplayName;
typedef struct InventoryCrafting InventoryCrafting;
typedef bool (*InventoryCraftingNotify)(void *eventHandler, InventoryCrafting *inventory);
struct InventoryCrafting {
    mc_slot *stackList;
    int inventoryWidth, inventoryHeight, size;
    void *eventHandler;
    InventoryCraftingNotify onCraftMatrixChanged;
    bool ownsStackList, failed;
};
bool InventoryCrafting_init(InventoryCrafting *inventory, void *eventHandler,
    InventoryCraftingNotify notify, int width, int height);
bool InventoryCrafting_attach(InventoryCrafting *inventory, mc_slot *storage,
    void *eventHandler, InventoryCraftingNotify notify, int width, int height);
void InventoryCrafting_free(InventoryCrafting *inventory);
int InventoryCrafting_getSizeInventory(const InventoryCrafting *inventory);
mc_slot *InventoryCrafting_getStackInSlot(InventoryCrafting *inventory, int index);
mc_slot *InventoryCrafting_getStackInRowAndColumn(InventoryCrafting *inventory, int row, int column);
const char *InventoryCrafting_getName(const InventoryCrafting *inventory);
bool InventoryCrafting_hasCustomName(const InventoryCrafting *inventory);
InventoryDisplayName InventoryCrafting_getDisplayName(const InventoryCrafting *inventory);
bool InventoryCrafting_removeStackFromSlot(InventoryCrafting *inventory, int index, mc_slot *removed);
/* Mutation precedes the callback exactly as in Java. A callback failure leaves
   this working view mutated; the caller's outer transaction must roll back. */
bool InventoryCrafting_decrStackSize(InventoryCrafting *inventory, int index, int count, mc_slot *removed);
bool InventoryCrafting_setInventorySlotContents(InventoryCrafting *inventory, int index, const mc_slot *stack);
int InventoryCrafting_getInventoryStackLimit(const InventoryCrafting *inventory);
void InventoryCrafting_markDirty(InventoryCrafting *inventory);
bool InventoryCrafting_isUseableByPlayer(const InventoryCrafting *inventory, const void *player);
void InventoryCrafting_openInventory(InventoryCrafting *inventory, const void *player);
void InventoryCrafting_closeInventory(InventoryCrafting *inventory, const void *player);
bool InventoryCrafting_isItemValidForSlot(const InventoryCrafting *inventory, int index, const mc_slot *stack);
int InventoryCrafting_getField(const InventoryCrafting *inventory, int id);
void InventoryCrafting_setField(InventoryCrafting *inventory, int id, int value);
int InventoryCrafting_getFieldCount(const InventoryCrafting *inventory);
void InventoryCrafting_clear(InventoryCrafting *inventory);
int InventoryCrafting_getHeight(const InventoryCrafting *inventory);
int InventoryCrafting_getWidth(const InventoryCrafting *inventory);
#endif
