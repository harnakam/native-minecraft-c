#include "item/crafting/RecipesBanners.h"
#include "tileentity/TileEntityBanner.h"
#include "nbt/NBTTagCompound.h"
static void trace(MCObject *o,MCObjectVisitor v,void *c) {RecipesBannersRecipeAddPattern *r=(RecipesBannersRecipeAddPattern *)o;r->registry=(EnumBannerPatternRegistry *)v((MCObject *)r->registry,c);}
static const MCObjectClass addClass={"RecipesBanners.RecipeAddPattern",MCObjectHeap_plainClone,trace,NULL};
static const MCObjectClass duplicateClass={"RecipesBanners.RecipeDuplicatePattern",MCObjectHeap_plainClone,NULL,NULL};
RecipesBannersRecipeAddPattern *RecipesBannersRecipeAddPattern_new(MCObjectHeap *h) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    RecipesBannersRecipeAddPattern *r=(RecipesBannersRecipeAddPattern *)MCObjectHeap_alloc(h,sizeof(*r),&addClass);
    if(r){r->registry=EnumBannerPattern_nativeRegistry(h);if(!r->registry)r=NULL;}MCObjectRootScope_end(&scope);return r;
}
RecipesBannersRecipeDuplicatePattern *RecipesBannersRecipeDuplicatePattern_new(MCObjectHeap *h) {return (RecipesBannersRecipeDuplicatePattern *)MCObjectHeap_alloc(h,sizeof(RecipesBannersRecipeDuplicatePattern),&duplicateClass);}
static bool begin(MCObjectRootScope *scope,MCObject *r,InventoryCrafting *g) {return MCObjectRootScope_begin(scope,r->heap)&&MCObjectRootScope_pin(scope,r)&&MCObjectRootScope_pin(scope,(MCObject *)g);}
static bool is(ItemStack *s,int32_t id) {return s->item==ItemStack_registryItem(id);}
static bool char_is(BannerStringArray *layers,int32_t row,int32_t col,bool space,MCObjectHeap *h) {
    if(row<0||row>=layers->length||!layers->items[row]||(size_t)col>=NBTString_length(layers->items[row])){MCObjectHeap_fail(h);return false;}
    return (NBTString_units(layers->items[row])[col]==32)==space;
}
/* The actual private matching method iterates the enum values in order. A
   special ingredient needs no dye; a shaped layer always uses fixed /3,%3. */
static EnumBannerPattern *func_179533_c(InventoryCrafting *g) {
    MCObjectHeap *h=g->object.heap;EnumBannerPatternArray *values=EnumBannerPattern_values(h);if(!values)return NULL;
    for(int32_t n=0;n<values->length;n++) {
        EnumBannerPattern *p=values->items[n];if(!EnumBannerPattern_hasValidCrafting(p))continue;
        bool flag=true;
        if(EnumBannerPattern_hasCraftingStack(p)) {
            bool flag1=false,flag2=false;
            for(int32_t i=0;i<InventoryCrafting_getSizeInventory(g)&&flag;i++) {
                ItemStack *s=InventoryCrafting_getStackInSlot(g,i);
                if(s&&!is(s,425)) {
                    if(is(s,351)){if(flag2){flag=false;break;}flag2=true;}
                    else {if(flag1||!ItemStack_isItemEqual(s,EnumBannerPattern_getCraftingStack(p))){flag=false;break;}flag1=true;}
                }
            }
            if(!flag1)flag=false;
        } else {
            BannerStringArray *layers=EnumBannerPattern_getCraftingLayers(p);
            if(!layers->items[0]){MCObjectHeap_fail(h);return NULL;}
            size_t length=NBTString_length(layers->items[0]);
            if((int64_t)InventoryCrafting_getSizeInventory(g)==(int64_t)layers->length*(int64_t)length) {
                int32_t j=-1;
                for(int32_t k=0;k<InventoryCrafting_getSizeInventory(g)&&flag;k++) {
                    ItemStack *s=InventoryCrafting_getStackInSlot(g,k);int32_t row=k/3,col=k%3;
                    if(s&&!is(s,425)) {
                        if(!is(s,351)){flag=false;break;}
                        if(j!=-1&&j!=ItemStack_getMetadata(s)){flag=false;break;}
                        if(char_is(layers,row,col,true,h)){flag=false;break;}
                        if(MCObjectHeap_failed(h))return NULL;
                        j=ItemStack_getMetadata(s);
                    } else if(!char_is(layers,row,col,true,h)) {if(MCObjectHeap_failed(h))return NULL;flag=false;break;}
                }
            } else flag=false;
        }
        if(flag)return p;
    }
    return NULL;
}
bool RecipesBannersRecipeAddPattern_matches(RecipesBannersRecipeAddPattern *r,InventoryCrafting *g,MCObject *world) {
    (void)world;MCObjectRootScope scope={0};bool ok=begin(&scope,(MCObject *)r,g),flag=false,out=false;
    for(int32_t i=0;ok&&i<InventoryCrafting_getSizeInventory(g);i++) {ItemStack *s=InventoryCrafting_getStackInSlot(g,i);if(s&&is(s,425)){if(flag||TileEntityBanner_getPatterns(s)>=6){ok=false;break;}flag=true;}}
    if(ok&&flag)out=func_179533_c(g)!=NULL;
    MCObjectRootScope_end(&scope);return !MCObjectHeap_failed(r->object.heap)&&out;
}
ItemStack *RecipesBannersRecipeAddPattern_getCraftingResult(RecipesBannersRecipeAddPattern *r,InventoryCrafting *g) {
    MCObjectHeap *h=r->object.heap;MCObjectRootScope scope={0};bool ok=begin(&scope,(MCObject *)r,g);ItemStack *out=NULL;
    if(ok)for(int32_t i=0;i<InventoryCrafting_getSizeInventory(g);i++){ItemStack *s=InventoryCrafting_getStackInSlot(g,i);if(s&&is(s,425)){out=ItemStack_copy(h,s);if(out){out->stackSize=1;MCObjectHeap_touch(h);}break;}}
    EnumBannerPattern *p=ok&&!MCObjectHeap_failed(h)?func_179533_c(g):NULL;
    if(p) {
        int32_t color=0;for(int32_t i=0;i<InventoryCrafting_getSizeInventory(g);i++){ItemStack *s=InventoryCrafting_getStackInSlot(g,i);if(s&&is(s,351)){color=ItemStack_getMetadata(s);break;}}
        if(!out){MCObjectHeap_fail(h);MCObjectRootScope_end(&scope);return NULL;}
        NBTString *key=NBTString_literalASCII(h,"BlockEntityTag");NBTTagCompound *bet=key?ItemStack_getSubCompound(out,key,true):NULL;NBTTagList *list=NULL;
        if(bet) {if(NBTTagCompound_hasKeyType_ascii(bet,"Patterns",9))list=NBTTagCompound_getTagList_ascii(bet,"Patterns",10);else {list=NBTTagList_new(h);if(list&&!NBTTagCompound_setTag_ascii(bet,"Patterns",(NBTBase *)list))list=NULL;}}
        NBTTagCompound *entry=list?NBTTagCompound_new(h):NULL;
        if(entry&&NBTTagCompound_setString_ascii(entry,"Pattern",EnumBannerPattern_getPatternID(p))&&NBTTagCompound_setInteger_ascii(entry,"Color",color)) {if(!NBTTagList_appendTag(list,(NBTBase *)entry))out=NULL;}else out=NULL;
    }
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(h)?NULL:out;
}
int32_t RecipesBannersRecipeAddPattern_getRecipeSize(const RecipesBannersRecipeAddPattern *r) {(void)r;return 10;}
ItemStack *RecipesBannersRecipeAddPattern_getRecipeOutput(RecipesBannersRecipeAddPattern *r) {(void)r;return NULL;}
static ItemStackArray *remaining(MCObject *r,InventoryCrafting *g,bool duplicate) {
    MCObjectHeap *h=r->heap;MCObjectRootScope scope={0};bool ok=begin(&scope,r,g);ItemStackArray *out=ok?ItemStackArray_new(h,InventoryCrafting_getSizeInventory(g)):NULL;
    for(int32_t i=0;out&&i<out->length;i++) {ItemStack *s=InventoryCrafting_getStackInSlot(g,i);if(s){if(!s->item){MCObjectHeap_fail(h);break;}const Item *container=ItemStack_registryContainerItem(s->item);if(container){out->items[i]=ItemStack_new_item(h,container);if(!out->items[i])break;}else if(duplicate&&ItemStack_hasTagCompound(s)&&TileEntityBanner_getPatterns(s)>0){out->items[i]=ItemStack_copy(h,s);if(!out->items[i])break;out->items[i]->stackSize=1;}MCObjectHeap_touch(h);}}
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(h)?NULL:out;
}
ItemStackArray *RecipesBannersRecipeAddPattern_getRemainingItems(RecipesBannersRecipeAddPattern *r,InventoryCrafting *g) {return remaining((MCObject *)r,g,false);}
bool RecipesBannersRecipeDuplicatePattern_matches(RecipesBannersRecipeDuplicatePattern *r,InventoryCrafting *g,MCObject *world) {
    (void)world;MCObjectRootScope scope={0};bool ok=begin(&scope,(MCObject *)r,g);ItemStack *patterned=NULL,*blank=NULL;
    for(int32_t i=0;ok&&i<InventoryCrafting_getSizeInventory(g);i++) {
        ItemStack *s=InventoryCrafting_getStackInSlot(g,i);if(!s)continue;if(!is(s,425)||(patterned&&blank)){ok=false;break;}
        int32_t color=TileEntityBanner_getBaseColor(s);bool has=TileEntityBanner_getPatterns(s)>0;
        if(patterned) {if(has||color!=TileEntityBanner_getBaseColor(patterned)){ok=false;break;}blank=s;}
        else if(blank) {if(!has||color!=TileEntityBanner_getBaseColor(blank)){ok=false;break;}patterned=s;}
        else if(has)patterned=s;else blank=s;
    }
    MCObjectRootScope_end(&scope);return !MCObjectHeap_failed(r->object.heap)&&ok&&patterned&&blank;
}
ItemStack *RecipesBannersRecipeDuplicatePattern_getCraftingResult(RecipesBannersRecipeDuplicatePattern *r,InventoryCrafting *g) {
    MCObjectHeap *h=r->object.heap;MCObjectRootScope scope={0};bool ok=begin(&scope,(MCObject *)r,g);ItemStack *out=NULL;
    if(ok)for(int32_t i=0;i<InventoryCrafting_getSizeInventory(g);i++){ItemStack *s=InventoryCrafting_getStackInSlot(g,i);if(s&&TileEntityBanner_getPatterns(s)>0){out=ItemStack_copy(h,s);if(out){out->stackSize=1;MCObjectHeap_touch(h);}break;}}
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(h)?NULL:out;
}
int32_t RecipesBannersRecipeDuplicatePattern_getRecipeSize(const RecipesBannersRecipeDuplicatePattern *r) {(void)r;return 2;}
ItemStack *RecipesBannersRecipeDuplicatePattern_getRecipeOutput(RecipesBannersRecipeDuplicatePattern *r) {(void)r;return NULL;}
ItemStackArray *RecipesBannersRecipeDuplicatePattern_getRemainingItems(RecipesBannersRecipeDuplicatePattern *r,InventoryCrafting *g) {return remaining((MCObject *)r,g,true);}
static bool am(MCObject *r,InventoryCrafting *g,MCObject *w){return RecipesBannersRecipeAddPattern_matches((RecipesBannersRecipeAddPattern *)r,g,w);}
static ItemStack *ac(MCObject *r,InventoryCrafting *g,ItemStackDisplayNameDispatch d,MCObject *c){(void)d;(void)c;return RecipesBannersRecipeAddPattern_getCraftingResult((RecipesBannersRecipeAddPattern *)r,g);}
static int32_t as(const MCObject *r){return RecipesBannersRecipeAddPattern_getRecipeSize((const RecipesBannersRecipeAddPattern *)r);}
static ItemStack *ap(MCObject *r){return RecipesBannersRecipeAddPattern_getRecipeOutput((RecipesBannersRecipeAddPattern *)r);}
static ItemStackArray *ar(MCObject *r,InventoryCrafting *g){return RecipesBannersRecipeAddPattern_getRemainingItems((RecipesBannersRecipeAddPattern *)r,g);}
static bool dm(MCObject *r,InventoryCrafting *g,MCObject *w){return RecipesBannersRecipeDuplicatePattern_matches((RecipesBannersRecipeDuplicatePattern *)r,g,w);}
static ItemStack *dc(MCObject *r,InventoryCrafting *g,ItemStackDisplayNameDispatch d,MCObject *c){(void)d;(void)c;return RecipesBannersRecipeDuplicatePattern_getCraftingResult((RecipesBannersRecipeDuplicatePattern *)r,g);}
static int32_t ds(const MCObject *r){return RecipesBannersRecipeDuplicatePattern_getRecipeSize((const RecipesBannersRecipeDuplicatePattern *)r);}
static ItemStack *dp(MCObject *r){return RecipesBannersRecipeDuplicatePattern_getRecipeOutput((RecipesBannersRecipeDuplicatePattern *)r);}
static ItemStackArray *dr(MCObject *r,InventoryCrafting *g){return RecipesBannersRecipeDuplicatePattern_getRemainingItems((RecipesBannersRecipeDuplicatePattern *)r,g);}
static const IRecipeMethods addMethods={am,ac,as,ap,ar},duplicateMethods={dm,dc,ds,dp,dr};
IRecipe RecipesBannersRecipeAddPattern_asRecipe(RecipesBannersRecipeAddPattern *r){return (IRecipe){(MCObject *)r,&addMethods,IRECIPE_OTHER};}
IRecipe RecipesBannersRecipeDuplicatePattern_asRecipe(RecipesBannersRecipeDuplicatePattern *r){return (IRecipe){(MCObject *)r,&duplicateMethods,IRECIPE_OTHER};}
