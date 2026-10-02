#ifndef C919_MAP_H
#define C919_MAP_H
#include "inventory/inventory.h"
#include "world/NativeWorld.h"
#include "world/storage/NativeMapData.h"

#define MC_MAP_SIDE 128u
#define MC_MAP_PIXELS (MC_MAP_SIDE * MC_MAP_SIDE)
#define MC_MAX_MAPS 96u
#define MC_MAX_MAP_ICONS 256u
typedef struct { uint8_t type, direction; int8_t x, z; } mc_map_icon;
struct mc_map_info {
    int32_t id, center_x, center_z;
    int8_t dimension;
    uint8_t scale;
    /* S34 supplies colors, scale and icons, but no center or dimension. */
    bool metadata_known;
    bool dirty; /* Native storage for WorldSavedData's transient dirty state. */
    uint8_t colors[MC_MAP_PIXELS];
    mc_map_icon icons[MC_MAX_MAP_ICONS];
    size_t icon_count;
    mc_nbt original_nbt;
    mc_nbt original_entry_nbt; /* Unknown fields on the combined store entry. */
    mc_MapData_tracking *tracking;
};
typedef struct mc_maps {
    mc_map_info *entries;
    size_t count, capacity;
    int32_t next_id;
    /* Unsigned 16-bit next-counter bits for source MapStorage allocation.
       Legacy mc_slot allocation still enforces its own nonnegative short cap. */
    mc_nbt original_nbt;
} mc_maps;

void mc_map_info_init(mc_map_info *map);
void mc_map_info_free(mc_map_info *map);
bool mc_map_info_copy(mc_map_info *destination, const mc_map_info *source);
bool mc_map_info_valid(const mc_map_info *map);
void mc_maps_init(mc_maps *maps);
void mc_maps_free(mc_maps *maps);
bool mc_maps_copy(mc_maps *destination, const mc_maps *source);
mc_map_info *mc_maps_find(mc_maps *maps, int32_t id);
const mc_map_info *mc_maps_find_const(const mc_maps *maps, int32_t id);
bool mc_maps_add(mc_maps *maps, const mc_map_info *map);
/* Align a map's center to the vanilla scale grid; finite int32 centers only. */
bool mc_map_center(double x, double z, uint8_t scale, int32_t *center_x, int32_t *center_z);
/* Authoritative only. Missing filled-map data allocates a scale-3 map at the
   world spawn and changes stack damage. Allocation stops at the legacy Slot
   nonnegative short limit instead of silently truncating an ID. */
bool mc_maps_resolve(mc_maps *maps, mc_slot *filled_map, int32_t spawn_x, int32_t spawn_z, int dimension);
bool mc_maps_create(mc_maps *maps, mc_slot *output, double x, double z, int dimension, uint8_t scale);
/* Pure scale 0..3 preview: copies the source's NBT and adds map_is_scaling=1.
   Unknown client-only S34 data cannot authorize a recipe. */
bool mc_maps_scale_preview(const mc_maps *maps, const mc_slot *source, mc_slot *output);
/* Authoritative only. A scaling marker allocates a new blank map, aligns the
   old center at the next scale, and changes damage. The marker and other item
   NBT survive, as they do in 1.8.9. Unmarked maps are unchanged. */
bool mc_maps_on_crafted(mc_maps *maps, mc_slot *item, int32_t spawn_x, int32_t spawn_z, int dimension);

/* Standard MapData NBT compound body (dimension/xCenter/zCenter/scale/width/
   height/colors). Unknown fields and named-root encoding survive. Persisting
   the combined Version1/NextId/Maps store is the caller's atomic transaction.
   All initialized owning outputs are unchanged on failure; 2 MiB total cap. */
bool mc_map_info_encode(const mc_map_info *map, mc_nbt *output);
bool mc_map_info_decode(int32_t id, const mc_nbt *input, mc_map_info *output);
bool mc_maps_encode(const mc_maps *maps, mc_nbt *output);
bool mc_maps_decode(const mc_nbt *input, mc_maps *output);
/* Protocol47 S34 packet, ID included. Rectangles are row-major; width=0 is
   an icons/scale-only update. Receiver takes payload after packet ID and must
   consume it exactly; it never allocates authoritative map IDs. */
bool mc_map_packet(const mc_map_info *map, unsigned x, unsigned z, unsigned width, unsigned height, mc_buf *output);
bool mc_maps_receive(mc_maps *maps, mc_buf *payload);
/* Survey loaded overworld terrain near a holder, one stripe per tick. Missing
   chunks preserve unexplored colors. Uses legacy top-color/height/water-depth
   shading; unsupported dimensions do not manufacture terrain. */
bool mc_map_update_terrain(mc_map_info *map, const mc_world *world, double player_x, double player_z, int dimension, uint32_t tick, bool *changed);
/* Convert a legacy palette pixel to RGB; unknown palette IDs fail. */
bool mc_map_pixel_rgb(uint8_t pixel, uint8_t rgb[3]);
#endif
