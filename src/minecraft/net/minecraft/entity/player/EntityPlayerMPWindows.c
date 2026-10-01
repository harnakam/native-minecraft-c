#include "entity/player/EntityPlayerMPWindows.h"
#include "inventory/SlotCrafting.h"

static bool fail(MCGameplayPlayer *player) {
    MCObjectHeap_fail(player ? player->living.entity.object.heap : NULL); return false;
}
static bool active(MCGameplayPlayer *player) {
    return player && !MCObjectHeap_failed(player->living.entity.object.heap);
}
static bool dependencies_ready(const EntityPlayerMPWindowsDependencies *d) {
    return d && d->sendWindowItems && d->sendSetSlot && d->sendCloseWindow;
}
static bool bound(MCGameplayPlayer *player) {
    return active(player) && (dependencies_ready(player->windowDependencies) || fail(player));
}
static bool effect(MCGameplayPlayer *player,bool result) {
    return result ? active(player) : fail(player);
}
static bool same_heap(MCGameplayPlayer *player,const MCObject *object) {
    return object && object->heap==player->living.entity.object.heap;
}
bool EntityPlayerMPWindows_bind(MCGameplayPlayer *player,const EntityPlayerMPWindowsDependencies *dependencies) {
    if (!active(player)) return false;
    if (!dependencies_ready(dependencies)) return fail(player);
    player->windowDependencies=dependencies; MCObjectHeap_touch(player->living.entity.object.heap); return true;
}
static bool listener_update(MCObject *target,Container *container,ContainerList *items) {
    return EntityPlayerMPWindows_updateCraftingInventory((MCGameplayPlayer *)target,container,items);
}
static bool listener_slot(MCObject *target,Container *container,int32_t index,ItemStack *stack) {
    return EntityPlayerMPWindows_sendSlotContents((MCGameplayPlayer *)target,container,index,stack);
}
static const ICraftingMethods listener_methods={listener_update,listener_slot};
ICrafting EntityPlayerMPWindows_listener(MCGameplayPlayer *player) {
    if (!bound(player)) return (ICrafting){0};
    return (ICrafting){(MCObject *)player,&listener_methods};
}
bool EntityPlayerMPWindows_sendSlotContents(MCGameplayPlayer *player,Container *container,int32_t index,ItemStack *stack) {
    if (!active(player)) return false;
    if (!same_heap(player,(MCObject *)container) || (stack && !same_heap(player,(MCObject *)stack))) return fail(player);
    Slot *slot=Container_getSlot(container,index);
    if (!active(player)) return false;
    if (!SlotCrafting_isInstance(slot) && !player->isChangingQuantityOnly) {
        if (!bound(player)) return false;
        return effect(player,player->windowDependencies->sendSetSlot(player,container->windowId,index,stack));
    }
    return active(player);
}
bool EntityPlayerMPWindows_updateCraftingInventory(MCGameplayPlayer *player,Container *container,ContainerList *items) {
    if (!bound(player)) return false;
    if (!same_heap(player,(MCObject *)container) || !same_heap(player,(MCObject *)items) ||
        !same_heap(player,(MCObject *)player->inventory)) return fail(player);
    if (!effect(player,player->windowDependencies->sendWindowItems(player,container->windowId,items))) return false;
    return effect(player,player->windowDependencies->sendSetSlot(player,-1,-1,InventoryPlayer_getItemStack(player->inventory)));
}
bool EntityPlayerMPWindows_sendContainerToPlayer(MCGameplayPlayer *player,Container *container) {
    if (!active(player)) return false;
    if (!same_heap(player,(MCObject *)container)) return fail(player);
    ContainerList *items=Container_getInventory(container);
    if (!items || !active(player)) return false;
    return EntityPlayerMPWindows_updateCraftingInventory(player,container,items);
}
bool EntityPlayerMPWindows_updateHeldItem(MCGameplayPlayer *player) {
    if (!active(player)) return false;
    if (!player->isChangingQuantityOnly) {
        if (!bound(player)) return false;
        if (!same_heap(player,(MCObject *)player->inventory)) return fail(player);
        return effect(player,player->windowDependencies->sendSetSlot(player,-1,-1,InventoryPlayer_getItemStack(player->inventory)));
    }
    return true;
}
bool EntityPlayerMPWindows_closeContainer(MCGameplayPlayer *player) {
    if (!active(player)) return false;
    if (!same_heap(player,(MCObject *)player->openContainer) || !same_heap(player,(MCObject *)player->inventory) ||
        !same_heap(player,(MCObject *)((ContainerPlayer *)(player->inventoryContainer)))) return fail(player);
    if (!effect(player,Container_onContainerClosed(player->openContainer,player->inventory))) return false;
    player->openContainer=player->inventoryContainer; MCObjectHeap_touch(player->living.entity.object.heap); return true;
}
bool EntityPlayerMPWindows_closeScreen(MCGameplayPlayer *player) {
    if (!bound(player)) return false;
    if (!same_heap(player,(MCObject *)player->openContainer)) return fail(player);
    if (!effect(player,player->windowDependencies->sendCloseWindow(player,player->openContainer->windowId))) return false;
    return EntityPlayerMPWindows_closeContainer(player);
}
