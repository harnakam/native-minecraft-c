#ifndef C919_SOURCE_ITEM_MAP_DATA_H
#define C919_SOURCE_ITEM_MAP_DATA_H
#include "util/MCGameplayWorld.h"
#include "world/storage/MapData.h"
/* Supplied ItemMap.getMapData body. A cache hit is the exact managed MapData;
   authoritative misses construct a distinct saved-data object. EXCEPTION is
   the reached healthy Source NULL/checkcast boundary with prior effects kept;
   malformed/foreign owners, missing native dependencies and OOM are FAILURE.
   Returned references are borrowed until collection/adoption. This does not
   translate the Item hierarchy or arbitrary World/ItemStack subclasses. */
WorldSavedDataResult ItemMap_getMapData(ItemStack *, World *, MapData **out);

/* Explicit legacy native-store view, retained until the coherent live map
   owner migration. Do not mix it with Source MapStorage for one map ID. */
mc_map_info *NativeItemMapData_getMapData(ItemStack *, MCGameplayWorld *);
int32_t NativeItemMapData_getUniqueDataId(MCGameplayWorld *);
mc_map_info *NativeItemMapData_setItemData(MCGameplayWorld *,
                                           const mc_map_info *);
void NativeItemMapData_calculateMapCenter(mc_map_info *, double x, double z,
                                          int32_t scale);
#endif
