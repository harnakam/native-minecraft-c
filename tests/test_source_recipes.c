#include "item/crafting/CraftingManager.h"
#include "item/crafting/RecipeBookCloning.h"
#include "nbt/NBTTagCompound.h"
#include "crafting/crafting.h"
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static bool notify(MCObject *object,InventoryCrafting *grid) { (void)object; (void)grid; return true; }
static const MCObjectClass actor_class={"Recipe test notification actor",MCObjectHeap_plainClone,NULL,NULL};
static InventoryCrafting *grid_new(MCObjectHeap *h,int32_t w,int32_t height) {
    MCObject *actor=MCObjectHeap_alloc(h,sizeof(MCObject),&actor_class); CHECK(actor);
    return InventoryCrafting_new(h,actor,notify,w,height);
}
static ItemStack *stack(MCObjectHeap *h,int id,int32_t n,int32_t damage) { ItemStack *out=ItemStack_new(h,ItemStack_registryItem(id),n,damage); CHECK(out); return out; }
static void shaped(void) {
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h));
    InventoryCrafting *grid=grid_new(h,2,2); ItemStackArray *inputs=ItemStackArray_new(h,2); CHECK(grid&&inputs);
    inputs->items[0]=stack(h,5,1,32767); inputs->items[1]=stack(h,280,1,0); ItemStack *template=stack(h,1,4,0);
    ShapedRecipes *recipe=ShapedRecipes_new(h,2,1,inputs,template); CHECK(recipe&&recipe->recipeItems==inputs&&ShapedRecipes_getRecipeOutput(recipe)==template);
    CHECK(InventoryCrafting_setInventorySlotContents(grid,0,stack(h,280,0,0))); CHECK(InventoryCrafting_setInventorySlotContents(grid,1,stack(h,5,-7,4))); CHECK(ShapedRecipes_matches(recipe,grid,NULL));
    ItemStack *out=ShapedRecipes_getCraftingResult(recipe,grid); CHECK(out&&out!=template&&out->stackSize==4);
    inputs->items[0]=stack(h,3,1,0); CHECK(!ShapedRecipes_matches(recipe,grid,NULL)); inputs->items[0]=stack(h,5,1,32767);
    NBTTagCompound *a=NBTTagCompound_new(h),*b=NBTTagCompound_new(h); CHECK(a&&b&&NBTTagCompound_setInteger_ascii(a,"which",1)&&NBTTagCompound_setInteger_ascii(b,"which",2));
    CHECK(ItemStack_setTagCompound(grid->stackList->items[0],a)&&ItemStack_setTagCompound(grid->stackList->items[1],b)); recipe->copyIngredientNBT=true; out=ShapedRecipes_getCraftingResult(recipe,grid); CHECK(out&&out->stackTagCompound!=b&&NBTTagCompound_getInteger_ascii(out->stackTagCompound,"which")==2);
    ItemStackArray *left=ShapedRecipes_getRemainingItems(recipe,grid); CHECK(left&&left->length==4&&!left->items[0]); CHECK(InventoryCrafting_setInventorySlotContents(grid,0,stack(h,335,0,7))); left=ShapedRecipes_getRemainingItems(recipe,grid); CHECK(left&&left->items[0]&&left->items[0]->item==ItemStack_registryItem(325)&&left->items[0]->stackSize==1&&left->items[0]->itemDamage==0);
    CHECK(ShapedRecipes_getRecipeSize(recipe)==2); CHECK(!MCObjectHeap_failed(h)); MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
}
static void shapeless(void) {
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); InventoryCrafting *grid=grid_new(h,3,3); ItemStackList *inputs=ItemStackList_new(h); CHECK(grid&&inputs);
    CHECK(ItemStackList_add(inputs,stack(h,351,1,32767))&&ItemStackList_add(inputs,stack(h,351,1,2))); ItemStack *template=stack(h,1,3,0); ShapelessRecipes *recipe=ShapelessRecipes_new(h,template,inputs); CHECK(recipe&&recipe->recipeItems==inputs&&ShapelessRecipes_getRecipeOutput(recipe)==template);
    CHECK(InventoryCrafting_setInventorySlotContents(grid,0,stack(h,351,0,2))&&InventoryCrafting_setInventorySlotContents(grid,8,stack(h,351,-1,3))); CHECK(!ShapelessRecipes_matches(recipe,grid,NULL)); /* first wildcard consumes damage2 */
    CHECK(InventoryCrafting_setInventorySlotContents(grid,0,stack(h,351,0,3))&&InventoryCrafting_setInventorySlotContents(grid,8,stack(h,351,-1,2))); CHECK(ShapelessRecipes_matches(recipe,grid,NULL)); CHECK(ShapelessRecipes_getRecipeSize(recipe)==2); ItemStack *out=ShapelessRecipes_getCraftingResult(recipe,grid); CHECK(out&&out!=template&&out->stackSize==3);
    CHECK(ItemStackList_remove(inputs,0)&&ItemStackList_size(inputs)==1); CHECK(!ShapelessRecipes_matches(recipe,grid,NULL)); ItemStackList_clear(inputs); CHECK(ItemStackList_size(inputs)==0); CHECK(!MCObjectHeap_failed(h)); MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
}
typedef struct { MCObject object; bool match; int32_t calls,size; ItemStack *output; ItemStackArray *remaining; RecipeList *clearList; } ProbeRecipe;
static void probe_trace(MCObject *o,MCObjectVisitor v,void *ctx) { ProbeRecipe *p=(ProbeRecipe *)o; p->output=(ItemStack *)v((MCObject *)p->output,ctx); p->remaining=(ItemStackArray *)v((MCObject *)p->remaining,ctx); p->clearList=(RecipeList *)v((MCObject *)p->clearList,ctx); }
static const MCObjectClass probe_class={"Recipe test dispatch fixture",MCObjectHeap_plainClone,probe_trace,NULL};
static bool probe_match(MCObject *o,InventoryCrafting *g,MCObject *w) { (void)g; (void)w; ProbeRecipe *p=(ProbeRecipe *)o; p->calls++; MCObjectHeap_touch(o->heap); if (p->clearList) RecipeList_clear(p->clearList); return p->match; }
static ItemStack *probe_result(MCObject *o,InventoryCrafting *g,ItemStackDisplayNameDispatch d,MCObject *ctx) { (void)g; (void)d; (void)ctx; return ((ProbeRecipe *)o)->output; }
static int32_t probe_size(const MCObject *o) { return ((const ProbeRecipe *)o)->size; }
static ItemStack *probe_output(MCObject *o) { return ((ProbeRecipe *)o)->output; }
static ItemStackArray *probe_remaining(MCObject *o,InventoryCrafting *g) { (void)g; return ((ProbeRecipe *)o)->remaining; }
static const IRecipeMethods probe_methods={probe_match,probe_result,probe_size,probe_output,probe_remaining};
static IRecipe probe_new(MCObjectHeap *h,bool match,int32_t size) { ProbeRecipe *p=(ProbeRecipe *)MCObjectHeap_alloc(h,sizeof(*p),&probe_class); CHECK(p); p->match=match; p->size=size; return (IRecipe){(MCObject *)p,&probe_methods,IRECIPE_OTHER}; }
static void manager_dispatch(void) {
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h));
    CraftingManager *manager=CraftingManager_newEmpty(h); InventoryCrafting *grid=grid_new(h,2,2); CHECK(manager&&grid); ItemStack *source=stack(h,1,0,0); CHECK(InventoryCrafting_setInventorySlotContents(grid,0,source));
    CHECK(!CraftingManager_findMatchingRecipe(manager,grid,NULL,NULL,NULL)); ItemStackArray *left=CraftingManager_func_180303_b(manager,grid,NULL); CHECK(left&&left->items[0]==source); CHECK(!left->items[1]);
    IRecipe first=probe_new(h,true,1),second=probe_new(h,true,1); ((ProbeRecipe *)second.instance)->output=stack(h,3,1,0); CHECK(CraftingManager_addRecipe(manager,first)&&CraftingManager_addRecipe(manager,second));
    CHECK(!CraftingManager_findMatchingRecipe(manager,grid,NULL,NULL,NULL)); CHECK(((ProbeRecipe *)first.instance)->calls==1&&((ProbeRecipe *)second.instance)->calls==0); /* first matches with null result */
    RecipeList *list=CraftingManager_getRecipeList(manager); CHECK(list==manager->recipes&&RecipeList_size(list)==2); CHECK(RecipeList_remove(list,0).instance==first.instance); CHECK(CraftingManager_findMatchingRecipe(manager,grid,NULL,NULL,NULL)==((ProbeRecipe *)second.instance)->output);
    CHECK(RecipeList_set(list,0,first)); CHECK(RecipeList_get(list,0).instance==first.instance); RecipeList_clear(list); CHECK(RecipeList_size(list)==0);
    RecipeBookCloning *book=RecipeBookCloning_new(h); CHECK(book&&CraftingManager_addRecipe(manager,RecipeBookCloning_asRecipe(book))); ItemStack *written=stack(h,387,2,7); NBTTagCompound *tag=NBTTagCompound_new(h); CHECK(tag&&NBTTagCompound_setInteger_ascii(tag,"generation",2)&&ItemStack_setTagCompound(written,tag)); InventoryCrafting_clear(grid); CHECK(InventoryCrafting_setInventorySlotContents(grid,0,written)&&InventoryCrafting_setInventorySlotContents(grid,1,stack(h,386,-1,0)));
    CHECK(!CraftingManager_findMatchingRecipe(manager,grid,NULL,NULL,NULL)); left=CraftingManager_func_180303_b(manager,grid,NULL); CHECK(left&&left->items[0]==written); CHECK(NBTTagCompound_setInteger_ascii(tag,"generation",0)); ItemStack *result=CraftingManager_findMatchingRecipe(manager,grid,NULL,NULL,NULL); CHECK(result&&result->itemDamage==0&&result->stackSize==1&&NBTTagCompound_getInteger_ascii(result->stackTagCompound,"generation")==1); CHECK(result->stackTagCompound!=tag);
    MCObjectRoot manager_root={0},grid_root={0}; CHECK(MCObjectRoot_init(&manager_root,h,(MCObject *)manager)&&MCObjectRoot_init(&grid_root,h,(MCObject *)grid)); MCObjectRootScope_end(&scope); CHECK(MCObjectHeap_collect(h)); MCObjectHeap *copy=MCObjectHeap_clone(h); CHECK(copy); MCObjectRoot copied_manager={0},copied_grid={0}; CHECK(MCObjectRoot_rebind(&copied_manager,copy,&manager_root)&&MCObjectRoot_rebind(&copied_grid,copy,&grid_root)); CHECK(MCObjectRootScope_begin(&scope,copy));
    manager=(CraftingManager *)MCObjectRoot_get(&copied_manager); grid=(InventoryCrafting *)MCObjectRoot_get(&copied_grid); left=CraftingManager_func_180303_b(manager,grid,NULL); CHECK(left&&left->items[0]==grid->stackList->items[0]&&left->items[0]!=written); CHECK(!MCObjectHeap_failed(copy)); MCObjectRootScope_end(&scope); MCObjectHeap_free(copy); MCObjectHeap_free(h);
}
static void registry(void) {
    MCObjectHeap *h=MCObjectHeap_new(64*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); IRecipe pending[CRAFTING_PENDING_COUNT];
    const int sizes[]={10,10,10,9,9,4,2}; for (int i=0;i<CRAFTING_PENDING_COUNT;i++) pending[i]=probe_new(h,false,sizes[i]);
    CraftingManager *manager=CraftingManager_new(h,pending); InventoryCrafting *grid=grid_new(h,3,3); CHECK(manager&&grid&&RecipeList_size(manager->recipes)==373); CHECK(mc_crafting_static_recipe_count()==365); mc_crafting_recipe_fact unchanged={0}; CHECK(!mc_crafting_static_recipe(365,&unchanged)&&unchanged.output==0);
    const int positions[]={0,1,2,71,72,216,280}; for (int i=0;i<CRAFTING_PENDING_COUNT;i++) CHECK(RecipeList_get(manager->recipes,positions[i]).instance==pending[i].instance); CHECK(RecipeList_get(manager->recipes,70).methods->getRecipeSize(RecipeList_get(manager->recipes,70).instance)==9);
    size_t fact_index=0;
    for (int32_t index=0;index<373;index++) {
        IRecipe recipe=RecipeList_get(manager->recipes,index); if (recipe.kind==IRECIPE_OTHER) continue;
        mc_crafting_recipe_fact fact; CHECK(mc_crafting_static_recipe(fact_index++,&fact)); InventoryCrafting_clear(grid);
        for (int i=0;i<fact.count;i++) if (fact.input[i].id>=0) { int target=fact.width?(i/fact.width)*3+i%fact.width:i; CHECK(InventoryCrafting_setInventorySlotContents(grid,target,stack(h,fact.input[i].id,(i&1)?-1:0,fact.input[i].damage==32767?0:fact.input[i].damage))); }
        CHECK(recipe.methods->matches(recipe.instance,grid,NULL)); ItemStack *template=recipe.methods->getRecipeOutput(recipe.instance); CHECK(template&&template->item==ItemStack_registryItem(fact.output)&&template->stackSize==fact.amount&&template->itemDamage==fact.damage);
        ItemStack *out=recipe.methods->getCraftingResult(recipe.instance,grid,NULL,NULL); CHECK(out&&out!=template&&ItemStack_areItemStacksEqual(out,template)); ItemStack *dispatched=CraftingManager_findMatchingRecipe(manager,grid,NULL,NULL,NULL); CHECK(dispatched&&dispatched->item==template->item&&dispatched->stackSize==template->stackSize&&dispatched->itemDamage==template->itemDamage);
    }
    CHECK(fact_index==365&&!MCObjectHeap_failed(h)); MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
}
static void iterator_boundary(void) {
    for (int method=0;method<2;method++) for (int matches=0;matches<2;matches++) {
        MCObjectHeap *h=MCObjectHeap_new(4*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); CraftingManager *m=CraftingManager_newEmpty(h); InventoryCrafting *grid=grid_new(h,2,2); CHECK(m&&grid);
        IRecipe first=probe_new(h,matches!=0,1),second=probe_new(h,true,1); CHECK(CraftingManager_addRecipe(m,first)&&CraftingManager_addRecipe(m,second)); ProbeRecipe *p=(ProbeRecipe *)first.instance; p->clearList=m->recipes; p->remaining=ItemStackArray_new(h,4); CHECK(p->remaining);
        if (method) { ItemStackArray *out=CraftingManager_func_180303_b(m,grid,NULL); CHECK(matches?out==p->remaining:out==NULL); }
        else CHECK(!CraftingManager_findMatchingRecipe(m,grid,NULL,NULL,NULL));
        CHECK(MCObjectHeap_failed(h)==!matches); /* matches=true returns before next iterator check */
        CHECK(((ProbeRecipe *)second.instance)->calls==0); MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
    }
}
static void independent_golden(const char *path) {
    FILE *file=fopen(path,"r"); CHECK(file); MCObjectHeap *h=MCObjectHeap_new(64*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); IRecipe pending[CRAFTING_PENDING_COUNT];
    const int sizes[]={10,10,10,9,9,4,2}; for (int i=0;i<CRAFTING_PENDING_COUNT;i++) pending[i]=probe_new(h,false,sizes[i]);
    CraftingManager *manager=CraftingManager_new(h,pending); InventoryCrafting *grid=grid_new(h,3,3); CHECK(manager&&grid); int id[9],damage[9],expected,amount,metadata; unsigned cases=0;
    while (fscanf(file,"%d",&id[0])==1) {
        CHECK(fscanf(file,"%d",&damage[0])==1); for (unsigned i=1;i<9;i++) CHECK(fscanf(file,"%d %d",&id[i],&damage[i])==2); CHECK(fscanf(file,"%d %d %d",&expected,&amount,&metadata)==3); InventoryCrafting_clear(grid);
        for (int i=0;i<9;i++) if (id[i]>=0) CHECK(InventoryCrafting_setInventorySlotContents(grid,i,stack(h,id[i],1,damage[i])));
        ItemStack *out=CraftingManager_findMatchingRecipe(manager,grid,NULL,NULL,NULL);
        if (!out || ItemStack_registryId(out->item)!=expected || out->stackSize!=amount || out->itemDamage!=metadata) {
            fprintf(stderr,"Independent recipe vector %u expected %d:%d:%d actual %d:%d:%d\n",cases,expected,amount,metadata,out?ItemStack_registryId(out->item):-1,out?out->stackSize:0,out?out->itemDamage:0); exit(1);
        }
        cases++;
    }
    CHECK(!ferror(file)&&cases>0&&!MCObjectHeap_failed(h)); CHECK(fclose(file)==0); MCObjectRootScope_end(&scope); MCObjectHeap_free(h); printf("Independent numeric recipe vectors: %u passed\n",cases);
}
int main(int argc,char **argv) { shaped(); shapeless(); manager_dispatch(); registry(); iterator_boundary(); if (argc>1) independent_golden(argv[1]); printf("Source recipe classes: %u checks passed\n",checks); return 0; }
