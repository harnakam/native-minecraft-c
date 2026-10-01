#include "item/crafting/FurnaceRecipes.h"
#include <limits.h>

typedef struct MapEntry {
    MCObject object;
    ItemStack *key,*value;
    float experience;
    struct MapEntry *next;
} MapEntry;
struct FurnaceRecipeMap {
    MCObject object;
    MapEntry *head,*tail;
    int32_t count;
    uint32_t modCount;
    bool floats;
};
static void entry_trace(MCObject *o,MCObjectVisitor v,void *c) {
    MapEntry *e=(MapEntry *)o;
    e->key=(ItemStack *)v((MCObject *)e->key,c);
    e->value=(ItemStack *)v((MCObject *)e->value,c);
    e->next=(MapEntry *)v((MCObject *)e->next,c);
}
static void map_trace(MCObject *o,MCObjectVisitor v,void *c) {
    FurnaceRecipeMap *m=(FurnaceRecipeMap *)o;
    m->head=(MapEntry *)v((MCObject *)m->head,c);
    m->tail=(MapEntry *)v((MCObject *)m->tail,c);
}
static void recipes_trace(MCObject *o,MCObjectVisitor v,void *c) {
    FurnaceRecipes *r=(FurnaceRecipes *)o;
    r->smeltingList=(FurnaceRecipeMap *)v((MCObject *)r->smeltingList,c);
    r->experienceList=(FurnaceRecipeMap *)v((MCObject *)r->experienceList,c);
}
static const MCObjectClass entry_class={"C919.FurnaceIdentityMap.Entry",MCObjectHeap_plainClone,entry_trace,NULL};
static const MCObjectClass map_class={"C919.FurnaceIdentityMap",MCObjectHeap_plainClone,map_trace,NULL};
static const MCObjectClass recipes_class={"FurnaceRecipes",MCObjectHeap_plainClone,recipes_trace,NULL};
static bool refs(FurnaceRecipeMap *m,ItemStack *key,ItemStack *value) {
    MCObjectHeap *h=m?m->object.heap:NULL;
    if (!m || (key&&key->object.heap!=h) || (value&&value->object.heap!=h)) {
        MCObjectHeap_fail(h);return false;
    }
    return true;
}
static bool put(FurnaceRecipeMap *m,ItemStack *key,ItemStack *value,float experience) {
    if(!refs(m,key,value))return false;
    MCObjectHeap *h=m->object.heap;
    for(MapEntry *e=m->head;e;e=e->next)if(e->key==key) {
        e->value=value;e->experience=experience;MCObjectHeap_touch(h);return true;
    }
    if(m->count==INT32_MAX){MCObjectHeap_fail(h);return false;}
    MapEntry *e=(MapEntry *)MCObjectHeap_alloc(h,sizeof(*e),&entry_class);
    if(!e)return false;
    e->key=key;e->value=value;e->experience=experience;
    if(m->tail)m->tail->next=e;else m->head=e;
    m->tail=e;++m->count;++m->modCount;MCObjectHeap_touch(h);return true;
}
int32_t FurnaceRecipeMap_size(const FurnaceRecipeMap *m){return m?m->count:0;}
uint32_t FurnaceRecipeMap_modCount(const FurnaceRecipeMap *m){return m?m->modCount:0;}
bool FurnaceRecipeMap_entry(FurnaceRecipeMap *m,int32_t index,ItemStack **key,ItemStack **value) {
    if(!m||index<0||index>=m->count||!key||!value){MCObjectHeap_fail(m?m->object.heap:NULL);return false;}
    MapEntry *e=m->head;for(int32_t i=0;i<index;i++)e=e->next;
    *key=e->key;*value=e->value;return true;
}
bool FurnaceRecipeMap_put(FurnaceRecipeMap *m,ItemStack *key,ItemStack *value) {
    if(m&&m->floats){MCObjectHeap_fail(m->object.heap);return false;}
    return put(m,key,value,0);
}
bool FurnaceRecipeMap_remove(FurnaceRecipeMap *m,ItemStack *key) {
    if(!refs(m,key,NULL))return false;
    MapEntry *previous=NULL;
    for(MapEntry *e=m->head;e;previous=e,e=e->next)if(e->key==key) {
        if(previous)previous->next=e->next;else m->head=e->next;
        if(m->tail==e)m->tail=previous;
        --m->count;++m->modCount;MCObjectHeap_touch(m->object.heap);return true;
    }
    return false;
}
void FurnaceRecipeMap_clear(FurnaceRecipeMap *m) {
    if(!m)return;
    m->head=m->tail=NULL;m->count=0;++m->modCount;MCObjectHeap_touch(m->object.heap);
}
FurnaceRecipes *FurnaceRecipes_newEmpty(MCObjectHeap *h) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    FurnaceRecipes *r=(FurnaceRecipes *)MCObjectHeap_alloc(h,sizeof(*r),&recipes_class);
    if(r){r->smeltingList=(FurnaceRecipeMap *)MCObjectHeap_alloc(h,sizeof(FurnaceRecipeMap),&map_class);
        r->experienceList=(FurnaceRecipeMap *)MCObjectHeap_alloc(h,sizeof(FurnaceRecipeMap),&map_class);
        if(r->experienceList)r->experienceList->floats=true;}
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(h)?NULL:r;
}
bool FurnaceRecipes_addSmeltingRecipe(FurnaceRecipes *r,ItemStack *input,ItemStack *output,float experience) {
    MCObjectHeap *h=r?r->object.heap:NULL;
    if(!r||!refs(r->smeltingList,input,output)){MCObjectHeap_fail(h);return false;}
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)input)&&
        MCObjectRootScope_pin(&scope,(MCObject *)output)&&put(r->smeltingList,input,output,0)&&
        put(r->experienceList,output,NULL,experience);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(h);
}
bool FurnaceRecipes_addSmelting(FurnaceRecipes *r,const Item *input,ItemStack *output,float experience) {
    MCObjectHeap *h=r?r->object.heap:NULL;
    MCObjectRootScope scope={0};if(!r||!MCObjectRootScope_begin(&scope,h))return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)output);
    ItemStack *key=ok?ItemStack_new(h,input,1,32767):NULL;
    ok=key&&FurnaceRecipes_addSmeltingRecipe(r,key,output,experience);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(h);
}
bool FurnaceRecipes_addSmeltingRecipeForBlock(FurnaceRecipes *r,const Block *input,FurnaceRecipesBlockItem get,
    ItemStack *output,float experience) {
    MCObjectHeap *h=r?r->object.heap:NULL;
    if(!r||!get){MCObjectHeap_fail(h);return false;}
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)r)&&MCObjectRootScope_pin(&scope,(MCObject *)output);
    const Item *item=ok?get(input):NULL;
    ok=ok&&!MCObjectHeap_failed(h)&&FurnaceRecipes_addSmelting(r,item,output,experience);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(h);
}
bool FurnaceRecipes_compareItemStacks(FurnaceRecipes *r,const ItemStack *a,const ItemStack *b) {
    MCObjectHeap *h=r?r->object.heap:NULL;
    if(!r||!a||!b||a->object.heap!=h||b->object.heap!=h){MCObjectHeap_fail(h);return false;}
    return ItemStack_getItem(b)==ItemStack_getItem(a)&&
        (ItemStack_getMetadata(b)==32767||ItemStack_getMetadata(b)==ItemStack_getMetadata(a));
}
ItemStack *FurnaceRecipes_getSmeltingResult(FurnaceRecipes *r,ItemStack *stack) {
    if(!r)return NULL;
    for(MapEntry *e=r->smeltingList->head;e;e=e->next) {
        if(FurnaceRecipes_compareItemStacks(r,stack,e->key))return e->value;
        if(MCObjectHeap_failed(r->object.heap))return NULL;
    }
    return NULL;
}
FurnaceRecipeMap *FurnaceRecipes_getSmeltingList(FurnaceRecipes *r){return r?r->smeltingList:NULL;}
float FurnaceRecipes_getSmeltingExperience(FurnaceRecipes *r,ItemStack *stack) {
    if(!r)return 0;
    for(MapEntry *e=r->experienceList->head;e;e=e->next) {
        if(FurnaceRecipes_compareItemStacks(r,stack,e->key))return e->experience;
        if(MCObjectHeap_failed(r->object.heap))return 0;
    }
    return 0;
}
FurnaceRecipes *FurnaceRecipes_new(MCObjectHeap *h) {
    /* Original constructor order and immutable registry/enum numeric facts.
       A wildcard input corresponds to addSmelting/addSmeltingForBlock. */
    static const struct {int32_t input,metadata,output,damage;float experience;} registrations[]={
        {15,32767,265,0,.7f},{14,32767,266,0,1.f},{56,32767,264,0,1.f},{12,32767,20,0,.1f},
        {319,32767,320,0,.35f},{363,32767,364,0,.35f},{365,32767,366,0,.35f},
        {411,32767,412,0,.35f},{423,32767,424,0,.35f},{4,32767,1,0,.1f},
        {98,0,98,2,.1f},{337,32767,336,0,.3f},{82,32767,172,0,.35f},{81,32767,351,2,.2f},
        {17,32767,263,1,.15f},{162,32767,263,1,.15f},{129,32767,388,0,1.f},
        {392,32767,393,0,.35f},{87,32767,405,0,.1f},{19,1,19,0,.15f},
        {349,0,350,0,.35f},{349,1,350,1,.35f},{16,32767,263,0,.1f},
        {73,32767,331,0,.7f},{21,32767,351,4,.2f},{153,32767,406,0,.2f}
    };
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    FurnaceRecipes *r=FurnaceRecipes_newEmpty(h);
    for(size_t i=0;r&&i<sizeof(registrations)/sizeof(*registrations)&&!MCObjectHeap_failed(h);i++) {
        ItemStack *output=ItemStack_new(h,ItemStack_registryItem(registrations[i].output),1,registrations[i].damage);
        if(!output)break;
        if(registrations[i].metadata==32767) {
            if(!FurnaceRecipes_addSmelting(r,ItemStack_registryItem(registrations[i].input),output,registrations[i].experience))break;
        } else {
            ItemStack *input=ItemStack_new(h,ItemStack_registryItem(registrations[i].input),1,registrations[i].metadata);
            if(!input||!FurnaceRecipes_addSmeltingRecipe(r,input,output,registrations[i].experience))break;
        }
    }
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(h)?NULL:r;
}
