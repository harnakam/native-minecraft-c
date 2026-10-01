#include "item/crafting/ShapelessRecipes.h"
static void trace(MCObject *o,MCObjectVisitor v,void *ctx) { ShapelessRecipes *r=(ShapelessRecipes *)o; r->recipeItems=(ItemStackList *)v((MCObject *)r->recipeItems,ctx); r->recipeOutput=(ItemStack *)v((MCObject *)r->recipeOutput,ctx); }
static const MCObjectClass klass={"ShapelessRecipes",MCObjectHeap_plainClone,trace,NULL};
ShapelessRecipes *ShapelessRecipes_new(MCObjectHeap *h,ItemStack *output,ItemStackList *items) {
    if ((items&&((MCObject *)items)->heap!=h)||(output&&output->object.heap!=h)) { MCObjectHeap_fail(h); return NULL; }
    ShapelessRecipes *r=(ShapelessRecipes *)MCObjectHeap_alloc(h,sizeof(*r),&klass); if (r) { r->recipeOutput=output; r->recipeItems=items; } return r;
}
ItemStack *ShapelessRecipes_getRecipeOutput(ShapelessRecipes *r) { return r->recipeOutput; }
ItemStackArray *ShapelessRecipes_getRemainingItems(ShapelessRecipes *r,InventoryCrafting *grid) {
    MCObjectHeap *h=r->object.heap; MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)grid);
    ItemStackArray *out=ok?ItemStackArray_new(h,InventoryCrafting_getSizeInventory(grid)):NULL;
    for (int32_t i=0;out && i<out->length;i++) {
        ItemStack *s=InventoryCrafting_getStackInSlot(grid,i);
        if (s) { if (!s->item) { MCObjectHeap_fail(h); break; } const Item *container=ItemStack_registryContainerItem(s->item); if (container) { out->items[i]=ItemStack_new_item(h,container); if (!out->items[i]) break; MCObjectHeap_touch(h); } }
    }
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(h)?NULL:out;
}
bool ShapelessRecipes_matches(ShapelessRecipes *r,InventoryCrafting *grid,MCObject *world) {
    (void)world; MCObjectHeap *h=r->object.heap; MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)grid);
    if (ok && !r->recipeItems) { MCObjectHeap_fail(h); ok=false; }
    ItemStackList *list=ok?ItemStackList_new(h):NULL;
    if (list) {
        for (int32_t i=0;i<ItemStackList_size(r->recipeItems);i++) if (!ItemStackList_add(list,ItemStackList_get(r->recipeItems,i))) { ok=false; break; }
    } else ok=false;
    for (int32_t i=0;ok && i<InventoryCrafting_getHeight(grid);i++) for (int32_t j=0;ok && j<InventoryCrafting_getWidth(grid);j++) {
        ItemStack *actual=InventoryCrafting_getStackInRowAndColumn(grid,j,i);
        if (actual) {
            bool found=false;
            for (int32_t index=0;index<ItemStackList_size(list);index++) {
                ItemStack *expected=ItemStackList_get(list,index); if (!expected) { MCObjectHeap_fail(h); ok=false; break; }
                if (actual->item==expected->item && (ItemStack_getMetadata(expected)==32767 || ItemStack_getMetadata(actual)==ItemStack_getMetadata(expected))) { ItemStackList_remove(list,index); found=true; break; }
            }
            if (!found) ok=false;
        }
    }
    bool matched=ok && ItemStackList_size(list)==0; MCObjectRootScope_end(&scope); return matched && !MCObjectHeap_failed(h);
}
ItemStack *ShapelessRecipes_getCraftingResult(ShapelessRecipes *r,InventoryCrafting *grid) {
    MCObjectRootScope scope={0}; MCObjectHeap *h=r->object.heap; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)grid);
    if (ok && !r->recipeOutput) { MCObjectHeap_fail(h); ok=false; }
    ItemStack *out=ok?ItemStack_copy(h,r->recipeOutput):NULL; MCObjectRootScope_end(&scope); return out;
}
int32_t ShapelessRecipes_getRecipeSize(const ShapelessRecipes *r) { if (!r->recipeItems) { MCObjectHeap_fail(r->object.heap); return 0; } return ItemStackList_size(r->recipeItems); }
static bool m(MCObject *o,InventoryCrafting *g,MCObject *w) { return ShapelessRecipes_matches((ShapelessRecipes *)o,g,w); }
static ItemStack *c(MCObject *o,InventoryCrafting *g,ItemStackDisplayNameDispatch d,MCObject *ctx) { (void)d; (void)ctx; return ShapelessRecipes_getCraftingResult((ShapelessRecipes *)o,g); }
static int32_t s(const MCObject *o) { return ShapelessRecipes_getRecipeSize((const ShapelessRecipes *)o); }
static ItemStack *p(MCObject *o) { return ShapelessRecipes_getRecipeOutput((ShapelessRecipes *)o); }
static ItemStackArray *l(MCObject *o,InventoryCrafting *g) { return ShapelessRecipes_getRemainingItems((ShapelessRecipes *)o,g); }
static const IRecipeMethods methods={m,c,s,p,l};
IRecipe ShapelessRecipes_asRecipe(ShapelessRecipes *r) { return (IRecipe){(MCObject *)r,&methods,IRECIPE_SHAPELESS}; }
