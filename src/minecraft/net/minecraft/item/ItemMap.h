#ifndef C919_ITEM_MAP_H
#define C919_ITEM_MAP_H
#include "world/map.h"
#include "item/ItemMapData.h"
#include "item/ItemMapPacket.h"
typedef struct MCGameplayPlayer MCGameplayPlayer;
/* Legacy native value-store implementation. Its dense terrain/color survey
   and tracking owner remain explicit adapters until the live owner migration.
   Actual Source saved-data methods are declared by ItemMapData/ItemMapLoad. */
mc_map_info *mc_ItemMap_getMapData(mc_maps *maps,mc_slot *stack,bool remote,
    int32_t spawn_x,int32_t spawn_z,int dimension);
bool mc_ItemMap_updateMapData(mc_map_info *map,const mc_world *world,int32_t viewer_id,
    double x,double z,int dimension,bool no_sky,bool *changed);
bool mc_ItemMap_onUpdate(mc_maps *maps,const mc_world *world,mc_slot *stack,
    const mc_map_player *viewer,const mc_map_player *players,size_t count,bool selected,
    int32_t spawn_x,int32_t spawn_z,bool no_sky,int64_t world_time,bool *changed);
/* changed reports persistent colors, created metadata or stack ID mutation;
   transient tracking must still be retained when it is false. */
int mc_ItemMap_createMapDataPacket(mc_maps *maps,const mc_slot *stack,int32_t viewer_id,mc_buf *packet);
/* Original method includes getMapData: missing authoritative data may allocate
   and mutate the stack even when no viewer MapInfo exists to emit a packet. */
int mc_ItemMap_createMapDataPacket_at(mc_maps *maps,mc_slot *stack,int32_t viewer_id,
    int32_t spawn_x,int32_t spawn_z,int dimension,mc_buf *packet);
/* Compatibility adapter with an explicit phase; also used for narrow survey
   fixtures. Server gameplay uses the per-viewer counter above. */
bool mc_ItemMap_survey(mc_map_info *map,const mc_world *world,double x,double z,
    int dimension,bool no_sky,uint32_t phase,bool *changed);

/* Legacy native reference entry points retain exact ItemStack references but
   still store map state in the native value owner.
   The native World/actor inherited fields supply isRemote, dimension/isDead,
   time/noSky and terrain. Other ItemMap methods/full inheritance are not claimed.
   Entity arguments follow the original instanceof EntityPlayer branches.
   changed excludes transient MapInfo/decorations; retain the entire graph even
   when false. Failure means abort the complete working graph. */
bool NativeItemMap_updateMapData(MCGameplayWorld *,MCObject *viewer,mc_map_info *,bool *changed);
bool NativeItemMap_onUpdate(ItemStack *,MCGameplayWorld *,MCObject *entity,int32_t itemSlot,
    bool isSelected,bool *changed);
int NativeItemMap_createMapDataPacket(ItemStack *,MCGameplayWorld *,MCGameplayPlayer *,mc_buf *);
#endif
