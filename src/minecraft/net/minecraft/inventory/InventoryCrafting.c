#include "inventory/InventoryCrafting.h"
#include <string.h>
static void trace(MCObject *o,MCObjectVisitor visit,void *ctx) {
    InventoryCrafting *g=(InventoryCrafting *)o;
    g->stackList=(ItemStackArray *)visit((MCObject *)g->stackList,ctx);
    g->eventHandler=visit(g->eventHandler,ctx);
}
static const MCObjectClass klass={"InventoryCrafting",MCObjectHeap_plainClone,trace,NULL};
InventoryCrafting *InventoryCrafting_new(MCObjectHeap *heap,MCObject *handler,InventoryCraftingNotify notify,int32_t w,int32_t h) {
    if (handler && handler->heap!=heap) { MCObjectHeap_fail(heap); return NULL; }
    uint32_t bits=(uint32_t)w*(uint32_t)h; int32_t count; memcpy(&count,&bits,sizeof(count));
    InventoryCrafting *g=(InventoryCrafting *)MCObjectHeap_alloc(heap,sizeof(*g),&klass);
    if (!g) return NULL;
    g->stackList=ItemStackArray_new(heap,count); if (!g->stackList) return NULL;
    g->eventHandler=handler; g->onCraftMatrixChanged=notify; g->inventoryWidth=w; g->inventoryHeight=h;
    return g;
}
int32_t InventoryCrafting_getSizeInventory(const InventoryCrafting *g) { return g->stackList->length; }
static bool index_ok(InventoryCrafting *g,int32_t index) {
    if (index<0 || index>=g->stackList->length) { MCObjectHeap_fail(g->object.heap); return false; }
    return true;
}
ItemStack *InventoryCrafting_getStackInSlot(InventoryCrafting *g,int32_t index) { return index>=InventoryCrafting_getSizeInventory(g)?NULL:(index_ok(g,index)?g->stackList->items[index]:NULL); }
ItemStack *InventoryCrafting_getStackInRowAndColumn(InventoryCrafting *g,int32_t row,int32_t column) {
    if (row<0 || row>=g->inventoryWidth || column<0 || column>g->inventoryHeight) return NULL;
    uint32_t bits=(uint32_t)row+(uint32_t)column*(uint32_t)g->inventoryWidth; int32_t index; memcpy(&index,&bits,sizeof(index));
    return InventoryCrafting_getStackInSlot(g,index);
}
const char *InventoryCrafting_getName(const InventoryCrafting *g) { (void)g; return "container.crafting"; }
bool InventoryCrafting_hasCustomName(const InventoryCrafting *g) { (void)g; return false; }
InventoryDisplayName InventoryCrafting_getDisplayName(const InventoryCrafting *g) { InventoryDisplayName d={InventoryCrafting_getName(g),!InventoryCrafting_hasCustomName(g)}; return d; }
ItemStack *InventoryCrafting_removeStackFromSlot(InventoryCrafting *g,int32_t index) {
    if (!index_ok(g,index)) return NULL;
    ItemStack *s=g->stackList->items[index]; if (s) { g->stackList->items[index]=NULL; MCObjectHeap_touch(g->object.heap); } return s;
}
static bool notify(InventoryCrafting *g,ItemStack *local) {
    if (!g->eventHandler || !g->onCraftMatrixChanged) { MCObjectHeap_fail(g->object.heap); return false; }
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,g->object.heap)) return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)g) && MCObjectRootScope_pin(&scope,(MCObject *)local) && g->onCraftMatrixChanged(g->eventHandler,g);
    if (!ok) MCObjectHeap_fail(g->object.heap);
    MCObjectRootScope_end(&scope); return ok && !MCObjectHeap_failed(g->object.heap);
}
ItemStack *InventoryCrafting_decrStackSize(InventoryCrafting *g,int32_t index,int32_t count) {
    if (!index_ok(g,index)) return NULL;
    ItemStack *s=g->stackList->items[index],*out;
    if (!s) return NULL;
    if (s->stackSize<=count) { out=s; g->stackList->items[index]=NULL; }
    else { out=ItemStack_splitStack(g->object.heap,s,count); if (!out) return NULL; if (s->stackSize==0) g->stackList->items[index]=NULL; }
    MCObjectHeap_touch(g->object.heap); return notify(g,out)?out:NULL;
}
bool InventoryCrafting_setInventorySlotContents(InventoryCrafting *g,int32_t index,ItemStack *s) {
    if (!index_ok(g,index)) return false;
    if (s && s->object.heap!=g->object.heap) { MCObjectHeap_fail(g->object.heap); return false; }
    g->stackList->items[index]=s; MCObjectHeap_touch(g->object.heap); return notify(g,NULL);
}
int32_t InventoryCrafting_getInventoryStackLimit(const InventoryCrafting *g) { (void)g; return 64; }
/* These empty methods are empty in the original class, not missing hooks. */
void InventoryCrafting_markDirty(InventoryCrafting *g) { (void)g; }
bool InventoryCrafting_isUseableByPlayer(const InventoryCrafting *g,const MCObject *p) { (void)g; (void)p; return true; }
void InventoryCrafting_openInventory(InventoryCrafting *g,MCObject *p) { (void)g; (void)p; }
void InventoryCrafting_closeInventory(InventoryCrafting *g,MCObject *p) { (void)g; (void)p; }
bool InventoryCrafting_isItemValidForSlot(const InventoryCrafting *g,int32_t i,const ItemStack *s) { (void)g; (void)i; (void)s; return true; }
int32_t InventoryCrafting_getField(const InventoryCrafting *g,int32_t id) { (void)g; (void)id; return 0; }
void InventoryCrafting_setField(InventoryCrafting *g,int32_t id,int32_t value) { (void)g; (void)id; (void)value; }
int32_t InventoryCrafting_getFieldCount(const InventoryCrafting *g) { (void)g; return 0; }
void InventoryCrafting_clear(InventoryCrafting *g) { for (int32_t i=0;i<g->stackList->length;i++) g->stackList->items[i]=NULL; MCObjectHeap_touch(g->object.heap); }
int32_t InventoryCrafting_getHeight(const InventoryCrafting *g) { return g->inventoryHeight; }
int32_t InventoryCrafting_getWidth(const InventoryCrafting *g) { return g->inventoryWidth; }
