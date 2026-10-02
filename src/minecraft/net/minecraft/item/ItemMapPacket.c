#include "item/ItemMapPacket.h"
WorldSavedDataResult ItemMap_createMapDataPacket(ItemStack *stack,World *world,
    MCGameplayPlayer *player,S34PacketMaps **out) {
    MapData *map=NULL;
    WorldSavedDataResult result=ItemMap_getMapData(stack,world,&map);
    if(result!=WORLD_SAVED_DATA_OK)return result;
    return MapData_getMapPacket(map,stack,world,player,out);
}
