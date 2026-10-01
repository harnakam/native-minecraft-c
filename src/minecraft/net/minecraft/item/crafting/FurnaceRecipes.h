#ifndef C919_SOURCE_FURNACE_RECIPES_H
#define C919_SOURCE_FURNACE_RECIPES_H
#include "item/ItemStack.h"

/* Managed native identity-key Map storage for the original two live maps.
   ItemStack has identity equals/hashCode. Bucket/tree iteration order is a
   JDK collection dependency; this adapter iterates insertion order. Keys and
   values are retained directly, including nulls, not copied or normalized. */
typedef struct FurnaceRecipeMap FurnaceRecipeMap;
typedef struct FurnaceRecipes {
    MCObject object;
    FurnaceRecipeMap *smeltingList,*experienceList;
} FurnaceRecipes;
FurnaceRecipes *FurnaceRecipes_new(MCObjectHeap *);
/* Explicit native empty/per-heap registry, not the original static singleton. */
FurnaceRecipes *FurnaceRecipes_newEmpty(MCObjectHeap *);
typedef const Item *(*FurnaceRecipesBlockItem)(const Block *);
bool FurnaceRecipes_addSmeltingRecipeForBlock(FurnaceRecipes *,const Block *,FurnaceRecipesBlockItem,
    ItemStack *,float experience);
bool FurnaceRecipes_addSmelting(FurnaceRecipes *,const Item *,ItemStack *,float experience);
bool FurnaceRecipes_addSmeltingRecipe(FurnaceRecipes *,ItemStack *input,ItemStack *output,float experience);
ItemStack *FurnaceRecipes_getSmeltingResult(FurnaceRecipes *,ItemStack *);
bool FurnaceRecipes_compareItemStacks(FurnaceRecipes *,const ItemStack *,const ItemStack *);
FurnaceRecipeMap *FurnaceRecipes_getSmeltingList(FurnaceRecipes *);
float FurnaceRecipes_getSmeltingExperience(FurnaceRecipes *,ItemStack *);

/* Direct mutation/iteration boundary for getSmeltingList's original Map.
   Mutating that Map alone does not update the separate experience Map. */
int32_t FurnaceRecipeMap_size(const FurnaceRecipeMap *);
uint32_t FurnaceRecipeMap_modCount(const FurnaceRecipeMap *);
bool FurnaceRecipeMap_entry(FurnaceRecipeMap *,int32_t index,ItemStack **key,ItemStack **value);
bool FurnaceRecipeMap_put(FurnaceRecipeMap *,ItemStack *key,ItemStack *value);
bool FurnaceRecipeMap_remove(FurnaceRecipeMap *,ItemStack *key);
void FurnaceRecipeMap_clear(FurnaceRecipeMap *);
/* Constructor registrations use immutable Item/Block-to-Item and FishType/
   dye/stone metadata facts. Those source registries and Java static instance
   initialization are explicit adapters, not ported class bodies. */
#endif
