#include "inventory/SlotCrafting.h"
#include <limits.h>
static int32_t add(int32_t a,int32_t b) {
    uint32_t value=(uint32_t)a+(uint32_t)b;
    return value<=INT32_MAX ? (int32_t)value : -1-(int32_t)(UINT32_MAX-value);
}
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    SlotCrafting *slot=(SlotCrafting *)object;
    Slot_trace(&slot->slot,visitor,context);
    slot->craftMatrix=(InventoryCrafting *)visitor((MCObject *)slot->craftMatrix,context);
    slot->thePlayer=visitor(slot->thePlayer,context);
}
static bool valid(const Slot *slot,const ItemStack *stack) { return SlotCrafting_isItemValid((const SlotCrafting *)slot,stack); }
static ItemStack *decr(Slot *slot,int32_t amount) { return SlotCrafting_decrStackSize((SlotCrafting *)slot,amount); }
static void craft_amount(Slot *slot,ItemStack *stack,int32_t amount) { (void)SlotCrafting_onCraftingAmount((SlotCrafting *)slot,stack,amount); }
static void craft(Slot *slot,ItemStack *stack) { (void)SlotCrafting_onCrafting((SlotCrafting *)slot,stack); }
static bool pickup(Slot *slot,MCObject *player,ItemStack *stack) { return SlotCrafting_onPickupFromSlot((SlotCrafting *)slot,player,stack); }
static const SlotOverrides overrides={
    .onCraftingAmount=craft_amount,.onCrafting=craft,.onPickupFromSlot=pickup,
    .isItemValid=valid,.decrStackSize=decr
};
static const MCObjectClass klass={"net.minecraft.inventory.SlotCrafting",MCObjectHeap_plainClone,trace,NULL};
bool SlotCrafting_isInstance(const Slot *slot) { return slot && slot->object.klass==&klass; }
SlotCrafting *SlotCrafting_new(MCObjectHeap *heap,MCObject *player,InventoryCrafting *matrix,IInventory result,
    int32_t index,int32_t x,int32_t y,const mc_crafting_dispatch *dependencies) {
    if (!player || player->heap!=heap || !matrix || matrix->object.heap!=heap || !dependencies ||
        !dependencies->inventory || !dependencies->world || !dependencies->getRemainingItems ||
        !dependencies->onCrafting || !dependencies->triggerAchievement || !dependencies->drop ||
        !dependencies->isPickaxe || !dependencies->isHoe || !dependencies->isSword || !dependencies->isWoodPickaxe) {
        MCObjectHeap_fail(heap); return NULL;
    }
    SlotCrafting *slot=(SlotCrafting *)MCObjectHeap_alloc(heap,sizeof(*slot),&klass);
    if (!slot || !Slot_construct(&slot->slot,result,index,x,y,&overrides)) return NULL;
    slot->thePlayer=player; slot->craftMatrix=matrix; slot->dependencies=dependencies; return slot;
}
bool SlotCrafting_isItemValid(const SlotCrafting *slot,const ItemStack *stack) { (void)slot; (void)stack; return false; }
ItemStack *SlotCrafting_decrStackSize(SlotCrafting *slot,int32_t amount) {
    if (Slot_getHasStack(&slot->slot)) {
        int32_t count=Slot_getStack(&slot->slot)->stackSize;
        slot->amountCrafted=add(slot->amountCrafted,amount<count ? amount : count);
        MCObjectHeap_touch(slot->slot.object.heap);
    }
    return Slot_decrStackSizeBase(&slot->slot,amount);
}
bool SlotCrafting_onCraftingAmount(SlotCrafting *slot,ItemStack *stack,int32_t amount) {
    slot->amountCrafted=add(slot->amountCrafted,amount); MCObjectHeap_touch(slot->slot.object.heap);
    return SlotCrafting_onCrafting(slot,stack);
}
static bool achievement(SlotCrafting *slot,mc_crafting_achievement value) {
    if (slot->dependencies->triggerAchievement(slot->thePlayer,value)) return true;
    MCObjectHeap_fail(slot->slot.object.heap); return false;
}
bool SlotCrafting_onCrafting(SlotCrafting *slot,ItemStack *stack) {
    MCObjectHeap *heap=slot->slot.object.heap;
    if (!stack || stack->object.heap!=heap) { MCObjectHeap_fail(heap); return false; }
    if (slot->amountCrafted>0) {
        MCObject *world=slot->dependencies->world(slot->thePlayer);
        if (!world || world->heap!=heap || !slot->dependencies->onCrafting(stack,world,slot->thePlayer,slot->amountCrafted)) {
            MCObjectHeap_fail(heap); return false;
        }
    }
    slot->amountCrafted=0; MCObjectHeap_touch(heap);
    const Item *item=ItemStack_getItem(stack);
    if (item==ItemStack_registryItem(58) && !achievement(slot,MC_ACH_BUILD_WORKBENCH)) return false;
    if (slot->dependencies->isPickaxe(item) && !achievement(slot,MC_ACH_BUILD_PICKAXE)) return false;
    if (item==ItemStack_registryItem(61) && !achievement(slot,MC_ACH_BUILD_FURNACE)) return false;
    if (slot->dependencies->isHoe(item) && !achievement(slot,MC_ACH_BUILD_HOE)) return false;
    if (item==ItemStack_registryItem(297) && !achievement(slot,MC_ACH_MAKE_BREAD)) return false;
    if (item==ItemStack_registryItem(354) && !achievement(slot,MC_ACH_BAKE_CAKE)) return false;
    if (slot->dependencies->isPickaxe(item) && !slot->dependencies->isWoodPickaxe(item) && !achievement(slot,MC_ACH_BUILD_BETTER_PICKAXE)) return false;
    if (slot->dependencies->isSword(item) && !achievement(slot,MC_ACH_BUILD_SWORD)) return false;
    if (item==ItemStack_registryItem(116) && !achievement(slot,MC_ACH_ENCHANTMENTS)) return false;
    if (item==ItemStack_registryItem(47) && !achievement(slot,MC_ACH_BOOKCASE)) return false;
    if (item==ItemStack_registryItem(322) && ItemStack_getMetadata(stack)==1 && !achievement(slot,MC_ACH_OVERPOWERED)) return false;
    return !MCObjectHeap_failed(heap);
}
bool SlotCrafting_onPickupFromSlot(SlotCrafting *slot,MCObject *player,ItemStack *stack) {
    MCObjectHeap *heap=slot->slot.object.heap;
    if (!player || player->heap!=heap) { MCObjectHeap_fail(heap); return false; }
    if (!SlotCrafting_onCrafting(slot,stack)) return false;
    MCObject *world=slot->dependencies->world(player);
    if (!world || world->heap!=heap) { MCObjectHeap_fail(heap); return false; }
    ItemStackArray *remaining=slot->dependencies->getRemainingItems(slot->craftMatrix,world);
    if (!remaining || remaining->object.heap!=heap) { MCObjectHeap_fail(heap); return false; }
    for (int32_t i=0;i<remaining->length;i++) {
        ItemStack *input=InventoryCrafting_getStackInSlot(slot->craftMatrix,i), *remainder=remaining->items[i];
        if (input) (void)InventoryCrafting_decrStackSize(slot->craftMatrix,i,1);
        if (MCObjectHeap_failed(heap)) return false;
        if (remainder) {
            if (!InventoryCrafting_getStackInSlot(slot->craftMatrix,i)) {
                if (!InventoryCrafting_setInventorySlotContents(slot->craftMatrix,i,remainder)) return false;
            } else {
                InventoryPlayer *inventory=slot->dependencies->inventory(slot->thePlayer);
                if (!inventory || inventory->object.heap!=heap) { MCObjectHeap_fail(heap); return false; }
                if (!InventoryPlayer_addItemStackToInventory(inventory,remainder)) {
                    if (MCObjectHeap_failed(heap)) return false;
                    if (!slot->dependencies->drop(slot->thePlayer,remainder,false)) { MCObjectHeap_fail(heap); return false; }
                }
            }
        }
    }
    return !MCObjectHeap_failed(heap);
}
