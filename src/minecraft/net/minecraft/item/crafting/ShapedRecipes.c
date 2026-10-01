#include "item/crafting/ShapedRecipes.h"
#include "nbt/NBTBase.h"
#include <string.h>
void ShapedRecipes_trace(ShapedRecipes *r,MCObjectVisitor v,void *ctx) { r->recipeItems=(ItemStackArray *)v((MCObject *)r->recipeItems,ctx); r->recipeOutput=(ItemStack *)v((MCObject *)r->recipeOutput,ctx); }
static void trace(MCObject *o,MCObjectVisitor v,void *ctx) { ShapedRecipes_trace((ShapedRecipes *)o,v,ctx); }
static const MCObjectClass klass={"ShapedRecipes",MCObjectHeap_plainClone,trace,NULL};
bool ShapedRecipes_construct(ShapedRecipes *r,int32_t w,int32_t height,ItemStackArray *items,ItemStack *output) {
    MCObjectHeap *h=r->object.heap;
    if ((items&&items->object.heap!=h)||(output&&output->object.heap!=h)) { MCObjectHeap_fail(h); return false; }
    r->recipeWidth=w; r->recipeHeight=height; r->recipeItems=items; r->recipeOutput=output; MCObjectHeap_touch(h); return true;
}
ShapedRecipes *ShapedRecipes_new(MCObjectHeap *h,int32_t w,int32_t height,ItemStackArray *items,ItemStack *output) {
    ShapedRecipes *r=(ShapedRecipes *)MCObjectHeap_alloc(h,sizeof(*r),&klass); return r&&ShapedRecipes_construct(r,w,height,items,output)?r:NULL;
}
ItemStack *ShapedRecipes_getRecipeOutput(ShapedRecipes *r) { return r->recipeOutput; }
ItemStackArray *ShapedRecipes_getRemainingItems(ShapedRecipes *r,InventoryCrafting *grid) {
    MCObjectHeap *h=r->object.heap; MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)grid);
    ItemStackArray *out=ok?ItemStackArray_new(h,InventoryCrafting_getSizeInventory(grid)):NULL;
    for (int32_t i=0;out && i<out->length;i++) {
        ItemStack *s=InventoryCrafting_getStackInSlot(grid,i);
        if (s) {
            if (!s->item) { MCObjectHeap_fail(h); break; }
            const Item *container=ItemStack_registryContainerItem(s->item);
            if (container) { out->items[i]=ItemStack_new_item(h,container); if (!out->items[i]) break; MCObjectHeap_touch(h); }
        }
    }
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(h)?NULL:out;
}
static int32_t wrap(uint32_t bits) { int32_t result; memcpy(&result,&bits,sizeof(result)); return result; }
bool ShapedRecipes_checkMatch(ShapedRecipes *r,InventoryCrafting *grid,int32_t x,int32_t y,bool mirrored) {
    for (int32_t i=0;i<3;i++) for (int32_t j=0;j<3;j++) {
        int32_t k=wrap((uint32_t)i-(uint32_t)x),l=wrap((uint32_t)j-(uint32_t)y); ItemStack *expected=NULL;
        if (k>=0 && l>=0 && k<r->recipeWidth && l<r->recipeHeight) {
            int32_t index=mirrored?wrap((uint32_t)r->recipeWidth-(uint32_t)k-1+(uint32_t)l*(uint32_t)r->recipeWidth):wrap((uint32_t)k+(uint32_t)l*(uint32_t)r->recipeWidth);
            if (!r->recipeItems || index<0 || index>=r->recipeItems->length) { MCObjectHeap_fail(r->object.heap); return false; }
            expected=r->recipeItems->items[index];
        }
        ItemStack *actual=InventoryCrafting_getStackInRowAndColumn(grid,i,j);
        if (actual || expected) {
            if (!actual || !expected || expected->item!=actual->item) return false;
            if (ItemStack_getMetadata(expected)!=32767 && ItemStack_getMetadata(expected)!=ItemStack_getMetadata(actual)) return false;
        }
    }
    return true;
}
bool ShapedRecipes_matches(ShapedRecipes *r,InventoryCrafting *grid,MCObject *world) {
    (void)world; int32_t max_x=wrap(3-(uint32_t)r->recipeWidth),max_y=wrap(3-(uint32_t)r->recipeHeight);
    for (int32_t i=0;i<=max_x;i=wrap((uint32_t)i+1)) for (int32_t j=0;j<=max_y;j=wrap((uint32_t)j+1)) {
        if (ShapedRecipes_checkMatch(r,grid,i,j,true) || ShapedRecipes_checkMatch(r,grid,i,j,false)) return true;
        if (MCObjectHeap_failed(r->object.heap)) return false;
    }
    return false;
}
ItemStack *ShapedRecipes_getCraftingResult(ShapedRecipes *r,InventoryCrafting *grid) {
    MCObjectHeap *h=r->object.heap; MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)grid);
    if (ok && !r->recipeOutput) { MCObjectHeap_fail(h); ok=false; }
    ItemStack *out=ok?ItemStack_copy(h,ShapedRecipes_getRecipeOutput(r)):NULL;
    if (out && r->copyIngredientNBT) for (int32_t i=0;i<InventoryCrafting_getSizeInventory(grid);i++) {
        ItemStack *s=InventoryCrafting_getStackInSlot(grid,i);
        if (s && ItemStack_hasTagCompound(s)) {
            NBTTagCompound *tag=(NBTTagCompound *)NBTBase_copy(h,(NBTBase *)ItemStack_getTagCompound(s));
            if (!tag || !ItemStack_setTagCompound(out,tag)) { out=NULL; break; }
        }
    }
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(h)?NULL:out;
}
int32_t ShapedRecipes_getRecipeSize(const ShapedRecipes *r) { return wrap((uint32_t)r->recipeWidth*(uint32_t)r->recipeHeight); }
static bool m(MCObject *o,InventoryCrafting *g,MCObject *w) { return ShapedRecipes_matches((ShapedRecipes *)o,g,w); }
static ItemStack *c(MCObject *o,InventoryCrafting *g,ItemStackDisplayNameDispatch d,MCObject *ctx) { (void)d; (void)ctx; return ShapedRecipes_getCraftingResult((ShapedRecipes *)o,g); }
static int32_t s(const MCObject *o) { return ShapedRecipes_getRecipeSize((const ShapedRecipes *)o); }
static ItemStack *p(MCObject *o) { return ShapedRecipes_getRecipeOutput((ShapedRecipes *)o); }
static ItemStackArray *l(MCObject *o,InventoryCrafting *g) { return ShapedRecipes_getRemainingItems((ShapedRecipes *)o,g); }
static const IRecipeMethods methods={m,c,s,p,l};
IRecipe ShapedRecipes_asRecipe(ShapedRecipes *r) { return (IRecipe){(MCObject *)r,&methods,IRECIPE_SHAPED}; }
