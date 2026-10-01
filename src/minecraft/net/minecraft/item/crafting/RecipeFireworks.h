#ifndef C919_SOURCE_RECIPE_FIREWORKS_H
#define C919_SOURCE_RECIPE_FIREWORKS_H
#include "item/crafting/IRecipe.h"
typedef struct RecipeFireworks { MCObject object; ItemStack *field_92102_a; } RecipeFireworks;
RecipeFireworks *RecipeFireworks_new(MCObjectHeap *);
IRecipe RecipeFireworks_asRecipe(RecipeFireworks *);
bool RecipeFireworks_matches(RecipeFireworks *,InventoryCrafting *,MCObject *world);
ItemStack *RecipeFireworks_getCraftingResult(RecipeFireworks *,InventoryCrafting *);
int32_t RecipeFireworks_getRecipeSize(const RecipeFireworks *);
ItemStack *RecipeFireworks_getRecipeOutput(RecipeFireworks *);
ItemStackArray *RecipeFireworks_getRemainingItems(RecipeFireworks *,InventoryCrafting *);
/* Cached output is a borrowed mutable reference. matches resets it first;
   false may still leave a cached output, as in the original fade path.
   ItemDye colors and canonical Item identities are immutable numeric adapters. */
#endif
