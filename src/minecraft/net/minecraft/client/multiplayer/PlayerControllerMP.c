#include "client/multiplayer/PlayerControllerMP.h"
static void trace(MCObject *object, MCObjectVisitor visit, void *context) {
    PlayerControllerMP *self = (PlayerControllerMP *)object;
    self->netClientHandler =
        (NetHandlerPlayClient *)visit((MCObject *)self->netClientHandler, context);
    self->dependencyContext = visit(self->dependencyContext, context);
    self->mc = visit(self->mc, context);
    self->actionsContext = visit(self->actionsContext, context);
}
static const MCObjectClass klass = {"net.minecraft.client.multiplayer.PlayerControllerMP",
                                    MCObjectHeap_plainClone, trace, NULL};
bool PlayerControllerMP_isInstance(const MCObject *o) { return o && o->klass == &klass; }
PlayerControllerMP *
PlayerControllerMP_nativeNew(MCObjectHeap *heap, NetHandlerPlayClient *handler, MCObject *context,
                             const PlayerControllerMPDependencies *dependencies) {
    if (!heap || !NetHandlerPlayClient_isInstance((MCObject *)handler) ||
        handler->object.heap != heap || (context && context->heap != heap) || !dependencies ||
        !dependencies->addToSendQueue) {
        MCObjectHeap_fail(heap);
        return NULL;
    }
    PlayerControllerMP *self =
        (PlayerControllerMP *)MCObjectHeap_alloc(heap, sizeof(*self), &klass);
    if (self) {
        self->netClientHandler = handler;
        self->dependencyContext = context;
        self->dependencies = dependencies;
        self->currentGameType = &PlayerControllerMP_SURVIVAL;
    }
    return self;
}
static bool actions_begin(PlayerControllerMP *self, MCObjectRootScope *scope) {
    if (!self || !PlayerControllerMP_isInstance((MCObject *)self)) {
        MCObjectHeap_fail(self ? self->object.heap : NULL);
        return false;
    }
    return MCObjectRootScope_begin(scope, self->object.heap) &&
           MCObjectRootScope_pin(scope, (MCObject *)self);
}
static bool actions_end(PlayerControllerMP *self, MCObjectRootScope *scope, bool ok) {
    ok = ok && !MCObjectHeap_failed(self->object.heap);
    if (!ok)
        MCObjectHeap_fail(self->object.heap);
    MCObjectRootScope_end(scope);
    return ok;
}
bool PlayerControllerMP_bindActions(PlayerControllerMP *self, MCObject *mc, MCObject *ctx,
                                    const PlayerControllerMPActionsDependencies *d) {
    MCObjectRootScope scope = {0};
    if (!actions_begin(self, &scope))
        return false;
    bool ok = mc && d && d->getPlayer && MCObjectRootScope_pin(&scope, mc) &&
              MCObjectRootScope_pin(&scope, ctx);
    if (ok) {
        self->mc = mc;
        self->actionsContext = ctx;
        self->actionsDependencies = d;
        MCObjectHeap_touch(self->object.heap);
    }
    return actions_end(self, &scope, ok);
}
bool PlayerControllerMP_setPlayerCapabilities(PlayerControllerMP *self,MCGameplayPlayer *player) {
    MCObjectRootScope scope={0};if (!actions_begin(self,&scope)) return false;
    bool ok=MCGameplayPlayer_isInstance((MCObject *)player) && MCObjectRootScope_pin(&scope,(MCObject *)player);
    const WorldSettingsGameType *receiver=self->currentGameType;
    PlayerCapabilities *caps=ok?MCGameplayPlayer_capabilities((MCObject *)player):NULL;
    ok=caps && WorldSettingsGameType_configurePlayerCapabilities(receiver,caps);
    return actions_end(self,&scope,ok);
}
bool PlayerControllerMP_setGameType(PlayerControllerMP *self,const WorldSettingsGameType *type) {
    MCObjectRootScope scope={0};if (!actions_begin(self,&scope)) return false;
    /* Original assignment precedes dereferencing Minecraft.thePlayer. */
    self->currentGameType=type;MCObjectHeap_touch(self->object.heap);
    const WorldSettingsGameType *receiver=self->currentGameType;
    const PlayerControllerMPActionsDependencies *d=self->actionsDependencies;
    bool ok=d && d->getPlayer && self->mc && MCObjectRootScope_pin(&scope,self->mc) && MCObjectRootScope_pin(&scope,self->actionsContext);
    MCGameplayPlayer *player=ok?d->getPlayer(self->actionsContext,self->mc):NULL;
    ok=ok && MCGameplayPlayer_isInstance((MCObject *)player) && MCObjectRootScope_pin(&scope,(MCObject *)player);
    PlayerCapabilities *caps=ok?MCGameplayPlayer_capabilities((MCObject *)player):NULL;
    ok=caps && WorldSettingsGameType_configurePlayerCapabilities(receiver,caps);
    return actions_end(self,&scope,ok);
}
bool PlayerControllerMP_syncCurrentPlayItem(PlayerControllerMP *self) {
    MCObjectRootScope scope = {0};
    if (!actions_begin(self, &scope))
        return false;
    const PlayerControllerMPActionsDependencies *d = self->actionsDependencies;
    bool ok = d && d->getPlayer && self->mc && MCObjectRootScope_pin(&scope, self->mc) &&
              MCObjectRootScope_pin(&scope, self->actionsContext);
    MCGameplayPlayer *p = ok ? d->getPlayer(self->actionsContext, self->mc) : NULL;
    ok = ok && !MCObjectHeap_failed(self->object.heap) &&
         MCGameplayPlayer_isInstance((MCObject *)p) &&
         MCObjectRootScope_pin(&scope, (MCObject *)p) && p->inventory &&
         MCObjectRootScope_pin(&scope, (MCObject *)p->inventory);
    if (ok && p->inventory->currentItem != self->currentPlayerItem) {
        self->currentPlayerItem = p->inventory->currentItem;
        MCObjectHeap_touch(self->object.heap);
        C09PacketHeldItemChange *packet =
            C09PacketHeldItemChange_new(self->object.heap, self->currentPlayerItem);
        ok = packet && d->addHeldItemToSendQueue &&
             NetHandlerPlayClient_isInstance((MCObject *)self->netClientHandler) &&
             MCObjectRootScope_pin(&scope, (MCObject *)self->netClientHandler) &&
             d->addHeldItemToSendQueue(self->actionsContext, self->netClientHandler, packet);
    }
    return actions_end(self, &scope, ok);
}
static bool send_creative(PlayerControllerMP *self, ItemStack *stack, int32_t slotId) {
    MCObjectRootScope scope = {0};
    if (!actions_begin(self, &scope))
        return false;
    bool ok = self->actionsDependencies && self->actionsDependencies->addCreativeItemToSendQueue &&
              MCObjectRootScope_pin(&scope, self->actionsContext) &&
              NetHandlerPlayClient_isInstance((MCObject *)self->netClientHandler) &&
              MCObjectRootScope_pin(&scope, (MCObject *)self->netClientHandler);
    C10PacketCreativeInventoryAction *packet =
        ok ? C10PacketCreativeInventoryAction_new(self->object.heap, slotId, stack) : NULL;
    if (packet)
        ok = self->actionsDependencies->addCreativeItemToSendQueue(self->actionsContext,
                                                                   self->netClientHandler, packet);
    else
        ok = false;
    return actions_end(self, &scope, ok);
}
bool PlayerControllerMP_sendSlotPacket(PlayerControllerMP *self, ItemStack *stack, int32_t slotId) {
    MCObjectRootScope scope = {0};
    if (!actions_begin(self, &scope))
        return false;
    bool ok = self->currentGameType && (self->currentGameType == &PlayerControllerMP_CREATIVE
                                            ? send_creative(self, stack, slotId)
                                            : true);
    return actions_end(self, &scope, ok);
}
bool PlayerControllerMP_sendPacketDropItem(PlayerControllerMP *self, ItemStack *stack) {
    MCObjectRootScope scope = {0};
    if (!actions_begin(self, &scope))
        return false;
    bool ok =
        self->currentGameType && (self->currentGameType == &PlayerControllerMP_CREATIVE && stack
                                      ? send_creative(self, stack, -1)
                                      : true);
    return actions_end(self, &scope, ok);
}
bool PlayerControllerMP_sendUseItem(PlayerControllerMP *self, MCGameplayPlayer *p,
                                    MCGameplayWorld *world, ItemStack *stack, bool *out) {
    MCObjectRootScope scope = {0};
    if (!out || !actions_begin(self, &scope))
        return false;
    if (self->currentGameType == &PlayerControllerMP_SPECTATOR) {
        *out = false;
        return actions_end(self, &scope, true);
    }
    bool ok = PlayerControllerMP_syncCurrentPlayItem(self);
    if (ok)
        ok = MCGameplayPlayer_isInstance((MCObject *)p) &&
             MCObjectRootScope_pin(&scope, (MCObject *)p) &&
             MCObjectRootScope_pin(&scope, (MCObject *)world) && p->inventory &&
             MCObjectRootScope_pin(&scope, (MCObject *)p->inventory);
    C08PacketPlayerBlockPlacement *packet =
        ok ? C08PacketPlayerBlockPlacement_new_useItem(self->object.heap,
                                                       InventoryPlayer_getCurrentItem(p->inventory))
           : NULL;
    if (packet)
        ok = self->actionsDependencies && self->actionsDependencies->addPlacementToSendQueue &&
             self->actionsDependencies->addPlacementToSendQueue(self->actionsContext,
                                                                self->netClientHandler, packet);
    else
        ok = false;
    int32_t count = 0;
    ItemStack *result = NULL;
    if (ok) {
        ok = stack && ItemStack_isInstance((MCObject *)stack) &&
             MCObjectRootScope_pin(&scope, (MCObject *)stack);
        if (ok) {
            count = stack->stackSize;
            ok = ItemStack_useItemRightClick(stack, (MCObject *)world, (MCObject *)p,
                                             self->actionsDependencies->itemUse,
                                             self->actionsContext, &result);
        }
    }
    bool changed = ok && (result != stack || (result && result->stackSize != count));
    if (changed) {
        int32_t index = p->inventory->currentItem;
        ItemStackArray *array = p->inventory->mainInventory;
        ok = array && MCObjectRootScope_pin(&scope, (MCObject *)array) && index >= 0 &&
             index < array->length;
        if (ok) {
            array->items[index] = result;
            MCObjectHeap_touch(self->object.heap);
            /* The original dereferences the assigned return, even when NULL. */
            if (!result)
                ok = false;
            else if (result->stackSize == 0)
                array->items[index] = NULL;
        }
    }
    if (ok && !MCObjectHeap_failed(self->object.heap))
        *out = changed;
    return actions_end(self, &scope, ok);
}
bool PlayerControllerMP_windowClick(PlayerControllerMP *self, int32_t windowId, int32_t slotId,
                                    int32_t mouseButtonClicked, int32_t mode,
                                    MCGameplayPlayer *playerIn, ItemStack **out) {
    if (!self || !out)
        return false;
    MCObjectHeap *heap = self->object.heap;
    if (!PlayerControllerMP_isInstance((MCObject *)self) ||
        !MCGameplayPlayer_isInstance((MCObject *)playerIn) || playerIn->object.heap != heap ||
        !playerIn->openContainer || playerIn->openContainer->object.heap != heap ||
        !playerIn->inventory || playerIn->inventory->object.heap != heap ||
        !NetHandlerPlayClient_isInstance((MCObject *)self->netClientHandler) ||
        self->netClientHandler->object.heap != heap || !self->dependencies ||
        !self->dependencies->addToSendQueue ||
        (self->dependencyContext && self->dependencyContext->heap != heap)) {
        MCObjectHeap_fail(heap);
        return false;
    }
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap))
        return false;
    int16_t short1 = Container_getNextTransactionID(playerIn->openContainer, playerIn->inventory);
    ItemStack *itemstack = Container_slotClick(playerIn->openContainer, slotId, mouseButtonClicked,
                                               mode, playerIn->inventory);
    bool ok = !MCObjectHeap_failed(heap);
    C0EPacketClickWindow *packet =
        ok ? C0EPacketClickWindow_new(heap, windowId, slotId, mouseButtonClicked, mode, itemstack,
                                      short1)
           : NULL;
    if (packet)
        ok = self->dependencies->addToSendQueue(self->dependencyContext, self->netClientHandler,
                                                packet);
    else
        ok = false;
    ok = ok && !MCObjectHeap_failed(heap);
    if (ok)
        *out = itemstack;
    else
        MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);
    return ok;
}
