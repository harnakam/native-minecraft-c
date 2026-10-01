#include "util/MCGameplayCrafting.h"
#include "util/MCGameplayPlayer.h"
#include "item/ItemMapData.h"
#include "item/ItemArmor.h"
#include "item/crafting/RecipesArmorDyes.h"
#include "item/crafting/RecipeFireworks.h"
#include "item/crafting/RecipesBanners.h"
#include "item/crafting/RecipesMapCloning.h"
#include "item/crafting/RecipesMapExtending.h"
#include "item/crafting/RecipeRepairItem.h"
#include "stats/StatList.h"
static const mc_map_info *get_map(MCObject *world, ItemStack *stack) {
    return ItemMap_getMapData(stack, (MCGameplayWorld *)world);
}
CraftingManager *MCGameplayCrafting_newManager(MCObjectHeap *h) {
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return NULL;
    RecipesArmorDyes *armor = RecipesArmorDyes_new(h);
    RecipeFireworks *fireworks = RecipeFireworks_new(h);
    RecipesBannersRecipeAddPattern *add = RecipesBannersRecipeAddPattern_new(h);
    RecipesMapCloning *clone = RecipesMapCloning_new(h);
    RecipesMapExtending *extend = RecipesMapExtending_new(h, get_map);
    RecipeRepairItem *repair = RecipeRepairItem_new(h);
    RecipesBannersRecipeDuplicatePattern *duplicate = RecipesBannersRecipeDuplicatePattern_new(h);
    CraftingManager *out = NULL;
    if (armor && fireworks && add && clone && extend && repair && duplicate &&
        !MCObjectHeap_failed(h)) {
        IRecipe recipes[CRAFTING_PENDING_COUNT] = {
            RecipesArmorDyes_asRecipe(armor),
            RecipeFireworks_asRecipe(fireworks),
            RecipesBannersRecipeAddPattern_asRecipe(add),
            RecipesMapCloning_asRecipe(clone),
            RecipesMapExtending_asRecipe(extend),
            RecipeRepairItem_asRecipe(repair),
            RecipesBannersRecipeDuplicatePattern_asRecipe(duplicate)};
        out = CraftingManager_new(h, recipes);
    }
    MCObjectRootScope_end(&scope);
    return MCObjectHeap_failed(h) ? NULL : out;
}
bool MCGameplayCrafting_configureWorld(MCGameplayWorld *world, ItemStackDisplayNameDispatch display,
                                       MCObject *context) {
    MCObjectHeap *h = world ? world->object.heap : NULL;
    if (!MCGameplayWorld_isInstance((MCObject *)world) || !display ||
        (context && context->heap != h)) {
        MCObjectHeap_fail(h);
        return false;
    }
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return false;
    bool ok =
        MCObjectRootScope_pin(&scope, (MCObject *)world) && MCObjectRootScope_pin(&scope, context);
    CraftingManager *manager = ok ? MCGameplayCrafting_newManager(h) : NULL;
    FurnaceRecipes *furnace = manager ? FurnaceRecipes_new(h) : NULL;
    StatList *stats = furnace ? StatList_new(h) : NULL;
    StatBase *craftStats[MC_GAMEPLAY_CRAFT_STAT_COUNT] = {0};
    ok = stats && StatList_initCraftableStats(stats, manager, furnace) &&
         StatList_initAchievementIdentities(stats) &&
         StatList_fillCraftStats(stats, craftStats, MC_GAMEPLAY_CRAFT_STAT_COUNT) &&
         !MCObjectHeap_failed(h);
    /* Native constructor/registration adapter for this needed StatList use
       entry. The ID and Item reference are the original empty-map identity. */
    StatCrafting *mapUse = ok ? StatCrafting_newIdentity(h,
        NBTString_literalASCII(h,"stat.useItem."),NBTString_literalASCII(h,"minecraft.map"),
        ItemStack_registryItem(395)) : NULL;
    if (ok)
        ok = mapUse && StatList_registerStat(stats,(StatBase *)mapUse);
    if (ok) {
        world->manager = manager;
        world->furnace = furnace;
        world->statList = stats;
        world->emptyMapUseStat = (StatBase *)mapUse;
        for (size_t i = 0; i < MC_GAMEPLAY_CRAFT_STAT_COUNT; i++)
            world->craftStats[i] = craftStats[i];
        world->itemDisplayName = display;
        world->itemDisplayContext = context;
        MCObjectHeap_touch(h);
    }
    MCObjectRootScope_end(&scope);
    return ok && !MCObjectHeap_failed(h);
}
ItemStack *MCGameplayCrafting_findMatchingRecipe(InventoryCrafting *grid, MCObject *object) {
    MCGameplayWorld *world = (MCGameplayWorld *)object;
    MCObjectHeap *h = grid ? grid->object.heap : object ? object->heap : NULL;
    if (!grid || !MCGameplayWorld_isInstance(object) || object->heap != h || !world->manager ||
        !world->itemDisplayName) {
        MCObjectHeap_fail(h);
        return NULL;
    }
    return CraftingManager_findMatchingRecipe(world->manager, grid, object, world->itemDisplayName,
                                              world->itemDisplayContext);
}
ItemStackArray *MCGameplayCrafting_getRemainingItems(InventoryCrafting *grid, MCObject *object) {
    MCGameplayWorld *world = (MCGameplayWorld *)object;
    MCObjectHeap *h = grid ? grid->object.heap : object ? object->heap : NULL;
    if (!grid || !MCGameplayWorld_isInstance(object) || object->heap != h || !world->manager) {
        MCObjectHeap_fail(h);
        return NULL;
    }
    return CraftingManager_func_180303_b(world->manager, grid, object);
}
/* Immutable identity facts for the original instanceof checks. This is an
   adapter to registered Item subclasses, not a translation of those classes. */
static bool pickaxe(const Item *i) {
    int32_t n = ItemStack_registryId(i);
    return n == 257 || n == 270 || n == 274 || n == 278 || n == 285;
}
static bool hoe(const Item *i) {
    int32_t n = ItemStack_registryId(i);
    return n >= 290 && n <= 294;
}
static bool sword(const Item *i) {
    int32_t n = ItemStack_registryId(i);
    return n == 267 || n == 268 || n == 272 || n == 276 || n == 283;
}
static bool wood_pickaxe(const Item *i) {
    return ItemStack_registryId(i) == 270;
}
static int32_t armor_type(const Item *i) {
    int32_t n = ItemStack_registryId(i);
    return ItemArmor_isInstance(i) ? (n - 298) % 4 : -1;
}
bool MCGameplayCrafting_nativeDispatch(mc_crafting_dispatch *out,
                                       const MCGameplayCraftingEffects *effects) {
    if (!out || !effects || !effects->onCrafting || !effects->triggerAchievement || !effects->drop)
        return false;
    *out = (mc_crafting_dispatch){MCGameplayPlayer_inventory,
                                  MCGameplayPlayer_world,
                                  MCGameplayCrafting_findMatchingRecipe,
                                  MCGameplayCrafting_getRemainingItems,
                                  effects->onCrafting,
                                  effects->triggerAchievement,
                                  effects->drop,
                                  pickaxe,
                                  hoe,
                                  sword,
                                  wood_pickaxe,
                                  armor_type,
                                  MCGameplayWorld_isRemote,
                                  MCGameplayWorld_isCraftingTable,
                                  MCGameplayPlayer_getDistanceSq};
    return true;
}
