#ifndef C919_SOURCE_ITEM_STACK_ANIMATION_H
#define C919_SOURCE_ITEM_STACK_ANIMATION_H
#include "item/ItemStack.h"

/* Immutable required virtual-method dispatch. context is a managed reference
   retained by the caller's graph; unknown Item overrides must fail explicitly.
   A false result models an exception and marks the current heap failed. */
typedef struct {
    bool (*onUpdate)(MCObject *context, const Item *self, ItemStack *, MCObject *world,
                     MCObject *entity, int32_t inventorySlot, bool isSelected);
} ItemStackAnimationDependencies;

/* Source updateAnimation decrements a positive animationsToGo before invoking
   Item.onUpdate, including when item is NULL or dispatch fails. It does not
   roll back partial source mutations. NULL World/Entity arguments are allowed;
   non-NULL managed arguments must belong to the receiver's current heap. */
bool ItemStack_updateAnimation(ItemStack *, MCObject *world, MCObject *entity,
                               int32_t inventorySlot, bool isSelected,
                               const ItemStackAnimationDependencies *, MCObject *context);
#endif
