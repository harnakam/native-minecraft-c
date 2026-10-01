#ifndef C919_SOURCE_RECIPE_REPAIR_ITEM_H
#define C919_SOURCE_RECIPE_REPAIR_ITEM_H
#include "item/crafting/IRecipe.h"
typedef struct { MCObject object; } RecipeRepairItem;
RecipeRepairItem *RecipeRepairItem_new(MCObjectHeap *);
IRecipe RecipeRepairItem_asRecipe(RecipeRepairItem *);
bool RecipeRepairItem_matches(RecipeRepairItem *,InventoryCrafting *,MCObject *world);
ItemStack *RecipeRepairItem_getCraftingResult(RecipeRepairItem *,InventoryCrafting *);
int32_t RecipeRepairItem_getRecipeSize(const RecipeRepairItem *);
ItemStack *RecipeRepairItem_getRecipeOutput(RecipeRepairItem *);
ItemStackArray *RecipeRepairItem_getRemainingItems(RecipeRepairItem *,InventoryCrafting *);
#endif
