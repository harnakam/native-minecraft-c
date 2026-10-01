#ifndef C919_SOURCE_RECIPES_BANNERS_H
#define C919_SOURCE_RECIPES_BANNERS_H
#include "item/crafting/IRecipe.h"
#include "tileentity/EnumBannerPattern.h"

/* Native static-registry retention is the only additional class field. The
   original nested recipe bodies and their direct input references remain
   authoritative. The outer addRecipes registration helper is represented by
   the existing numeric registration adapter, not claimed as translated. */
typedef struct { MCObject object; EnumBannerPatternRegistry *registry; } RecipesBannersRecipeAddPattern;
typedef struct { MCObject object; } RecipesBannersRecipeDuplicatePattern;
RecipesBannersRecipeAddPattern *RecipesBannersRecipeAddPattern_new(MCObjectHeap *);
IRecipe RecipesBannersRecipeAddPattern_asRecipe(RecipesBannersRecipeAddPattern *);
bool RecipesBannersRecipeAddPattern_matches(RecipesBannersRecipeAddPattern *,InventoryCrafting *,MCObject *world);
ItemStack *RecipesBannersRecipeAddPattern_getCraftingResult(RecipesBannersRecipeAddPattern *,InventoryCrafting *);
int32_t RecipesBannersRecipeAddPattern_getRecipeSize(const RecipesBannersRecipeAddPattern *);
ItemStack *RecipesBannersRecipeAddPattern_getRecipeOutput(RecipesBannersRecipeAddPattern *);
ItemStackArray *RecipesBannersRecipeAddPattern_getRemainingItems(RecipesBannersRecipeAddPattern *,InventoryCrafting *);
RecipesBannersRecipeDuplicatePattern *RecipesBannersRecipeDuplicatePattern_new(MCObjectHeap *);
IRecipe RecipesBannersRecipeDuplicatePattern_asRecipe(RecipesBannersRecipeDuplicatePattern *);
bool RecipesBannersRecipeDuplicatePattern_matches(RecipesBannersRecipeDuplicatePattern *,InventoryCrafting *,MCObject *world);
ItemStack *RecipesBannersRecipeDuplicatePattern_getCraftingResult(RecipesBannersRecipeDuplicatePattern *,InventoryCrafting *);
int32_t RecipesBannersRecipeDuplicatePattern_getRecipeSize(const RecipesBannersRecipeDuplicatePattern *);
ItemStack *RecipesBannersRecipeDuplicatePattern_getRecipeOutput(RecipesBannersRecipeDuplicatePattern *);
ItemStackArray *RecipesBannersRecipeDuplicatePattern_getRemainingItems(RecipesBannersRecipeDuplicatePattern *,InventoryCrafting *);
#endif
