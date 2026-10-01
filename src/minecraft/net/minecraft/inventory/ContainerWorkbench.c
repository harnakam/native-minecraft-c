#include "inventory/ContainerWorkbench.h"
#include "inventory/inventory_dispatch.h"
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    ContainerWorkbench *container=(ContainerWorkbench *)object;
    Container_trace(&container->container,visitor,context);
    container->craftMatrix=(InventoryCrafting *)visitor((MCObject *)container->craftMatrix,context);
    container->craftResult=(InventoryCraftResult *)visitor((MCObject *)container->craftResult,context);
    container->worldObj=visitor(container->worldObj,context);
}
static bool matrix(Container *base,IInventory inventory) { return ContainerWorkbench_onCraftMatrixChanged((ContainerWorkbench *)base,inventory); }
static bool close_container(Container *base,InventoryPlayer *player) { return ContainerWorkbench_onContainerClosed((ContainerWorkbench *)base,player); }
static bool interact(Container *base,InventoryPlayer *player) { return ContainerWorkbench_canInteractWith((ContainerWorkbench *)base,player); }
static ItemStack *transfer(Container *base,InventoryPlayer *player,int32_t index) { return ContainerWorkbench_transferStackInSlot((ContainerWorkbench *)base,player,index); }
static bool merge(Container *base,const ItemStack *stack,const Slot *slot) { return ContainerWorkbench_canMergeSlot((ContainerWorkbench *)base,stack,slot); }
static bool notify(MCObject *object,InventoryCrafting *inventory) { return Container_onCraftMatrixChanged((Container *)object,mc_IInventory_crafting(inventory)); }
static const ContainerOverrides overrides={.transferStackInSlot=transfer,.canMergeSlot=merge,.canInteractWith=interact,.onCraftMatrixChanged=matrix,.onContainerClosed=close_container};
static const MCObjectClass klass={"net.minecraft.inventory.ContainerWorkbench",MCObjectHeap_plainClone,trace,NULL};
bool ContainerWorkbench_isInstance(const MCObject *object) { return object && object->klass==&klass; }
ContainerWorkbench *ContainerWorkbench_new(InventoryPlayer *inventory,MCObject *world,const mc_crafting_position *pos,const mc_crafting_dispatch *dependencies) {
    MCObjectHeap *heap=inventory ? inventory->object.heap : NULL;
    if (!heap || !world || world->heap!=heap || !dependencies || !dependencies->findMatchingRecipe || !dependencies->isRemote ||
        !dependencies->isCraftingTable || !dependencies->getDistanceSq || !dependencies->drop) {
        MCObjectHeap_fail(heap); return NULL;
    }
    ContainerWorkbench *container=(ContainerWorkbench *)MCObjectHeap_alloc(heap,sizeof(*container),&klass);
    if (!container || !Container_construct(&container->container,&overrides,dependencies->drop)) return NULL;
    container->craftMatrix=InventoryCrafting_new(heap,(MCObject *)container,notify,3,3);
    container->craftResult=InventoryCraftResult_new(heap);
    if (!container->craftMatrix || !container->craftResult) return NULL;
    container->worldObj=world; container->hasPosition=pos!=NULL; if (pos) container->pos=*pos;
    container->dependencies=dependencies;
    SlotCrafting *output=SlotCrafting_new(heap,inventory->player,container->craftMatrix,mc_IInventory_result(container->craftResult),0,124,35,dependencies);
    if (!output || !Container_addSlotToContainer(&container->container,&output->slot)) return NULL;
    for (int32_t i=0;i<3;i++) for (int32_t j=0;j<3;j++) {
        Slot *slot=Slot_new(heap,mc_IInventory_crafting(container->craftMatrix),j+i*3,30+j*18,17+i*18);
        if (!slot || !Container_addSlotToContainer(&container->container,slot)) return NULL;
    }
    for (int32_t k=0;k<3;k++) for (int32_t j=0;j<9;j++) {
        Slot *slot=Slot_new(heap,mc_IInventory_player(inventory),j+k*9+9,8+j*18,84+k*18);
        if (!slot || !Container_addSlotToContainer(&container->container,slot)) return NULL;
    }
    for (int32_t i=0;i<9;i++) {
        Slot *slot=Slot_new(heap,mc_IInventory_player(inventory),i,8+i*18,142);
        if (!slot || !Container_addSlotToContainer(&container->container,slot)) return NULL;
    }
    if (!ContainerWorkbench_onCraftMatrixChanged(container,mc_IInventory_crafting(container->craftMatrix))) return NULL;
    return container;
}
bool ContainerWorkbench_onCraftMatrixChanged(ContainerWorkbench *container,IInventory inventory) {
    (void)inventory;
    ItemStack *result=container->dependencies->findMatchingRecipe(container->craftMatrix,container->worldObj);
    return !MCObjectHeap_failed(container->container.object.heap) && InventoryCraftResult_setInventorySlotContents(container->craftResult,0,result);
}
bool ContainerWorkbench_onContainerClosed(ContainerWorkbench *container,InventoryPlayer *player) {
    if (!Container_onContainerClosedBase(&container->container,player)) return false;
    if (!container->dependencies->isRemote(container->worldObj)) for (int32_t i=0;i<9;i++) {
        ItemStack *stack=InventoryCrafting_removeStackFromSlot(container->craftMatrix,i);
        if (MCObjectHeap_failed(container->container.object.heap)) return false;
        if (stack && !container->dependencies->drop(player->player,stack,false)) {
            MCObjectHeap_fail(container->container.object.heap); return false;
        }
    }
    return true;
}
bool ContainerWorkbench_canInteractWith(ContainerWorkbench *container,InventoryPlayer *player) {
    if (!container->hasPosition || !player || !player->player) { MCObjectHeap_fail(container->container.object.heap); return false; }
    const mc_crafting_position *pos=&container->pos;
    return container->dependencies->isCraftingTable(container->worldObj,pos->x,pos->y,pos->z) &&
        container->dependencies->getDistanceSq(player->player,(double)pos->x+0.5,(double)pos->y+0.5,(double)pos->z+0.5)<=64.0;
}
ItemStack *ContainerWorkbench_transferStackInSlot(ContainerWorkbench *container,InventoryPlayer *player,int32_t index) {
    Container *base=&container->container; MCObjectHeap *heap=base->object.heap; ItemStack *result=NULL;
    Slot *slot=Container_getSlot(base,index);
    if (MCObjectHeap_failed(heap)) return NULL;
    if (slot && Slot_getHasStack(slot)) {
        ItemStack *stack=Slot_getStack(slot); result=ItemStack_copy(heap,stack);
        if (!result) return NULL;
        if (index==0) {
            if (!Container_mergeItemStack(base,stack,10,46,true)) return NULL;
            Slot_onSlotChange(slot,stack,result);
        } else if (index>=10 && index<37) {
            if (!Container_mergeItemStack(base,stack,37,46,false)) return NULL;
        } else if (index>=37 && index<46) {
            if (!Container_mergeItemStack(base,stack,10,37,false)) return NULL;
        } else if (!Container_mergeItemStack(base,stack,10,46,false)) return NULL;
        if (MCObjectHeap_failed(heap)) return NULL;
        if (stack->stackSize==0) { if (!Slot_putStack(slot,NULL)) return NULL; }
        else Slot_onSlotChanged(slot);
        if (stack->stackSize==result->stackSize) return NULL;
        if (!Slot_onPickupFromSlot(slot,player->player,stack)) { MCObjectHeap_fail(heap); return NULL; }
    }
    return result;
}
bool ContainerWorkbench_canMergeSlot(ContainerWorkbench *container,const ItemStack *stack,const Slot *slot) {
    (void)stack; return slot->inventory.instance!=(MCObject *)container->craftResult;
}
