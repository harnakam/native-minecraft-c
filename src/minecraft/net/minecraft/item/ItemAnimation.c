#include "item/ItemAnimation.h"

void Item_onUpdate(const Item *self, ItemStack *stack, MCObject *world, MCObject *entity,
                   int32_t inventorySlot, bool isSelected) {
    /* The original base method has no statements. Its arguments are unused;
       only the virtual caller checks the receiver and native lifetime. */
    (void)self;
    (void)stack;
    (void)world;
    (void)entity;
    (void)inventorySlot;
    (void)isSelected;
}
