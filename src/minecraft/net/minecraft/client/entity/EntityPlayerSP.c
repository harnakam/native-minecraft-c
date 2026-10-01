#include "client/entity/EntityPlayerSP.h"

static void trace(MCObject *o, MCObjectVisitor v, void *ctx) {
    EntityPlayerSP *p = (EntityPlayerSP *)o;
    p->nativeActor = (MCGameplayPlayer *)v((MCObject *)p->nativeActor, ctx);
    p->sendQueue = (NetHandlerPlayClient *)v((MCObject *)p->sendQueue, ctx);
    p->mc = v(p->mc, ctx);
    p->dependencyContext = v(p->dependencyContext, ctx);
}
static const MCObjectClass klass = {"net.minecraft.client.entity.EntityPlayerSP",
                                    MCObjectHeap_plainClone, trace, NULL};
bool EntityPlayerSP_isInstance(const MCObject *o) { return o && o->klass == &klass; }
static bool failure(MCObjectHeap *h) {
    MCObjectHeap_fail(h);
    return false;
}
static bool begin(EntityPlayerSP *p, MCObjectRootScope *scope) {
    if (!p || !EntityPlayerSP_isInstance((MCObject *)p))
        return failure(p ? p->object.heap : NULL);
    return MCObjectRootScope_begin(scope, p->object.heap) &&
           MCObjectRootScope_pin(scope, (MCObject *)p);
}
static bool end(EntityPlayerSP *p, MCObjectRootScope *scope, bool ok) {
    ok = ok && !MCObjectHeap_failed(p->object.heap);
    if (!ok)
        failure(p->object.heap);
    MCObjectRootScope_end(scope);
    return ok;
}
EntityPlayerSP *EntityPlayerSP_nativeNew(MCGameplayPlayer *actor, NetHandlerPlayClient *handler,
                                         MCObject *mc, MCObject *ctx,
                                         const EntityPlayerSPDependencies *d) {
    MCObjectHeap *h = actor ? actor->living.entity.object.heap : NULL;
    MCObjectRootScope scope = {0};
    if (!MCGameplayPlayer_isInstance((MCObject *)actor) ||
        !NetHandlerPlayClient_isInstance((MCObject *)handler) || !mc || !d || !d->addToSendQueue ||
        !d->blockPosOrigin || !d->displayGuiScreenNull || !MCObjectRootScope_begin(&scope, h)) {
        failure(h);
        return NULL;
    }
    bool ok = MCObjectRootScope_pin(&scope, (MCObject *)actor) &&
              MCObjectRootScope_pin(&scope, (MCObject *)handler) &&
              MCObjectRootScope_pin(&scope, mc) && MCObjectRootScope_pin(&scope, ctx);
    EntityPlayerSP *p = ok ? (EntityPlayerSP *)MCObjectHeap_alloc(h, sizeof(*p), &klass) : NULL;
    if (p) {
        p->nativeActor = actor;
        p->sendQueue = handler;
        p->mc = mc;
        p->dependencyContext = ctx;
        p->dependencies = d;
    }
    if (!p)
        failure(h);
    MCObjectRootScope_end(&scope);
    return p;
}
static bool queue_packet(EntityPlayerSP *p, MCObject *packet, MCObjectRootScope *scope) {
    return packet && p->dependencies && p->dependencies->addToSendQueue &&
           NetHandlerPlayClient_isInstance((MCObject *)p->sendQueue) &&
           MCObjectRootScope_pin(scope, (MCObject *)p->sendQueue) &&
           MCObjectRootScope_pin(scope, p->dependencyContext) &&
           p->dependencies->addToSendQueue(p->dependencyContext, p->sendQueue, packet);
}
bool EntityPlayerSP_dropOneItem(EntityPlayerSP *p, bool all, EntityItem **out) {
    MCObjectRootScope scope = {0};
    if (!out || !begin(p, &scope))
        return false;
    bool ok = p->dependencies && p->dependencies->blockPosOrigin &&
              MCObjectRootScope_pin(&scope, p->dependencyContext);
    DataWatcherBlockPos *pos = ok ? p->dependencies->blockPosOrigin(p->dependencyContext) : NULL;
    ok = ok && !MCObjectHeap_failed(p->object.heap) && pos &&
         DataWatcher_blockPosIsInstance((MCObject *)pos) &&
         MCObjectRootScope_pin(&scope, (MCObject *)pos);
    C07PacketPlayerDigging *packet =
        ok ? C07PacketPlayerDigging_new(
                 p->object.heap,
                 C07PacketPlayerDigging_action(all ? C07_DROP_ALL_ITEMS : C07_DROP_ITEM), pos,
                 C07PacketPlayerDigging_facing(0))
           : NULL;
    ok = queue_packet(p, (MCObject *)packet, &scope);
    if (ok && !MCObjectHeap_failed(p->object.heap))
        *out = NULL;
    return end(p, &scope, ok);
}
bool EntityPlayerSP_closeScreen(EntityPlayerSP *p) {
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    MCGameplayPlayer *actor = p->nativeActor;
    bool ok = MCGameplayPlayer_isInstance((MCObject *)actor) &&
              MCObjectRootScope_pin(&scope, (MCObject *)actor) && actor->openContainer &&
              MCObjectRootScope_pin(&scope, (MCObject *)actor->openContainer);
    C0DPacketCloseWindow *packet =
        ok ? C0DPacketCloseWindow_new(p->object.heap, actor->openContainer->windowId) : NULL;
    ok = queue_packet(p, (MCObject *)packet, &scope) && !MCObjectHeap_failed(p->object.heap);
    if (ok)
        ok = EntityPlayerSP_closeScreenAndDropStack(p);
    return end(p, &scope, ok);
}
bool EntityPlayerSP_closeScreenAndDropStack(EntityPlayerSP *p) {
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    MCGameplayPlayer *actor = p->nativeActor;
    bool ok = MCGameplayPlayer_isInstance((MCObject *)actor) &&
              MCObjectRootScope_pin(&scope, (MCObject *)actor) && actor->inventory &&
              MCObjectRootScope_pin(&scope, (MCObject *)actor->inventory);
    if (ok)
        ok = InventoryPlayer_setItemStack(actor->inventory, NULL);
    if (ok) {
        /* Original EntityPlayer.closeScreen superclass body: only this assignment.
           Minecraft's GUI dependency subsequently closes its retained old GUI. */
        actor->openContainer =
            ((ContainerPlayer *)(actor->inventoryContainer)) ? actor->inventoryContainer : NULL;
        MCObjectHeap_touch(p->object.heap);
        ok = p->dependencies && p->dependencies->displayGuiScreenNull && p->mc &&
             MCObjectRootScope_pin(&scope, p->mc) &&
             MCObjectRootScope_pin(&scope, p->dependencyContext) &&
             p->dependencies->displayGuiScreenNull(p->dependencyContext, p->mc);
    }
    return end(p, &scope, ok);
}
bool EntityPlayerSP_joinEntityItemWithWorld(EntityPlayerSP *p, EntityItem *item) {
    (void)item;
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    /* The original SP override has an empty body, including for a NULL argument. */
    return end(p, &scope, true);
}
bool EntityPlayerSP_addStat(EntityPlayerSP *p, StatBase *stat, int32_t amount) {
    (void)amount;
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    bool ok = MCObjectRootScope_pin(&scope, (MCObject *)stat);
    if (ok && stat && stat->isIndependent) {
        /* Original EntityPlayer.addStat superclass body is empty. */
    }
    return end(p, &scope, ok);
}
bool EntityPlayerSP_triggerAchievement(EntityPlayerSP *p, StatBase *stat) {
    /* Original inherited EntityPlayer method dynamically calls this.addStat. */
    return EntityPlayerSP_addStat(p, stat, 1);
}
