#ifndef C919_SOURCE_ITEM_MAP_LOAD_H
#define C919_SOURCE_ITEM_MAP_LOAD_H
#include "world/storage/MapData.h"

/* Supplied ItemMap.loadMapData static body on the real World saved-data store.
   The returned MapData is the exact borrowed managed cache/new-object reference;
   no native map-color/value mirror is consulted. Healthy EXCEPTION denotes the
   reached Source checkcast/NULL boundary, native ownership/OOM is FAILURE.
   World NULL supplies no native heap for preceding String/class allocation;
   this ownerless invocation is explicitly a bounded native NULL boundary. */
WorldSavedDataResult ItemMap_loadMapData(int32_t mapId, World *worldIn, MapData **out);
#endif
