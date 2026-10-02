#ifndef C919_NATIVE_MAP_DATA_H
#define C919_NATIVE_MAP_DATA_H
#include "inventory/inventory.h"
#include "util/MCObjectHeap.h"

typedef struct World MCGameplayWorld;
typedef struct MCGameplayPlayer MCGameplayPlayer;
typedef struct ItemStack ItemStack;
typedef struct mc_maps mc_maps;

#define MC_MAP_MAX_VIEWERS 64u
#define MC_MAP_DECORATION_KEY 128u
typedef struct mc_map_info mc_map_info;
typedef struct mc_MapData_tracking mc_MapData_tracking;
typedef struct {
    int32_t entity_id;
    const char *name;
    const mc_inventory *inventory;
    double x,z,yaw;
    int dimension;
    bool alive;
} mc_map_player;
typedef struct {
    int32_t entity_id;
    uint32_t update_counter,packet_counter;
    bool dirty;
    uint8_t min_x,min_z,max_x,max_z;
} mc_MapInfo;
/* MapInfo is transient, as in Java. Working-store copies retain its counters
   and dirty rectangles; it is not included in standard MapData NBT. */
bool mc_MapData_copy_tracking(mc_map_info *destination,const mc_map_info *source);
void mc_MapData_free_tracking(mc_map_info *map);
mc_MapInfo *mc_MapData_getMapInfo(mc_map_info *map,int32_t entity_id);
bool mc_MapData_updateVisiblePlayers(mc_map_info *map,const mc_slot *stack,
    const mc_map_player *viewer,const mc_map_player *players,size_t count,int64_t world_time);
bool mc_MapData_updateDecorations(mc_map_info *map,int type,const char *identifier,
    double x,double z,double rotation,int64_t world_time);
void mc_MapData_updateMapData(mc_map_info *map,unsigned x,unsigned z);
/* 1 packet, 0 no update, -1 failure. Failed encoding retains dirty state. */
int mc_MapData_getMapPacket(mc_map_info *map,const mc_slot *stack,int32_t entity_id,mc_buf *packet);

/* Source-reference entry points over a borrowed native MapData store view.
   owner retains the view in owner->maps; caller releases it before any store
   replacement/adoption. Players and UTF-16 decoration keys are strong managed
   references, not serialized inventory copies. Lookup preserves the source
   Entity.equals/hashCode ID equality and HashMap's cached key hash.
   World.trace must invoke NativeMapData_traceReferences after its native deep copy.
   Native terrain/color buffers, bounds and S34 encoding remain adapters.
   ItemFrame dependencies are not ported: a non-NULL frame fails the heap.
   A failed operation retains source-ordered partial mutations; abort the whole
   working graph. Successful transient changes require adoption even when no
   persistent pixels/metadata changed. Do not mix both tracking APIs on one map. */
void NativeMapData_traceReferences(mc_maps *,MCObjectVisitor,void *context);
mc_MapInfo *NativeMapData_getMapInfo(mc_map_info *,MCGameplayWorld *owner,MCGameplayPlayer *);
bool NativeMapData_updateVisiblePlayers(mc_map_info *,MCGameplayWorld *owner,
    MCGameplayPlayer *,ItemStack *);
/* Native packet result: 1 packet, 0 original NULL, -1 failed dependency/heap.
   S34 encoder failure is atomic at this native boundary and retains dirtiness. */
int NativeMapData_getMapPacket(mc_map_info *,ItemStack *,MCGameplayWorld *,
    MCGameplayPlayer *,mc_buf *);
#endif
