#ifndef C919_CRAFTING_H
#define C919_CRAFTING_H
#include "inventory/container_runtime.h"
#define MC_CRAFTING_MAX_EFFECTS 1024u
typedef struct { int16_t id,damage; } mc_crafting_ingredient_fact;
typedef struct {
    uint8_t width,height,count;
    mc_crafting_ingredient_fact input[9];
    int16_t output; uint8_t amount; int16_t damage;
} mc_crafting_recipe_fact;
/* Read-only numeric registration facts; no live ItemStack/grid ownership. */
size_t mc_crafting_static_recipe_count(void);
bool mc_crafting_static_recipe(size_t index,mc_crafting_recipe_fact *output);
typedef struct { size_t count, capacity, bytes; mc_slot *dropped; } mc_crafting_effects;
typedef struct mc_maps mc_maps;
typedef struct {
    bool creative, authoritative;
    mc_maps *maps;
    double player_x, player_z;
    int32_t spawn_x, spawn_z;
    int dimension;
} mc_crafting_context;
void mc_crafting_effects_init(mc_crafting_effects *effects);
void mc_crafting_effects_free(mc_crafting_effects *effects);
bool mc_crafting_effects_append(mc_crafting_effects *effects, const mc_slot *item);
/* Pure recipe lookup. Grid is row-major, dimensions 1..3. Results and one
   remainder per grid cell are initialized owning slots; failure leaves them
   unchanged. A valid unmatched grid succeeds with an empty result. The current
   registry contains all static 1.8.9 recipes and dynamic player/workbench
   recipes, including contextual map enlargement. */
bool mc_crafting_match(const mc_slot *grid,unsigned width,unsigned height,
                       mc_slot *result,mc_slot *remaining);
/* Pure contextual lookup never creates or changes MapData or grid slots. */
bool mc_crafting_match_context(const mc_slot *grid,unsigned width,unsigned height,
    const mc_crafting_context *context,mc_slot *result,mc_slot *remaining);
/* Player, container, caller's working MapData and owning outputs are unchanged
   on failure. A non-authoritative context preserves a supplied map-extension
   result when local MapData is unavailable, awaiting the server's resync. */
bool mc_container_update(mc_inventory *player,mc_container *container,mc_crafting_context *context);
bool mc_container_click(mc_inventory *player,mc_container *container,mc_crafting_context *context,
    int index,int button,int mode,mc_slot *returned,mc_crafting_effects *effects);
bool mc_container_close(mc_inventory *player,mc_container *container,mc_crafting_effects *effects);
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
