#ifndef C919_SOURCE_SLOT_CRAFTING_H
#define C919_SOURCE_SLOT_CRAFTING_H
#include "inventory/crafting_dispatch.h"
typedef struct SlotCrafting {
    Slot slot;
    InventoryCrafting *craftMatrix;
    MCObject *thePlayer;
    int32_t amountCrafted;
    const mc_crafting_dispatch *dependencies;
} SlotCrafting;
/* Native instanceof boundary for the currently translated concrete class. */
bool SlotCrafting_isInstance(const Slot *);
SlotCrafting *SlotCrafting_new(MCObjectHeap *,MCObject *player,InventoryCrafting *,IInventory result,
    int32_t index,int32_t x,int32_t y,const mc_crafting_dispatch *);
bool SlotCrafting_isItemValid(const SlotCrafting *,const ItemStack *);
ItemStack *SlotCrafting_decrStackSize(SlotCrafting *,int32_t amount);
bool SlotCrafting_onCraftingAmount(SlotCrafting *,ItemStack *,int32_t amount);
bool SlotCrafting_onCrafting(SlotCrafting *,ItemStack *);
bool SlotCrafting_onPickupFromSlot(SlotCrafting *,MCObject *player,ItemStack *);
#endif
