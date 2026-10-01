#include "inventory/InventoryCraftResult.h"
static void trace(MCObject *o,MCObjectVisitor visit,void *ctx) { InventoryCraftResult *r=(InventoryCraftResult *)o; r->stackResult=(ItemStackArray *)visit((MCObject *)r->stackResult,ctx); }
static const MCObjectClass klass={"InventoryCraftResult",MCObjectHeap_plainClone,trace,NULL};
InventoryCraftResult *InventoryCraftResult_new(MCObjectHeap *h) { InventoryCraftResult *r=(InventoryCraftResult *)MCObjectHeap_alloc(h,sizeof(*r),&klass); if (r) { r->stackResult=ItemStackArray_new(h,1); if (!r->stackResult) return NULL; } return r; }
int32_t InventoryCraftResult_getSizeInventory(const InventoryCraftResult *r) { (void)r; return 1; }
ItemStack *InventoryCraftResult_getStackInSlot(const InventoryCraftResult *r,int32_t index) { (void)index; return r->stackResult->items[0]; }
const char *InventoryCraftResult_getName(const InventoryCraftResult *r) { (void)r; return "Result"; }
bool InventoryCraftResult_hasCustomName(const InventoryCraftResult *r) { (void)r; return false; }
InventoryDisplayName InventoryCraftResult_getDisplayName(const InventoryCraftResult *r) { InventoryDisplayName d={InventoryCraftResult_getName(r),!InventoryCraftResult_hasCustomName(r)}; return d; }
ItemStack *InventoryCraftResult_removeStackFromSlot(InventoryCraftResult *r,int32_t index) { (void)index; ItemStack *s=r->stackResult->items[0]; if (s) { r->stackResult->items[0]=NULL; MCObjectHeap_touch(r->object.heap); } return s; }
ItemStack *InventoryCraftResult_decrStackSize(InventoryCraftResult *r,int32_t index,int32_t count) { (void)count; return InventoryCraftResult_removeStackFromSlot(r,index); }
bool InventoryCraftResult_setInventorySlotContents(InventoryCraftResult *r,int32_t index,ItemStack *s) { (void)index; if (s&&s->object.heap!=r->object.heap) { MCObjectHeap_fail(r->object.heap); return false; } r->stackResult->items[0]=s; MCObjectHeap_touch(r->object.heap); return true; }
int32_t InventoryCraftResult_getInventoryStackLimit(const InventoryCraftResult *r) { (void)r; return 64; }
/* Original IInventory methods intentionally empty or constant. */
void InventoryCraftResult_markDirty(InventoryCraftResult *r) { (void)r; }
bool InventoryCraftResult_isUseableByPlayer(const InventoryCraftResult *r,const MCObject *p) { (void)r; (void)p; return true; }
void InventoryCraftResult_openInventory(InventoryCraftResult *r,MCObject *p) { (void)r; (void)p; }
void InventoryCraftResult_closeInventory(InventoryCraftResult *r,MCObject *p) { (void)r; (void)p; }
bool InventoryCraftResult_isItemValidForSlot(const InventoryCraftResult *r,int32_t i,const ItemStack *s) { (void)r; (void)i; (void)s; return true; }
int32_t InventoryCraftResult_getField(const InventoryCraftResult *r,int32_t id) { (void)r; (void)id; return 0; }
void InventoryCraftResult_setField(InventoryCraftResult *r,int32_t id,int32_t value) { (void)r; (void)id; (void)value; }
int32_t InventoryCraftResult_getFieldCount(const InventoryCraftResult *r) { (void)r; return 0; }
void InventoryCraftResult_clear(InventoryCraftResult *r) { r->stackResult->items[0]=NULL; MCObjectHeap_touch(r->object.heap); }
