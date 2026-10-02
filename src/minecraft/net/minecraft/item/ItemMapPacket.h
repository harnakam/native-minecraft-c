#ifndef C919_SOURCE_ITEM_MAP_PACKET_H
#define C919_SOURCE_ITEM_MAP_PACKET_H
#include "item/ItemMapData.h"
#include "network/play/server/S34PacketMaps.h"
/* Original ItemMap packet factory over the actual saved-data receiver. A
   missing authoritative map can allocate an ID and mutate the stack before
   returning NULL for an unregistered viewer. A missing remote map reaches the
   Source NULL receiver exception. No native packet/value-store projection is
   substituted here; full Item subclass dispatch remains a separate port. */
WorldSavedDataResult ItemMap_createMapDataPacket(ItemStack *,World *,MCGameplayPlayer *,
    S34PacketMaps **out);
#endif
