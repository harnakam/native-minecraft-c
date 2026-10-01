#ifndef C919_SOURCE_CRAFTING_MANAGER_H
#define C919_SOURCE_CRAFTING_MANAGER_H
#include "item/crafting/ShapedRecipes.h"
#include "item/crafting/ShapelessRecipes.h"
typedef struct { MCObject object; RecipeList *recipes; } CraftingManager;
typedef enum {
    CRAFTING_PENDING_ARMOR_DYES,CRAFTING_PENDING_FIREWORKS,CRAFTING_PENDING_BANNER_ADD,
    CRAFTING_PENDING_MAP_CLONING,CRAFTING_PENDING_MAP_EXTENDING,CRAFTING_PENDING_REPAIR,
    CRAFTING_PENDING_BANNER_DUPLICATE,CRAFTING_PENDING_COUNT
} CraftingPendingRecipe;
/* Native per-heap singleton/registration boundary: roots retain this manager.
   The 365 existing numeric registration facts build actual shaped/shapeless
   instances; BookCloning is translated. The other seven original recipe class
   dependencies must be supplied explicitly. They may use a transient pure
   lookup adapter, but must never own/replace authoritative grid references. */
CraftingManager *CraftingManager_new(MCObjectHeap *,const IRecipe pending[CRAFTING_PENDING_COUNT]);
/* Explicit native empty registry for caller registration/custom-registry tests,
   not the original default constructor or a fake complete recipe registry. */
CraftingManager *CraftingManager_newEmpty(MCObjectHeap *);
bool CraftingManager_addRecipe(CraftingManager *,IRecipe);
ItemStack *CraftingManager_findMatchingRecipe(CraftingManager *,InventoryCrafting *,MCObject *world,
    ItemStackDisplayNameDispatch,MCObject *displayContext);
ItemStackArray *CraftingManager_func_180303_b(CraftingManager *,InventoryCrafting *,MCObject *world);
RecipeList *CraftingManager_getRecipeList(CraftingManager *);
/* The registration adapter uses the verified original final stable-sort
   order. addRecipe(IRecipe) itself appends, as in Java; it does not re-sort.
   Java varargs addRecipe/addShapelessRecipe and original recipe-registration
   helper classes remain dependencies rather than fake partial methods. */
#endif
