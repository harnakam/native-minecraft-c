#include "inventory/Slot.h"
#include "item/ItemStack.h"
#include <limits.h>
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    Slot_trace((Slot *)object,visitor,context);
}
static const MCObjectClass slot_class={"net.minecraft.inventory.Slot",MCObjectHeap_plainClone,trace,NULL};
bool Slot_construct(Slot *slot,IInventory inventory,int32_t index,int32_t x,int32_t y,
                    const SlotOverrides *overrides) {
    if (!slot || !inventory.instance || inventory.instance->heap!=slot->object.heap ||
        !inventory.methods || !inventory.methods->getStackInSlot ||
        !inventory.methods->setInventorySlotContents || !inventory.methods->decrStackSize ||
        !inventory.methods->markDirty || !inventory.methods->getInventoryStackLimit) {
        if (slot) MCObjectHeap_fail(slot->object.heap);
        return false;
    }
    slot->inventory=inventory; slot->slotIndex=index;
    slot->xDisplayPosition=x; slot->yDisplayPosition=y; slot->overrides=overrides;
    MCObjectHeap_touch(slot->object.heap); return true;
}
Slot *Slot_new(MCObjectHeap *heap,IInventory inventory,int32_t index,int32_t x,int32_t y) {
    Slot *slot=(Slot *)MCObjectHeap_alloc(heap,sizeof(Slot),&slot_class);
    return slot && Slot_construct(slot,inventory,index,x,y,NULL) ? slot : NULL;
}
void Slot_trace(Slot *slot,MCObjectVisitor visitor,void *context) {
    slot->inventory.instance=visitor(slot->inventory.instance,context);
}
static int32_t java_difference(int32_t a,int32_t b) {
    uint32_t value=(uint32_t)a-(uint32_t)b;
    return value<=INT32_MAX ? (int32_t)value : -1-(int32_t)(UINT32_MAX-value);
}
void Slot_onSlotChange(Slot *slot,ItemStack *before,ItemStack *after) {
    if (before && after && ItemStack_getItem(before)==ItemStack_getItem(after)) {
        int32_t increase=java_difference(after->stackSize,before->stackSize);
        if (increase>0) Slot_onCraftingAmount(slot,before,increase);
    }
}
void Slot_onCraftingAmount(Slot *slot,ItemStack *stack,int32_t amount) {
    if (slot->overrides && slot->overrides->onCraftingAmount)
        slot->overrides->onCraftingAmount(slot,stack,amount);
    /* Original protected base body is empty. */
}
void Slot_onCrafting(Slot *slot,ItemStack *stack) {
    if (slot->overrides && slot->overrides->onCrafting)
        slot->overrides->onCrafting(slot,stack);
    /* Original protected base body is empty. */
}
bool Slot_onPickupFromSlot(Slot *slot,MCObject *player,ItemStack *stack) {
    if (slot->overrides && slot->overrides->onPickupFromSlot) {
        if (!slot->overrides->onPickupFromSlot(slot,player,stack))
            MCObjectHeap_fail(slot->object.heap);
        return !MCObjectHeap_failed(slot->object.heap);
    }
    Slot_onSlotChanged(slot); return !MCObjectHeap_failed(slot->object.heap);
}
bool Slot_isItemValid(const Slot *slot,const ItemStack *stack) {
    return slot->overrides && slot->overrides->isItemValid ?
        slot->overrides->isItemValid(slot,stack) : true;
}
ItemStack *Slot_getStack(Slot *slot) {
    return slot->inventory.methods->getStackInSlot(slot->inventory.instance,slot->slotIndex);
}
bool Slot_getHasStack(Slot *slot) { return Slot_getStack(slot)!=NULL; }
bool Slot_putStack(Slot *slot,ItemStack *stack) {
    if (!slot->inventory.methods->setInventorySlotContents(slot->inventory.instance,slot->slotIndex,stack)) {
        MCObjectHeap_fail(slot->object.heap); return false;
    }
    Slot_onSlotChanged(slot); return !MCObjectHeap_failed(slot->object.heap);
}
void Slot_onSlotChanged(Slot *slot) { slot->inventory.methods->markDirty(slot->inventory.instance); }
int32_t Slot_getSlotStackLimit(const Slot *slot) {
    return slot->overrides && slot->overrides->getSlotStackLimit ?
        slot->overrides->getSlotStackLimit(slot) :
        slot->inventory.methods->getInventoryStackLimit(slot->inventory.instance);
}
int32_t Slot_getItemStackLimit(const Slot *slot,const ItemStack *stack) {
    return slot->overrides && slot->overrides->getItemStackLimit ?
        slot->overrides->getItemStackLimit(slot,stack) : Slot_getSlotStackLimit(slot);
}
const char *Slot_getSlotTexture(const Slot *slot) {
    return slot->overrides && slot->overrides->getSlotTexture ?
        slot->overrides->getSlotTexture(slot) : NULL;
}
ItemStack *Slot_decrStackSizeBase(Slot *slot,int32_t count) {
    return slot->inventory.methods->decrStackSize(slot->inventory.instance,slot->slotIndex,count);
}
ItemStack *Slot_decrStackSize(Slot *slot,int32_t count) {
    return slot->overrides && slot->overrides->decrStackSize ?
        slot->overrides->decrStackSize(slot,count) :
        Slot_decrStackSizeBase(slot,count);
}
bool Slot_isHere(const Slot *slot,IInventory inventory,int32_t index) {
    return slot->inventory.instance==inventory.instance && slot->slotIndex==index;
}
bool Slot_canTakeStack(const Slot *slot,const MCObject *player) {
    return slot->overrides && slot->overrides->canTakeStack ?
        slot->overrides->canTakeStack(slot,player) : true;
}
bool Slot_canBeHovered(const Slot *slot) {
    return slot->overrides && slot->overrides->canBeHovered ?
        slot->overrides->canBeHovered(slot) : true;
}
