#include "item/ItemArmor.h"
#include "item/crafting/RecipesArmorDyes.h"
#include "item/crafting/RecipeFireworks.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static InventoryCrafting *grid_new(MCObjectHeap *h) { InventoryCrafting *g=InventoryCrafting_new(h,NULL,NULL,3,3); CHECK(g); return g; }
static ItemStack *stack(MCObjectHeap *h,int32_t id,int32_t n,int32_t damage) { ItemStack *s=ItemStack_new(h,ItemStack_registryItem(id),n,damage); CHECK(s); return s; }
static void set(InventoryCrafting *g,int32_t index,ItemStack *s) { g->stackList->items[index]=s; MCObjectHeap_touch(g->object.heap); }
static void clear(InventoryCrafting *g) { InventoryCrafting_clear(g); }
static NBTTagCompound *compound(MCObjectHeap *h) { NBTTagCompound *t=NBTTagCompound_new(h); CHECK(t); return t; }
static NBTIntArrayStorage *ints(NBTTagCompound *t,const char *key) { NBTBase *v=NBTTagCompound_getTag_ascii(t,key); CHECK(v&&NBTBase_getId(v)==11); return NBTTagIntArray_getIntArray((NBTTagIntArray *)v); }
static void item_armor(void) {
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h));
    const Item *leather=ItemStack_registryItem(298),*iron=ItemStack_registryItem(306); ItemStack *s=stack(h,1,0,7);
    CHECK(!ItemArmor_isInstance(s->item)); CHECK(ItemArmor_getArmorMaterial(leather)==ITEMARMOR_LEATHER); CHECK(ItemArmor_getArmorMaterial(iron)==ITEMARMOR_IRON);
    for (int id=298;id<=317;id++) CHECK(ItemArmor_isInstance(ItemStack_registryItem(id)));
    CHECK(!ItemArmor_hasColor(h,leather,s)); CHECK(ItemArmor_getColor(h,leather,s)==10511680); CHECK(ItemArmor_getColor(h,iron,NULL)==-1);
    CHECK(ItemArmor_getColorFromItemStack(h,leather,NULL,1)==16777215); CHECK(!ItemArmor_hasColor(h,iron,NULL)); CHECK(ItemArmor_removeColor(h,iron,NULL));
    NBTTagCompound *tag=compound(h),*display=compound(h); CHECK(NBTTagCompound_setTag_ascii(tag,"display",(NBTBase *)display)); CHECK(ItemStack_setTagCompound(s,tag));
    CHECK(NBTTagCompound_setShort_ascii(display,"color",12)); CHECK(!ItemArmor_hasColor(h,leather,s)); CHECK(ItemArmor_getColor(h,leather,s)==10511680);
    CHECK(ItemArmor_removeColor(h,leather,s)); CHECK(!NBTTagCompound_hasKey_ascii(display,"color")); CHECK(s->stackTagCompound==tag&&NBTTagCompound_getTag_ascii(tag,"display")== (NBTBase *)display);
    CHECK(ItemArmor_setColor(h,leather,s,INT32_MIN)); CHECK(ItemArmor_hasColor(h,leather,s)); CHECK(ItemArmor_getColor(h,leather,s)==INT32_MIN); CHECK(ItemArmor_getColorFromItemStack(h,leather,s,0)==16777215);
    CHECK(ItemArmor_setColor(h,leather,s,0x12345678)); CHECK(ItemArmor_getColorFromItemStack(h,leather,s,-1)==0x12345678); CHECK(s->item==ItemStack_registryItem(1)&&s->stackSize==0&&s->itemDamage==7);
    CHECK(NBTTagCompound_setInteger_ascii(tag,"display",4)); CHECK(!ItemArmor_hasColor(h,leather,s)); CHECK(ItemArmor_getColor(h,leather,s)==10511680);
    CHECK(ItemArmor_setColor(h,leather,s,-1)); CHECK(NBTTagCompound_hasKeyType_ascii(tag,"display",10)); CHECK(ItemArmor_getColor(h,leather,s)==-1);
    CHECK(ItemArmor_removeColor(h,leather,s)); CHECK(!ItemArmor_hasColor(h,leather,s)); CHECK(NBTTagCompound_hasKeyType_ascii(tag,"display",10)); CHECK(!MCObjectHeap_failed(h));
    MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
    h=MCObjectHeap_new(1024*1024); CHECK(h); s=stack(h,306,1,0); CHECK(!ItemArmor_setColor(h,iron,s,2)&&MCObjectHeap_failed(h)); MCObjectHeap_free(h);
}
static void armor_recipe(void) {
    static const int expected[]={0x191919,0x993333,0x667f33,0x664c33,0x334cb2,0x7f3fb2,0x4c7f99,0x999999,0x4c4c4c,0xf27fa5,0x7fcc19,0xe5e533,0x6699d8,0xb24cd8,0xd87f33,0xffffff};
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); InventoryCrafting *g=grid_new(h); RecipesArmorDyes *r=RecipesArmorDyes_new(h); CHECK(r);
    IRecipe api=RecipesArmorDyes_asRecipe(r); CHECK(api.methods->getRecipeSize(api.instance)==10&&!api.methods->getRecipeOutput(api.instance)); CHECK(!RecipesArmorDyes_matches(r,g,NULL)); CHECK(!RecipesArmorDyes_getCraftingResult(r,g));
    ItemStack *armor=stack(h,298,-7,91); set(g,8,armor);
    for (int dye=0;dye<16;dye++) {
        set(g,0,stack(h,351,0,dye)); CHECK(RecipesArmorDyes_matches(r,g,NULL)); ItemStack *out=api.methods->getCraftingResult(api.instance,g,NULL,NULL); CHECK(out&&out!=armor&&out->stackSize==1&&out->itemDamage==91&&out->item==armor->item); CHECK(ItemArmor_getColor(h,armor->item,out)==expected[dye]); CHECK(!armor->stackTagCompound);
    }
    set(g,0,stack(h,351,-8,16)); CHECK(RecipesArmorDyes_matches(r,g,NULL)); ItemStack *out=RecipesArmorDyes_getCraftingResult(r,g); CHECK(out&&ItemArmor_getColor(h,armor->item,out)==expected[0]); g->stackList->items[0]->itemDamage=-1; out=RecipesArmorDyes_getCraftingResult(r,g); CHECK(out&&ItemArmor_getColor(h,armor->item,out)==expected[0]);
    CHECK(ItemArmor_setColor(h,armor->item,armor,0)); set(g,0,NULL); CHECK(!RecipesArmorDyes_matches(r,g,NULL)); out=RecipesArmorDyes_getCraftingResult(r,g); CHECK(out&&ItemArmor_getColor(h,armor->item,out)==0); CHECK(out->stackTagCompound!=armor->stackTagCompound);
    CHECK(ItemArmor_setColor(h,armor->item,armor,-1)); out=RecipesArmorDyes_getCraftingResult(r,g); CHECK(out&&ItemArmor_getColor(h,armor->item,out)==0xffffff);
    set(g,0,stack(h,351,1,1)); set(g,1,stack(h,298,1,0)); CHECK(!RecipesArmorDyes_matches(r,g,NULL)); CHECK(!RecipesArmorDyes_getCraftingResult(r,g)); set(g,1,NULL); set(g,8,stack(h,306,1,0)); CHECK(!RecipesArmorDyes_matches(r,g,NULL)); CHECK(!RecipesArmorDyes_getCraftingResult(r,g));
    set(g,8,NULL); CHECK(!RecipesArmorDyes_matches(r,g,NULL)); CHECK(!RecipesArmorDyes_getCraftingResult(r,g)); set(g,0,stack(h,335,0,8)); ItemStackArray *left=RecipesArmorDyes_getRemainingItems(r,g); CHECK(left&&left->length==9&&left->items[0]&&left->items[0]->item==ItemStack_registryItem(325)&&left->items[0]->stackSize==1);
    CHECK(!MCObjectHeap_failed(h)); MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
    h=MCObjectHeap_new(1024*1024); CHECK(h); g=grid_new(h); r=RecipesArmorDyes_new(h); set(g,0,stack(h,298,1,0)); CHECK(!RecipesArmorDyes_getCraftingResult(r,g)&&MCObjectHeap_failed(h)); MCObjectHeap_free(h);
}
static void fireworks_recipe(void) {
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); InventoryCrafting *g=grid_new(h); RecipeFireworks *r=RecipeFireworks_new(h); CHECK(r);
    IRecipe api=RecipeFireworks_asRecipe(r); CHECK(api.methods->getRecipeSize(api.instance)==10&&!api.methods->getRecipeOutput(api.instance));
    set(g,0,stack(h,339,0,0)); set(g,1,stack(h,289,-4,0)); CHECK(RecipeFireworks_matches(r,g,NULL)); ItemStack *out=RecipeFireworks_getRecipeOutput(r); CHECK(out&&out->item==ItemStack_registryItem(401)&&out->stackSize==1&&!out->stackTagCompound);
    set(g,2,stack(h,289,64,0)); CHECK(RecipeFireworks_matches(r,g,NULL)); CHECK(!r->field_92102_a->stackTagCompound);
    ItemStack *star=stack(h,402,-3,7); NBTTagCompound *root=compound(h),*explosion=compound(h); CHECK(NBTTagCompound_setInteger_ascii(explosion,"marker",9)); CHECK(NBTTagCompound_setTag_ascii(root,"Explosion",(NBTBase *)explosion)); CHECK(ItemStack_setTagCompound(star,root)); set(g,3,star); CHECK(RecipeFireworks_matches(r,g,NULL));
    NBTTagCompound *fireworks=NBTTagCompound_getCompoundTag_ascii(r->field_92102_a->stackTagCompound,"Fireworks"); NBTTagList *list=NBTTagCompound_getTagList_ascii(fireworks,"Explosions",10); CHECK(NBTTagCompound_getByte_ascii(fireworks,"Flight")==2&&NBTTagList_tagCount(list)==1); CHECK(NBTTagList_getCompoundTagAt(list,0)==explosion);
    out=RecipeFireworks_getCraftingResult(r,NULL); CHECK(out&&out!=r->field_92102_a); NBTTagList *copied=NBTTagCompound_getTagList_ascii(NBTTagCompound_getCompoundTag_ascii(out->stackTagCompound,"Fireworks"),"Explosions",10); CHECK(NBTTagList_getCompoundTagAt(copied,0)!=explosion); CHECK(NBTTagCompound_setInteger_ascii(explosion,"marker",10)); CHECK(NBTTagCompound_getInteger_ascii(NBTTagList_getCompoundTagAt(list,0),"marker")==10); CHECK(NBTTagCompound_getInteger_ascii(NBTTagList_getCompoundTagAt(copied,0),"marker")==9);
    clear(g); set(g,0,star); set(g,1,stack(h,351,-1,17)); CHECK(RecipeFireworks_matches(r,g,NULL)); CHECK(r->field_92102_a!=star&&r->field_92102_a->stackSize==1&&r->field_92102_a->itemDamage==7); NBTTagCompound *fade=NBTTagCompound_getCompoundTag_ascii(r->field_92102_a->stackTagCompound,"Explosion"); NBTIntArrayStorage *colors=ints(fade,"FadeColors"); CHECK(NBTIntArrayStorage_length(colors)==1&&NBTIntArrayStorage_data(colors)[0]==11743532); CHECK(!NBTTagCompound_hasKey_ascii(explosion,"FadeColors"));
    set(g,0,stack(h,402,7,4)); CHECK(!RecipeFireworks_matches(r,g,NULL)); CHECK(r->field_92102_a&&r->field_92102_a->stackSize==1&&!r->field_92102_a->stackTagCompound); out=RecipeFireworks_getCraftingResult(r,NULL); CHECK(out&&out!=r->field_92102_a&&!out->stackTagCompound);
    CHECK(ItemStack_setTagCompound(g->stackList->items[0],compound(h))); CHECK(RecipeFireworks_matches(r,g,NULL)); CHECK(NBTBase_hasNoTags((NBTBase *)r->field_92102_a->stackTagCompound)); CHECK(!NBTTagCompound_hasKey_ascii(r->field_92102_a->stackTagCompound,"Explosion"));
    CHECK(NBTTagCompound_setInteger_ascii(g->stackList->items[0]->stackTagCompound,"Explosion",42)); CHECK(RecipeFireworks_matches(r,g,NULL)); CHECK(NBTTagCompound_getInteger_ascii(r->field_92102_a->stackTagCompound,"Explosion")==42);
    clear(g); set(g,0,stack(h,289,0,0)); set(g,1,stack(h,351,0,33)); set(g,2,stack(h,348,1,0)); set(g,3,stack(h,264,1,0)); set(g,4,stack(h,288,1,0)); set(g,5,stack(h,351,1,15)); CHECK(RecipeFireworks_matches(r,g,NULL)); explosion=NBTTagCompound_getCompoundTag_ascii(r->field_92102_a->stackTagCompound,"Explosion"); CHECK(NBTTagCompound_getByte_ascii(explosion,"Type")==4&&NBTTagCompound_getBoolean_ascii(explosion,"Flicker")&&NBTTagCompound_getBoolean_ascii(explosion,"Trail")); colors=ints(explosion,"Colors"); CHECK(NBTIntArrayStorage_length(colors)==2&&NBTIntArrayStorage_data(colors)[0]==11743532&&NBTIntArrayStorage_data(colors)[1]==15790320);
    set(g,6,stack(h,397,1,0)); CHECK(!RecipeFireworks_matches(r,g,NULL)); CHECK(!RecipeFireworks_getRecipeOutput(r)); set(g,0,stack(h,335,-1,0)); ItemStackArray *left=RecipeFireworks_getRemainingItems(r,g); CHECK(left&&left->items[0]&&left->items[0]->item==ItemStack_registryItem(325)); CHECK(!MCObjectHeap_failed(h));
    MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
    h=MCObjectHeap_new(1024*1024); CHECK(h); r=RecipeFireworks_new(h); CHECK(r&&!RecipeFireworks_getCraftingResult(r,NULL)&&MCObjectHeap_failed(h)); MCObjectHeap_free(h);
}
static void graph_ownership(void) {
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); InventoryCrafting *g=grid_new(h); RecipeFireworks *r=RecipeFireworks_new(h); CHECK(r);
    ItemStack *star=stack(h,402,1,0); NBTTagCompound *tag=compound(h),*explosion=compound(h); CHECK(NBTTagCompound_setTag_ascii(tag,"Explosion",(NBTBase *)explosion)&&ItemStack_setTagCompound(star,tag)); set(g,0,stack(h,339,1,0)); set(g,1,stack(h,289,1,0)); set(g,2,star); set(g,3,star); CHECK(RecipeFireworks_matches(r,g,NULL));
    NBTTagList *list=NBTTagCompound_getTagList_ascii(NBTTagCompound_getCompoundTag_ascii(r->field_92102_a->stackTagCompound,"Fireworks"),"Explosions",10); CHECK(NBTTagList_tagCount(list)==2&&NBTTagList_getCompoundTagAt(list,0)==explosion&&NBTTagList_getCompoundTagAt(list,1)==explosion);
    ItemStack *copy=RecipeFireworks_getCraftingResult(r,NULL); CHECK(copy); list=NBTTagCompound_getTagList_ascii(NBTTagCompound_getCompoundTag_ascii(copy->stackTagCompound,"Fireworks"),"Explosions",10); CHECK(NBTTagList_getCompoundTagAt(list,0)!=explosion&&NBTTagList_getCompoundTagAt(list,0)!=NBTTagList_getCompoundTagAt(list,1));
    MCObjectRoot gr={0},rr={0}; CHECK(MCObjectRoot_init(&gr,h,(MCObject *)g)&&MCObjectRoot_init(&rr,h,(MCObject *)r)); MCObjectRootScope_end(&scope); CHECK(MCObjectHeap_collect(h)); MCObjectHeap *branch=MCObjectHeap_clone(h); CHECK(branch); MCObjectRoot bg={0},br={0}; CHECK(MCObjectRoot_rebind(&bg,branch,&gr)&&MCObjectRoot_rebind(&br,branch,&rr)); CHECK(MCObjectRootScope_begin(&scope,branch));
    InventoryCrafting *cg=(InventoryCrafting *)MCObjectRoot_get(&bg); RecipeFireworks *cr=(RecipeFireworks *)MCObjectRoot_get(&br); NBTTagCompound *ce=NBTTagCompound_getCompoundTag_ascii(cg->stackList->items[2]->stackTagCompound,"Explosion"); NBTTagList *cl=NBTTagCompound_getTagList_ascii(NBTTagCompound_getCompoundTag_ascii(cr->field_92102_a->stackTagCompound,"Fireworks"),"Explosions",10); CHECK(ce!=explosion&&NBTTagList_getCompoundTagAt(cl,0)==ce&&NBTTagList_getCompoundTagAt(cl,1)==ce&&cg->stackList->items[2]==cg->stackList->items[3]); CHECK(NBTTagCompound_setInteger_ascii(ce,"branch",1)); CHECK(!NBTTagCompound_hasKey_ascii(explosion,"branch")); CHECK(!MCObjectHeap_failed(h)&&!MCObjectHeap_failed(branch));
    MCObjectRootScope_end(&scope); MCObjectRoot_drop(&bg); MCObjectRoot_drop(&br); MCObjectHeap_free(branch); MCObjectRoot_drop(&gr); CHECK(MCObjectHeap_collect(h)); CHECK(MCObjectRoot_get(&rr)); MCObjectRoot_drop(&rr); CHECK(MCObjectHeap_collect(h)&&MCObjectHeap_liveObjects(h)==0); MCObjectHeap_free(h);
    h=MCObjectHeap_new(1024*1024); MCObjectHeap *other=MCObjectHeap_new(1024*1024); CHECK(h&&other); RecipesArmorDyes *a=RecipesArmorDyes_new(h); CHECK(a); g=grid_new(other); CHECK(!RecipesArmorDyes_matches(a,g,NULL)&&MCObjectHeap_failed(h)); CHECK(!MCObjectHeap_hasBorrowers(h)&&!MCObjectHeap_failed(other)); MCObjectHeap_free(h); MCObjectHeap_free(other);
}
/* Numeric differential vectors contain no code, mappings or game resources.
   The optional stream is compared privately with the licensed original JAR. */
static ItemStack *token(MCObjectHeap *h,int index) {
    static const int ids[]={-1,289,339,351,351,351,402,402,402,402,348,264,385,288,371,397,1,298,298,306};
    if (!index) return NULL;
    int damage=index==3?0:index==4?16:index==5?-1:index>=6&&index<=9?9:7;
    ItemStack *s=stack(h,ids[index],index%3-1,damage); s->itemDamage=damage;
    if (index==7||index==8||index==9) { NBTTagCompound *tag=compound(h); CHECK(ItemStack_setTagCompound(s,tag)); if (index==7) { NBTTagCompound *e=compound(h); CHECK(NBTTagCompound_setInteger_ascii(e,"marker",9)&&NBTTagCompound_setTag_ascii(tag,"Explosion",(NBTBase *)e)); } if (index==9) CHECK(NBTTagCompound_setInteger_ascii(tag,"Explosion",7)); }
    if (index==17) CHECK(ItemArmor_setColor(h,s->item,s,-1));
    return s;
}
static void summary(const char *label,int a,int b,int c,bool match,ItemStack *out,bool failed) {
    printf("%s %d %d %d %d %d %d %d %d %d\n",label,a,b,c,match?1:0,out?ItemStack_registryId(out->item):-1,out?out->stackSize:0,out?out->itemDamage:0,out&&out->stackTagCompound?NBTBase_hashCode((NBTBase *)out->stackTagCompound):0,failed?1:0);
}
static void differential(bool emit) {
    for (int a=0;a<20;a++) for (int b=0;b<20;b++) for (int c=0;c<20;c++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); InventoryCrafting *g=grid_new(h); set(g,0,token(h,a)); set(g,4,token(h,b)); set(g,8,token(h,c));
        RecipeFireworks *f=RecipeFireworks_new(h); RecipesArmorDyes *r=RecipesArmorDyes_new(h); CHECK(f&&r); bool matched=RecipeFireworks_matches(f,g,NULL); ItemStack *out=RecipeFireworks_getRecipeOutput(f);
        CHECK(!MCObjectHeap_failed(h)); if (matched) { ItemStack *copy=RecipeFireworks_getCraftingResult(f,NULL); CHECK(copy&&copy!=out&&ItemStack_areItemStacksEqual(copy,out)); }
        if (emit) summary("F",a,b,c,matched,out,false);
        matched=RecipesArmorDyes_matches(r,g,NULL); out=RecipesArmorDyes_getCraftingResult(r,g); if (matched) CHECK(out&&!MCObjectHeap_failed(h));
        if (emit) summary("A",a,b,c,matched,out,MCObjectHeap_failed(h));
        MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
    }
    static const int32_t extra_damage[]={-1,0,1,7,15,16};
    for (int damage=-2;damage<34;damage++) for (int state=0;state<12;state++) for (int extra=0;extra<6;extra++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); InventoryCrafting *g=grid_new(h); RecipesArmorDyes *r=RecipesArmorDyes_new(h); CHECK(r);
        ItemStack *armor=stack(h,298,-5,93); set(g,4,armor); ItemStack *dye=stack(h,351,0,damage); dye->itemDamage=damage; set(g,0,dye); dye=stack(h,351,-2,extra_damage[extra]); dye->itemDamage=extra_damage[extra]; set(g,8,dye);
        if (state) {
            NBTTagCompound *tag=compound(h),*display=compound(h); CHECK(ItemStack_setTagCompound(armor,tag)&&NBTTagCompound_setTag_ascii(tag,"display",(NBTBase *)display));
            switch (state) {
                case 1: CHECK(NBTTagCompound_setByte_ascii(display,"color",1)); break;
                case 2: CHECK(NBTTagCompound_setShort_ascii(display,"color",1)); break;
                case 3: CHECK(NBTTagCompound_setFloat_ascii(display,"color",INFINITY)); break;
                case 4: CHECK(NBTTagCompound_setDouble_ascii(display,"color",NAN)); break;
                case 5: CHECK(NBTTagCompound_setInteger_ascii(display,"color",0)); break;
                case 6: CHECK(NBTTagCompound_setInteger_ascii(display,"color",1)); break;
                case 7: CHECK(NBTTagCompound_setInteger_ascii(display,"color",0x112233)); break;
                case 8: CHECK(NBTTagCompound_setInteger_ascii(display,"color",INT32_MIN)); break;
                case 9: CHECK(NBTTagCompound_setInteger_ascii(display,"color",-1)); break;
                case 10: CHECK(NBTTagCompound_setInteger_ascii(tag,"display",3)); break;
                case 11: CHECK(NBTTagCompound_setString_ascii(display,"color",NBTString_fromASCII(h,"bad"))); break;
            }
        }
        CHECK(RecipesArmorDyes_matches(r,g,NULL)); ItemStack *out=RecipesArmorDyes_getCraftingResult(r,g); CHECK(out&&!MCObjectHeap_failed(h)); CHECK(out!=armor&&out->stackTagCompound!=armor->stackTagCompound&&out->stackSize==1&&out->itemDamage==93);
        if (emit) summary("D",damage,state,extra,true,out,false);
        MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
    }
    static const int32_t shapes[]={0,385,288,371,397};
    for (int damage=0;damage<16;damage++) for (int shape=0;shape<5;shape++) for (int flags=0;flags<4;flags++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024); CHECK(h); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h)); InventoryCrafting *g=grid_new(h); RecipeFireworks *r=RecipeFireworks_new(h); CHECK(r);
        set(g,0,stack(h,289,0,0)); set(g,2,stack(h,351,-1,damage)); if (shape) set(g,4,stack(h,shapes[shape],-3,0)); if (flags&1) set(g,6,stack(h,348,0,0)); if (flags&2) set(g,8,stack(h,264,-1,0));
        CHECK(RecipeFireworks_matches(r,g,NULL)); ItemStack *out=RecipeFireworks_getRecipeOutput(r); CHECK(out&&!MCObjectHeap_failed(h)); ItemStack *copy=RecipeFireworks_getCraftingResult(r,NULL); CHECK(copy&&copy!=out&&ItemStack_areItemStacksEqual(copy,out));
        if (emit) summary("S",damage,shape,flags,true,out,false);
        MCObjectRootScope_end(&scope); MCObjectHeap_free(h);
    }
}
int main(int argc,char **argv) {
    bool emit=argc==2&&strcmp(argv[1],"--facts")==0;
    if (!emit) { item_armor(); armor_recipe(); fireworks_recipe(); graph_ownership(); }
    differential(emit);
    if (!emit) printf("source armor/fireworks: %u checks\n",checks);
    return 0;
}
