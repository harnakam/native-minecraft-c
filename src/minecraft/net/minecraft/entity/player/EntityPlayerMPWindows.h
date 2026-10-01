#ifndef C919_ENTITY_PLAYER_MP_WINDOWS_H
#define C919_ENTITY_PLAYER_MP_WINDOWS_H
#include "util/MCGameplayPlayer.h"

/* Source EntityPlayerMP window/listener methods on the explicit native actor
   owner. Packet construction/handler transmission are required dependencies;
   callbacks queue managed effects, with direct source references, until the
   gameplay graph and its journal have committed. No socket effect belongs in
   these callbacks. Actual packet constructors perform their original copies. */
struct EntityPlayerMPWindowsDependencies {
    bool (*sendWindowItems)(MCGameplayPlayer *,int32_t window,ContainerList *items);
    bool (*sendSetSlot)(MCGameplayPlayer *,int32_t window,int32_t slot,ItemStack *stack);
    bool (*sendCloseWindow)(MCGameplayPlayer *,int32_t window);
};
/* Immutable dependencies outlive the managed graph and every snapshot. The
   listener target retains the exact Player identity, including after cloning. */
bool EntityPlayerMPWindows_bind(MCGameplayPlayer *,const EntityPlayerMPWindowsDependencies *);
ICrafting EntityPlayerMPWindows_listener(MCGameplayPlayer *);
bool EntityPlayerMPWindows_sendSlotContents(MCGameplayPlayer *,Container *,int32_t index,ItemStack *);
bool EntityPlayerMPWindows_sendContainerToPlayer(MCGameplayPlayer *,Container *);
bool EntityPlayerMPWindows_updateCraftingInventory(MCGameplayPlayer *,Container *,ContainerList *items);
bool EntityPlayerMPWindows_updateHeldItem(MCGameplayPlayer *);
bool EntityPlayerMPWindows_closeContainer(MCGameplayPlayer *);
bool EntityPlayerMPWindows_closeScreen(MCGameplayPlayer *);
/* Every borrowed argument and returned listener is used under caller RootScope.
   S31 properties and the remaining EntityPlayerMP class are not this subset. */
#endif
