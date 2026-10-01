#ifndef C919_SOURCE_ITEM_IN_WORLD_MANAGER_USE_H
#define C919_SOURCE_ITEM_IN_WORLD_MANAGER_USE_H
#include "item/ItemStackUse.h"
#include "util/MCGameplayPlayer.h"

/* Actual tryUseItem method subset on the native owner actor. The game type
   predicates are explicit inherited manager dependencies. Their values need
   not equal the player's current capability flags. Full construction, block
   destruction/activation and the remaining class fields are separate ports. */
typedef struct {
    bool (*isSpectator)(MCObject *context);
    bool (*isCreative)(MCObject *context);
    const ItemStackUseDependencies *itemUse;
    int32_t (*getMaxItemUseDuration)(MCObject *context, ItemStack *);
    bool (*isUsingItem)(MCObject *context, MCGameplayPlayer *);
    bool (*sendContainerToPlayer)(MCObject *context, MCGameplayPlayer *, Container *);
} ItemInWorldManagerUseDependencies;
bool ItemInWorldManager_tryUseItem(MCGameplayPlayer *, MCGameplayWorld *, ItemStack *,
                                   const ItemInWorldManagerUseDependencies *, MCObject *context,
                                   bool *sourceResult);
#endif
