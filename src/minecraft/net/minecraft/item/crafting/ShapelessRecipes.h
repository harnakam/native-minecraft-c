#ifndef C919_SOURCE_SHAPELESS_RECIPES_H
#define C919_SOURCE_SHAPELESS_RECIPES_H
#include "item/crafting/IRecipe.h"
typedef struct { MCObject object; ItemStack *recipeOutput; ItemStackList *recipeItems; } ShapelessRecipes;
ShapelessRecipes *ShapelessRecipes_new(MCObjectHeap *,ItemStack *output,ItemStackList *);
IRecipe ShapelessRecipes_asRecipe(ShapelessRecipes *);
ItemStack *ShapelessRecipes_getRecipeOutput(ShapelessRecipes *);
ItemStackArray *ShapelessRecipes_getRemainingItems(ShapelessRecipes *,InventoryCrafting *);
bool ShapelessRecipes_matches(ShapelessRecipes *,InventoryCrafting *,MCObject *world);
ItemStack *ShapelessRecipes_getCraftingResult(ShapelessRecipes *,InventoryCrafting *);
int32_t ShapelessRecipes_getRecipeSize(const ShapelessRecipes *);
#endif
