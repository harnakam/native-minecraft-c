#ifndef C919_BLOCK_H
#define C919_BLOCK_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { float min_x, min_y, min_z, max_x, max_y, max_z; } mc_box;
bool mc_block_valid(uint16_t state);
bool mc_block_replaceable(uint16_t state);
bool mc_block_opaque(uint16_t state);
bool mc_block_placeable_item(int16_t item_id);
/* Up to three boxes for simple legacy shapes; fences/connections are resolved
   by the world-aware caller. Fluids, plants, rails and air have no collision. */
unsigned mc_block_collision(uint16_t state, mc_box boxes[3]);
/* Native immutable material identity/property facts for supported registry IDs. */
bool mc_block_material_flags(uint16_t state,bool *movement,bool *leaves);
#endif
