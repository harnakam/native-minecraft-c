#include "stats/StatList.h"
#include "stats/StatFileWriter.h"
#include "util/MCGameplayCrafting.h"
#include "nbt/NBTTagCompound.h"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"craft stats check %u at %d: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
static ItemStack *stack(MCObjectHeap *h,int id,int n,int damage){ItemStack *s=ItemStack_new(h,ItemStack_registryItem(id),n,damage);CHECK(s);return s;}
static uint32_t bits(float v){uint32_t b;memcpy(&b,&v,sizeof(b));return b;}
static void furnace_default(void) {
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024);CHECK(h);FurnaceRecipes *r=FurnaceRecipes_new(h);CHECK(r);
    FurnaceRecipeMap *m=FurnaceRecipes_getSmeltingList(r);CHECK(m==r->smeltingList&&FurnaceRecipeMap_size(m)==26);
    static const int facts[][4]={{15,32767,265,0},{14,32767,266,0},{56,32767,264,0},{12,32767,20,0},
        {319,32767,320,0},{363,32767,364,0},{365,32767,366,0},{411,32767,412,0},{423,32767,424,0},
        {4,32767,1,0},{98,0,98,2},{337,32767,336,0},{82,32767,172,0},{81,32767,351,2},
        {17,32767,263,1},{162,32767,263,1},{129,32767,388,0},{392,32767,393,0},
        {87,32767,405,0},{19,1,19,0},{349,0,350,0},{349,1,350,1},{16,32767,263,0},
        {73,32767,331,0},{21,32767,351,4},{153,32767,406,0}};
    static const float xp[]={.7f,1,1,.1f,.35f,.35f,.35f,.35f,.35f,.1f,.1f,.3f,.35f,.2f,
        .15f,.15f,1,.35f,.1f,.15f,.35f,.35f,.1f,.7f,.2f,.2f};
    for(int i=0;i<26;i++) {
        ItemStack *key,*value;CHECK(FurnaceRecipeMap_entry(m,i,&key,&value));
        CHECK(ItemStack_registryId(key->item)==facts[i][0]&&key->itemDamage==facts[i][1]);
        CHECK(ItemStack_registryId(value->item)==facts[i][2]&&value->itemDamage==facts[i][3]&&value->stackSize==1);
        CHECK(FurnaceRecipes_getSmeltingResult(r,key)==value);
        CHECK(bits(FurnaceRecipes_getSmeltingExperience(r,value))==bits(xp[i]));
        ItemStack *input=stack(h,facts[i][0],0,facts[i][1]==32767?12345:facts[i][1]);
        CHECK(ItemStack_setTagCompound(input,NBTTagCompound_new(h)));
        CHECK(NBTTagCompound_setInteger_ascii(input->stackTagCompound,"ignored",-17));
        CHECK(FurnaceRecipes_getSmeltingResult(r,input)==value);
    }
    CHECK(FurnaceRecipes_getSmeltingResult(r,stack(h,349,1,2))==NULL);
    CHECK(FurnaceRecipes_getSmeltingResult(r,stack(h,98,1,1))==NULL);
    CHECK(FurnaceRecipes_getSmeltingExperience(r,stack(h,358,1,0))==0);
    CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static const Item *block_item(const Block *b){CHECK(b==(const Block *)(uintptr_t)15);return ItemStack_registryItem(15);}
static void live_maps(void) {
    MCObjectHeap *h=MCObjectHeap_new(8*1024*1024);CHECK(h);FurnaceRecipes *r=FurnaceRecipes_newEmpty(h);CHECK(r);
    FurnaceRecipeMap *m=FurnaceRecipes_getSmeltingList(r);ItemStack *key=stack(h,1,0,0),*a=stack(h,265,-2,0),*b=stack(h,266,0,0);
    CHECK(FurnaceRecipes_addSmeltingRecipe(r,key,a,.7f));CHECK(FurnaceRecipes_addSmeltingRecipe(r,key,b,1.f));
    CHECK(FurnaceRecipeMap_size(m)==1&&FurnaceRecipes_getSmeltingResult(r,key)==b);
    CHECK(FurnaceRecipes_getSmeltingExperience(r,a)==.7f&&FurnaceRecipes_getSmeltingExperience(r,b)==1.f);
    CHECK(FurnaceRecipeMap_put(m,key,a));CHECK(FurnaceRecipes_getSmeltingResult(r,key)==a);
    CHECK(FurnaceRecipes_getSmeltingExperience(r,b)==1.f);
    a->itemDamage=3;key->itemDamage=4;MCObjectHeap_touch(h);
    CHECK(FurnaceRecipes_getSmeltingResult(r,stack(h,1,-1,4))==a&&FurnaceRecipes_getSmeltingResult(r,stack(h,1,1,0))==NULL);
    CHECK(FurnaceRecipes_getSmeltingExperience(r,stack(h,265,9,3))==.7f);
    CHECK(FurnaceRecipeMap_remove(m,key)&&FurnaceRecipeMap_size(m)==0);
    CHECK(FurnaceRecipes_getSmeltingResult(r,NULL)==NULL&&!MCObjectHeap_failed(h));
    CHECK(FurnaceRecipes_getSmeltingExperience(r,b)==1.f);
    CHECK(FurnaceRecipes_addSmeltingRecipeForBlock(r,(const Block *)(uintptr_t)15,block_item,b,.35f));
    CHECK(FurnaceRecipes_getSmeltingResult(r,stack(h,15,-9,INT32_MAX))==b);
    ItemStack *sourceKey,*sourceValue;
    CHECK(FurnaceRecipeMap_entry(m,0,&sourceKey,&sourceValue));
    CHECK(sourceKey&&sourceKey->item==ItemStack_registryItem(15)&&sourceKey->stackSize==1&&sourceKey->itemDamage==32767&&sourceValue==b);
    /* The removed key is unreachable and is freed by collection. Compare the
       clone with this actual retained entry, whose address remains live. */
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)r));CHECK(MCObjectHeap_collect(h));
    ItemStack *retainedKey,*retainedValue;
    CHECK(FurnaceRecipeMap_entry(m,0,&retainedKey,&retainedValue));CHECK(retainedKey==sourceKey&&retainedValue==sourceValue);
    MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);MCObjectRoot cr={0};CHECK(MCObjectRoot_rebind(&cr,copy,&root));
    FurnaceRecipes *rr=(FurnaceRecipes *)MCObjectRoot_get(&cr);ItemStack *ck,*cv;CHECK(rr!=r&&rr->smeltingList!=m);
    CHECK(FurnaceRecipeMap_entry(rr->smeltingList,0,&ck,&cv));CHECK(ck!=sourceKey&&cv!=sourceValue&&ck->item==sourceKey->item&&cv->item==sourceValue->item);
    CHECK(ck->object.heap==copy&&cv->object.heap==copy&&sourceKey->object.heap==h&&sourceValue->object.heap==h);
    CHECK(ck->stackSize==sourceKey->stackSize&&ck->itemDamage==sourceKey->itemDamage&&cv->stackSize==sourceValue->stackSize&&cv->itemDamage==sourceValue->itemDamage);
    CHECK(MCObjectHeap_identityHashCode((MCObject *)ck)==MCObjectHeap_identityHashCode((MCObject *)sourceKey)&&MCObjectHeap_identityHashCode((MCObject *)cv)==MCObjectHeap_identityHashCode((MCObject *)sourceValue));
    CHECK(FurnaceRecipes_getSmeltingResult(rr,ck)==cv&&FurnaceRecipes_getSmeltingExperience(rr,cv)==.35f);
    cv->itemDamage=7;MCObjectHeap_touch(copy);CHECK(b->itemDamage==0);
    MCObjectRoot_drop(&cr);MCObjectHeap_free(copy);MCObjectRoot_drop(&root);MCObjectHeap_free(h);
}
static void all_craft_stats(void) {
    MCObjectHeap *h=MCObjectHeap_new(64*1024*1024);CHECK(h);StatList *s=StatList_new(h);CraftingManager *c=MCGameplayCrafting_newManager(h);FurnaceRecipes *f=FurnaceRecipes_new(h);CHECK(s&&c&&f);
    CHECK(RecipeList_size(CraftingManager_getRecipeList(c))==373&&StatList_initCraftableStats(s,c,f));
    unsigned filled=0;for(size_t i=0;i<STATLIST_CRAFT_STATS_COUNT;i++)if(s->objectCraftStats[i]){filled++;CHECK(i<2268);CHECK(StatBase_getKind(s->objectCraftStats[i])==STAT_BASE_KIND_CRAFTING);CHECK(StatList_getOneShotStat(s,s->objectCraftStats[i]->statId)==s->objectCraftStats[i]);}
    CHECK(filled==232);CHECK(!s->objectCraftStats[358]&&!s->objectCraftStats[387]&&!s->objectCraftStats[401]&&!s->objectCraftStats[402]);
    CHECK(s->objectCraftStats[395]&&s->objectCraftStats[424]&&s->objectCraftStats[339]);
    CHECK(s->objectCraftStats[2]==s->objectCraftStats[3]&&s->objectCraftStats[60]==s->objectCraftStats[3]);
    CHECK(s->objectCraftStats[61]==s->objectCraftStats[62]&&s->objectCraftStats[91]==s->objectCraftStats[86]);
    CHECK(NBTString_equalsASCII(s->objectCraftStats[61]->statId,"stat.craftItem.minecraft.furnace"));
    CHECK(StatCrafting_func_150959_a((StatCrafting *)s->objectCraftStats[61])==ItemStack_registryItem(61));
    StatBase *output[2268];CHECK(StatList_fillCraftStats(s,output,2268));for(int i=0;i<2268;i++)CHECK(output[i]==s->objectCraftStats[i]);
    CHECK(StatList_initAchievementIdentities(s));CHECK(s->dropStat==StatList_getOneShotStat_ascii(s,"stat.drop"));
    CHECK(s->dropStat->isIndependent&&StatBase_getKind(s->dropStat)==STAT_BASE_KIND_BASIC);
    Achievement *open=(Achievement *)StatList_getOneShotStat_ascii(s,"achievement.openInventory"),*wood=(Achievement *)StatList_getOneShotStat_ascii(s,"achievement.mineWood"),*bench=(Achievement *)StatList_getOneShotStat_ascii(s,"achievement.buildWorkBench");
    CHECK(open&&wood&&bench&&open->base.isIndependent&&!open->parentAchievement&&wood->parentAchievement==open&&bench->parentAchievement==wood);
    CHECK(Achievement_getSpecial((Achievement *)StatList_getOneShotStat_ascii(s,"achievement.overpowered")));
    StatFileWriter *writer=StatFileWriter_new(h);CHECK(writer);
    CHECK(StatFileWriter_increaseStat(writer,NULL,&bench->base,1)&&StatFileWriter_readStat(writer,&bench->base)==0);
    CHECK(StatFileWriter_increaseStat(writer,NULL,&open->base,1)&&StatFileWriter_increaseStat(writer,NULL,&wood->base,1)&&StatFileWriter_increaseStat(writer,NULL,&bench->base,1));
    CHECK(StatFileWriter_readStat(writer,&bench->base)==1);
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)s));CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);MCObjectRoot cr={0};CHECK(MCObjectRoot_rebind(&cr,copy,&root));
    StatList *ss=(StatList *)MCObjectRoot_get(&cr);CHECK(ss->objectCraftStats[61]!=s->objectCraftStats[61]);
    CHECK(ss->objectCraftStats[61]==ss->objectCraftStats[62]&&ss->objectCraftStats[61]==StatList_getOneShotStat_ascii(ss,"stat.craftItem.minecraft.furnace"));
    Achievement *bb=(Achievement *)StatList_getOneShotStat_ascii(ss,"achievement.buildWorkBench"),*ww=(Achievement *)StatList_getOneShotStat_ascii(ss,"achievement.mineWood");CHECK(bb!=bench&&bb->parentAchievement==ww&&ww!=wood);
    MCObjectRoot_drop(&cr);MCObjectHeap_free(copy);MCObjectRoot_drop(&root);MCObjectHeap_free(h);
}
static void merge_registration(void) {
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024);CHECK(h);StatList *s=StatList_new(h);CHECK(s);StatBase *array[200]={0};
    StatBase *a=StatBase_newIdentity(h,NBTString_fromASCII(h,"native.first"),STAT_BASE_KIND_BASE),*b=StatBase_newIdentity(h,NBTString_fromASCII(h,"native.second"),STAT_BASE_KIND_BASE);CHECK(a&&b);
    CHECK(StatList_registerStat(s,a)==a&&StatList_registerStat(s,b)==b);CHECK(StatListList_add(s->objectMineStats,a)&&StatListList_add(s->generalStats,a));array[9]=a;
    CHECK(StatList_mergeStatBases(s,array,200,9,8)&&array[8]==a&&StatListList_size(s->allStats)==2);
    array[8]=b;CHECK(StatList_mergeStatBases(s,array,200,9,8)&&array[9]==b);
    CHECK(StatListList_size(s->allStats)==1&&StatListList_get(s->allStats,0)==b&&StatListList_size(s->objectMineStats)==0&&StatListList_size(s->generalStats)==0);
    CHECK(StatList_getOneShotStat_ascii(s,"native.first")==a);
    CHECK(StatListList_add(s->allStats,NULL)&&StatListList_add(s->generalStats,NULL));
    CHECK(StatList_mergeStatBases(s,array,200,11,10)&&StatListList_size(s->allStats)==1&&StatListList_size(s->generalStats)==0);
    CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
typedef struct {MCObject object;ItemStack *first,*second;RecipeList *list;unsigned calls;bool clear;} ProbeRecipe;
static void probe_trace(MCObject *o,MCObjectVisitor v,void *c){ProbeRecipe *p=(ProbeRecipe *)o;p->first=(ItemStack *)v((MCObject *)p->first,c);p->second=(ItemStack *)v((MCObject *)p->second,c);p->list=(RecipeList *)v((MCObject *)p->list,c);}
static const MCObjectClass probe_class={"private source recipe output test",MCObjectHeap_plainClone,probe_trace,NULL};
static ItemStack *output(MCObject *o){ProbeRecipe *p=(ProbeRecipe *)o;CHECK(MCObjectHeap_hasBorrowers(o->heap)&&!MCObjectHeap_collect(o->heap));p->calls++;MCObjectHeap_touch(o->heap);if(p->clear)RecipeList_clear(p->list);return p->calls==1?p->first:p->second;}
static const IRecipeMethods probe_methods={NULL,NULL,NULL,output,NULL};
static void output_evaluation(void) {
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024);CHECK(h);StatList *s=StatList_new(h);CraftingManager *c=CraftingManager_newEmpty(h);FurnaceRecipes *f=FurnaceRecipes_newEmpty(h);CHECK(s&&c&&f);
    ProbeRecipe *p=(ProbeRecipe *)MCObjectHeap_alloc(h,sizeof(*p),&probe_class);CHECK(p);p->first=stack(h,339,0,0);p->second=stack(h,265,-2,0);p->list=CraftingManager_getRecipeList(c);
    CHECK(CraftingManager_addRecipe(c,(IRecipe){(MCObject *)p,&probe_methods,IRECIPE_OTHER}));CHECK(StatList_initCraftableStats(s,c,f));
    CHECK(p->calls==2&&!s->objectCraftStats[339]&&s->objectCraftStats[265]);CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
    h=MCObjectHeap_new(4*1024*1024);s=StatList_new(h);c=CraftingManager_newEmpty(h);f=FurnaceRecipes_newEmpty(h);p=(ProbeRecipe *)MCObjectHeap_alloc(h,sizeof(*p),&probe_class);CHECK(s&&c&&f&&p);
    p->first=stack(h,339,1,0);p->list=CraftingManager_getRecipeList(c);CHECK(CraftingManager_addRecipe(c,(IRecipe){(MCObject *)p,&probe_methods,IRECIPE_OTHER}));CHECK(!StatList_initCraftableStats(s,c,f)&&p->calls==2&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static void failures_and_utf16(void) {
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024);CHECK(h);StatList *s=StatList_new(h);CHECK(s);
    uint16_t prefix[]={0,0xd800},suffix[]={0x6771,0xdfff};StatCrafting *a=StatCrafting_newIdentity(h,NBTString_fromUTF16(h,prefix,2),NBTString_fromUTF16(h,suffix,2),ItemStack_registryItem(339));CHECK(a);CHECK(NBTString_length(a->base.statId)==4);CHECK(NBTString_units(a->base.statId)[1]==0xd800&&NBTString_units(a->base.statId)[3]==0xdfff);
    StatCrafting *b=StatCrafting_newIdentity(h,NULL,NULL,NULL);CHECK(b&&NBTString_equalsASCII(b->base.statId,"nullnull"));CHECK(StatList_registerStat(s,&a->base)==&a->base);
    CHECK(!StatList_registerStat(s,&a->base)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
    h=MCObjectHeap_new(4*1024*1024);s=StatList_new(h);MCObjectHeap *foreign=MCObjectHeap_new(1024*1024);CHECK(s&&foreign);FurnaceRecipes *f=FurnaceRecipes_newEmpty(h);ItemStack *other=stack(foreign,339,1,0);CHECK(!FurnaceRecipes_addSmeltingRecipe(f,NULL,other,0)&&MCObjectHeap_failed(h)&&!MCObjectHeap_failed(foreign));MCObjectHeap_free(h);MCObjectHeap_free(foreign);
    h=MCObjectHeap_new(4*1024*1024);s=StatList_new(h);CraftingManager *c=CraftingManager_newEmpty(h);f=FurnaceRecipes_newEmpty(h);CHECK(s&&c&&f);CHECK(FurnaceRecipeMap_put(FurnaceRecipes_getSmeltingList(f),NULL,NULL));CHECK(!StatList_initCraftableStats(s,c,f)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
    h=MCObjectHeap_new(64);CHECK(h&&!FurnaceRecipes_new(h)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static void facts(void) {
    MCObjectHeap *h=MCObjectHeap_new(64*1024*1024);CHECK(h);FurnaceRecipes *f=FurnaceRecipes_new(h);StatList *s=StatList_new(h);CraftingManager *c=MCGameplayCrafting_newManager(h);CHECK(f&&s&&c&&StatList_initCraftableStats(s,c,f)&&StatList_initAchievementIdentities(s));
    static const int damages[]={-1,0,1,2,3,32767,INT32_MAX};
    for(int i=0;i<2268;i++)if(ItemStack_registryItem(i))for(size_t d=0;d<sizeof(damages)/sizeof(*damages);d++) {
        ItemStack *in=stack(h,i,-3,damages[d]),*out=FurnaceRecipes_getSmeltingResult(f,in);
        printf("F %d %d %d %d\n",i,damages[d],out?ItemStack_registryId(out->item):-1,out?out->itemDamage:0);
    }
    for(int i=0;i<26;i++){ItemStack *key,*value;CHECK(FurnaceRecipeMap_entry(f->smeltingList,i,&key,&value));printf("R %d %d %d %d %08" PRIx32 "\n",ItemStack_registryId(key->item),key->itemDamage,ItemStack_registryId(value->item),value->itemDamage,bits(FurnaceRecipes_getSmeltingExperience(f,value)));}
    for(int i=0;i<2268;i++) {
        StatBase *a=s->objectCraftStats[i];if(!a){printf("S %d -\n",i);continue;}int first=0;while(s->objectCraftStats[first]!=a)first++;char id[256];CHECK(NBTString_toUTF8(a->statId,id,sizeof(id)));
        printf("S %d %s %d %d\n",i,id,ItemStack_registryId(StatCrafting_func_150959_a((StatCrafting *)a)),first);
    }
    for(int i=0;i<StatListList_size(s->allStats);i++){StatBase *a=StatListList_get(s->allStats,i);if(!StatBase_isAchievement(a))continue;Achievement *ach=(Achievement *)a;char id[256],parent[256]="-";CHECK(NBTString_toUTF8(a->statId,id,sizeof(id)));if(ach->parentAchievement)CHECK(NBTString_toUTF8(ach->parentAchievement->base.statId,parent,sizeof(parent)));printf("A %s %s %d %d\n",id,parent,ach->isSpecial,a->isIndependent);}
    CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
int main(int argc,char **argv){if(argc==2&&strcmp(argv[1],"--facts")==0){facts();return 0;}furnace_default();live_maps();all_craft_stats();merge_registration();output_evaluation();failures_and_utf16();printf("source craft stats: %u checks\n",checks);return 0;}
