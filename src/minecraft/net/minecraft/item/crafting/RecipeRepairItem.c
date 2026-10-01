#include "item/crafting/RecipeRepairItem.h"
#include <string.h>
static const MCObjectClass klass={"RecipeRepairItem",MCObjectHeap_plainClone,NULL,NULL};
RecipeRepairItem *RecipeRepairItem_new(MCObjectHeap *h) { return (RecipeRepairItem *)MCObjectHeap_alloc(h,sizeof(RecipeRepairItem),&klass); }
static bool item_damageable(ItemStack *s) {
    if (!s->item) { MCObjectHeap_fail(s->object.heap); return false; }
    return ItemStack_getMaxDamage(s)>0 && !ItemStack_getHasSubtypes(s);
}
static ItemStackList *scan(MCObjectHeap *h,InventoryCrafting *grid) {
    ItemStackList *list=ItemStackList_new(h); if (!list) return NULL;
    for (int32_t i=0;i<InventoryCrafting_getSizeInventory(grid);i++) {
        ItemStack *s=InventoryCrafting_getStackInSlot(grid,i);
        if (s) {
            if (!ItemStackList_add(list,s)) return NULL;
            if (ItemStackList_size(list)>1) {
                ItemStack *first=ItemStackList_get(list,0);
                if (s->item!=first->item || first->stackSize!=1 || s->stackSize!=1 || !item_damageable(first)) return NULL;
            }
        }
    }
    return list;
}
bool RecipeRepairItem_matches(RecipeRepairItem *r,InventoryCrafting *grid,MCObject *world) {
    (void)world; MCObjectHeap *h=r->object.heap; MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)grid); ItemStackList *list=ok?scan(h,grid):NULL; bool match=list && ItemStackList_size(list)==2;
    MCObjectRootScope_end(&scope); return match&&!MCObjectHeap_failed(h);
}
static int32_t wrap(uint32_t bits) { int32_t value; memcpy(&value,&bits,sizeof(value)); return value; }
ItemStack *RecipeRepairItem_getCraftingResult(RecipeRepairItem *r,InventoryCrafting *grid) {
    MCObjectHeap *h=r->object.heap; MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)grid); ItemStackList *list=ok?scan(h,grid):NULL; ItemStack *out=NULL;
    if (list && ItemStackList_size(list)==2) {
        ItemStack *a=ItemStackList_get(list,0),*b=ItemStackList_get(list,1);
        if (a->item==b->item && a->stackSize==1 && b->stackSize==1 && item_damageable(a)) {
            int32_t max=ItemStack_getMaxDamage(a);
            int32_t j=wrap((uint32_t)max-(uint32_t)ItemStack_getItemDamage(a));
            int32_t k=wrap((uint32_t)max-(uint32_t)ItemStack_getItemDamage(b));
            int32_t bonus=wrap((uint32_t)max*5)/100;
            int32_t total=wrap((uint32_t)j+(uint32_t)k+(uint32_t)bonus);
            int32_t damage=wrap((uint32_t)max-(uint32_t)total); if (damage<0) damage=0;
            out=ItemStack_new(h,a->item,1,damage);
        }
    }
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(h)?NULL:out;
}
int32_t RecipeRepairItem_getRecipeSize(const RecipeRepairItem *r) { (void)r; return 4; }
ItemStack *RecipeRepairItem_getRecipeOutput(RecipeRepairItem *r) { (void)r; return NULL; }
ItemStackArray *RecipeRepairItem_getRemainingItems(RecipeRepairItem *r,InventoryCrafting *grid) {
    MCObjectHeap *h=r->object.heap; MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)grid); ItemStackArray *out=ok?ItemStackArray_new(h,InventoryCrafting_getSizeInventory(grid)):NULL;
    for (int32_t i=0;out && i<out->length;i++) { ItemStack *s=InventoryCrafting_getStackInSlot(grid,i); if (s) { if (!s->item) { MCObjectHeap_fail(h); break; } const Item *container=ItemStack_registryContainerItem(s->item); if (container) { out->items[i]=ItemStack_new_item(h,container); if (!out->items[i]) break; MCObjectHeap_touch(h); } } }
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(h)?NULL:out;
}
static bool m(MCObject *o,InventoryCrafting *g,MCObject *w) { return RecipeRepairItem_matches((RecipeRepairItem *)o,g,w); }
static ItemStack *c(MCObject *o,InventoryCrafting *g,ItemStackDisplayNameDispatch d,MCObject *ctx) { (void)d; (void)ctx; return RecipeRepairItem_getCraftingResult((RecipeRepairItem *)o,g); }
static int32_t s(const MCObject *o) { return RecipeRepairItem_getRecipeSize((const RecipeRepairItem *)o); }
static ItemStack *p(MCObject *o) { return RecipeRepairItem_getRecipeOutput((RecipeRepairItem *)o); }
static ItemStackArray *l(MCObject *o,InventoryCrafting *g) { return RecipeRepairItem_getRemainingItems((RecipeRepairItem *)o,g); }
static const IRecipeMethods methods={m,c,s,p,l};
IRecipe RecipeRepairItem_asRecipe(RecipeRepairItem *r) { return (IRecipe){(MCObject *)r,&methods,IRECIPE_OTHER}; }
