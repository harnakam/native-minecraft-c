#include "item/crafting/RecipesMapCloning.h"
#include "item/crafting/RecipesMapExtending.h"
#include "item/crafting/RecipeRepairItem.h"
#include "item/crafting/CraftingManager.h"
#include "nbt/NBTTagCompound.h"
#include "world/map.h"
#include <limits.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"dynamic source recipes line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct {
    MCObject object; mc_map_info map; bool missing,fail;
    int32_t mapCalls,displayCalls,changedDamage;
    ItemStack *last; NBTString *baseName;
} World;
static void trace(MCObject *o,MCObjectVisitor v,void *ctx) { World *w=(World *)o; w->last=(ItemStack *)v((MCObject *)w->last,ctx); w->baseName=(NBTString *)v((MCObject *)w->baseName,ctx); }
static const MCObjectClass world_class={"dynamic recipe real-field test World",MCObjectHeap_plainClone,trace,NULL};
static const MCObjectClass filler_class={"allocation budget test object",MCObjectHeap_plainClone,NULL,NULL};
static bool notify(MCObject *o,InventoryCrafting *g) { (void)o; (void)g; return true; }
static World *world_new(MCObjectHeap *h) { World *w=(World *)MCObjectHeap_alloc(h,sizeof(*w),&world_class); CHECK(w); w->changedDamage=-1; w->baseName=NBTString_fromASCII(h,"base name"); CHECK(w->baseName); return w; }
static InventoryCrafting *grid_new(MCObjectHeap *h,World *w,int width) { InventoryCrafting *g=InventoryCrafting_new(h,(MCObject *)w,notify,width,width); CHECK(g); return g; }
static ItemStack *stack(MCObjectHeap *h,int id,int32_t n,int32_t damage) { ItemStack *s=ItemStack_new(h,ItemStack_registryItem(id),n,damage); CHECK(s); return s; }
static void put(InventoryCrafting *g,int index,ItemStack *s) { CHECK(InventoryCrafting_setInventorySlotContents(g,index,s)); }
static NBTString *display(MCObject *o,const ItemStack *s) { World *w=(World *)o; w->displayCalls++; w->last=(ItemStack *)s; MCObjectHeap_touch(o->heap); return w->baseName; }
static const mc_map_info *get_map(MCObject *o,ItemStack *s) { World *w=(World *)o; CHECK(s); CHECK(MCObjectHeap_hasBorrowers(o->heap)); w->mapCalls++; w->last=s; if (w->changedDamage>=0) ItemStack_setItemDamage(s,w->changedDamage); MCObjectHeap_touch(o->heap); if (w->fail) { MCObjectHeap_fail(o->heap); return NULL; } return w->missing?NULL:&w->map; }
static void map_clone(void) {
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); World *w=world_new(h); InventoryCrafting *g=grid_new(h,w,3); RecipesMapCloning *r=RecipesMapCloning_new(h); CHECK(r);
    ItemStack *source=stack(h,358,0,1234); NBTTagCompound *tag=NBTTagCompound_new(h); CHECK(tag&&NBTTagCompound_setInteger_ascii(tag,"custom",7)&&ItemStack_setTagCompound(source,tag)); put(g,4,source);
    for (int i=0;i<9;i++) if (i!=4) put(g,i,stack(h,395,-1,7));
    CHECK(RecipesMapCloning_matches(r,g,(MCObject *)w)); ItemStack *out=RecipesMapCloning_getCraftingResult(r,g,display,(MCObject *)w); CHECK(out&&out!=source&&out->stackSize==9&&out->itemDamage==1234&&!out->stackTagCompound&&w->displayCalls==0); /* only custom Name, not arbitrary source NBT, survives */
    CHECK(ItemStack_setStackDisplayName(source,NBTString_fromUTF8(h,"地図"))); out=RecipesMapCloning_getCraftingResult(r,g,display,(MCObject *)w); CHECK(out&&out->stackTagCompound&&out->stackTagCompound!=tag&&ItemStack_hasDisplayName(out)); CHECK(NBTString_equals(ItemStack_getDisplayName(out,display,(MCObject *)w),NBTTagCompound_getString_ascii(NBTTagCompound_getCompoundTag_ascii(source->stackTagCompound,"display"),"Name"))); CHECK(!NBTTagCompound_hasKey_ascii(out->stackTagCompound,"custom")&&w->displayCalls==2);
    ItemStackArray *left=RecipesMapCloning_getRemainingItems(r,g); CHECK(left&&left->length==9&&!left->items[4]); put(g,8,source); CHECK(!RecipesMapCloning_matches(r,g,NULL)&&!RecipesMapCloning_getCraftingResult(r,g,display,(MCObject *)w)); put(g,8,stack(h,335,0,7)); left=RecipesMapCloning_getRemainingItems(r,g); CHECK(left&&left->items[8]&&left->items[8]->item==ItemStack_registryItem(325)&&left->items[8]->stackSize==1&&left->items[8]->itemDamage==0); CHECK(RecipesMapCloning_getRecipeSize(r)==9&&!RecipesMapCloning_getRecipeOutput(r)); CHECK(!MCObjectHeap_failed(h)); MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
}
static void map_extending(void) {
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); World *w=world_new(h); InventoryCrafting *g=grid_new(h,w,3); RecipesMapExtending *r=RecipesMapExtending_new(h,get_map); CHECK(r);
    CHECK(r->shaped.recipeWidth==3&&r->shaped.recipeHeight==3&&r->shaped.recipeItems->length==9&&r->shaped.recipeItems->items[4]->stackSize==0&&r->shaped.recipeItems->items[4]->itemDamage==32767);
    IRecipe recipe=RecipesMapExtending_asRecipe(r); ItemStack *template=recipe.methods->getRecipeOutput(recipe.instance); CHECK(recipe.kind==IRECIPE_SHAPED&&template&&template->item==ItemStack_registryItem(395)&&template->stackSize==0&&template->itemDamage==0&&recipe.methods->getRecipeSize(recipe.instance)==9);
    ItemStack *source=stack(h,358,-3,4); NBTTagCompound *tag=NBTTagCompound_new(h); CHECK(tag&&NBTTagCompound_setInteger_ascii(tag,"custom",7)&&ItemStack_setTagCompound(source,tag));
    for (int i=0;i<9;i++) put(g,i,i==4?source:stack(h,339,0,0));
    for (int scale=0;scale<=4;scale++) { w->map.scale=(uint8_t)scale; CHECK(RecipesMapExtending_matches(r,g,(MCObject *)w)==(scale<4)); CHECK(w->last==source&&w->mapCalls==scale+1); }
    const uint8_t signed_scales[]={128,255,0,3,4,5,127}; const bool expected_scales[]={true,true,true,true,false,false,false};
    for (size_t i=0;i<sizeof(signed_scales);i++) { w->map.scale=signed_scales[i]; CHECK(RecipesMapExtending_matches(r,g,(MCObject *)w)==expected_scales[i]); }
    w->missing=true; CHECK(!RecipesMapExtending_matches(r,g,(MCObject *)w)); w->missing=false; w->map.scale=1; w->changedDamage=10; CHECK(RecipesMapExtending_matches(r,g,(MCObject *)w)&&source->itemDamage==10); ItemStack *out=RecipesMapExtending_getCraftingResult(r,g); CHECK(out&&out!=source&&out->stackSize==1&&out->itemDamage==10&&out->stackTagCompound!=tag&&NBTTagCompound_getInteger_ascii(out->stackTagCompound,"custom")==7&&NBTTagCompound_getBoolean_ascii(out->stackTagCompound,"map_is_scaling")); CHECK(!NBTTagCompound_hasKey_ascii(tag,"map_is_scaling"));
    ItemStackArray *left=recipe.methods->getRemainingItems(recipe.instance,g); CHECK(left&&left->length==9&&!left->items[4]); int calls=w->mapCalls; put(g,0,stack(h,5,1,0)); CHECK(!RecipesMapExtending_matches(r,g,(MCObject *)w)&&w->mapCalls==calls); CHECK(!MCObjectHeap_failed(h)); MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
    h=MCObjectHeap_new(4*1024*1024); CHECK(h); CHECK(MCObjectRootScope_begin(&scope,h)); w=world_new(h); g=grid_new(h,w,3); r=RecipesMapExtending_new(h,get_map); CHECK(r); for (int i=0;i<9;i++) put(g,i,stack(h,i==4?358:339,1,0)); w->fail=true; CHECK(!RecipesMapExtending_matches(r,g,(MCObject *)w)&&MCObjectHeap_failed(h)); MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
}
static void repair(void) {
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); World *w=world_new(h); InventoryCrafting *g=grid_new(h,w,2); RecipeRepairItem *r=RecipeRepairItem_new(h); CHECK(r);
    ItemStack *first=stack(h,276,1,1500),*second=stack(h,276,1,1500); put(g,0,first); put(g,3,second); NBTTagCompound *tag=NBTTagCompound_new(h); CHECK(tag&&NBTTagCompound_setBoolean_ascii(tag,"Unbreakable",true)&&ItemStack_setTagCompound(first,tag)); CHECK(RecipeRepairItem_matches(r,g,NULL)); ItemStack *out=RecipeRepairItem_getCraftingResult(r,g); CHECK(out&&out->item==first->item&&out->stackSize==1&&out->itemDamage==1361&&!out->stackTagCompound); CHECK(first->itemDamage==1500&&second->itemDamage==1500);
    first->stackSize=0; CHECK(!RecipeRepairItem_matches(r,g,NULL)&&!RecipeRepairItem_getCraftingResult(r,g)); first->stackSize=1; put(g,1,stack(h,276,1,1500)); CHECK(!RecipeRepairItem_matches(r,g,NULL)&&!RecipeRepairItem_getCraftingResult(r,g)); CHECK(RecipeRepairItem_getRecipeSize(r)==4&&!RecipeRepairItem_getRecipeOutput(r)); InventoryCrafting_clear(g); put(g,0,stack(h,335,0,7)); ItemStackArray *left=RecipeRepairItem_getRemainingItems(r,g); CHECK(left&&left->items[0]&&left->items[0]->item==ItemStack_registryItem(325)&&left->items[0]->stackSize==1&&left->items[0]->itemDamage==0); CHECK(!MCObjectHeap_failed(h)); MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
}
typedef struct { int id; int32_t a,b,damage; } RepairGolden;
/* Actual original class observations. Negative metadata here is deliberately
   assigned to the existing field, as the private Java reflection oracle did;
   it does not bypass the original constructor clamp in production. */
static const RepairGolden repair_goldens[]={
    {259,64,64,61},{259,63,63,59},{259,1500,1500,2933},{259,INT32_MAX,INT32_MAX,0},
    {259,INT32_MIN,1,2147483582},{259,65,66,64},{259,-1,-1,0},
    {267,250,250,238},{267,1500,1500,2738},{267,INT32_MIN,1,2147483387},
    {276,1561,1561,1483},{276,1560,1560,1481},{276,1500,1500,1361},
    {276,INT32_MAX,INT32_MAX,0},{276,INT32_MIN,1,2147482010},{276,INT32_MIN,INT32_MAX,0},
    {298,55,55,53},{298,56,57,56},{298,INT32_MIN,1,2147483592},
    {300,75,75,72},{300,1500,1500,2922},{300,INT32_MIN,1,2147483571}
};
static void repair_vector(RecipeRepairItem *r,InventoryCrafting *g,int id,int32_t a,int32_t b,bool matches,int32_t expected) {
    MCObjectHeap *h=r->object.heap; InventoryCrafting_clear(g); ItemStack *first=stack(h,id,1,0),*second=stack(h,id,1,0); first->itemDamage=a; second->itemDamage=b; put(g,0,first); put(g,3,second); CHECK(RecipeRepairItem_matches(r,g,NULL)==matches); ItemStack *out=RecipeRepairItem_getCraftingResult(r,g); CHECK((out!=NULL)==matches); if (out) CHECK(out->item==first->item&&out->stackSize==1&&out->itemDamage==expected&&!out->stackTagCompound); CHECK(first->itemDamage==a&&second->itemDamage==b);
}
static void repair_numeric(const char *path) {
    MCObjectHeap *h=MCObjectHeap_new(16*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); World *w=world_new(h); InventoryCrafting *g=grid_new(h,w,2); RecipeRepairItem *r=RecipeRepairItem_new(h); CHECK(r);
    for (size_t i=0;i<sizeof(repair_goldens)/sizeof(*repair_goldens);i++) { const RepairGolden *v=&repair_goldens[i]; repair_vector(r,g,v->id,v->a,v->b,true,v->damage); }
    if (path) { FILE *f=fopen(path,"r"); CHECK(f); int id,match; int32_t a,b,expected; unsigned count=0; while (fscanf(f,"%d",&id)==1) { CHECK(fscanf(f,"%" SCNd32 " %" SCNd32 " %d %" SCNd32,&a,&b,&match,&expected)==4); repair_vector(r,g,id,a,b,match!=0,expected); count++; } CHECK(!ferror(f)&&count==450&&fclose(f)==0); printf("Independent original repair vectors: %u passed\n",count); }
    CHECK(!MCObjectHeap_failed(h)); MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
}
static bool never_matches(MCObject *o,InventoryCrafting *g,MCObject *w) { (void)o; (void)g; (void)w; return false; }
static ItemStack *no_result(MCObject *o,InventoryCrafting *g,ItemStackDisplayNameDispatch d,MCObject *ctx) { (void)o; (void)g; (void)d; (void)ctx; return NULL; }
static int32_t other_size(const MCObject *o) { (void)o; return 10; }
static ItemStack *no_template(MCObject *o) { (void)o; return NULL; }
static ItemStackArray *no_remaining(MCObject *o,InventoryCrafting *g) { (void)o; (void)g; return NULL; }
static const IRecipeMethods unavailable_test_recipes={never_matches,no_result,other_size,no_template,no_remaining};
static void dispatch_and_clone(void) {
    MCObjectHeap *h=MCObjectHeap_new(16*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); World *w=world_new(h); InventoryCrafting *g=grid_new(h,w,3); RecipesMapCloning *clone=RecipesMapCloning_new(h); RecipesMapExtending *extend=RecipesMapExtending_new(h,get_map); RecipeRepairItem *repair=RecipeRepairItem_new(h); CHECK(clone&&extend&&repair);
    IRecipe pending[CRAFTING_PENDING_COUNT]; for (int i=0;i<CRAFTING_PENDING_COUNT;i++) pending[i]=(IRecipe){(MCObject *)w,&unavailable_test_recipes,IRECIPE_OTHER}; pending[CRAFTING_PENDING_MAP_CLONING]=RecipesMapCloning_asRecipe(clone); pending[CRAFTING_PENDING_MAP_EXTENDING]=RecipesMapExtending_asRecipe(extend); pending[CRAFTING_PENDING_REPAIR]=RecipeRepairItem_asRecipe(repair); CraftingManager *manager=CraftingManager_new(h,pending); CHECK(manager);
    ItemStack *source=stack(h,358,0,4); w->map.scale=2; for (int i=0;i<9;i++) put(g,i,i==4?source:stack(h,339,-1,0)); ItemStack *out=CraftingManager_findMatchingRecipe(manager,g,(MCObject *)w,display,(MCObject *)w); CHECK(out&&out->stackSize==1&&out->itemDamage==4&&NBTTagCompound_getBoolean_ascii(out->stackTagCompound,"map_is_scaling")&&w->last==source);
    InventoryCrafting_clear(g); put(g,1,source); put(g,8,stack(h,395,0,0)); out=CraftingManager_findMatchingRecipe(manager,g,(MCObject *)w,display,(MCObject *)w); CHECK(out&&out->stackSize==2&&!out->stackTagCompound); ItemStackArray *remaining=CraftingManager_func_180303_b(manager,g,(MCObject *)w); CHECK(remaining&&!remaining->items[1]);
    MCObjectRoot root_manager={0},root_grid={0}; CHECK(MCObjectRoot_init(&root_manager,h,(MCObject *)manager)&&MCObjectRoot_init(&root_grid,h,(MCObject *)g)); MCObjectRootScope_end(&scope); CHECK(MCObjectHeap_collect(h)); MCObjectHeap *copy=MCObjectHeap_clone(h); CHECK(copy); MCObjectRoot copied_manager={0},copied_grid={0}; CHECK(MCObjectRoot_rebind(&copied_manager,copy,&root_manager)&&MCObjectRoot_rebind(&copied_grid,copy,&root_grid)); CHECK(MCObjectRootScope_begin(&scope,copy)); manager=(CraftingManager *)MCObjectRoot_get(&copied_manager); g=(InventoryCrafting *)MCObjectRoot_get(&copied_grid); IRecipe copied_extend=RecipeList_get(manager->recipes,72); extend=(RecipesMapExtending *)copied_extend.instance;
    CHECK(extend->shaped.recipeItems->items[4]->stackSize==0&&extend->shaped.recipeOutput->stackSize==0&&extend->shaped.recipeOutput->object.heap==copy&&extend->getMapData==get_map); out=CraftingManager_findMatchingRecipe(manager,g,g->eventHandler,display,g->eventHandler); CHECK(out&&out->stackSize==2&&out->object.heap==copy&&!MCObjectHeap_failed(copy)); MCObjectRootScope_end(&scope); MCObjectHeap_free(copy); MCObjectHeap_free(h);
}
static void allocation_exception(void) {
    const size_t budget=65536; MCObjectHeap *h=MCObjectHeap_new(budget); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); World *w=world_new(h); InventoryCrafting *g=grid_new(h,w,3); RecipesMapExtending *r=RecipesMapExtending_new(h,get_map); CHECK(r); ItemStack *source=stack(h,358,0,4); put(g,4,source);
    size_t filler=budget-MCObjectHeap_liveBytes(h)-sizeof(ItemStack); CHECK(filler>=sizeof(MCObject)&&MCObjectHeap_alloc(h,filler,&filler_class));
    CHECK(!RecipesMapExtending_getCraftingResult(r,g)&&MCObjectHeap_failed(h)); CHECK(source->stackSize==0&&source->itemDamage==4&&!source->stackTagCompound); MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
}
int main(int argc,char **argv) { map_clone(); map_extending(); repair(); repair_numeric(argc>1?argv[1]:NULL); dispatch_and_clone(); allocation_exception(); printf("Source dynamic recipes: %u checks passed\n",checks); return 0; }
