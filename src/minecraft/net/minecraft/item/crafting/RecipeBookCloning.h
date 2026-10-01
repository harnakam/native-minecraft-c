#ifndef C919_RECIPE_BOOK_CLONING_H
#define C919_RECIPE_BOOK_CLONING_H
#include "item/crafting/IRecipe.h"
typedef struct { MCObject object; } RecipeBookCloning;
RecipeBookCloning *RecipeBookCloning_new(MCObjectHeap *);
IRecipe RecipeBookCloning_asRecipe(RecipeBookCloning *);
/* Stateless original class. World argument is unused by its original matches. */
bool RecipeBookCloning_matches(InventoryCrafting *,const MCObject *world);
ItemStack *RecipeBookCloning_getCraftingResult(InventoryCrafting *,ItemStackDisplayNameDispatch,MCObject *displayContext);
int32_t RecipeBookCloning_getRecipeSize(void);
ItemStack *RecipeBookCloning_getRecipeOutput(void);
ItemStackArray *RecipeBookCloning_getRemainingItems(InventoryCrafting *);
#endif
