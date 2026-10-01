#ifndef C919_ITEM_STACK_CRAFTING_H
#define C919_ITEM_STACK_CRAFTING_H
#include "item/ItemStack.h"

/* Dependencies of the original two-operation ItemStack.onCrafting body.
   addCraftStat resolves StatList.objectCraftStats for this Item and executes
   the player's actual addStat implementation, including its null-stat case.
   onCreated dispatches the current Item subclass method. Both are required;
   a failure aborts the working heap, rather than claiming the side effect ran. */
typedef struct ItemStackCraftingDispatch {
    bool (*addCraftStat)(MCObject *player,const Item *item,int32_t amount);
    bool (*onCreated)(ItemStack *stack,MCObject *world,MCObject *player);
} ItemStackCraftingDispatch;
bool ItemStack_onCrafting(ItemStack *stack,MCObject *world,MCObject *player,
    int32_t amount,const ItemStackCraftingDispatch *dependencies);
#endif
