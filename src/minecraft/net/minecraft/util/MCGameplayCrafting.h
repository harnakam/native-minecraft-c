#ifndef C919_NATIVE_GAMEPLAY_CRAFTING_H
#define C919_NATIVE_GAMEPLAY_CRAFTING_H
#include "util/MCGameplayWorld.h"
#include "inventory/crafting_dispatch.h"
/* Native per-heap registration adapter. All 373 entries dispatch actual
   translated recipe bodies; original singleton/registration helper classes,
   varargs and Java Collections.sort are not claimed as translated here. */
CraftingManager *MCGameplayCrafting_newManager(MCObjectHeap *);
/* Installs the source recipe manager, furnace registry, source craft stats,
   achievement identities and required Item/localization display dispatch.
   Global Java static initialization, chat/criteria and other StatList arrays
   remain explicit dependencies; absent craft stats remain null.
   The same-heap context is traced by World; callbacks must outlive snapshots.
   Failure leaves previous World fields intact and fails the working heap. */
bool MCGameplayCrafting_configureWorld(MCGameplayWorld *, ItemStackDisplayNameDispatch,
                                       MCObject *displayContext);
ItemStack *MCGameplayCrafting_findMatchingRecipe(InventoryCrafting *, MCObject *world);
ItemStackArray *MCGameplayCrafting_getRemainingItems(InventoryCrafting *, MCObject *world);
/* Supplies the existing native actor/world reads and source Item subclass
   identity facts. Side effects remain required caller implementations, never
   no-op success handlers. The caller owns this immutable dispatch table. */
typedef struct {
    bool (*onCrafting)(ItemStack *, MCObject *world, MCObject *player, int32_t amount);
    bool (*triggerAchievement)(MCObject *player, mc_crafting_achievement);
    bool (*drop)(MCObject *player, ItemStack *, bool scatter);
} MCGameplayCraftingEffects;
bool MCGameplayCrafting_nativeDispatch(mc_crafting_dispatch *, const MCGameplayCraftingEffects *);
#endif
