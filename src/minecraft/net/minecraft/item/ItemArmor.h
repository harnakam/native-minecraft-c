#ifndef C919_SOURCE_ITEM_ARMOR_H
#define C919_SOURCE_ITEM_ARMOR_H
#include "item/ItemStack.h"

typedef enum {
    ITEMARMOR_NONE, ITEMARMOR_LEATHER, ITEMARMOR_CHAIN,
    ITEMARMOR_IRON, ITEMARMOR_GOLD, ITEMARMOR_DIAMOND
} ItemArmorMaterial;
/* Canonical Item identity/subclass/material metadata is an immutable registry
   adapter. These are the original color method bodies, not the equipment,
   dispenser, repair, attribute or constructor dependencies of ItemArmor. */
bool ItemArmor_isInstance(const Item *item);
ItemArmorMaterial ItemArmor_getArmorMaterial(const Item *item);
bool ItemArmor_hasColor(MCObjectHeap *,const Item *,ItemStack *);
int32_t ItemArmor_getColor(MCObjectHeap *,const Item *,ItemStack *);
bool ItemArmor_removeColor(MCObjectHeap *,const Item *,ItemStack *);
bool ItemArmor_setColor(MCObjectHeap *,const Item *,ItemStack *,int32_t color);
int32_t ItemArmor_getColorFromItemStack(MCObjectHeap *,const Item *,ItemStack *,int32_t renderPass);
#endif
