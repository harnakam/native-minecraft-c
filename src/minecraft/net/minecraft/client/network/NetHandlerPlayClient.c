#include "client/network/NetHandlerPlayClient.h"

static void handler_trace(MCObject *o, MCObjectVisitor v, void *context) {
    NetHandlerPlayClient *h = (NetHandlerPlayClient *)o;
    h->gameController = v(h->gameController, context);
    h->dependencyContext = v(h->dependencyContext, context);
    h->clientWorldController = (MCGameplayWorld *)v((MCObject *)h->clientWorldController, context);
}
static const MCObjectClass handler_class = {"net.minecraft.client.network.NetHandlerPlayClient",
                                            MCObjectHeap_plainClone, handler_trace, NULL};
bool NetHandlerPlayClient_isInstance(const MCObject *object) {
    return object && object->klass == &handler_class;
}
static bool failed(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return false;
}
static bool same_heap(MCObjectHeap *heap, const MCObject *object) {
    return !object || object->heap == heap || failed(heap);
}
static bool dependencies_ready(const NetHandlerPlayClientDependencies *d) {
    return d && d->checkThreadAndEnqueue && d->getPlayer && d->isCreativeScreen &&
           d->selectedCreativeTabIndex && d->inventoryCreativeTabIndex &&
           d->closeScreenAndDropStack && d->addToSendQueue;
}
NetHandlerPlayClient *NetHandlerPlayClient_nativeNew(MCGameplayPlayer *player, MCObject *controller,
                                                     MCObject *context,
                                                     const NetHandlerPlayClientDependencies *d) {
    MCObjectHeap *heap = player ? player->living.entity.object.heap : NULL;
    if (!heap || !MCGameplayPlayer_isInstance((MCObject *)player) || !((MCGameplayWorld *)(player->living.entity.worldObj)) ||
        !same_heap(heap, (MCObject *)((MCGameplayWorld *)(player->living.entity.worldObj))) || !((MCGameplayWorld *)(player->living.entity.worldObj))->remote ||
        !controller || !same_heap(heap, controller) || !same_heap(heap, context) ||
        !dependencies_ready(d)) {
        failed(heap);
        return NULL;
    }
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap))
        return NULL;
    NetHandlerPlayClient *h =
        (NetHandlerPlayClient *)MCObjectHeap_alloc(heap, sizeof(*h), &handler_class);
    if (h) {
        h->gameController = controller;
        h->dependencyContext = context;
        h->clientWorldController = ((MCGameplayWorld *)(player->living.entity.worldObj));
        h->dependencies = d;
        player->handler = (MCObject *)h;
        MCObjectHeap_touch(heap);
    }
    MCObjectRootScope_end(&scope);
    return h;
}
static MCPacketThreadResult begin(NetHandlerPlayClient *h, MCObject *packet,
                                  MCObjectRootScope *scope) {
    MCObjectHeap *heap = h ? h->object.heap : NULL;
    if (!heap || h->object.klass != &handler_class || !packet || !same_heap(heap, packet) ||
        !h->gameController || !same_heap(heap, h->gameController) ||
        !same_heap(heap, h->dependencyContext) || !dependencies_ready(h->dependencies)) {
        failed(heap);
        return MC_PACKET_THREAD_FAILED;
    }
    if (!MCObjectRootScope_begin(scope, heap) || !MCObjectRootScope_pin(scope, (MCObject *)h) ||
        !MCObjectRootScope_pin(scope, packet))
        return MC_PACKET_THREAD_FAILED;
    MCPacketThreadResult result =
        h->dependencies->checkThreadAndEnqueue(h->dependencyContext, h, packet);
    if (MCObjectHeap_failed(heap) || result == MC_PACKET_THREAD_FAILED ||
        (result != MC_PACKET_THREAD_EXECUTE && result != MC_PACKET_THREAD_QUEUED)) {
        failed(heap);
        return MC_PACKET_THREAD_FAILED;
    }
    return result;
}
static bool end(NetHandlerPlayClient *h, MCObjectRootScope *scope, bool result) {
    MCObjectHeap *heap = h ? h->object.heap : NULL;
    result = result ? !MCObjectHeap_failed(heap) : failed(heap);
    MCObjectRootScope_end(scope);
    return result;
}
static MCGameplayPlayer *current_player(NetHandlerPlayClient *h) {
    MCGameplayPlayer *p = h->dependencies->getPlayer(h->dependencyContext, h->gameController);
    if (MCObjectHeap_failed(h->object.heap) || !p || !same_heap(h->object.heap, (MCObject *)p) ||
        !MCGameplayPlayer_isInstance((MCObject *)p)) {
        failed(h->object.heap);
        return NULL;
    }
    return p;
}
static Container *inventory_container(NetHandlerPlayClient *h, MCGameplayPlayer *p) {
    ContainerPlayer *c = ((ContainerPlayer *)(p->inventoryContainer));
    if (!c || !same_heap(h->object.heap, (MCObject *)c)) {
        failed(h->object.heap);
        return NULL;
    }
    return &c->container;
}
static Container *open_container(NetHandlerPlayClient *h, MCGameplayPlayer *p) {
    Container *c = p->openContainer;
    if (!c || !same_heap(h->object.heap, (MCObject *)c)) {
        failed(h->object.heap);
        return NULL;
    }
    return c;
}
bool NetHandlerPlayClient_handleCloseWindow(NetHandlerPlayClient *h, S2EPacketCloseWindow *packet) {
    MCObjectRootScope scope = {0};
    MCPacketThreadResult thread = begin(h, (MCObject *)packet, &scope);
    if (thread != MC_PACKET_THREAD_EXECUTE)
        return end(h, &scope, thread == MC_PACKET_THREAD_QUEUED);
    MCGameplayPlayer *p = current_player(h);
    return end(h, &scope, p && h->dependencies->closeScreenAndDropStack(h->dependencyContext, p));
}
bool NetHandlerPlayClient_handleSetSlot(NetHandlerPlayClient *h, S2FPacketSetSlot *packet) {
    MCObjectRootScope scope = {0};
    MCPacketThreadResult thread = begin(h, (MCObject *)packet, &scope);
    if (thread != MC_PACKET_THREAD_EXECUTE)
        return end(h, &scope, thread == MC_PACKET_THREAD_QUEUED);
    MCGameplayPlayer *p = current_player(h);
    bool ok = p != NULL;
    if (ok && S2FPacketSetSlot_func_149175_c(packet) == -1) {
        if (!p->inventory || !same_heap(h->object.heap, (MCObject *)p->inventory))
            ok = failed(h->object.heap);
        else
            ok = InventoryPlayer_setItemStack(p->inventory, S2FPacketSetSlot_func_149174_e(packet));
    } else if (ok) {
        bool flag = false;
        bool creative = h->dependencies->isCreativeScreen(h->dependencyContext, h->gameController);
        if (MCObjectHeap_failed(h->object.heap))
            ok = false;
        else if (creative) {
            int32_t selected =
                h->dependencies->selectedCreativeTabIndex(h->dependencyContext, h->gameController);
            if (MCObjectHeap_failed(h->object.heap))
                ok = false;
            else {
                int32_t inventory =
                    h->dependencies->inventoryCreativeTabIndex(h->dependencyContext);
                flag = selected != inventory;
                ok = !MCObjectHeap_failed(h->object.heap);
            }
        }
        if (ok && S2FPacketSetSlot_func_149175_c(packet) == 0 &&
            S2FPacketSetSlot_func_149173_d(packet) >= 36 &&
            S2FPacketSetSlot_func_149173_d(packet) < 45) {
            Container *c = inventory_container(h, p);
            Slot *slot = c ? Container_getSlot(c, S2FPacketSetSlot_func_149173_d(packet)) : NULL;
            ItemStack *old = slot ? Slot_getStack(slot) : NULL;
            ok = slot && !MCObjectHeap_failed(h->object.heap);
            ItemStack *item = S2FPacketSetSlot_func_149174_e(packet);
            if (ok && !same_heap(h->object.heap, (MCObject *)item))
                ok = false;
            if (ok && item && (!old || old->stackSize < item->stackSize)) {
                item->animationsToGo = 5;
                MCObjectHeap_touch(h->object.heap);
            }
            if (ok)
                ok = Container_putStackInSlot(c, S2FPacketSetSlot_func_149173_d(packet),
                                              S2FPacketSetSlot_func_149174_e(packet));
        } else if (ok) {
            Container *c = open_container(h, p);
            ok = c != NULL;
            if (ok && S2FPacketSetSlot_func_149175_c(packet) == c->windowId &&
                (S2FPacketSetSlot_func_149175_c(packet) != 0 || !flag))
                ok = Container_putStackInSlot(c, S2FPacketSetSlot_func_149173_d(packet),
                                              S2FPacketSetSlot_func_149174_e(packet));
        }
    }
    return end(h, &scope, ok);
}
bool NetHandlerPlayClient_handleWindowItems(NetHandlerPlayClient *h, S30PacketWindowItems *packet) {
    MCObjectRootScope scope = {0};
    MCPacketThreadResult thread = begin(h, (MCObject *)packet, &scope);
    if (thread != MC_PACKET_THREAD_EXECUTE)
        return end(h, &scope, thread == MC_PACKET_THREAD_QUEUED);
    MCGameplayPlayer *p = current_player(h);
    bool ok = p != NULL;
    if (ok && S30PacketWindowItems_func_148911_c(packet) == 0) {
        Container *c = inventory_container(h, p);
        ok = c && Container_putStacksInSlots(c, S30PacketWindowItems_getItemStacks(packet));
    } else if (ok) {
        Container *c = open_container(h, p);
        ok = c != NULL;
        if (ok && S30PacketWindowItems_func_148911_c(packet) == c->windowId)
            ok = Container_putStacksInSlots(c, S30PacketWindowItems_getItemStacks(packet));
    }
    return end(h, &scope, ok);
}
bool NetHandlerPlayClient_handleConfirmTransaction(NetHandlerPlayClient *h,
                                                   S32PacketConfirmTransaction *packet) {
    MCObjectRootScope scope = {0};
    MCPacketThreadResult thread = begin(h, (MCObject *)packet, &scope);
    if (thread != MC_PACKET_THREAD_EXECUTE)
        return end(h, &scope, thread == MC_PACKET_THREAD_QUEUED);
    Container *container = NULL;
    MCGameplayPlayer *p = current_player(h);
    bool ok = p != NULL;
    if (ok && S32PacketConfirmTransaction_getWindowId(packet) == 0) {
        if (!same_heap(h->object.heap, (MCObject *)((ContainerPlayer *)(p->inventoryContainer))))
            ok = false;
        else
            container = ((ContainerPlayer *)(p->inventoryContainer)) ? p->inventoryContainer : NULL;
    } else if (ok) {
        Container *c = open_container(h, p);
        ok = c != NULL;
        if (ok && S32PacketConfirmTransaction_getWindowId(packet) == c->windowId)
            container = c;
    }
    if (ok && container && !S32PacketConfirmTransaction_func_148888_e(packet)) {
        C0FPacketConfirmTransaction *reply = C0FPacketConfirmTransaction_new(
            h->object.heap, S32PacketConfirmTransaction_getWindowId(packet),
            S32PacketConfirmTransaction_getActionNumber(packet), true);
        ok = reply && h->dependencies->addToSendQueue(h->dependencyContext, h, reply);
    }
    return end(h, &scope, ok);
}
bool NetHandlerPlayClient_handleEntityMetadata(NetHandlerPlayClient *h,
                                               S1CPacketEntityMetadata *packet) {
    MCObjectRootScope scope = {0};
    MCPacketThreadResult thread = begin(h, (MCObject *)packet, &scope);
    if (thread != MC_PACKET_THREAD_EXECUTE)
        return end(h, &scope, thread == MC_PACKET_THREAD_QUEUED);
    MCGameplayWorld *world = h->clientWorldController;
    if (!world || !same_heap(h->object.heap, (MCObject *)world) ||
        !MCGameplayWorld_isInstance((MCObject *)world) || !h->dependencies->getEntityByID)
        return end(h, &scope, false);
    MCObject *entity = h->dependencies->getEntityByID(h->dependencyContext, world,
                                                      S1CPacketEntityMetadata_getEntityId(packet));
    bool ok = !MCObjectHeap_failed(h->object.heap) && same_heap(h->object.heap, entity);
    WatchableObjectList *list = S1CPacketEntityMetadata_func_149376_c(packet);
    if (ok && entity && list) {
        DataWatcher *watcher = h->dependencies->getDataWatcher
                                   ? h->dependencies->getDataWatcher(h->dependencyContext, entity)
                                   : NULL;
        ok = watcher && !MCObjectHeap_failed(h->object.heap) &&
             same_heap(h->object.heap, (MCObject *)watcher) &&
             DataWatcher_isInstance((MCObject *)watcher) &&
             DataWatcher_updateWatchedObjectsFromList(watcher, list);
    }
    return end(h, &scope, ok);
}
static bool metadata_dispatch(MCObject *o, S1CPacketEntityMetadata *p) {
    return NetHandlerPlayClient_handleEntityMetadata((NetHandlerPlayClient *)o, p);
}
bool NetHandlerPlayClient_handlePlayerAbilities(NetHandlerPlayClient *h,S39PacketPlayerAbilities *packet) {
    MCObjectRootScope scope={0};MCPacketThreadResult thread=begin(h,(MCObject *)packet,&scope);
    if (thread!=MC_PACKET_THREAD_EXECUTE) return end(h,&scope,thread==MC_PACKET_THREAD_QUEUED);
    MCGameplayPlayer *player=current_player(h);
    bool ok=player && S39PacketPlayerAbilities_isInstance((MCObject *)packet) && MCObjectRootScope_pin(&scope,(MCObject *)player);
    /* Source captures the player once; each statement captures its current
       capabilities receiver before evaluating the packet getter. */
    PlayerCapabilities *caps=ok?MCGameplayPlayer_capabilities((MCObject *)player):NULL;
    if (caps) {caps->isFlying=S39PacketPlayerAbilities_isFlying(packet);MCObjectHeap_touch(h->object.heap);} else ok=false;
    caps=ok?MCGameplayPlayer_capabilities((MCObject *)player):NULL;
    if (caps) {caps->isCreativeMode=S39PacketPlayerAbilities_isCreativeMode(packet);MCObjectHeap_touch(h->object.heap);} else ok=false;
    caps=ok?MCGameplayPlayer_capabilities((MCObject *)player):NULL;
    if (caps) {caps->disableDamage=S39PacketPlayerAbilities_isInvulnerable(packet);MCObjectHeap_touch(h->object.heap);} else ok=false;
    caps=ok?MCGameplayPlayer_capabilities((MCObject *)player):NULL;
    if (caps) {caps->allowFlying=S39PacketPlayerAbilities_isAllowFlying(packet);MCObjectHeap_touch(h->object.heap);} else ok=false;
    caps=ok?MCGameplayPlayer_capabilities((MCObject *)player):NULL;
    ok=caps && PlayerCapabilities_setFlySpeed(caps,S39PacketPlayerAbilities_getFlySpeed(packet));
    caps=ok?MCGameplayPlayer_capabilities((MCObject *)player):NULL;
    ok=caps && PlayerCapabilities_setPlayerWalkSpeed(caps,S39PacketPlayerAbilities_getWalkSpeed(packet));
    return end(h,&scope,ok);
}
static bool abilities_dispatch(MCObject *h,S39PacketPlayerAbilities *p) {
    return NetHandlerPlayClient_handlePlayerAbilities((NetHandlerPlayClient *)h,p);
}
static bool close_dispatch(MCObject *o, S2EPacketCloseWindow *p) {
    return NetHandlerPlayClient_handleCloseWindow((NetHandlerPlayClient *)o, p);
}
static bool slot_dispatch(MCObject *o, S2FPacketSetSlot *p) {
    return NetHandlerPlayClient_handleSetSlot((NetHandlerPlayClient *)o, p);
}
static bool items_dispatch(MCObject *o, S30PacketWindowItems *p) {
    return NetHandlerPlayClient_handleWindowItems((NetHandlerPlayClient *)o, p);
}
static bool confirm_dispatch(MCObject *o, S32PacketConfirmTransaction *p) {
    return NetHandlerPlayClient_handleConfirmTransaction((NetHandlerPlayClient *)o, p);
}
static const INetHandlerPlayClientMethods handler_methods = {
    .handleCloseWindow = close_dispatch,
    .handleSetSlot = slot_dispatch,
    .handleWindowItems = items_dispatch,
    .handleConfirmTransaction = confirm_dispatch,
    .handleEntityMetadata = metadata_dispatch,
    .handlePlayerAbilities = abilities_dispatch};
INetHandlerPlayClient NetHandlerPlayClient_asHandler(NetHandlerPlayClient *h) {
    return (INetHandlerPlayClient){(MCObject *)h, &handler_methods};
}
