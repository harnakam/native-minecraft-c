#ifndef C919_SOURCE_RECIPES_ARMOR_DYES_H
#define C919_SOURCE_RECIPES_ARMOR_DYES_H
#include "item/crafting/IRecipe.h"
typedef struct RecipesArmorDyes { MCObject object; } RecipesArmorDyes;
RecipesArmorDyes *RecipesArmorDyes_new(MCObjectHeap *);
IRecipe RecipesArmorDyes_asRecipe(RecipesArmorDyes *);
bool RecipesArmorDyes_matches(RecipesArmorDyes *,InventoryCrafting *,MCObject *world);
ItemStack *RecipesArmorDyes_getCraftingResult(RecipesArmorDyes *,InventoryCrafting *);
int32_t RecipesArmorDyes_getRecipeSize(const RecipesArmorDyes *);
ItemStack *RecipesArmorDyes_getRecipeOutput(RecipesArmorDyes *);
ItemStackArray *RecipesArmorDyes_getRemainingItems(RecipesArmorDyes *,InventoryCrafting *);
/* EntitySheep dye RGB, EnumDyeColor lookup and Item subclass metadata use
   immutable canonical numeric adapters. World is unused in the source. */
#endif
