#ifndef C919_SOURCE_ITEM_MAP_DATA_H
#define C919_SOURCE_ITEM_MAP_DATA_H
#include "util/MCGameplayWorld.h"
/* Translated ItemMap.getMapData body over the actual native World map owner.
   A returned MapData store view is borrowed until its next native allocation,
   replacement or graph adoption. The store remains a bounded native adapter, not a claim
   that MapStorage/WorldSavedData/MapData object identity is fully translated. */
mc_map_info *ItemMap_getMapData(ItemStack *,MCGameplayWorld *);
/* Reusable native dependencies for MapStorage.getUniqueDataId("map") and
   World.setItemData. Counter state belongs to the actual Source storage provider; collisions are
   replaced rather than skipped. No ItemStack/slot value conversion occurs.
   Call these borrowed native dependencies under the actor's RootScope. */
int32_t ItemMapData_getUniqueDataId(MCGameplayWorld *);
mc_map_info *ItemMapData_nativeSetItemData(MCGameplayWorld *,const mc_map_info *);
void ItemMapData_calculateMapCenter(mc_map_info *,double x,double z,int32_t scale);
#endif
