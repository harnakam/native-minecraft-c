#include "item/crafting/RecipeFireworks.h"
#include "nbt/NBTTagCompound.h"
#include <string.h>

static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    RecipeFireworks *recipe=(RecipeFireworks *)object;
    recipe->field_92102_a=(ItemStack *)visitor((MCObject *)recipe->field_92102_a,context);
}
static const MCObjectClass klass={"RecipeFireworks",MCObjectHeap_plainClone,trace,NULL};
RecipeFireworks *RecipeFireworks_new(MCObjectHeap *h) { return (RecipeFireworks *)MCObjectHeap_alloc(h,sizeof(RecipeFireworks),&klass); }
static bool begin(RecipeFireworks *r,InventoryCrafting *grid,MCObjectRootScope *scope) {
    MCObjectHeap *h=r->object.heap;
    if (!MCObjectRootScope_begin(scope,h)) return false;
    if (!MCObjectRootScope_pin(scope,(MCObject *)r)||!MCObjectRootScope_pin(scope,(MCObject *)grid)||!grid) {
        MCObjectHeap_fail(h); MCObjectRootScope_end(scope); return false;
    }
    return true;
}
static int32_t add(int32_t a,int32_t b) { uint32_t bits=(uint32_t)a+(uint32_t)b; int32_t out; memcpy(&out,&bits,sizeof(out)); return out; }
/* Immutable ItemDye numeric color facts. Temporary Integer list values are
   held in managed array storage; boxed Integer identity does not escape. */
static const int32_t dye_colors[16]={1973019,11743532,3887386,5320730,2437522,8073150,2651799,11250603,4408131,14188952,4312372,14602026,6719955,12801229,15435844,15790320};
static bool color_tag(NBTTagCompound *tag,const char *key,NBTIntArrayStorage *array) {
    NBTTagIntArray *value=NBTTagIntArray_new(((MCObject *)tag)->heap,array);
    return value && NBTTagCompound_setTag_ascii(tag,key,(NBTBase *)value);
}
bool RecipeFireworks_matches(RecipeFireworks *r,InventoryCrafting *grid,MCObject *world) {
    (void)world; MCObjectHeap *h=r->object.heap; r->field_92102_a=NULL; MCObjectHeap_touch(h);
    MCObjectRootScope scope={0}; if (!begin(r,grid,&scope)) return false;
    int32_t paper=0,powder=0,dyes=0,stars=0,special=0,shape=0; bool valid=true,matched=false;
    for (int32_t index=0;valid && index<InventoryCrafting_getSizeInventory(grid);index++) {
        ItemStack *stack=InventoryCrafting_getStackInSlot(grid,index); if (!stack) continue;
        const Item *item=stack->item;
        if (item==ItemStack_registryItem(289)) powder=add(powder,1);
        else if (item==ItemStack_registryItem(402)) stars=add(stars,1);
        else if (item==ItemStack_registryItem(351)) dyes=add(dyes,1);
        else if (item==ItemStack_registryItem(339)) paper=add(paper,1);
        else if (item==ItemStack_registryItem(348)) special=add(special,1);
        else if (item==ItemStack_registryItem(264)) special=add(special,1);
        else if (item==ItemStack_registryItem(385)) shape=add(shape,1);
        else if (item==ItemStack_registryItem(288)) shape=add(shape,1);
        else if (item==ItemStack_registryItem(371)) shape=add(shape,1);
        else if (item==ItemStack_registryItem(397)) shape=add(shape,1);
        else valid=false;
    }
    special=add(add(special,dyes),shape);
    if (valid && powder<=3 && paper<=1) {
        if (powder>=1 && paper==1 && special==0) {
            r->field_92102_a=ItemStack_new_item(h,ItemStack_registryItem(401)); MCObjectHeap_touch(h);
            if (stars>0 && r->field_92102_a) {
                NBTTagCompound *root=NBTTagCompound_new(h),*fireworks=NBTTagCompound_new(h); NBTTagList *explosions=NBTTagList_new(h);
                for (int32_t index=0;explosions && index<InventoryCrafting_getSizeInventory(grid) && !MCObjectHeap_failed(h);index++) {
                    ItemStack *stack=InventoryCrafting_getStackInSlot(grid,index);
                    if (stack && stack->item==ItemStack_registryItem(402) && ItemStack_hasTagCompound(stack) && NBTTagCompound_hasKeyType_ascii(stack->stackTagCompound,"Explosion",10))
                        NBTTagList_appendTag(explosions,(NBTBase *)NBTTagCompound_getCompoundTag_ascii(stack->stackTagCompound,"Explosion"));
                }
                if (root && fireworks && explosions) {
                    NBTTagCompound_setTag_ascii(fireworks,"Explosions",(NBTBase *)explosions);
                    NBTTagCompound_setByte_ascii(fireworks,"Flight",(int8_t)powder);
                    NBTTagCompound_setTag_ascii(root,"Fireworks",(NBTBase *)fireworks);
                    ItemStack_setTagCompound(r->field_92102_a,root);
                }
            }
            matched=r->field_92102_a!=NULL;
        } else if (powder==1 && paper==0 && stars==0 && dyes>0 && shape<=1) {
            r->field_92102_a=ItemStack_new_item(h,ItemStack_registryItem(402)); MCObjectHeap_touch(h);
            NBTTagCompound *root=NBTTagCompound_new(h),*explosion=NBTTagCompound_new(h);
            NBTIntArrayStorage *colors=NBTIntArrayStorage_new(h,NULL,dyes); int32_t color_index=0; int8_t type=0;
            for (int32_t index=0;colors && explosion && index<InventoryCrafting_getSizeInventory(grid) && !MCObjectHeap_failed(h);index++) {
                ItemStack *stack=InventoryCrafting_getStackInSlot(grid,index); if (!stack) continue;
                const Item *item=stack->item;
                if (item==ItemStack_registryItem(351)) NBTIntArrayStorage_set(colors,color_index++,dye_colors[(uint32_t)ItemStack_getMetadata(stack)&15]);
                else if (item==ItemStack_registryItem(348)) NBTTagCompound_setBoolean_ascii(explosion,"Flicker",true);
                else if (item==ItemStack_registryItem(264)) NBTTagCompound_setBoolean_ascii(explosion,"Trail",true);
                else if (item==ItemStack_registryItem(385)) type=1;
                else if (item==ItemStack_registryItem(288)) type=4;
                else if (item==ItemStack_registryItem(371)) type=2;
                else if (item==ItemStack_registryItem(397)) type=3;
            }
            if (root && explosion && colors && r->field_92102_a) {
                color_tag(explosion,"Colors",colors); NBTTagCompound_setByte_ascii(explosion,"Type",type);
                NBTTagCompound_setTag_ascii(root,"Explosion",(NBTBase *)explosion); ItemStack_setTagCompound(r->field_92102_a,root); matched=true;
            }
        } else if (powder==0 && paper==0 && stars==1 && dyes>0 && dyes==special) {
            NBTIntArrayStorage *colors=NBTIntArrayStorage_new(h,NULL,dyes); int32_t color_index=0;
            for (int32_t index=0;colors && index<InventoryCrafting_getSizeInventory(grid) && !MCObjectHeap_failed(h);index++) {
                ItemStack *stack=InventoryCrafting_getStackInSlot(grid,index); if (!stack) continue;
                if (stack->item==ItemStack_registryItem(351)) NBTIntArrayStorage_set(colors,color_index++,dye_colors[(uint32_t)ItemStack_getMetadata(stack)&15]);
                else if (stack->item==ItemStack_registryItem(402)) {
                    r->field_92102_a=ItemStack_copy(h,stack);
                    if (r->field_92102_a) { r->field_92102_a->stackSize=1; MCObjectHeap_touch(h); }
                }
            }
            if (r->field_92102_a && ItemStack_hasTagCompound(r->field_92102_a)) {
                NBTTagCompound *explosion=NBTTagCompound_getCompoundTag_ascii(r->field_92102_a->stackTagCompound,"Explosion");
                if (explosion && colors) matched=color_tag(explosion,"FadeColors",colors);
            }
        }
    }
    MCObjectRootScope_end(&scope); return matched && !MCObjectHeap_failed(h);
}
ItemStack *RecipeFireworks_getCraftingResult(RecipeFireworks *r,InventoryCrafting *grid) {
    (void)grid; MCObjectHeap *h=r->object.heap; MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,h)) return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r);
    if (ok && !r->field_92102_a) { MCObjectHeap_fail(h); ok=false; }
    ItemStack *out=ok?ItemStack_copy(h,r->field_92102_a):NULL; MCObjectRootScope_end(&scope); return out;
}
int32_t RecipeFireworks_getRecipeSize(const RecipeFireworks *r) { (void)r; return 10; }
ItemStack *RecipeFireworks_getRecipeOutput(RecipeFireworks *r) { return r->field_92102_a; }
ItemStackArray *RecipeFireworks_getRemainingItems(RecipeFireworks *r,InventoryCrafting *grid) {
    MCObjectHeap *h=r->object.heap; MCObjectRootScope scope={0}; if (!begin(r,grid,&scope)) return NULL;
    ItemStackArray *out=ItemStackArray_new(h,InventoryCrafting_getSizeInventory(grid));
    for (int32_t i=0;out && i<out->length && !MCObjectHeap_failed(h);i++) {
        ItemStack *stack=InventoryCrafting_getStackInSlot(grid,i);
        if (stack) {
            if (!stack->item) { MCObjectHeap_fail(h); break; }
            const Item *item=ItemStack_registryContainerItem(stack->item);
            if (item) { out->items[i]=ItemStack_new_item(h,item); MCObjectHeap_touch(h); }
        }
    }
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(h)?NULL:out;
}
static bool m(MCObject *o,InventoryCrafting *g,MCObject *w) { return RecipeFireworks_matches((RecipeFireworks *)o,g,w); }
static ItemStack *c(MCObject *o,InventoryCrafting *g,ItemStackDisplayNameDispatch d,MCObject *ctx) { (void)d; (void)ctx; return RecipeFireworks_getCraftingResult((RecipeFireworks *)o,g); }
static int32_t s(const MCObject *o) { return RecipeFireworks_getRecipeSize((const RecipeFireworks *)o); }
static ItemStack *p(MCObject *o) { return RecipeFireworks_getRecipeOutput((RecipeFireworks *)o); }
static ItemStackArray *l(MCObject *o,InventoryCrafting *g) { return RecipeFireworks_getRemainingItems((RecipeFireworks *)o,g); }
static const IRecipeMethods methods={m,c,s,p,l};
IRecipe RecipeFireworks_asRecipe(RecipeFireworks *r) { return (IRecipe){(MCObject *)r,&methods,IRECIPE_OTHER}; }
