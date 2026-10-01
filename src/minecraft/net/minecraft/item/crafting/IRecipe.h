#ifndef C919_SOURCE_I_RECIPE_H
#define C919_SOURCE_I_RECIPE_H
#include "inventory/InventoryCrafting.h"
typedef struct {
    bool (*matches)(MCObject *,InventoryCrafting *,MCObject *world);
    ItemStack *(*getCraftingResult)(MCObject *,InventoryCrafting *,ItemStackDisplayNameDispatch,MCObject *displayContext);
    int32_t (*getRecipeSize)(const MCObject *);
    ItemStack *(*getRecipeOutput)(MCObject *);
    ItemStackArray *(*getRemainingItems)(MCObject *,InventoryCrafting *);
} IRecipeMethods;
/* Immutable dispatch/type information adapts the Java interface/instanceof.
   Instance is the actual managed recipe identity, never a value-grid owner. */
typedef enum { IRECIPE_OTHER,IRECIPE_SHAPED,IRECIPE_SHAPELESS } IRecipeKind;
typedef struct { MCObject *instance; const IRecipeMethods *methods; IRecipeKind kind; } IRecipe;
typedef struct RecipeList RecipeList;
RecipeList *RecipeList_new(MCObjectHeap *);
int32_t RecipeList_size(const RecipeList *);
/* Native ArrayList iterator dependency: structural modCount, Java int bits. */
uint32_t RecipeList_modCount(const RecipeList *);
IRecipe RecipeList_get(RecipeList *,int32_t);
bool RecipeList_add(RecipeList *,IRecipe);
bool RecipeList_set(RecipeList *,int32_t,IRecipe);
IRecipe RecipeList_remove(RecipeList *,int32_t);
void RecipeList_clear(RecipeList *);
typedef struct ItemStackList ItemStackList;
ItemStackList *ItemStackList_new(MCObjectHeap *);
int32_t ItemStackList_size(const ItemStackList *);
ItemStack *ItemStackList_get(ItemStackList *,int32_t);
bool ItemStackList_add(ItemStackList *,ItemStack *);
bool ItemStackList_set(ItemStackList *,int32_t,ItemStack *);
ItemStack *ItemStackList_remove(ItemStackList *,int32_t);
void ItemStackList_clear(ItemStackList *);
/* Lists are explicit native managed ArrayList storage. Getters return borrowed
   direct references; mutations retain Java-style null entries and identity. */
#endif
