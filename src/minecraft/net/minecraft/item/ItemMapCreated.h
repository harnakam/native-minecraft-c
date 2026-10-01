#ifndef C919_ITEM_MAP_CREATED_H
#define C919_ITEM_MAP_CREATED_H
#include "item/ItemMapData.h"
/* Original ItemMap.onCreated body over the canonical stack and native World
   owner. The map store/World methods remain explicit native dependencies.
   The unused original player parameter is retained. Counter narrowing and
   replacement use the same dependencies as ItemMap.getMapData. */
bool ItemMap_onCreated(ItemStack *, MCGameplayWorld *, MCObject *player);
#endif
