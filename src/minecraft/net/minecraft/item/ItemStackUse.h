#ifndef C919_SOURCE_ITEM_STACK_USE_H
#define C919_SOURCE_ITEM_STACK_USE_H
#include "item/ItemStack.h"
typedef struct {
    bool (*onItemRightClick)(MCObject *context, const Item *self, ItemStack *, MCObject *world,
                             MCObject *player, ItemStack **out);
} ItemStackUseDependencies;
/* Original one-method ItemStack body. Actual Item subclass dispatch is required;
   it may return the same object, a different object, or NULL. This does not port
   the rest of Item use/combat, PlayerController rightclick or GUI behavior. */
bool ItemStack_useItemRightClick(ItemStack *, MCObject *world, MCObject *player,
                                 const ItemStackUseDependencies *, MCObject *context,
                                 ItemStack **out);
#endif
