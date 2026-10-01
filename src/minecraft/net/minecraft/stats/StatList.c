#include "stats/StatList.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
typedef struct ListEntry {MCObject object;StatBase *value;struct ListEntry *next;} ListEntry;
struct StatListList {MCObject object;ListEntry *head,*tail;int32_t count;};
static void entry_trace(MCObject *o,MCObjectVisitor v,void *c) {
    ListEntry *e=(ListEntry *)o;e->value=(StatBase *)v((MCObject *)e->value,c);e->next=(ListEntry *)v((MCObject *)e->next,c);
}
static void list_trace(MCObject *o,MCObjectVisitor v,void *c) {
    StatListList *l=(StatListList *)o;l->head=(ListEntry *)v((MCObject *)l->head,c);l->tail=(ListEntry *)v((MCObject *)l->tail,c);
}
static void trace(MCObject *o,MCObjectVisitor v,void *c) {
    StatList *s=(StatList *)o;s->oneShotStats=(StatBaseRegistry *)v((MCObject *)s->oneShotStats,c);
    s->allStats=(StatListList *)v((MCObject *)s->allStats,c);s->generalStats=(StatListList *)v((MCObject *)s->generalStats,c);
    s->objectMineStats=(StatListList *)v((MCObject *)s->objectMineStats,c);s->dropStat=(StatBase *)v((MCObject *)s->dropStat,c);
    for(size_t i=0;i<STATLIST_CRAFT_STATS_COUNT;i++)s->objectCraftStats[i]=(StatBase *)v((MCObject *)s->objectCraftStats[i],c);
}
static const MCObjectClass entry_class={"C919.StatListArrayList.Entry",MCObjectHeap_plainClone,entry_trace,NULL};
static const MCObjectClass list_class={"C919.StatListArrayList",MCObjectHeap_plainClone,list_trace,NULL};
static const MCObjectClass klass={"C919.StatListStaticFields",MCObjectHeap_plainClone,trace,NULL};
int32_t StatListList_size(const StatListList *l){return l?l->count:0;}
StatBase *StatListList_get(StatListList *l,int32_t index) {
    if(!l||index<0||index>=l->count){MCObjectHeap_fail(l?l->object.heap:NULL);return NULL;}
    ListEntry *e=l->head;for(int32_t i=0;i<index;i++)e=e->next;return e->value;
}
bool StatListList_add(StatListList *l,StatBase *value) {
    MCObjectHeap *h=l?l->object.heap:NULL;
    if(!l||(value&&value->object.heap!=h)||l->count==INT32_MAX){MCObjectHeap_fail(h);return false;}
    ListEntry *e=(ListEntry *)MCObjectHeap_alloc(h,sizeof(*e),&entry_class);if(!e)return false;
    e->value=value;if(l->tail)l->tail->next=e;else l->head=e;l->tail=e;++l->count;MCObjectHeap_touch(h);return true;
}
bool StatListList_remove(StatListList *l,StatBase *value) {
    MCObjectHeap *h=l?l->object.heap:NULL;
    if(!l||(value&&value->object.heap!=h)){MCObjectHeap_fail(h);return false;}
    ListEntry *previous=NULL;
    for(ListEntry *e=l->head;e;previous=e,e=e->next) {
        if(value?StatBase_equals(value,e->value):e->value==NULL) {
            if(previous)previous->next=e->next;else l->head=e->next;
            if(l->tail==e)l->tail=previous;
            --l->count;MCObjectHeap_touch(h);return true;
        }
        if(MCObjectHeap_failed(h))return false;
    }
    return false;
}
StatList *StatList_new(MCObjectHeap *h) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    StatList *s=(StatList *)MCObjectHeap_alloc(h,sizeof(*s),&klass);
    if(s){s->oneShotStats=StatBaseRegistry_new(h);
        s->allStats=(StatListList *)MCObjectHeap_alloc(h,sizeof(StatListList),&list_class);
        s->generalStats=(StatListList *)MCObjectHeap_alloc(h,sizeof(StatListList),&list_class);
        s->objectMineStats=(StatListList *)MCObjectHeap_alloc(h,sizeof(StatListList),&list_class);}
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(h)?NULL:s;
}
StatBase *StatList_getOneShotStat(StatList *s,const NBTString *id){return s?StatBaseRegistry_find(s->oneShotStats,id):NULL;}
StatBase *StatList_getOneShotStat_ascii(StatList *s,const char *id){return s?StatBaseRegistry_find_ascii(s->oneShotStats,id):NULL;}
StatBase *StatList_registerStat(StatList *s,StatBase *stat) {
    return StatBase_registerStat(stat,s);
}
bool StatList_mergeStatBases(StatList *s,StatBase **array,size_t count,int32_t first,int32_t second) {
    MCObjectHeap *h=s?s->object.heap:NULL;
    if(!s||!array||first<0||second<0||(size_t)first>=count||(size_t)second>=count||
        (array[first]&&array[first]->object.heap!=h)||(array[second]&&array[second]->object.heap!=h)) {
        MCObjectHeap_fail(h);return false;
    }
    if(array[first]&&!array[second])array[second]=array[first];
    else {
        StatListList_remove(s->allStats,array[first]);StatListList_remove(s->objectMineStats,array[first]);
        StatListList_remove(s->generalStats,array[first]);array[first]=array[second];
    }
    MCObjectHeap_touch(h);return !MCObjectHeap_failed(h);
}
bool StatList_replaceAllSimilarBlocks(StatList *s,StatBase **array,size_t count) {
    static const int32_t pairs[][2]={{9,8},{11,10},{91,86},{62,61},{74,73},{94,93},{150,149},
        {76,75},{124,123},{43,44},{125,126},{181,182},{2,3},{60,3}};
    for(size_t i=0;i<sizeof(pairs)/sizeof(*pairs);i++)
        if(!StatList_mergeStatBases(s,array,count,pairs[i][0],pairs[i][1]))return false;
    return true;
}
bool StatList_fillCraftStats(StatList *s,StatBase **output,size_t count) {
    if(!s||!output||count>STATLIST_CRAFT_STATS_COUNT){MCObjectHeap_fail(s?s->object.heap:NULL);return false;}
    for(size_t i=count;i<STATLIST_CRAFT_STATS_COUNT;i++)if(s->objectCraftStats[i]){MCObjectHeap_fail(s->object.heap);return false;}
    memcpy(output,s->objectCraftStats,count*sizeof(*output));MCObjectHeap_touch(s->object.heap);return true;
}
bool StatList_initCraftableStats(StatList *s,CraftingManager *manager,FurnaceRecipes *furnace) {
    MCObjectHeap *h=s?s->object.heap:NULL;
    if(!s||!manager||!furnace||manager->object.heap!=h||furnace->object.heap!=h){MCObjectHeap_fail(h);return false;}
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)s)&&MCObjectRootScope_pin(&scope,(MCObject *)manager)&&MCObjectRootScope_pin(&scope,(MCObject *)furnace);
    /* Native identity Set adapter. Ordering is not a port of HashSet buckets. */
    const Item *items[2268];size_t used=0;
    RecipeList *recipes=CraftingManager_getRecipeList(manager);uint32_t mod=RecipeList_modCount(recipes);
    for(int32_t i=0;ok&&i!=RecipeList_size(recipes);i++) {
        if(mod!=RecipeList_modCount(recipes)){MCObjectHeap_fail(h);ok=false;break;}
        IRecipe r=RecipeList_get(recipes,i);
        if(!r.instance||!r.methods||!r.methods->getRecipeOutput){MCObjectHeap_fail(h);ok=false;break;}
        ItemStack *output=r.methods->getRecipeOutput(r.instance);
        if(MCObjectHeap_failed(h)){ok=false;break;}
        if(output) {
            /* Source evaluates getRecipeOutput again, not the first reference. */
            output=r.methods->getRecipeOutput(r.instance);
            if(!output||output->object.heap!=h||MCObjectHeap_failed(h)){MCObjectHeap_fail(h);ok=false;break;}
            const Item *item=ItemStack_getItem(output);bool found=false;
            for(size_t j=0;j<used;j++)if(items[j]==item){found=true;break;}
            if(!found){if(used==sizeof(items)/sizeof(*items)){MCObjectHeap_fail(h);ok=false;break;}items[used++]=item;}
        }
    }
    FurnaceRecipeMap *map=FurnaceRecipes_getSmeltingList(furnace);mod=FurnaceRecipeMap_modCount(map);
    for(int32_t i=0;ok&&i!=FurnaceRecipeMap_size(map);i++) {
        ItemStack *key,*output;
        if(mod!=FurnaceRecipeMap_modCount(map)||!FurnaceRecipeMap_entry(map,i,&key,&output)||!output) {MCObjectHeap_fail(h);ok=false;break;}
        (void)key;const Item *item=ItemStack_getItem(output);bool found=false;
        for(size_t j=0;j<used;j++)if(items[j]==item){found=true;break;}
        if(!found){if(used==sizeof(items)/sizeof(*items)){MCObjectHeap_fail(h);ok=false;break;}items[used++]=item;}
    }
    NBTString *prefix=ok?NBTString_literalASCII(h,"stat.craftItem."):NULL;
    for(size_t i=0;ok&&i<used;i++)if(items[i]) {
        int32_t id=ItemStack_registryId(items[i]);const char *name=ItemStack_registryResourceName(items[i]);
        if(!name)continue;
        if(id<0||(size_t)id>=STATLIST_CRAFT_STATS_COUNT){MCObjectHeap_fail(h);ok=false;break;}
        size_t len=strlen(name);char *suffix=malloc(len+1);
        if(!suffix){MCObjectHeap_fail(h);ok=false;break;}
        for(size_t j=0;j<len;j++)suffix[j]=name[j]==':'?'.':name[j];
        suffix[len]=0;
        NBTString *part=NBTString_fromASCII(h,suffix);free(suffix);
        StatCrafting *stat=part?StatCrafting_newIdentity(h,prefix,part,items[i]):NULL;
        if(!stat||!StatList_registerStat(s,&stat->base)){ok=false;break;}
        s->objectCraftStats[id]=&stat->base;MCObjectHeap_touch(h);
    }
    ok=ok&&!MCObjectHeap_failed(h)&&StatList_replaceAllSimilarBlocks(s,s->objectCraftStats,STATLIST_CRAFT_STATS_COUNT);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(h);
}
bool StatList_initAchievementIdentities(StatList *s) {
    /* Source AchievementList static identity/parent/flag facts only. GUI icon,
       localization, display bounds and JsonSerializableSet are not adapters
       to gameplay counters and are deliberately not invented here. */
    static const struct {const char *id;int parent;bool special;} facts[]={
        {"openInventory",-1,false},{"mineWood",0,false},{"buildWorkBench",1,false},
        {"buildPickaxe",2,false},{"buildFurnace",3,false},{"acquireIron",4,false},
        {"buildHoe",2,false},{"makeBread",6,false},{"bakeCake",6,false},
        {"buildBetterPickaxe",3,false},{"cookFish",4,false},{"onARail",5,true},
        {"buildSword",2,false},{"killEnemy",12,false},{"killCow",12,false},
        {"flyPig",14,true},{"snipeSkeleton",13,true},{"diamonds",5,false},
        {"diamondsToYou",17,false},{"portal",17,false},{"ghast",19,true},
        {"blazeRod",19,false},{"potion",21,false},{"theEnd",21,true},{"theEnd2",23,true},
        {"enchantments",17,false},{"overkill",25,true},{"bookcase",25,false},
        {"breedCow",14,false},{"spawnWither",24,false},{"killWither",29,false},
        {"fullBeacon",30,true},{"exploreAllBiomes",23,true},{"overpowered",9,true}
    };
    MCObjectHeap *h=s?s->object.heap:NULL;
    if(!s){MCObjectHeap_fail(h);return false;}
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)s);
    NBTString *drop=ok?NBTString_literalASCII(h,"stat.drop"):NULL;
    StatBase *stat=drop?StatBase_newIdentity(h,drop,STAT_BASE_KIND_BASIC):NULL;
    if(stat){StatBase_initIndependentStat(stat);stat=StatList_registerStat(s,stat);}
    if(!stat)ok=false;else{s->dropStat=stat;MCObjectHeap_touch(h);}
    Achievement *achievements[sizeof(facts)/sizeof(*facts)]={0};
    for(size_t i=0;ok&&i<sizeof(facts)/sizeof(*facts);i++) {
        char name[80];size_t prefix=strlen("achievement."),length=strlen(facts[i].id);
        memcpy(name,"achievement.",prefix);memcpy(name+prefix,facts[i].id,length+1);
        NBTString *id=NBTString_fromASCII(h,name);
        Achievement *a=id?Achievement_newIdentity(h,id,facts[i].parent<0?NULL:achievements[facts[i].parent]):NULL;
        if(!a){ok=false;break;}
        if(i==0)Achievement_initIndependentStat(a);
        if(facts[i].special)Achievement_setSpecial(a);
        if(!StatList_registerStat(s,&a->base)){ok=false;break;}
        achievements[i]=a;
    }
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(h);
}
