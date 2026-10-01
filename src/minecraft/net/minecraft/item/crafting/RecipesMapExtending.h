#ifndef C919_SOURCE_RECIPES_MAP_EXTENDING_H
#define C919_SOURCE_RECIPES_MAP_EXTENDING_H
#include "item/crafting/ShapedRecipes.h"
typedef struct mc_map_info mc_map_info;
/* Required original ItemMap.getMapData/World dependency. Returns the real
   working-store view or NULL, and may mutate this exact input ItemStack. The
   recipe reads scale immediately, before another map-store allocation. Heap
   failure reports native exceptions; NULL with a healthy heap means missing
   MapData. No fabricated map, authoritative-only filter or DTO grid is used. */
typedef const mc_map_info *(*RecipesMapExtendingGetMapData)(MCObject *world,ItemStack *);
typedef struct {
    ShapedRecipes shaped;
    RecipesMapExtendingGetMapData getMapData;
} RecipesMapExtending;
RecipesMapExtending *RecipesMapExtending_new(MCObjectHeap *,RecipesMapExtendingGetMapData);
IRecipe RecipesMapExtending_asRecipe(RecipesMapExtending *);
bool RecipesMapExtending_matches(RecipesMapExtending *,InventoryCrafting *,MCObject *world);
ItemStack *RecipesMapExtending_getCraftingResult(RecipesMapExtending *,InventoryCrafting *);
/* RecipeOutput/RecipeSize/RemainingItems are the actual inherited
   ShapedRecipes methods, applied to &recipe->shaped. */
#endif
