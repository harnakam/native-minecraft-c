#ifndef C919_SOURCE_ITEM_ANIMATION_H
#define C919_SOURCE_ITEM_ANIMATION_H
#include "item/ItemStack.h"

/* Original Item.onUpdate base body. It is actually empty; this is not a
   fallback for an unknown or untranslated Item override. The canonical Item
   registry remains an immutable native identity adapter, not the full class. */
void Item_onUpdate(const Item *, ItemStack *, MCObject *world, MCObject *entity,
                   int32_t inventorySlot, bool isSelected);
#endif
