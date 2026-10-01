#include "item/crafting/CraftingManager.h"
#include "item/crafting/RecipeBookCloning.h"
#include "crafting/crafting.h"
static void trace(MCObject *o,MCObjectVisitor v,void *ctx) { CraftingManager *m=(CraftingManager *)o; m->recipes=(RecipeList *)v((MCObject *)m->recipes,ctx); }
static const MCObjectClass klass={"CraftingManager",MCObjectHeap_plainClone,trace,NULL};
CraftingManager *CraftingManager_newEmpty(MCObjectHeap *h) {
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    CraftingManager *m=(CraftingManager *)MCObjectHeap_alloc(h,sizeof(*m),&klass);
    if (m) m->recipes=RecipeList_new(h);
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(h)?NULL:m;
}
bool CraftingManager_addRecipe(CraftingManager *m,IRecipe recipe) { return RecipeList_add(m->recipes,recipe); }
RecipeList *CraftingManager_getRecipeList(CraftingManager *m) { return m->recipes; }
static bool callable(MCObjectHeap *h,IRecipe recipe) {
    if (!recipe.instance || recipe.instance->heap!=h || !recipe.methods || !recipe.methods->matches || !recipe.methods->getCraftingResult || !recipe.methods->getRecipeSize || !recipe.methods->getRecipeOutput || !recipe.methods->getRemainingItems) { MCObjectHeap_fail(h); return false; }
    return true;
}
static IRecipe from_fact(MCObjectHeap *h,const mc_crafting_recipe_fact *f) {
    IRecipe empty={0}; ItemStack *output=ItemStack_new(h,ItemStack_registryItem(f->output),f->amount,f->damage); if (!output) return empty;
    if (f->width) {
        ItemStackArray *items=ItemStackArray_new(h,f->count); if (!items) return empty;
        for (int32_t i=0;i<items->length;i++) if (f->input[i].id>=0) { items->items[i]=ItemStack_new(h,ItemStack_registryItem(f->input[i].id),1,f->input[i].damage); if (!items->items[i]) return empty; }
        ShapedRecipes *recipe=ShapedRecipes_new(h,f->width,f->height,items,output); return recipe?ShapedRecipes_asRecipe(recipe):empty;
    }
    ItemStackList *items=ItemStackList_new(h); if (!items) return empty;
    for (int32_t i=0;i<f->count;i++) { ItemStack *item=ItemStack_new(h,ItemStack_registryItem(f->input[i].id),1,f->input[i].damage); if (!item || !ItemStackList_add(items,item)) return empty; }
    ShapelessRecipes *recipe=ShapelessRecipes_new(h,output,items); return recipe?ShapelessRecipes_asRecipe(recipe):empty;
}
CraftingManager *CraftingManager_new(MCObjectHeap *h,const IRecipe pending[CRAFTING_PENDING_COUNT]) {
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    bool ok=pending!=NULL; if (!ok) MCObjectHeap_fail(h);
    for (int i=0;ok && i<CRAFTING_PENDING_COUNT;i++) ok=callable(h,pending[i])&&MCObjectRootScope_pin(&scope,pending[i].instance);
    if (ok && mc_crafting_static_recipe_count()!=365) { MCObjectHeap_fail(h); ok=false; }
    CraftingManager *m=ok?CraftingManager_newEmpty(h):NULL;
    RecipeBookCloning *book=m?RecipeBookCloning_new(h):NULL; if (!book) ok=false;
    /* Original 1.8.9 final recipe list positions, independently verified against
       the actual registry. This adapts registration helpers and Collections.sort
       without copying their Java/resource data or changing dispatch order. */
    size_t fact_index=0;
    for (int32_t index=0;ok && index<373;index++) {
        IRecipe recipe={0};
        switch (index) {
            case 0:recipe=pending[CRAFTING_PENDING_ARMOR_DYES];break;
            case 1:recipe=pending[CRAFTING_PENDING_FIREWORKS];break;
            case 2:recipe=pending[CRAFTING_PENDING_BANNER_ADD];break;
            case 70:recipe=RecipeBookCloning_asRecipe(book);break;
            case 71:recipe=pending[CRAFTING_PENDING_MAP_CLONING];break;
            case 72:recipe=pending[CRAFTING_PENDING_MAP_EXTENDING];break;
            case 216:recipe=pending[CRAFTING_PENDING_REPAIR];break;
            case 280:recipe=pending[CRAFTING_PENDING_BANNER_DUPLICATE];break;
            default: { mc_crafting_recipe_fact fact; if (!mc_crafting_static_recipe(fact_index++,&fact)) { MCObjectHeap_fail(h); ok=false; } else recipe=from_fact(h,&fact); break; }
        }
        if (ok) ok=callable(h,recipe)&&CraftingManager_addRecipe(m,recipe);
    }
    MCObjectRootScope_end(&scope); return ok && !MCObjectHeap_failed(h)?m:NULL;
}
ItemStack *CraftingManager_findMatchingRecipe(CraftingManager *m,InventoryCrafting *grid,MCObject *world,ItemStackDisplayNameDispatch display,MCObject *display_context) {
    MCObjectRootScope scope={0}; MCObjectHeap *h=m->object.heap; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)m)&&MCObjectRootScope_pin(&scope,(MCObject *)grid)&&MCObjectRootScope_pin(&scope,world)&&MCObjectRootScope_pin(&scope,display_context); ItemStack *out=NULL;
    uint32_t expected_mod_count=RecipeList_modCount(m->recipes);
    /* ArrayList.Itr.hasNext is cursor != size; next checks modCount. The
       failure check is deliberately deferred until next, not after matches. */
    for (int32_t i=0;ok && i!=RecipeList_size(m->recipes);i++) {
        if (expected_mod_count!=RecipeList_modCount(m->recipes)) { MCObjectHeap_fail(h); ok=false; break; }
        IRecipe recipe=RecipeList_get(m->recipes,i); if (!callable(h,recipe)) { ok=false; break; }
        bool matches=recipe.methods->matches(recipe.instance,grid,world); if (MCObjectHeap_failed(h)) { ok=false; break; }
        if (matches) { out=recipe.methods->getCraftingResult(recipe.instance,grid,display,display_context); ok=MCObjectRootScope_pin(&scope,(MCObject *)out); break; }
    }
    MCObjectRootScope_end(&scope); return ok && !MCObjectHeap_failed(h)?out:NULL;
}
ItemStackArray *CraftingManager_func_180303_b(CraftingManager *m,InventoryCrafting *grid,MCObject *world) {
    MCObjectRootScope scope={0}; MCObjectHeap *h=m->object.heap; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)m)&&MCObjectRootScope_pin(&scope,(MCObject *)grid)&&MCObjectRootScope_pin(&scope,world); ItemStackArray *out=NULL; bool matched=false;
    uint32_t expected_mod_count=RecipeList_modCount(m->recipes);
    for (int32_t i=0;ok && i!=RecipeList_size(m->recipes);i++) {
        if (expected_mod_count!=RecipeList_modCount(m->recipes)) { MCObjectHeap_fail(h); ok=false; break; }
        IRecipe recipe=RecipeList_get(m->recipes,i); if (!callable(h,recipe)) { ok=false; break; }
        bool matches=recipe.methods->matches(recipe.instance,grid,world); if (MCObjectHeap_failed(h)) { ok=false; break; }
        if (matches) { matched=true; out=recipe.methods->getRemainingItems(recipe.instance,grid); ok=MCObjectRootScope_pin(&scope,(MCObject *)out); break; }
    }
    if (ok && !matched) {
        out=ItemStackArray_new(h,InventoryCrafting_getSizeInventory(grid));
        if (out) { for (int32_t i=0;i<out->length;i++) out->items[i]=InventoryCrafting_getStackInSlot(grid,i); MCObjectHeap_touch(h); }
    }
    MCObjectRootScope_end(&scope); return ok && !MCObjectHeap_failed(h)?out:NULL;
}
