#include "item/ItemStackCrafting.h"

bool ItemStack_onCrafting(ItemStack *stack,MCObject *world,MCObject *player,
    int32_t amount,const ItemStackCraftingDispatch *dependencies) {
    if (!stack) return false;
    MCObjectHeap *heap=stack->object.heap;
    if (!world || world->heap!=heap || !player || player->heap!=heap ||
        !dependencies || !dependencies->addCraftStat || !dependencies->onCreated) {
        MCObjectHeap_fail(heap); return false;
    }
    MCObjectRootScope scope={0};
    if (!MCObjectRootScope_begin(&scope,heap)) return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)stack) &&
        MCObjectRootScope_pin(&scope,world) && MCObjectRootScope_pin(&scope,player);
    if (ok) ok=dependencies->addCraftStat(player,ItemStack_getItem(stack),amount);
    if (ok && !MCObjectHeap_failed(heap)) ok=dependencies->onCreated(stack,world,player);
    if (!ok) MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);
    return ok && !MCObjectHeap_failed(heap);
}
