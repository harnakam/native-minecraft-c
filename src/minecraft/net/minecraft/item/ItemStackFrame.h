#ifndef C919_SOURCE_ITEM_STACK_FRAME_H
#define C919_SOURCE_ITEM_STACK_FRAME_H
#include "item/ItemStack.h"
bool ItemStack_isOnItemFrame(ItemStack *);
bool ItemStack_setItemFrame(ItemStack *, EntityItemFrame *nullableFrame);
EntityItemFrame *ItemStack_getItemFrame(ItemStack *);
/* Original three direct-field methods with native same-heap/type/lifetime
   validation. The existing ItemStack trace owns this actual reference. */
#endif
