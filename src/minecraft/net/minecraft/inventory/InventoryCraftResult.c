#include "InventoryCraftResult.h"
#include <string.h>
void InventoryCraftResult_init(InventoryCraftResult *inventory) {
    memset(inventory,0,sizeof(*inventory)); mc_slot_init(&inventory->ownedResult);
    inventory->stackResult=&inventory->ownedResult; inventory->ownsStackResult=true;
}
void InventoryCraftResult_attach(InventoryCraftResult *inventory,mc_slot *storage) {
    InventoryCraftResult_init(inventory); inventory->stackResult=storage; inventory->ownsStackResult=false;
}
void InventoryCraftResult_free(InventoryCraftResult *inventory) {
    if (inventory->ownsStackResult) mc_slot_free(&inventory->ownedResult);
    memset(inventory,0,sizeof(*inventory));
}
int InventoryCraftResult_getSizeInventory(const InventoryCraftResult *inventory) { (void)inventory; return 1; }
mc_slot *InventoryCraftResult_getStackInSlot(InventoryCraftResult *inventory,int index) {
    (void)index; return inventory->stackResult->item_id>=0 ? inventory->stackResult : NULL;
}
const char *InventoryCraftResult_getName(const InventoryCraftResult *inventory) { (void)inventory; return "Result"; }
bool InventoryCraftResult_hasCustomName(const InventoryCraftResult *inventory) { (void)inventory; return false; }
InventoryDisplayName InventoryCraftResult_getDisplayName(const InventoryCraftResult *inventory) {
    return (InventoryDisplayName){InventoryCraftResult_getName(inventory),!InventoryCraftResult_hasCustomName(inventory)};
}
bool InventoryCraftResult_removeStackFromSlot(InventoryCraftResult *inventory,int index,mc_slot *removed) {
    (void)index; if (!removed || removed==inventory->stackResult) return false;
    mc_slot_free(removed); *removed=*inventory->stackResult; mc_slot_init(inventory->stackResult); return true;
}
bool InventoryCraftResult_decrStackSize(InventoryCraftResult *inventory,int index,int count,mc_slot *removed) {
    (void)count; return InventoryCraftResult_removeStackFromSlot(inventory,index,removed);
}
bool InventoryCraftResult_setInventorySlotContents(InventoryCraftResult *inventory,int index,const mc_slot *stack) {
    (void)index;
    if (stack) return mc_slot_copy(inventory->stackResult,stack);
    mc_slot_free(inventory->stackResult); return true;
}
int InventoryCraftResult_getInventoryStackLimit(const InventoryCraftResult *inventory) { (void)inventory; return 64; }
/* The source intentionally has no dirty/viewer/field behavior for this slot. */
void InventoryCraftResult_markDirty(InventoryCraftResult *inventory) { (void)inventory; }
bool InventoryCraftResult_isUseableByPlayer(const InventoryCraftResult *inventory,const void *player) { (void)inventory; (void)player; return true; }
void InventoryCraftResult_openInventory(InventoryCraftResult *inventory,const void *player) { (void)inventory; (void)player; }
void InventoryCraftResult_closeInventory(InventoryCraftResult *inventory,const void *player) { (void)inventory; (void)player; }
bool InventoryCraftResult_isItemValidForSlot(const InventoryCraftResult *inventory,int index,const mc_slot *stack) { (void)inventory; (void)index; (void)stack; return true; }
int InventoryCraftResult_getField(const InventoryCraftResult *inventory,int id) { (void)inventory; (void)id; return 0; }
void InventoryCraftResult_setField(InventoryCraftResult *inventory,int id,int value) { (void)inventory; (void)id; (void)value; }
int InventoryCraftResult_getFieldCount(const InventoryCraftResult *inventory) { (void)inventory; return 0; }
void InventoryCraftResult_clear(InventoryCraftResult *inventory) { mc_slot_free(inventory->stackResult); }
