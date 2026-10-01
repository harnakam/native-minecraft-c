#ifndef C919_SOURCE_RECIPES_MAP_CLONING_H
#define C919_SOURCE_RECIPES_MAP_CLONING_H
#include "item/crafting/IRecipe.h"
typedef struct { MCObject object; } RecipesMapCloning;
RecipesMapCloning *RecipesMapCloning_new(MCObjectHeap *);
IRecipe RecipesMapCloning_asRecipe(RecipesMapCloning *);
bool RecipesMapCloning_matches(RecipesMapCloning *,InventoryCrafting *,MCObject *world);
ItemStack *RecipesMapCloning_getCraftingResult(RecipesMapCloning *,InventoryCrafting *,ItemStackDisplayNameDispatch,MCObject *displayContext);
int32_t RecipesMapCloning_getRecipeSize(const RecipesMapCloning *);
ItemStack *RecipesMapCloning_getRecipeOutput(RecipesMapCloning *);
ItemStackArray *RecipesMapCloning_getRemainingItems(RecipesMapCloning *,InventoryCrafting *);
#endif
