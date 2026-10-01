#ifndef C919_SOURCE_SHAPED_RECIPES_H
#define C919_SOURCE_SHAPED_RECIPES_H
#include "item/crafting/IRecipe.h"
typedef struct {
    MCObject object;
    int32_t recipeWidth,recipeHeight;
    ItemStackArray *recipeItems;
    ItemStack *recipeOutput;
    bool copyIngredientNBT;
} ShapedRecipes;
ShapedRecipes *ShapedRecipes_new(MCObjectHeap *,int32_t width,int32_t height,ItemStackArray *,ItemStack *output);
/* Actual super constructor/trace bodies for a first-member subclass. Native
   allocation establishes its MCObject identity/class before construct. */
bool ShapedRecipes_construct(ShapedRecipes *,int32_t width,int32_t height,ItemStackArray *,ItemStack *output);
void ShapedRecipes_trace(ShapedRecipes *,MCObjectVisitor,void *);
IRecipe ShapedRecipes_asRecipe(ShapedRecipes *);
ItemStack *ShapedRecipes_getRecipeOutput(ShapedRecipes *);
ItemStackArray *ShapedRecipes_getRemainingItems(ShapedRecipes *,InventoryCrafting *);
bool ShapedRecipes_matches(ShapedRecipes *,InventoryCrafting *,MCObject *world);
bool ShapedRecipes_checkMatch(ShapedRecipes *,InventoryCrafting *,int32_t x,int32_t y,bool mirrored);
ItemStack *ShapedRecipes_getCraftingResult(ShapedRecipes *,InventoryCrafting *);
int32_t ShapedRecipes_getRecipeSize(const ShapedRecipes *);
#endif
