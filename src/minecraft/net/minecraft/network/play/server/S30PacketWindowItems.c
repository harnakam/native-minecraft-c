#include "network/play/server/S30PacketWindowItems.h"
#include "network/play/server/native_packet.h"
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
    S30PacketWindowItems *p = (S30PacketWindowItems *)o;
    p->itemStacks = (ItemStackArray *)v((MCObject *)p->itemStacks, c);
}
static const MCObjectClass klass = {"net.minecraft.network.play.server.S30PacketWindowItems",
                                    MCObjectHeap_plainClone, trace, NULL};
S30PacketWindowItems *S30PacketWindowItems_new_empty(MCObjectHeap *heap) {
    return (S30PacketWindowItems *)MCObjectHeap_alloc(heap, sizeof(S30PacketWindowItems), &klass);
}
S30PacketWindowItems *S30PacketWindowItems_new(MCObjectHeap *heap, int32_t windowId,
                                               ContainerList *list) {
    if (!list || ((MCObject *)list)->heap != heap) {
        MCObjectHeap_fail(heap);
        return NULL;
    }
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap))
        return NULL;
    S30PacketWindowItems *p = S30PacketWindowItems_new_empty(heap);
    if (p) {
        p->windowId = windowId;
        p->itemStacks = ItemStackArray_new(heap, ContainerList_size(list));
        if (!p->itemStacks)
            p = NULL;
        else
            for (int32_t i = 0; i < p->itemStacks->length; i++) {
                ItemStack *stack = (ItemStack *)ContainerList_get(list, i);
                if (MCObjectHeap_failed(heap)) {
                    p = NULL;
                    break;
                }
                p->itemStacks->items[i] = stack ? ItemStack_copy(heap, stack) : NULL;
                if (stack && !p->itemStacks->items[i]) {
                    p = NULL;
                    break;
                }
            }
    }
    MCObjectRootScope_end(&scope);
    return p;
}
bool S30PacketWindowItems_readPacketData(S30PacketWindowItems *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    uint8_t window = mc_get_u8(b->buffer);
    if (b->buffer->failed)
        goto done;
    p->windowId = window;
    int32_t length = mc_get_i16(b->buffer);
    if (b->buffer->failed)
        goto done;
    ItemStackArray *items = ItemStackArray_new(p->object.heap, length);
    if (!items)
        goto done;
    p->itemStacks = items;
    for (int32_t i = 0; i < length; i++)
        if (!PacketBuffer_readItemStackFromBuffer(b, &items->items[i]))
            goto done;
done:
    return mc_packet_finish((MCObject *)p, b, &scope, !b->buffer->failed);
}
bool S30PacketWindowItems_writePacketData(S30PacketWindowItems *p, PacketBuffer *b) {
    MCObjectRootScope scope = {0};
    if (!mc_packet_begin((MCObject *)p, b, &scope))
        return false;
    mc_put_u8(b->buffer, (uint8_t)p->windowId);
    if (!p->itemStacks) {
        MCObjectHeap_fail(p->object.heap);
        return mc_packet_finish((MCObject *)p, b, &scope, false);
    }
    mc_put_i16(b->buffer, mc_packet_short(p->itemStacks->length));
    bool ok = !b->buffer->failed;
    for (int32_t i = 0; ok && i < p->itemStacks->length; i++)
        ok = PacketBuffer_writeItemStackToBuffer(b, p->itemStacks->items[i]);
    return mc_packet_finish((MCObject *)p, b, &scope, ok);
}
bool S30PacketWindowItems_processPacket(S30PacketWindowItems *p, INetHandlerPlayClient h) {
    MCObjectRootScope scope = {0};
    if (!mc_client_handler_begin((MCObject *)p, h, &scope))
        return false;
    bool ok = h.methods->handleWindowItems && h.methods->handleWindowItems(h.instance, p);
    return mc_packet_handler_finish((MCObject *)p, &scope, ok);
}
int32_t S30PacketWindowItems_func_148911_c(const S30PacketWindowItems *p) {
    return p->windowId;
}
ItemStackArray *S30PacketWindowItems_getItemStacks(const S30PacketWindowItems *p) {
    return p->itemStacks;
}
