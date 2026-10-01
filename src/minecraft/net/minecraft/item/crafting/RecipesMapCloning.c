#include "item/crafting/RecipesMapCloning.h"
#include <string.h>
static const MCObjectClass klass={"RecipesMapCloning",MCObjectHeap_plainClone,NULL,NULL};
RecipesMapCloning *RecipesMapCloning_new(MCObjectHeap *h) { return (RecipesMapCloning *)MCObjectHeap_alloc(h,sizeof(RecipesMapCloning),&klass); }
static ItemStack *scan(InventoryCrafting *grid,int32_t *blank) {
    ItemStack *filled=NULL; *blank=0;
    for (int32_t i=0;i<InventoryCrafting_getSizeInventory(grid);i++) {
        ItemStack *s=InventoryCrafting_getStackInSlot(grid,i);
        if (s) {
            if (s->item==ItemStack_registryItem(358)) { if (filled) return NULL; filled=s; }
            else { if (s->item!=ItemStack_registryItem(395)) return NULL; ++*blank; }
        }
    }
    return filled;
}
bool RecipesMapCloning_matches(RecipesMapCloning *r,InventoryCrafting *grid,MCObject *world) { (void)r; (void)world; int32_t blank; ItemStack *filled=scan(grid,&blank); return filled&&blank>0; }
ItemStack *RecipesMapCloning_getCraftingResult(RecipesMapCloning *r,InventoryCrafting *grid,ItemStackDisplayNameDispatch display,MCObject *context) {
    MCObjectHeap *h=r->object.heap; MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)grid)&&MCObjectRootScope_pin(&scope,context); ItemStack *out=NULL;
    if (ok) {
        int32_t blank; ItemStack *filled=scan(grid,&blank);
        if (filled && blank>=1) {
            uint32_t bits=(uint32_t)blank+1; int32_t count; memcpy(&count,&bits,sizeof(count));
            out=ItemStack_new(h,ItemStack_registryItem(358),count,ItemStack_getMetadata(filled));
            if (out && ItemStack_hasDisplayName(filled)) {
                NBTString *name=ItemStack_getDisplayName(filled,display,context);
                if (!name || !ItemStack_setStackDisplayName(out,name)) out=NULL;
            }
        }
    }
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(h)?NULL:out;
}
int32_t RecipesMapCloning_getRecipeSize(const RecipesMapCloning *r) { (void)r; return 9; }
ItemStack *RecipesMapCloning_getRecipeOutput(RecipesMapCloning *r) { (void)r; return NULL; }
ItemStackArray *RecipesMapCloning_getRemainingItems(RecipesMapCloning *r,InventoryCrafting *grid) {
    MCObjectHeap *h=r->object.heap; MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)grid); ItemStackArray *out=ok?ItemStackArray_new(h,InventoryCrafting_getSizeInventory(grid)):NULL;
    for (int32_t i=0;out && i<out->length;i++) {
        ItemStack *s=InventoryCrafting_getStackInSlot(grid,i);
        if (s) { if (!s->item) { MCObjectHeap_fail(h); break; } const Item *container=ItemStack_registryContainerItem(s->item); if (container) { out->items[i]=ItemStack_new_item(h,container); if (!out->items[i]) break; MCObjectHeap_touch(h); } }
    }
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(h)?NULL:out;
}
static bool m(MCObject *o,InventoryCrafting *g,MCObject *w) { return RecipesMapCloning_matches((RecipesMapCloning *)o,g,w); }
static ItemStack *c(MCObject *o,InventoryCrafting *g,ItemStackDisplayNameDispatch d,MCObject *ctx) { return RecipesMapCloning_getCraftingResult((RecipesMapCloning *)o,g,d,ctx); }
static int32_t s(const MCObject *o) { return RecipesMapCloning_getRecipeSize((const RecipesMapCloning *)o); }
static ItemStack *p(MCObject *o) { return RecipesMapCloning_getRecipeOutput((RecipesMapCloning *)o); }
static ItemStackArray *l(MCObject *o,InventoryCrafting *g) { return RecipesMapCloning_getRemainingItems((RecipesMapCloning *)o,g); }
static const IRecipeMethods methods={m,c,s,p,l};
IRecipe RecipesMapCloning_asRecipe(RecipesMapCloning *r) { return (IRecipe){(MCObject *)r,&methods,IRECIPE_OTHER}; }
