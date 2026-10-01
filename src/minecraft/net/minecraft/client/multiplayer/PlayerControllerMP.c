#include "client/multiplayer/PlayerControllerMP.h"
static void trace(MCObject *object, MCObjectVisitor visit, void *context) {
    PlayerControllerMP *self = (PlayerControllerMP *)object;
    self->netClientHandler =
        (NetHandlerPlayClient *)visit((MCObject *)self->netClientHandler, context);
    self->dependencyContext = visit(self->dependencyContext, context);
}
static const MCObjectClass klass = {"net.minecraft.client.multiplayer.PlayerControllerMP",
                                    MCObjectHeap_plainClone, trace, NULL};
bool PlayerControllerMP_isInstance(const MCObject *o) {
    return o && o->klass == &klass;
}
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
    }
    return self;
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
