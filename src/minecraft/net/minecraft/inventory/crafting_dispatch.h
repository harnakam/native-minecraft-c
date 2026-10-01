#ifndef C919_CRAFTING_DISPATCH_H
#define C919_CRAFTING_DISPATCH_H
#include "inventory/Container.h"
#include "inventory/InventoryCraftResult.h"
/* Explicit dependency dispatch until the corresponding EntityPlayer, World,
   CraftingManager, Item subclasses, stats and achievement classes are ported.
   No required operation has an empty/default-success implementation. */
typedef enum {
    MC_ACH_BUILD_WORKBENCH, MC_ACH_BUILD_PICKAXE, MC_ACH_BUILD_FURNACE,
    MC_ACH_BUILD_HOE, MC_ACH_MAKE_BREAD, MC_ACH_BAKE_CAKE,
    MC_ACH_BUILD_BETTER_PICKAXE, MC_ACH_BUILD_SWORD, MC_ACH_ENCHANTMENTS,
    MC_ACH_BOOKCASE, MC_ACH_OVERPOWERED
} mc_crafting_achievement;
typedef struct {
    InventoryPlayer *(*inventory)(MCObject *player);
    MCObject *(*world)(MCObject *player);
    ItemStack *(*findMatchingRecipe)(InventoryCrafting *matrix,MCObject *world);
    ItemStackArray *(*getRemainingItems)(InventoryCrafting *matrix,MCObject *world);
    bool (*onCrafting)(ItemStack *stack,MCObject *world,MCObject *player,int32_t amount);
    bool (*triggerAchievement)(MCObject *player,mc_crafting_achievement achievement);
    bool (*drop)(MCObject *player,ItemStack *stack,bool scatter);
    bool (*isPickaxe)(const Item *item);
    bool (*isHoe)(const Item *item);
    bool (*isSword)(const Item *item);
    bool (*isWoodPickaxe)(const Item *item);
    int32_t (*armorType)(const Item *item); /* -1 when not an ItemArmor. */
    bool (*isRemote)(const MCObject *world);
    bool (*isCraftingTable)(const MCObject *world,int32_t x,int32_t y,int32_t z);
    double (*getDistanceSq)(const MCObject *player,double x,double y,double z);
} mc_crafting_dispatch;
#endif
