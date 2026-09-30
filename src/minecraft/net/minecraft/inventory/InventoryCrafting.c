#include "InventoryCrafting.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static bool dimensions(int width,int height,int *size) {
    /* Java's int multiplication wraps before new ItemStack[size]. */
    int32_t length=(int32_t)((uint32_t)width*(uint32_t)height);
    if (length<0 || (size_t)length>SIZE_MAX/sizeof(mc_slot)) return false;
    *size=length; return true;
}
bool InventoryCrafting_attach(InventoryCrafting *inventory,mc_slot *storage,void *handler,InventoryCraftingNotify notify,int width,int height) {
    int size;
    if (!inventory || !dimensions(width,height,&size) || (size && !storage)) return false;
    memset(inventory,0,sizeof(*inventory)); inventory->stackList=storage;
    inventory->inventoryWidth=width; inventory->inventoryHeight=height; inventory->size=size;
    inventory->eventHandler=handler; inventory->onCraftMatrixChanged=notify; return true;
}
bool InventoryCrafting_init(InventoryCrafting *inventory,void *handler,InventoryCraftingNotify notify,int width,int height) {
    int size; if (!inventory || !dimensions(width,height,&size)) return false;
    mc_slot *storage=size ? calloc((size_t)size,sizeof(*storage)) : NULL;
    if (size && !storage) return false;
    for (int i=0;i<size;i++) mc_slot_init(&storage[i]);
    if (!InventoryCrafting_attach(inventory,storage,handler,notify,width,height)) { free(storage); return false; }
    inventory->ownsStackList=true; return true;
}
void InventoryCrafting_free(InventoryCrafting *inventory) {
    if (inventory->ownsStackList) {
        for (int i=0;i<inventory->size;i++) mc_slot_free(&inventory->stackList[i]);
        free(inventory->stackList);
    }
    memset(inventory,0,sizeof(*inventory));
}
static mc_slot *address(InventoryCrafting *inventory,int index) {
    if (index<0 || index>=inventory->size) { inventory->failed=true; return NULL; }
    return &inventory->stackList[index];
}
static bool notify_changed(InventoryCrafting *inventory) {
    bool okay=inventory->onCraftMatrixChanged && inventory->onCraftMatrixChanged(inventory->eventHandler,inventory);
    if (!okay) inventory->failed=true;
    return okay;
}
int InventoryCrafting_getSizeInventory(const InventoryCrafting *inventory) { return inventory->size; }
mc_slot *InventoryCrafting_getStackInSlot(InventoryCrafting *inventory,int index) {
    if (index>=inventory->size) return NULL;
    mc_slot *slot=address(inventory,index); return slot && slot->item_id>=0 ? slot : NULL;
}
mc_slot *InventoryCrafting_getStackInRowAndColumn(InventoryCrafting *inventory,int row,int column) {
    /* Preserve the original inclusive height comparison. The resulting index
       at column==height is rejected by getStackInSlot's upper-bound check. */
    if (row<0 || row>=inventory->inventoryWidth || column<0 || column>inventory->inventoryHeight) return NULL;
    int32_t index=(int32_t)((uint32_t)row+(uint32_t)column*(uint32_t)inventory->inventoryWidth);
    return InventoryCrafting_getStackInSlot(inventory,index);
}
const char *InventoryCrafting_getName(const InventoryCrafting *inventory) { (void)inventory; return "container.crafting"; }
bool InventoryCrafting_hasCustomName(const InventoryCrafting *inventory) { (void)inventory; return false; }
InventoryDisplayName InventoryCrafting_getDisplayName(const InventoryCrafting *inventory) {
    return (InventoryDisplayName){InventoryCrafting_getName(inventory),!InventoryCrafting_hasCustomName(inventory)};
}
bool InventoryCrafting_removeStackFromSlot(InventoryCrafting *inventory,int index,mc_slot *removed) {
    mc_slot *slot=address(inventory,index); if (!slot || !removed || removed==slot) return false;
    for (int i=0;i<inventory->size;i++) if (removed==&inventory->stackList[i]) return false;
    mc_slot_free(removed); *removed=*slot; mc_slot_init(slot); return true;
}
bool InventoryCrafting_decrStackSize(InventoryCrafting *inventory,int index,int count,mc_slot *removed) {
    mc_slot *slot=address(inventory,index); if (!slot || !removed || removed==slot || count<0) return false;
    for (int i=0;i<inventory->size;i++) if (removed==&inventory->stackList[i]) return false;
    if (slot->item_id<0) { mc_slot_free(removed); return true; }
    if (slot->count<=count) {
        if (!InventoryCrafting_removeStackFromSlot(inventory,index,removed)) return false;
    } else {
        mc_slot part; mc_slot_init(&part);
        if (!mc_slot_copy(&part,slot)) { inventory->failed=true; return false; }
        part.count=(uint8_t)count; slot->count=(uint8_t)(slot->count-count);
        mc_slot_free(removed); *removed=part;
        if (!slot->count) mc_slot_free(slot);
    }
    return notify_changed(inventory);
}
bool InventoryCrafting_setInventorySlotContents(InventoryCrafting *inventory,int index,const mc_slot *stack) {
    mc_slot *slot=address(inventory,index); if (!slot) return false;
    mc_slot replacement; mc_slot_init(&replacement);
    if (stack && !mc_slot_copy(&replacement,stack)) { inventory->failed=true; return false; }
    mc_slot_free(slot); *slot=replacement; return notify_changed(inventory);
}
int InventoryCrafting_getInventoryStackLimit(const InventoryCrafting *inventory) { (void)inventory; return 64; }
/* These methods are intentionally no-ops in the Java source. Crafting grids
   have no tile dirty state, viewer bookkeeping, or synchronized integer fields. */
void InventoryCrafting_markDirty(InventoryCrafting *inventory) { (void)inventory; }
bool InventoryCrafting_isUseableByPlayer(const InventoryCrafting *inventory,const void *player) { (void)inventory; (void)player; return true; }
void InventoryCrafting_openInventory(InventoryCrafting *inventory,const void *player) { (void)inventory; (void)player; }
void InventoryCrafting_closeInventory(InventoryCrafting *inventory,const void *player) { (void)inventory; (void)player; }
bool InventoryCrafting_isItemValidForSlot(const InventoryCrafting *inventory,int index,const mc_slot *stack) { (void)inventory; (void)index; (void)stack; return true; }
int InventoryCrafting_getField(const InventoryCrafting *inventory,int id) { (void)inventory; (void)id; return 0; }
void InventoryCrafting_setField(InventoryCrafting *inventory,int id,int value) { (void)inventory; (void)id; (void)value; }
int InventoryCrafting_getFieldCount(const InventoryCrafting *inventory) { (void)inventory; return 0; }
void InventoryCrafting_clear(InventoryCrafting *inventory) {
    for (int i=0;i<inventory->size;i++) mc_slot_free(&inventory->stackList[i]);
}
int InventoryCrafting_getHeight(const InventoryCrafting *inventory) { return inventory->inventoryHeight; }
int InventoryCrafting_getWidth(const InventoryCrafting *inventory) { return inventory->inventoryWidth; }
