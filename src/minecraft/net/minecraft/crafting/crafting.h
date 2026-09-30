#ifndef C919_CRAFTING_H
#define C919_CRAFTING_H
#include "inventory/inventory.h"
#define MC_CRAFTING_MAX_EFFECTS 5
typedef struct { size_t count; mc_slot dropped[MC_CRAFTING_MAX_EFFECTS]; } mc_crafting_effects;
void mc_crafting_effects_init(mc_crafting_effects *effects);
void mc_crafting_effects_free(mc_crafting_effects *effects);
/* Pure recipe lookup. Grid is row-major, dimensions 1..3. Results and one
   remainder per grid cell are initialized owning slots; failure leaves them
   unchanged. A valid unmatched grid succeeds with an empty result. The current
   registry contains the complete set usable in a player 2x2 crafting grid. */
bool mc_crafting_match(const mc_slot *grid,unsigned width,unsigned height,
                       mc_slot *result,mc_slot *remaining);
/* Recompute derived window-0 output from input slots 1..4, without consuming. */
bool mc_crafting_update(mc_inventory *inventory);
/* Vanilla window-0 transactions, including output consumption and world drops.
   Caller applies creative permissions and commits world effects and persistence.
   All outputs must be initialized, owning and disjoint from inventory. */
bool mc_crafting_click(mc_inventory *inventory,int index,int button,int mode,
                       mc_slot *returned,mc_crafting_effects *effects);
/* Closing window 0 drops cursor and four grid inputs; output is derived. */
bool mc_crafting_close(mc_inventory *inventory,mc_crafting_effects *effects);
#endif
