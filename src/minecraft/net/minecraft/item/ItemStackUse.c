#include "item/ItemStackUse.h"
bool ItemStack_useItemRightClick(ItemStack *s, MCObject *world, MCObject *player,
                                 const ItemStackUseDependencies *d, MCObject *ctx,
                                 ItemStack **out) {
    MCObjectHeap *h = s ? s->object.heap : NULL;
    MCObjectRootScope scope = {0};
    if (!s || !out || !ItemStack_isInstance((MCObject *)s) || !s->item || !d ||
        !d->onItemRightClick || !MCObjectRootScope_begin(&scope, h)) {
        MCObjectHeap_fail(h);
        return false;
    }
    bool ok = MCObjectRootScope_pin(&scope, (MCObject *)s) &&
              MCObjectRootScope_pin(&scope, world) && MCObjectRootScope_pin(&scope, player) &&
              MCObjectRootScope_pin(&scope, ctx);
    ItemStack *result = NULL;
    if (ok)
        ok = d->onItemRightClick(ctx, ItemStack_getItem(s), s, world, player, &result);
    ok = ok && !MCObjectHeap_failed(h) && MCObjectRootScope_pin(&scope, (MCObject *)result) &&
         (!result || ItemStack_isInstance((MCObject *)result));
    if (ok)
        *out = result;
    else
        MCObjectHeap_fail(h);
    MCObjectRootScope_end(&scope);
    return ok;
}
