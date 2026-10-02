#ifndef C919_ITEM_MAP_CREATED_H
#define C919_ITEM_MAP_CREATED_H
#include "item/ItemMapData.h"
/* Supplied ItemMap.onCreated body over actual saved-data references. The
   original player argument is unused, including NULL/foreign references.
   A newly constructed map replaces the cache ref only after field/dirty work;
   old map/color aliases remain distinct and alive under their existing roots.
   Healthy Source exceptions retain ID/stack/constructor prefixes. */
WorldSavedDataResult ItemMap_onCreated(ItemStack *, World *,
                                       MCObject *unusedPlayer);
/* Explicit old live native view, not a managed MapData constructor port. */
bool NativeItemMap_onCreated(ItemStack *, MCGameplayWorld *,
                             MCObject *unusedPlayer);
#endif
