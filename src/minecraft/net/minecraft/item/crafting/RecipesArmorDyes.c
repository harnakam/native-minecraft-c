#include "item/crafting/RecipesArmorDyes.h"
#include "item/ItemArmor.h"
#include <limits.h>
#include <math.h>
#include <string.h>

static const MCObjectClass klass={"RecipesArmorDyes",MCObjectHeap_plainClone,NULL,NULL};
RecipesArmorDyes *RecipesArmorDyes_new(MCObjectHeap *h) { return (RecipesArmorDyes *)MCObjectHeap_alloc(h,sizeof(RecipesArmorDyes),&klass); }
static bool begin(RecipesArmorDyes *r,InventoryCrafting *grid,MCObjectRootScope *scope) {
    MCObjectHeap *h=r->object.heap;
    if (!MCObjectRootScope_begin(scope,h)) return false;
    if (!MCObjectRootScope_pin(scope,(MCObject *)r)||!MCObjectRootScope_pin(scope,(MCObject *)grid)||!grid) {
        MCObjectHeap_fail(h); MCObjectRootScope_end(scope); return false;
    }
    return true;
}
/* EnumDyeColor.byDyeDamage and EntitySheep's immutable RGB facts, indexed in
   dye-damage order. Their full enum/entity classes are separate dependencies. */
static const float dye_rgb[16][3]={
    {0.1f,0.1f,0.1f},{0.6f,0.2f,0.2f},{0.4f,0.5f,0.2f},{0.4f,0.3f,0.2f},
    {0.2f,0.3f,0.7f},{0.5f,0.25f,0.7f},{0.3f,0.5f,0.6f},{0.6f,0.6f,0.6f},
    {0.3f,0.3f,0.3f},{0.95f,0.5f,0.65f},{0.5f,0.8f,0.1f},{0.9f,0.9f,0.2f},
    {0.4f,0.6f,0.85f},{0.7f,0.3f,0.85f},{0.85f,0.5f,0.2f},{1.0f,1.0f,1.0f}
};
static int32_t bits(uint32_t value) { int32_t out; memcpy(&out,&value,sizeof(out)); return out; }
static int32_t add(int32_t a,int32_t b) { return bits((uint32_t)a+(uint32_t)b); }
static int32_t maximum(int32_t a,int32_t b) { return a>b?a:b; }
static int32_t divide(int32_t a,int32_t b) { return a==INT32_MIN && b==-1?INT32_MIN:a/b; }
/* Force each Java float operation to round to binary32, including on hosts
   that contract expressions. Conversion uses Java's saturating FP cast. */
static float fm(float a,float b) { volatile float out=a*b; return out; }
static float fd(float a,float b) { volatile float out=a/b; return out; }
static float fa(float a,float b) { volatile float out=a+b; return out; }
static int32_t fi(float value) {
    if (isnan(value)) return 0;
    if (value>=2147483648.0f) return INT32_MAX;
    if (value<=-2147483648.0f) return INT32_MIN;
    return (int32_t)value;
}
bool RecipesArmorDyes_matches(RecipesArmorDyes *r,InventoryCrafting *grid,MCObject *world) {
    (void)world; MCObjectHeap *h=r->object.heap; MCObjectRootScope scope={0}; if (!begin(r,grid,&scope)) return false;
    ItemStack *armor=NULL; ItemStackList *dyes=ItemStackList_new(h); bool matched=dyes!=NULL;
    for (int32_t i=0;matched && i<InventoryCrafting_getSizeInventory(grid);i++) {
        ItemStack *stack=InventoryCrafting_getStackInSlot(grid,i);
        if (stack) {
            if (ItemArmor_isInstance(stack->item)) {
                if (ItemArmor_getArmorMaterial(stack->item)!=ITEMARMOR_LEATHER || armor) matched=false;
                else armor=stack;
            } else {
                if (stack->item!=ItemStack_registryItem(351)) matched=false;
                else matched=ItemStackList_add(dyes,stack);
            }
        }
    }
    matched=matched && armor && ItemStackList_size(dyes)!=0;
    MCObjectRootScope_end(&scope); return matched && !MCObjectHeap_failed(h);
}
ItemStack *RecipesArmorDyes_getCraftingResult(RecipesArmorDyes *r,InventoryCrafting *grid) {
    MCObjectHeap *h=r->object.heap; MCObjectRootScope scope={0}; if (!begin(r,grid,&scope)) return NULL;
    ItemStack *out=NULL; int32_t channels[3]={0},brightness=0,samples=0; const Item *armor=NULL; bool ok=true;
    for (int32_t index=0;ok && index<InventoryCrafting_getSizeInventory(grid);index++) {
        ItemStack *stack=InventoryCrafting_getStackInSlot(grid,index); if (!stack) continue;
        if (ItemArmor_isInstance(stack->item)) {
            armor=stack->item;
            if (ItemArmor_getArmorMaterial(armor)!=ITEMARMOR_LEATHER || out) { ok=false; break; }
            out=ItemStack_copy(h,stack); if (!out) { ok=false; break; }
            out->stackSize=1; MCObjectHeap_touch(h);
            if (ItemArmor_hasColor(h,armor,stack)) {
                uint32_t color=(uint32_t)ItemArmor_getColor(h,armor,out);
                float red=fd((float)((color>>16)&255),255.0f),green=fd((float)((color>>8)&255),255.0f),blue=fd((float)(color&255),255.0f);
                brightness=fi(fa((float)brightness,fm(fmaxf(red,fmaxf(green,blue)),255.0f)));
                channels[0]=fi(fa((float)channels[0],fm(red,255.0f)));
                channels[1]=fi(fa((float)channels[1],fm(green,255.0f)));
                channels[2]=fi(fa((float)channels[2],fm(blue,255.0f)));
                samples=add(samples,1);
            }
        } else {
            if (stack->item!=ItemStack_registryItem(351)) { ok=false; break; }
            int32_t damage=ItemStack_getMetadata(stack); if (damage<0 || damage>=16) damage=0;
            int32_t red=fi(fm(dye_rgb[damage][0],255.0f)),green=fi(fm(dye_rgb[damage][1],255.0f)),blue=fi(fm(dye_rgb[damage][2],255.0f));
            brightness=add(brightness,maximum(red,maximum(green,blue)));
            channels[0]=add(channels[0],red); channels[1]=add(channels[1],green); channels[2]=add(channels[2],blue); samples=add(samples,1);
        }
    }
    if (ok && armor) {
        if (!samples) { MCObjectHeap_fail(h); ok=false; }
        else {
            int32_t red=divide(channels[0],samples),green=divide(channels[1],samples),blue=divide(channels[2],samples);
            float mean=fd((float)brightness,(float)samples),max=(float)maximum(red,maximum(green,blue));
            red=fi(fd(fm((float)red,mean),max)); green=fi(fd(fm((float)green,mean),max)); blue=fi(fd(fm((float)blue,mean),max));
            int32_t color=bits((((uint32_t)red<<8)+(uint32_t)green)<<8); color=add(color,blue);
            ok=ItemArmor_setColor(h,armor,out,color);
        }
    } else ok=false;
    MCObjectRootScope_end(&scope); return ok && !MCObjectHeap_failed(h)?out:NULL;
}
int32_t RecipesArmorDyes_getRecipeSize(const RecipesArmorDyes *r) { (void)r; return 10; }
ItemStack *RecipesArmorDyes_getRecipeOutput(RecipesArmorDyes *r) { (void)r; return NULL; }
ItemStackArray *RecipesArmorDyes_getRemainingItems(RecipesArmorDyes *r,InventoryCrafting *grid) {
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
static bool m(MCObject *o,InventoryCrafting *g,MCObject *w) { return RecipesArmorDyes_matches((RecipesArmorDyes *)o,g,w); }
static ItemStack *c(MCObject *o,InventoryCrafting *g,ItemStackDisplayNameDispatch d,MCObject *ctx) { (void)d; (void)ctx; return RecipesArmorDyes_getCraftingResult((RecipesArmorDyes *)o,g); }
static int32_t s(const MCObject *o) { return RecipesArmorDyes_getRecipeSize((const RecipesArmorDyes *)o); }
static ItemStack *p(MCObject *o) { return RecipesArmorDyes_getRecipeOutput((RecipesArmorDyes *)o); }
static ItemStackArray *l(MCObject *o,InventoryCrafting *g) { return RecipesArmorDyes_getRemainingItems((RecipesArmorDyes *)o,g); }
static const IRecipeMethods methods={m,c,s,p,l};
IRecipe RecipesArmorDyes_asRecipe(RecipesArmorDyes *r) { return (IRecipe){(MCObject *)r,&methods,IRECIPE_OTHER}; }
