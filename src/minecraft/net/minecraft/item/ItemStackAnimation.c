#include "item/ItemStackAnimation.h"

bool ItemStack_updateAnimation(ItemStack *stack, MCObject *world, MCObject *entity,
                               int32_t inventorySlot, bool isSelected,
                               const ItemStackAnimationDependencies *dependencies,
                               MCObject *context) {
    MCObjectHeap *heap = stack ? stack->object.heap : NULL;
    MCObjectRootScope scope = {0};
    if (!ItemStack_isInstance((MCObject *)stack) ||
        !MCObjectRootScope_begin(&scope, heap)) {
        MCObjectHeap_fail(heap);
        return false;
    }
    bool ok = MCObjectRootScope_pin(&scope, (MCObject *)stack) &&
              MCObjectRootScope_pin(&scope, world) && MCObjectRootScope_pin(&scope, entity) &&
              MCObjectRootScope_pin(&scope, context);
    if (ok) {
        if (stack->animationsToGo > 0)
            --stack->animationsToGo;

        /* Preserve Java's dereference/exception order: a NULL Item or missing
           virtual implementation fails only after the animation decrement. */
        ok = stack->item && dependencies && dependencies->onUpdate;
        if (ok)
            ok = dependencies->onUpdate(context, stack->item, stack, world, entity,
                                          inventorySlot, isSelected);
        ok = ok && !MCObjectHeap_failed(heap);
    }
    if (!ok)
        MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);
    return ok;
}
