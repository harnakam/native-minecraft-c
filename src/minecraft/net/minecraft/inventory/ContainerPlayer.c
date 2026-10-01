#include "inventory/ContainerPlayer.h"
#include "inventory/inventory_dispatch.h"
typedef struct { Slot slot; int32_t armorType; const mc_crafting_dispatch *dependencies; } ArmorSlot;
static const char *const empty_slot_names[]={
    "minecraft:items/empty_armor_slot_helmet","minecraft:items/empty_armor_slot_chestplate",
    "minecraft:items/empty_armor_slot_leggings","minecraft:items/empty_armor_slot_boots"
};
static int32_t armor_limit(const Slot *slot) { (void)slot; return 1; }
static bool armor_valid(const Slot *base,const ItemStack *stack) {
    const ArmorSlot *slot=(const ArmorSlot *)base;
    if (!stack) return false;
    const Item *item=ItemStack_getItem(stack);
    int32_t armor=slot->dependencies->armorType(item);
    return armor>=0 ? armor==slot->armorType :
        (item==ItemStack_registryItem(86) || item==ItemStack_registryItem(397)) && slot->armorType==0;
}
static const char *armor_texture(const Slot *base) { return empty_slot_names[((const ArmorSlot *)base)->armorType]; }
static const SlotOverrides armor_overrides={.isItemValid=armor_valid,.getSlotStackLimit=armor_limit,.getSlotTexture=armor_texture};
static void armor_trace(MCObject *object,MCObjectVisitor visitor,void *context) { Slot_trace((Slot *)object,visitor,context); }
static const MCObjectClass armor_class={"net.minecraft.inventory.ContainerPlayer$1",MCObjectHeap_plainClone,armor_trace,NULL};
static ArmorSlot *armor_new(InventoryPlayer *inventory,int32_t armor,const mc_crafting_dispatch *dependencies) {
    ArmorSlot *slot=(ArmorSlot *)MCObjectHeap_alloc(inventory->object.heap,sizeof(*slot),&armor_class);
    if (!slot || !Slot_construct(&slot->slot,mc_IInventory_player(inventory),InventoryPlayer_getSizeInventory(inventory)-1-armor,8,8+armor*18,&armor_overrides)) return NULL;
    slot->armorType=armor; slot->dependencies=dependencies; return slot;
}
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    ContainerPlayer *container=(ContainerPlayer *)object;
    Container_trace(&container->container,visitor,context);
    container->craftMatrix=(InventoryCrafting *)visitor((MCObject *)container->craftMatrix,context);
    container->craftResult=(InventoryCraftResult *)visitor((MCObject *)container->craftResult,context);
    container->thePlayer=visitor(container->thePlayer,context);
}
static bool matrix(Container *base,IInventory inventory) { return ContainerPlayer_onCraftMatrixChanged((ContainerPlayer *)base,inventory); }
static bool close_container(Container *base,InventoryPlayer *player) { return ContainerPlayer_onContainerClosed((ContainerPlayer *)base,player); }
static bool interact(Container *base,InventoryPlayer *player) { return ContainerPlayer_canInteractWith((ContainerPlayer *)base,player); }
static ItemStack *transfer(Container *base,InventoryPlayer *player,int32_t index) { return ContainerPlayer_transferStackInSlot((ContainerPlayer *)base,player,index); }
static bool merge(Container *base,const ItemStack *stack,const Slot *slot) { return ContainerPlayer_canMergeSlot((ContainerPlayer *)base,stack,slot); }
static bool notify(MCObject *object,InventoryCrafting *inventory) { return Container_onCraftMatrixChanged((Container *)object,mc_IInventory_crafting(inventory)); }
static const ContainerOverrides overrides={.transferStackInSlot=transfer,.canMergeSlot=merge,.canInteractWith=interact,.onCraftMatrixChanged=matrix,.onContainerClosed=close_container};
static const MCObjectClass klass={"net.minecraft.inventory.ContainerPlayer",MCObjectHeap_plainClone,trace,NULL};
ContainerPlayer *ContainerPlayer_new(InventoryPlayer *inventory,bool local,MCObject *player,const mc_crafting_dispatch *dependencies) {
    MCObjectHeap *heap=inventory ? inventory->object.heap : NULL;
    if (!heap || !player || player->heap!=heap || !dependencies || !dependencies->findMatchingRecipe || !dependencies->world || !dependencies->armorType || !dependencies->drop) {
        MCObjectHeap_fail(heap); return NULL;
    }
    ContainerPlayer *container=(ContainerPlayer *)MCObjectHeap_alloc(heap,sizeof(*container),&klass);
    if (!container || !Container_construct(&container->container,&overrides,dependencies->drop)) return NULL;
    container->craftMatrix=InventoryCrafting_new(heap,(MCObject *)container,notify,2,2);
    container->craftResult=InventoryCraftResult_new(heap);
    if (!container->craftMatrix || !container->craftResult) return NULL;
    container->isLocalWorld=local; container->thePlayer=player; container->dependencies=dependencies;
    SlotCrafting *output=SlotCrafting_new(heap,inventory->player,container->craftMatrix,mc_IInventory_result(container->craftResult),0,144,36,dependencies);
    if (!output || !Container_addSlotToContainer(&container->container,&output->slot)) return NULL;
    for (int32_t i=0;i<2;i++) for (int32_t j=0;j<2;j++) {
        Slot *slot=Slot_new(heap,mc_IInventory_crafting(container->craftMatrix),j+i*2,88+j*18,26+i*18);
        if (!slot || !Container_addSlotToContainer(&container->container,slot)) return NULL;
    }
    for (int32_t k=0;k<4;k++) {
        ArmorSlot *slot=armor_new(inventory,k,dependencies);
        if (!slot || !Container_addSlotToContainer(&container->container,&slot->slot)) return NULL;
    }
    for (int32_t l=0;l<3;l++) for (int32_t j=0;j<9;j++) {
        Slot *slot=Slot_new(heap,mc_IInventory_player(inventory),j+(l+1)*9,8+j*18,84+l*18);
        if (!slot || !Container_addSlotToContainer(&container->container,slot)) return NULL;
    }
    for (int32_t i=0;i<9;i++) {
        Slot *slot=Slot_new(heap,mc_IInventory_player(inventory),i,8+i*18,142);
        if (!slot || !Container_addSlotToContainer(&container->container,slot)) return NULL;
    }
    if (!ContainerPlayer_onCraftMatrixChanged(container,mc_IInventory_crafting(container->craftMatrix))) return NULL;
    return container;
}
bool ContainerPlayer_onCraftMatrixChanged(ContainerPlayer *container,IInventory inventory) {
    (void)inventory;
    MCObjectHeap *heap=container->container.object.heap;
    MCObject *world=container->dependencies->world(container->thePlayer);
    if (!world || world->heap!=heap) { MCObjectHeap_fail(heap); return false; }
    ItemStack *result=container->dependencies->findMatchingRecipe(container->craftMatrix,world);
    return !MCObjectHeap_failed(heap) && InventoryCraftResult_setInventorySlotContents(container->craftResult,0,result);
}
bool ContainerPlayer_onContainerClosed(ContainerPlayer *container,InventoryPlayer *player) {
    if (!Container_onContainerClosedBase(&container->container,player)) return false;
    for (int32_t i=0;i<4;i++) {
        ItemStack *stack=InventoryCrafting_removeStackFromSlot(container->craftMatrix,i);
        if (MCObjectHeap_failed(container->container.object.heap)) return false;
        if (stack && !container->dependencies->drop(player->player,stack,false)) {
            MCObjectHeap_fail(container->container.object.heap); return false;
        }
    }
    return InventoryCraftResult_setInventorySlotContents(container->craftResult,0,NULL);
}
bool ContainerPlayer_canInteractWith(ContainerPlayer *container,InventoryPlayer *player) { (void)container; (void)player; return true; }
ItemStack *ContainerPlayer_transferStackInSlot(ContainerPlayer *container,InventoryPlayer *player,int32_t index) {
    Container *base=&container->container; MCObjectHeap *heap=base->object.heap; ItemStack *result=NULL;
    Slot *slot=Container_getSlot(base,index);
    if (MCObjectHeap_failed(heap)) return NULL;
    if (slot && Slot_getHasStack(slot)) {
        ItemStack *stack=Slot_getStack(slot); result=ItemStack_copy(heap,stack);
        if (!result) return NULL;
        if (index==0) {
            if (!Container_mergeItemStack(base,stack,9,45,true)) return NULL;
            Slot_onSlotChange(slot,stack,result);
        } else if (index>=1 && index<5) {
            if (!Container_mergeItemStack(base,stack,9,45,false)) return NULL;
        } else if (index>=5 && index<9) {
            if (!Container_mergeItemStack(base,stack,9,45,false)) return NULL;
        } else {
            int32_t armor=container->dependencies->armorType(ItemStack_getItem(result));
            if (armor>=0 && !Slot_getHasStack(Container_getSlot(base,5+armor))) {
                if (!Container_mergeItemStack(base,stack,5+armor,6+armor,false)) return NULL;
            } else if (index>=9 && index<36) {
                if (!Container_mergeItemStack(base,stack,36,45,false)) return NULL;
            } else if (index>=36 && index<45) {
                if (!Container_mergeItemStack(base,stack,9,36,false)) return NULL;
            } else if (!Container_mergeItemStack(base,stack,9,45,false)) return NULL;
        }
        if (MCObjectHeap_failed(heap)) return NULL;
        if (stack->stackSize==0) { if (!Slot_putStack(slot,NULL)) return NULL; }
        else Slot_onSlotChanged(slot);
        if (stack->stackSize==result->stackSize) return NULL;
        if (!Slot_onPickupFromSlot(slot,player->player,stack)) { MCObjectHeap_fail(heap); return NULL; }
    }
    return result;
}
bool ContainerPlayer_canMergeSlot(ContainerPlayer *container,const ItemStack *stack,const Slot *slot) {
    (void)stack; return slot->inventory.instance!=(MCObject *)container->craftResult;
}
