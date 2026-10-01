#ifndef C919_SOURCE_INVENTORY_PLAYER_ANIMATIONS_H
#define C919_SOURCE_INVENTORY_PLAYER_ANIMATIONS_H
#include "entity/player/InventoryPlayer.h"
#include "item/ItemStackAnimation.h"

typedef struct {
    /* Exact player.worldObj field dependency, read anew for each occupied main
       slot. The getter may return NULL; an exception marks the player's heap. */
    MCObject *(*getWorld)(MCObject *player);
    const ItemStackAnimationDependencies *item;
} InventoryPlayerAnimationDependencies;

/* Source decrementAnimations traverses mainInventory only, without filtering
   counts or deduplicating aliases. mainInventory/length, player/worldObj and
   currentItem are read in source expression order on every iteration. The
   virtual Item callbacks run inside a RootScope; failure retains source partial
   mutations. EntityPlayer/SP's complete tick driver is a separate dependency. */
bool InventoryPlayer_decrementAnimations(InventoryPlayer *,
    const InventoryPlayerAnimationDependencies *, MCObject *context);
#endif
