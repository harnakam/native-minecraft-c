#include "network/NetHandlerPlayServer.h"
#include <limits.h>
#include <string.h>

/* Closed native int-key/Short-value storage for the source IntHashMap field.
   This is not a translation of the general IntHashMap collection class. */
typedef struct {
    int32_t window;
    int16_t action;
} RejectedEntry;
typedef struct {
    MCObject object;
    int32_t capacity;
    RejectedEntry entries[];
} RejectedEntries;
struct NativeRejectedTransactions {
    MCObject object;
    int32_t size;
    RejectedEntries *storage;
};
static void rejected_trace(MCObject *o, MCObjectVisitor v, void *c) {
    NativeRejectedTransactions *m = (NativeRejectedTransactions *)o;
    m->storage = (RejectedEntries *)v((MCObject *)m->storage, c);
}
static const MCObjectClass entriesClass = {"native.NetHandler.RejectedEntries",
                                           MCObjectHeap_plainClone, NULL, NULL};
static const MCObjectClass mapClass = {"native.NetHandler.RejectedTransactions",
                                       MCObjectHeap_plainClone, rejected_trace, NULL};
static void handler_trace(MCObject *o, MCObjectVisitor v, void *c) {
    NetHandlerPlayServer *h = (NetHandlerPlayServer *)o;
    h->playerEntity = (MCGameplayPlayer *)v((MCObject *)h->playerEntity, c);
    h->field_147372_n = (NativeRejectedTransactions *)v((MCObject *)h->field_147372_n, c);
    h->dependencyContext = v(h->dependencyContext, c);
    h->entityActionContext = v(h->entityActionContext, c);
}
static const MCObjectClass handlerClass = {"net.minecraft.network.NetHandlerPlayServer",
                                           MCObjectHeap_plainClone, handler_trace, NULL};
bool NetHandlerPlayServer_isInstance(const MCObject *object) {
    return object && object->klass == &handlerClass;
}
static bool failed(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return false;
}
static bool completed(MCObjectHeap *heap, bool ok) {
    return ok ? !MCObjectHeap_failed(heap) : failed(heap);
}
static bool same_heap(MCObjectHeap *heap, const MCObject *o) {
    return !o || o->heap == heap || failed(heap);
}
static bool map_put(NativeRejectedTransactions *map, int32_t window, int16_t action) {
    MCObjectHeap *heap = map->object.heap;
    for (int32_t i = 0; i < map->size; i++)
        if (map->storage->entries[i].window == window) {
            map->storage->entries[i].action = action;
            MCObjectHeap_touch(heap);
            return true;
        }
    if (!map->storage || map->size == map->storage->capacity) {
        int64_t capacity = map->storage ? (int64_t)map->storage->capacity * 2 : 16;
        if (capacity > INT32_MAX ||
            (size_t)capacity > (SIZE_MAX - sizeof(RejectedEntries)) / sizeof(RejectedEntry))
            return failed(heap);
        RejectedEntries *next = (RejectedEntries *)MCObjectHeap_alloc(
            heap, sizeof(*next) + (size_t)capacity * sizeof(RejectedEntry), &entriesClass);
        if (!next)
            return false;
        next->capacity = (int32_t)capacity;
        if (map->size)
            memcpy(next->entries, map->storage->entries, (size_t)map->size * sizeof(RejectedEntry));
        map->storage = next;
    }
    map->storage->entries[map->size++] = (RejectedEntry){window, action};
    MCObjectHeap_touch(heap);
    return true;
}
bool NetHandlerPlayServer_rejectedAction(const NetHandlerPlayServer *handler, int32_t window,
                                         int16_t *action) {
    const NativeRejectedTransactions *map = handler ? handler->field_147372_n : NULL;
    if (!map)
        return false;
    for (int32_t i = 0; i < map->size; i++)
        if (map->storage->entries[i].window == window) {
            if (action)
                *action = map->storage->entries[i].action;
            return true;
        }
    return false;
}
static bool dependencies_ready(const NetHandlerPlayServerDependencies *d) {
    return d && d->checkThreadAndEnqueue && d->markPlayerActive && d->closeContainer &&
           d->sendConfirmTransaction && d->updateCraftingInventory && d->updateHeldItem &&
           d->getTileEntity && d->tileEntityWriteToNBT && d->dropPlayerItemWithRandomChoice;
}
NetHandlerPlayServer *
NetHandlerPlayServer_nativeNew(MCGameplayPlayer *player, MCObject *context,
                               const NetHandlerPlayServerDependencies *dependencies) {
    MCObjectHeap *heap = player ? player->living.entity.object.heap : NULL;
    if (!heap || !MCGameplayPlayer_isInstance((MCObject *)player) ||
        !dependencies_ready(dependencies) || !same_heap(heap, context)) {
        failed(heap);
        return NULL;
    }
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap))
        return NULL;
    NetHandlerPlayServer *handler =
        (NetHandlerPlayServer *)MCObjectHeap_alloc(heap, sizeof(*handler), &handlerClass);
    if (handler) {
        handler->playerEntity = player;
        handler->dependencyContext = context;
        handler->dependencies = dependencies;
        /* Original declaration initializer, before the unported full ctor. */
        handler->hasMoved = true;
        handler->field_147372_n = (NativeRejectedTransactions *)MCObjectHeap_alloc(
            heap, sizeof(NativeRejectedTransactions), &mapClass);
        if (handler->field_147372_n) {
            player->handler = (MCObject *)handler;
            MCObjectHeap_touch(heap);
        } else
            handler = NULL;
    }
    MCObjectRootScope_end(&scope);
    return handler;
}
bool NetHandlerPlayServer_nativeBindEntityActions(
    NetHandlerPlayServer *h, const NetHandlerPlayServerEntityActionDependencies *d,
    MCObject *context) {
    if (!NetHandlerPlayServer_isInstance((MCObject *)h) ||
        MCObjectHeap_objectSize((MCObject *)h) < sizeof(*h) ||
        !same_heap(h ? h->object.heap : NULL, context))
        return failed(h ? h->object.heap : NULL);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h->object.heap))
        return false;
    bool ok =
        MCObjectRootScope_pin(&scope, (MCObject *)h) && MCObjectRootScope_pin(&scope, context);
    if (ok) {
        h->entityActionDependencies = d;
        h->entityActionContext = context;
        MCObjectHeap_touch(h->object.heap);
    }
    MCObjectRootScope_end(&scope);
    return ok;
}

static MCPacketThreadResult begin(NetHandlerPlayServer *handler, MCObject *packet,
                                  MCObjectRootScope *scope) {
    MCObjectHeap *heap = handler ? handler->object.heap : NULL;
    if (!heap || !packet || !same_heap(heap, packet) || !handler->playerEntity ||
        !dependencies_ready(handler->dependencies)) {
        failed(heap);
        return MC_PACKET_THREAD_FAILED;
    }
    if (!MCObjectRootScope_begin(scope, heap))
        return MC_PACKET_THREAD_FAILED;
    if (!MCObjectRootScope_pin(scope, (MCObject *)handler) || !MCObjectRootScope_pin(scope, packet))
        return MC_PACKET_THREAD_FAILED;
    MCPacketThreadResult result =
        handler->dependencies->checkThreadAndEnqueue(handler->dependencyContext, handler, packet);
    if (MCObjectHeap_failed(heap) || result == MC_PACKET_THREAD_FAILED ||
        (result != MC_PACKET_THREAD_EXECUTE && result != MC_PACKET_THREAD_QUEUED)) {
        failed(heap);
        return MC_PACKET_THREAD_FAILED;
    }
    return result;
}
static bool end(NetHandlerPlayServer *handler, MCObjectRootScope *scope, bool result) {
    MCObjectHeap *heap = handler ? handler->object.heap : NULL;
    result = completed(heap, result);
    MCObjectRootScope_end(scope);
    return result;
}
static MCGameplayPlayer *action_player(NetHandlerPlayServer *h, MCObjectRootScope *scope) {
    MCGameplayPlayer *p = h->playerEntity;
    if (!MCGameplayPlayer_isInstance((MCObject *)p) || !same_heap(h->object.heap, (MCObject *)p) ||
        !MCObjectRootScope_pin(scope, (MCObject *)p)) {
        failed(h->object.heap);
        return NULL;
    }
    return p;
}
static MCObject *action_riding(NetHandlerPlayServer *h, MCObjectRootScope *scope) {
    MCGameplayPlayer *p = action_player(h, scope);
    if (!p)
        return NULL;
    MCObject *entity = (MCObject *)p->living.entity.ridingEntity;
    if (entity &&
        (!same_heap(h->object.heap, entity) || MCObjectHeap_objectSize(entity) < sizeof(Entity) ||
         !MCObjectRootScope_pin(scope, entity))) {
        failed(h->object.heap);
        return NULL;
    }
    return entity;
}
static bool action_context(NetHandlerPlayServer *h, MCObjectRootScope *scope) {
    return same_heap(h->object.heap, h->entityActionContext) &&
           MCObjectRootScope_pin(scope, h->entityActionContext);
}
bool NetHandlerPlayServer_processEntityAction(NetHandlerPlayServer *h,
                                              C0BPacketEntityAction *packet) {
    MCObjectHeap *heap = h ? h->object.heap : NULL;
    if (!NetHandlerPlayServer_isInstance((MCObject *)h) ||
        MCObjectHeap_objectSize((MCObject *)h) < sizeof(*h) ||
        !C0BPacketEntityAction_isInstance((MCObject *)packet))
        return failed(heap);
    MCObjectRootScope scope = {0};
    MCPacketThreadResult scheduled = begin(h, (MCObject *)packet, &scope);
    if (scheduled != MC_PACKET_THREAD_EXECUTE)
        return end(h, &scope, scheduled == MC_PACKET_THREAD_QUEUED);
    MCGameplayPlayer *p = action_player(h, &scope);
    if (!p || !dependencies_ready(h->dependencies) ||
        !completed(heap, h->dependencies->markPlayerActive(h->dependencyContext, p)))
        return end(h, &scope, false);
    int32_t ordinal;
    if (!C0BPacketEntityAction_nativeOrdinal(C0BPacketEntityAction_getAction(packet), &ordinal))
        return end(h, &scope, false);
    bool ok = false;
    switch (ordinal) {
    case C0B_START_SNEAKING:
    case C0B_STOP_SNEAKING:
        p = action_player(h, &scope);
        ok = p && Entity_setSneaking(&p->living.entity, ordinal == C0B_START_SNEAKING);
        break;
    case C0B_START_SPRINTING:
    case C0B_STOP_SPRINTING:
        p = action_player(h, &scope);
        ok = p && EntityLivingBase_setSprinting(&p->living, ordinal == C0B_START_SPRINTING);
        break;
    case C0B_STOP_SLEEPING:
        p = action_player(h, &scope);
        ok = p && h->entityActionDependencies && h->entityActionDependencies->wakeUpPlayer &&
             action_context(h, &scope);
        if (ok)
            ok = completed(heap, h->entityActionDependencies->wakeUpPlayer(h->entityActionContext,
                                                                           p, false, true, true));
        if (ok) {
            h->hasMoved = false;
            MCObjectHeap_touch(heap);
        }
        break;
    case C0B_RIDING_JUMP:
    case C0B_OPEN_INVENTORY: {
        MCObject *riding = action_riding(h, &scope);
        bool horse = false;
        ok = !MCObjectHeap_failed(heap);
        if (ok && riding) {
            ok = h->entityActionDependencies && h->entityActionDependencies->isEntityHorse &&
                 action_context(h, &scope);
            if (ok)
                ok = completed(heap, h->entityActionDependencies->isEntityHorse(
                                         h->entityActionContext, riding, &horse));
        }
        if (ok && horse) {
            /* The source reads ridingEntity again after instanceof, and saves
               the invocation receiver before evaluating the argument. */
            riding = action_riding(h, &scope);
            ok = riding && !MCObjectHeap_failed(heap);
            if (ok && ordinal == C0B_RIDING_JUMP) {
                ok = h->entityActionDependencies && h->entityActionDependencies->setJumpPower &&
                     action_context(h, &scope);
                if (ok)
                    ok = completed(heap, h->entityActionDependencies->setJumpPower(
                                             h->entityActionContext, riding,
                                             C0BPacketEntityAction_getAuxData(packet)));
            } else if (ok) {
                p = action_player(h, &scope);
                ok = p && h->entityActionDependencies &&
                     h->entityActionDependencies->openHorseGUI && action_context(h, &scope);
                if (ok)
                    ok = completed(heap, h->entityActionDependencies->openHorseGUI(
                                             h->entityActionContext, riding, p));
            }
        }
        break;
    }
    default:
        ok = false;
        break;
    }
    return end(h, &scope, ok);
}

static Container *open_container(NetHandlerPlayServer *handler) {
    Container *container = handler->playerEntity->openContainer;
    if (!container || !same_heap(handler->object.heap, (MCObject *)container) ||
        !handler->playerEntity->inventory) {
        failed(handler->object.heap);
        return NULL;
    }
    return container;
}
bool NetHandlerPlayServer_processCloseWindow(NetHandlerPlayServer *handler,
                                             C0DPacketCloseWindow *packet) {
    MCObjectRootScope scope = {0};
    MCPacketThreadResult scheduled = begin(handler, (MCObject *)packet, &scope);
    if (scheduled != MC_PACKET_THREAD_EXECUTE)
        return end(handler, &scope, scheduled == MC_PACKET_THREAD_QUEUED);
    /* The source ignores packetIn.windowId. */
    return end(
        handler, &scope,
        handler->dependencies->closeContainer(handler->dependencyContext, handler->playerEntity));
}
static bool update_inventory(NetHandlerPlayServer *handler) {
    Container *container = open_container(handler);
    if (!container)
        return false;
    /* Container.getInventory builds the same nullable direct-reference list as
       the two source loops; packet constructors perform their own copies. */
    ContainerList *list = Container_getInventory(container);
    if (!list)
        return false;
    return completed(handler->object.heap,
                     handler->dependencies->updateCraftingInventory(
                         handler->dependencyContext, handler->playerEntity, container, list));
}
bool NetHandlerPlayServer_processClickWindow(NetHandlerPlayServer *handler,
                                             C0EPacketClickWindow *packet) {
    MCObjectRootScope scope = {0};
    MCPacketThreadResult scheduled = begin(handler, (MCObject *)packet, &scope);
    if (scheduled != MC_PACKET_THREAD_EXECUTE)
        return end(handler, &scope, scheduled == MC_PACKET_THREAD_QUEUED);
    MCObjectHeap *heap = handler->object.heap;
    MCGameplayPlayer *player = handler->playerEntity;
    const NetHandlerPlayServerDependencies *d = handler->dependencies;
    if (!completed(heap, d->markPlayerActive(handler->dependencyContext, player)))
        return end(handler, &scope, false);
    Container *container = open_container(handler);
    if (!container)
        return end(handler, &scope, false);
    if (container->windowId == C0EPacketClickWindow_getWindowId(packet) &&
        Container_getCanCraft(container, (MCObject *)player)) {
        if (player->spectator) {
            if (!update_inventory(handler))
                return end(handler, &scope, false);
        } else {
            ItemStack *returned =
                Container_slotClick(container, C0EPacketClickWindow_getSlotId(packet),
                                    C0EPacketClickWindow_getUsedButton(packet),
                                    C0EPacketClickWindow_getMode(packet), player->inventory);
            if (MCObjectHeap_failed(heap))
                return end(handler, &scope, false);
            if (ItemStack_areItemStacksEqual(C0EPacketClickWindow_getClickedItem(packet),
                                             returned)) {
                if (!completed(heap, d->sendConfirmTransaction(
                                         handler->dependencyContext, player,
                                         C0EPacketClickWindow_getWindowId(packet),
                                         C0EPacketClickWindow_getActionNumber(packet), true)))
                    return end(handler, &scope, false);
                player->isChangingQuantityOnly = true;
                container = open_container(handler);
                if (!container || !Container_detectAndSendChanges(container) ||
                    !completed(heap, d->updateHeldItem(handler->dependencyContext, player)))
                    return end(handler, &scope, false);
                player->isChangingQuantityOnly = false;
            } else {
                container = open_container(handler);
                if (!container || !map_put(handler->field_147372_n, container->windowId,
                                           C0EPacketClickWindow_getActionNumber(packet)))
                    return end(handler, &scope, false);
                if (!completed(heap, d->sendConfirmTransaction(
                                         handler->dependencyContext, player,
                                         C0EPacketClickWindow_getWindowId(packet),
                                         C0EPacketClickWindow_getActionNumber(packet), false)))
                    return end(handler, &scope, false);
                container = open_container(handler);
                if (!container || !Container_setCanCraft(container, (MCObject *)player, false) ||
                    !update_inventory(handler))
                    return end(handler, &scope, false);
            }
        }
    }
    return end(handler, &scope, true);
}
bool NetHandlerPlayServer_processConfirmTransaction(NetHandlerPlayServer *handler,
                                                    C0FPacketConfirmTransaction *packet) {
    MCObjectRootScope scope = {0};
    MCPacketThreadResult scheduled = begin(handler, (MCObject *)packet, &scope);
    if (scheduled != MC_PACKET_THREAD_EXECUTE)
        return end(handler, &scope, scheduled == MC_PACKET_THREAD_QUEUED);
    Container *container = open_container(handler);
    if (!container)
        return end(handler, &scope, false);
    MCGameplayPlayer *player = handler->playerEntity;
    int16_t action;
    if (NetHandlerPlayServer_rejectedAction(handler, container->windowId, &action) &&
        C0FPacketConfirmTransaction_getUid(packet) == action &&
        container->windowId == C0FPacketConfirmTransaction_getWindowId(packet) &&
        !Container_getCanCraft(container, (MCObject *)player) && !player->spectator) {
        if (!Container_setCanCraft(container, (MCObject *)player, true))
            return end(handler, &scope, false);
    }
    /* packetIn.accepted is intentionally unused in the original method. */
    return end(handler, &scope, true);
}
static bool creative_tile_tag(NetHandlerPlayServer *handler, ItemStack *stack) {
    MCObjectHeap *heap = handler->object.heap;
    const NetHandlerPlayServerDependencies *d = handler->dependencies;
    if (stack && ItemStack_hasTagCompound(stack) &&
        NBTTagCompound_hasKeyType_ascii(ItemStack_getTagCompound(stack), "BlockEntityTag", 10)) {
        NBTTagCompound *tag =
            NBTTagCompound_getCompoundTag_ascii(ItemStack_getTagCompound(stack), "BlockEntityTag");
        if (NBTTagCompound_hasKey_ascii(tag, "x") && NBTTagCompound_hasKey_ascii(tag, "y") &&
            NBTTagCompound_hasKey_ascii(tag, "z")) {
            MCObject *tile =
                d->getTileEntity(handler->dependencyContext, ((MCGameplayWorld *)(handler->playerEntity->living.entity.worldObj)),
                                 NBTTagCompound_getInteger_ascii(tag, "x"),
                                 NBTTagCompound_getInteger_ascii(tag, "y"),
                                 NBTTagCompound_getInteger_ascii(tag, "z"));
            if (MCObjectHeap_failed(heap) || !same_heap(heap, tile))
                return false;
            if (tile) {
                NBTTagCompound *output = NBTTagCompound_new(heap);
                if (!output)
                    return false;
                if (!completed(heap,
                               d->tileEntityWriteToNBT(handler->dependencyContext, tile, output)) ||
                    !NBTTagCompound_removeTag_ascii(output, "x") ||
                    !NBTTagCompound_removeTag_ascii(output, "y") ||
                    !NBTTagCompound_removeTag_ascii(output, "z") ||
                    !ItemStack_setTagInfo_ascii(stack, "BlockEntityTag", (NBTBase *)output))
                    return false;
            }
        }
    }
    return !MCObjectHeap_failed(heap);
}
static int32_t add_twenty(int32_t value) {
    uint32_t bits = (uint32_t)value + 20u;
    return bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)(UINT32_MAX - bits);
}
bool NetHandlerPlayServer_processCreativeInventoryAction(NetHandlerPlayServer *handler,
                                                         C10PacketCreativeInventoryAction *packet) {
    MCObjectRootScope scope = {0};
    MCPacketThreadResult scheduled = begin(handler, (MCObject *)packet, &scope);
    if (scheduled != MC_PACKET_THREAD_EXECUTE)
        return end(handler, &scope, scheduled == MC_PACKET_THREAD_QUEUED);
    MCObjectHeap *heap = handler->object.heap;
    MCGameplayPlayer *player = handler->playerEntity;
    if (MCGameplayPlayer_isCreativeMode((MCObject *)player)) {
        bool flag = C10PacketCreativeInventoryAction_getSlotId(packet) < 0;
        ItemStack *stack = C10PacketCreativeInventoryAction_getStack(packet);
        if (!same_heap(heap, (MCObject *)stack) || !creative_tile_tag(handler, stack))
            return end(handler, &scope, false);
        int32_t slot = C10PacketCreativeInventoryAction_getSlotId(packet);
        bool flag1 = slot >= 1 && slot < 36 + InventoryPlayer_getHotbarSize();
        bool flag2 = !stack || ItemStack_getItem(stack) != NULL;
        bool flag3 = !stack || (ItemStack_getMetadata(stack) >= 0 && stack->stackSize <= 64 &&
                                stack->stackSize > 0);
        if (flag1 && flag2 && flag3) {
            if (!((ContainerPlayer *)(player->inventoryContainer)) ||
                !Container_putStackInSlot(player->inventoryContainer, slot, stack) ||
                !Container_setCanCraft(player->inventoryContainer, (MCObject *)player,
                                       true))
                return end(handler, &scope, false);
        } else if (flag && flag2 && flag3 && handler->itemDropThreshold < 200) {
            handler->itemDropThreshold = add_twenty(handler->itemDropThreshold);
            EntityItem *entity = handler->dependencies->dropPlayerItemWithRandomChoice(
                handler->dependencyContext, player, stack, true);
            if (MCObjectHeap_failed(heap) || !same_heap(heap, (MCObject *)entity))
                return end(handler, &scope, false);
            if (entity)
                EntityItem_setAgeToCreativeDespawnTime(entity);
        }
    }
    return end(handler, &scope, true);
}
bool NetHandlerPlayServer_processPlayerAbilities(NetHandlerPlayServer *h,C13PacketPlayerAbilities *packet) {
    MCObjectRootScope scope={0};MCPacketThreadResult scheduled=begin(h,(MCObject *)packet,&scope);
    if (scheduled!=MC_PACKET_THREAD_EXECUTE) return end(h,&scope,scheduled==MC_PACKET_THREAD_QUEUED);
    MCGameplayPlayer *player=h->playerEntity;
    bool ok=MCGameplayPlayer_isInstance((MCObject *)player) && same_heap(h->object.heap,(MCObject *)player) && MCObjectRootScope_pin(&scope,(MCObject *)player);
    PlayerCapabilities *target=ok?MCGameplayPlayer_capabilities((MCObject *)player):NULL;
    ok=target && C13PacketPlayerAbilities_isInstance((MCObject *)packet);
    bool flying=ok && C13PacketPlayerAbilities_isFlying(packet);
    if (flying) {
        player=h->playerEntity;
        PlayerCapabilities *current=MCGameplayPlayer_isInstance((MCObject *)player) && same_heap(h->object.heap,(MCObject *)player)?MCGameplayPlayer_capabilities((MCObject *)player):NULL;
        if (current) flying=current->allowFlying;else ok=false;
    }
    if (ok) {target->isFlying=flying;MCObjectHeap_touch(h->object.heap);}
    return end(h,&scope,ok);
}
static bool abilities_dispatch(MCObject *h,C13PacketPlayerAbilities *p) {
    return NetHandlerPlayServer_processPlayerAbilities((NetHandlerPlayServer *)h,p);
}
static bool entity_action_dispatch(MCObject *h,C0BPacketEntityAction *p) {
    return NetHandlerPlayServer_processEntityAction((NetHandlerPlayServer *)h,p);
}
static bool close_dispatch(MCObject *h, C0DPacketCloseWindow *p) {
    return NetHandlerPlayServer_processCloseWindow((NetHandlerPlayServer *)h, p);
}
static bool click_dispatch(MCObject *h, C0EPacketClickWindow *p) {
    return NetHandlerPlayServer_processClickWindow((NetHandlerPlayServer *)h, p);
}
static bool confirm_dispatch(MCObject *h, C0FPacketConfirmTransaction *p) {
    return NetHandlerPlayServer_processConfirmTransaction((NetHandlerPlayServer *)h, p);
}
static bool creative_dispatch(MCObject *h, C10PacketCreativeInventoryAction *p) {
    return NetHandlerPlayServer_processCreativeInventoryAction((NetHandlerPlayServer *)h, p);
}
INetHandlerPlayServer NetHandlerPlayServer_asHandler(NetHandlerPlayServer *handler) {
    static const INetHandlerPlayServerMethods methods = {
        .processCloseWindow=close_dispatch,
        .processClickWindow=click_dispatch,
        .processConfirmTransaction=confirm_dispatch,
        .processCreativeInventoryAction=creative_dispatch,
        .processPlayerAbilities=abilities_dispatch,
        .processEntityAction=entity_action_dispatch};
    return (INetHandlerPlayServer){(MCObject *)handler, &methods};
}
