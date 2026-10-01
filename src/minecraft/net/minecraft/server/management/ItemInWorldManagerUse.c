#include "server/management/ItemInWorldManagerUse.h"
static bool fail(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return false;
}
bool ItemInWorldManager_tryUseItem(MCGameplayPlayer *p, MCGameplayWorld *world, ItemStack *stack,
                                   const ItemInWorldManagerUseDependencies *d, MCObject *context,
                                   bool *out) {
    MCObjectHeap *heap = p ? p->object.heap : NULL;
    if (!p || !MCGameplayPlayer_isInstance((MCObject *)p) || !world ||
        !MCGameplayWorld_isInstance((MCObject *)world) || world->object.heap != heap ||
        !p->inventory || p->inventory->object.heap != heap || !out || !d || !d->isSpectator ||
        !d->isCreative || !d->itemUse || !d->getMaxItemUseDuration || !d->isUsingItem ||
        !d->sendContainerToPlayer || (context && context->heap != heap))
        return fail(heap);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap))
        return false;
    bool result = false, ok = true;
    bool spectator = d->isSpectator(context);
    ok = !MCObjectHeap_failed(heap);
    if (ok && !spectator) {
        if (!stack || stack->object.heap != heap)
            ok = fail(heap);
        int32_t count = ok ? stack->stackSize : 0, metadata = ok ? ItemStack_getMetadata(stack) : 0;
        ItemStack *used = NULL;
        if (ok)
            ok = ItemStack_useItemRightClick(stack, (MCObject *)world, (MCObject *)p, d->itemUse,
                                             context, &used);
        bool changed = ok && used != stack;
        if (ok && !changed && used) {
            changed = used->stackSize != count;
            if (!changed) {
                changed = d->getMaxItemUseDuration(context, used) > 0;
                ok = !MCObjectHeap_failed(heap);
            }
            if (ok && !changed)
                changed = ItemStack_getMetadata(used) != metadata;
        }
        if (ok && changed) {
            int32_t index = p->inventory->currentItem;
            if (index < 0 || index >= p->inventory->mainInventory->length)
                ok = fail(heap);
            else {
                p->inventory->mainInventory->items[index] = used;
                MCObjectHeap_touch(heap);
            }
            /* Preserve the original NULL dereference boundary after assignment. */
            bool creative = ok ? d->isCreative(context) : false;
            ok = ok && !MCObjectHeap_failed(heap);
            if (ok && creative) {
                if (!used)
                    ok = fail(heap);
                else {
                    used->stackSize = count;
                    MCObjectHeap_touch(heap);
                    if (ItemStack_isItemStackDamageable(used))
                        ItemStack_setItemDamage(used, metadata);
                }
            }
            if (ok && !used)
                ok = fail(heap);
            if (ok && used->stackSize == 0) {
                p->inventory->mainInventory->items[index] = NULL;
                MCObjectHeap_touch(heap);
            }
            if (ok) {
                bool usingItem = d->isUsingItem(context, p);
                ok = !MCObjectHeap_failed(heap);
                if (ok && !usingItem)
                    ok = d->sendContainerToPlayer(context, p, &p->inventoryContainer->container);
            }
            result = true;
        }
    }
    ok = ok && !MCObjectHeap_failed(heap);
    if (ok)
        *out = result;
    else
        fail(heap);
    MCObjectRootScope_end(&scope);
    return ok;
}
