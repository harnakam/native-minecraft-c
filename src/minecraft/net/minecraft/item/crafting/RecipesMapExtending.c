#include "item/crafting/RecipesMapExtending.h"
#include "nbt/NBTTagCompound.h"
#include "world/map.h"
#include <string.h>
static void trace(MCObject *o,MCObjectVisitor v,void *ctx) { ShapedRecipes_trace(&((RecipesMapExtending *)o)->shaped,v,ctx); }
static const MCObjectClass klass={"RecipesMapExtending",MCObjectHeap_plainClone,trace,NULL};
RecipesMapExtending *RecipesMapExtending_new(MCObjectHeap *h,RecipesMapExtendingGetMapData get_map) {
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    if (!get_map) { MCObjectHeap_fail(h); MCObjectRootScope_end(&scope); return NULL; }
    RecipesMapExtending *r=(RecipesMapExtending *)MCObjectHeap_alloc(h,sizeof(*r),&klass); ItemStackArray *items=r?ItemStackArray_new(h,9):NULL;
    if (items) for (int i=0;i<9;i++) { items->items[i]=ItemStack_new(h,ItemStack_registryItem(i==4?358:339),i==4?0:1,i==4?32767:0); if (!items->items[i]) break; }
    ItemStack *output=items&&!MCObjectHeap_failed(h)?ItemStack_new(h,ItemStack_registryItem(395),0,0):NULL;
    if (output && ShapedRecipes_construct(&r->shaped,3,3,items,output)) r->getMapData=get_map; else r=NULL;
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(h)?NULL:r;
}
static ItemStack *find_map(InventoryCrafting *g) {
    ItemStack *map=NULL;
    for (int32_t i=0;i<InventoryCrafting_getSizeInventory(g)&&!map;i++) { ItemStack *s=InventoryCrafting_getStackInSlot(g,i); if (s && s->item==ItemStack_registryItem(358)) map=s; }
    return map;
}
bool RecipesMapExtending_matches(RecipesMapExtending *r,InventoryCrafting *grid,MCObject *world) {
    MCObjectHeap *h=r->shaped.object.heap; MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)grid)&&MCObjectRootScope_pin(&scope,world); bool matched=false;
    if (ok && ShapedRecipes_matches(&r->shaped,grid,world)) {
        ItemStack *map=find_map(grid);
        if (map) {
            if (!r->getMapData) MCObjectHeap_fail(h);
            else {
                const mc_map_info *data=r->getMapData(world,map);
                if (data && !MCObjectHeap_failed(h)) { int8_t scale; memcpy(&scale,&data->scale,sizeof(scale)); matched=scale<4; }
            }
        }
    }
    MCObjectRootScope_end(&scope); return matched && !MCObjectHeap_failed(h);
}
ItemStack *RecipesMapExtending_getCraftingResult(RecipesMapExtending *r,InventoryCrafting *grid) {
    MCObjectHeap *h=r->shaped.object.heap; MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)grid); ItemStack *out=NULL;
    if (ok) {
        ItemStack *map=find_map(grid); if (!map) MCObjectHeap_fail(h);
        else {
            out=ItemStack_copy(h,map);
            if (out) {
                out->stackSize=1; MCObjectHeap_touch(h);
                if (!out->stackTagCompound) {
                    NBTTagCompound *tag=NBTTagCompound_new(h);
                    if (!tag || !ItemStack_setTagCompound(out,tag)) out=NULL;
                }
                if (out && !NBTTagCompound_setBoolean_ascii(out->stackTagCompound,"map_is_scaling",true)) out=NULL;
            }
        }
    }
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(h)?NULL:out;
}
static bool m(MCObject *o,InventoryCrafting *g,MCObject *w) { return RecipesMapExtending_matches((RecipesMapExtending *)o,g,w); }
static ItemStack *c(MCObject *o,InventoryCrafting *g,ItemStackDisplayNameDispatch d,MCObject *ctx) { (void)d; (void)ctx; return RecipesMapExtending_getCraftingResult((RecipesMapExtending *)o,g); }
static int32_t s(const MCObject *o) { return ShapedRecipes_getRecipeSize(&((const RecipesMapExtending *)o)->shaped); }
static ItemStack *p(MCObject *o) { return ShapedRecipes_getRecipeOutput(&((RecipesMapExtending *)o)->shaped); }
static ItemStackArray *l(MCObject *o,InventoryCrafting *g) { return ShapedRecipes_getRemainingItems(&((RecipesMapExtending *)o)->shaped,g); }
static const IRecipeMethods methods={m,c,s,p,l};
IRecipe RecipesMapExtending_asRecipe(RecipesMapExtending *r) { return (IRecipe){(MCObject *)r,&methods,IRECIPE_SHAPED}; }
