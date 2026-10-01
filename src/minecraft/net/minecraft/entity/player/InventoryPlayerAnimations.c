#include "entity/player/InventoryPlayerAnimations.h"

static bool valid_array(ItemStackArray *array, MCObjectRootScope *scope) {
    if (!array || !MCObjectRootScope_pin(scope, (MCObject *)array) ||
        !ItemStackArray_isInstance((MCObject *)array) || array->length < 0)
        return false;
    size_t bytes = MCObjectHeap_objectSize((MCObject *)array);
    return bytes >= sizeof(*array) &&
           (size_t)array->length <= (bytes - sizeof(*array)) / sizeof(*array->items);
}

bool InventoryPlayer_decrementAnimations(InventoryPlayer *inventory,
    const InventoryPlayerAnimationDependencies *dependencies, MCObject *context) {
    MCObjectHeap *heap = inventory ? inventory->object.heap : NULL;
    MCObjectRootScope scope = {0};
    if (!InventoryPlayer_isInstance((MCObject *)inventory) ||
        !MCObjectRootScope_begin(&scope, heap)) {
        MCObjectHeap_fail(heap);
        return false;
    }
    bool ok = MCObjectRootScope_pin(&scope, (MCObject *)inventory) &&
              MCObjectRootScope_pin(&scope, context);
    for (int32_t i = 0; ok; ++i) {
        /* Source reads the array and its length at every loop condition. Item
           callbacks can replace either, so do not retain a traversal snapshot. */
        ItemStackArray *array = inventory->mainInventory;
        if (!valid_array(array, &scope)) {
            ok = false;
            break;
        }
        if (i >= array->length)
            break;
        ItemStack *stack = array->items[i];
        if (!stack)
            continue;

        /* Java evaluates the ItemStack call receiver before the arguments,
           then player.worldObj, player, index and currentItem in that order. */
        MCObject *worldOwner = inventory->player;
        ok = MCObjectRootScope_pin(&scope, (MCObject *)stack) &&
             ItemStack_isInstance((MCObject *)stack) && worldOwner &&
             MCObjectRootScope_pin(&scope, worldOwner) && dependencies && dependencies->getWorld;
        if (!ok)
            break;
        MCObject *world = dependencies->getWorld(worldOwner);
        if (MCObjectHeap_failed(heap) || !MCObjectRootScope_pin(&scope, world)) {
            ok = false;
            break;
        }
        MCObject *entity = inventory->player;
        bool selected = inventory->currentItem == i;
        ok = ItemStack_updateAnimation(stack, world, entity, i, selected,
                                        dependencies->item, context);
    }
    if (!ok)
        MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);
    return ok;
}
