#include "util/NativeHashSet.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"Native HashSet check %u line %d: %s\n",checks,__LINE__,#x); exit(1); } } while(0)
typedef struct {
    MCObject object;
    int32_t value,hash;
    unsigned hashes,equals,failHashAt,failEqualsAt;
    bool stickyHash,stickyEquals;
} Key;
static const MCObjectClass keyClass={"fixture.NativeSetKey",MCObjectHeap_plainClone,NULL,NULL};
static bool hash(MCObject *context,MCObject *object,int32_t *out) {
    (void)context;Key *key=(Key *)object;
    CHECK(MCObjectHeap_hasBorrowers(object->heap));CHECK(!MCObjectHeap_collect(object->heap));
    ++key->hashes;*out=key->hash;
    if(key->stickyHash)MCObjectHeap_fail(object->heap);
    return !key->failHashAt||key->hashes!=key->failHashAt;
}
static bool equal(MCObject *context,MCObject *query,MCObject *stored,bool *out) {
    (void)context;Key *key=(Key *)query;
    CHECK(MCObjectHeap_hasBorrowers(query->heap));CHECK(!MCObjectHeap_collect(query->heap));
    ++key->equals;*out=stored&&key->value==((Key *)stored)->value;
    if(key->stickyEquals)MCObjectHeap_fail(query->heap);
    return !key->failEqualsAt||key->equals!=key->failEqualsAt;
}
static const NativeHashKeyMethods methods={hash,equal};
static Key *new_key(MCObjectHeap *heap,int32_t value,int32_t hashValue) {
    Key *key=(Key *)MCObjectHeap_alloc(heap,sizeof(*key),&keyClass);CHECK(key);
    key->value=value;key->hash=hashValue;return key;
}
static void operations(void) {
    MCObjectHeap *heap=MCObjectHeap_new(4u*1024u*1024u);CHECK(heap);
    NativeHashSet *set=NativeHashSet_newWithKeys(heap,&methods,NULL);CHECK(set);
    NativeHashSet *other=NativeHashSet_newWithKeys(heap,&methods,NULL);CHECK(other&&other->present==set->present);
    Key *first=new_key(heap,10,7),*same=new_key(heap,10,7),*collision=new_key(heap,11,7);
    bool changed=false;
    CHECK(NativeHashSet_add(set,(MCObject *)first,&changed)&&changed);
    CHECK(first->hashes==1&&first->equals==0); /* JDK8 HashSet.add performs one put. */
    CHECK(NativeHashSet_add(set,(MCObject *)same,&changed)&&!changed);
    CHECK(same->hashes==1&&same->equals==1);
    CHECK(NativeHashSet_size(set)==1);
    NativeIterator *iterator=NativeHashSet_iterator(set);CHECK(iterator);
    MCObject *out=(MCObject *)same;
    CHECK(NativeIterator_hasNext(iterator));CHECK(NativeIterator_next(iterator,&out)&&out==(MCObject *)first);
    CHECK(!NativeIterator_hasNext(iterator)); /* Equal replacement retains original key. */
    CHECK(NativeHashSet_add(set,(MCObject *)first,&changed)&&!changed);
    CHECK(first->hashes==2&&first->equals==0); /* Same-reference equality bypasses virtual equals. */
    CHECK(NativeHashSet_add(set,(MCObject *)collision,&changed)&&changed);
    CHECK(collision->hashes==1&&collision->equals==1);
    CHECK(NativeHashSet_add(set,NULL,&changed)&&changed);
    CHECK(NativeHashSet_add(set,NULL,&changed)&&!changed);
    CHECK(NativeHashSet_contains(set,NULL));CHECK(NativeHashSet_size(set)==3);
    CHECK(NativeHashSet_remove(set,(MCObject *)same,&changed)&&changed);
    CHECK(NativeHashSet_size(set)==2);CHECK(NativeHashSet_remove(set,(MCObject *)same,&changed)&&!changed);
    CHECK(NativeHashSet_remove(set,NULL,&changed)&&changed);CHECK(!NativeHashSet_contains(set,NULL));
    iterator=NativeHashSet_iterator(set);CHECK(iterator);
    MCObjectRootScope borrow={0};CHECK(MCObjectRootScope_begin(&borrow,heap));
    CHECK(MCObjectRootScope_pin(&borrow,(MCObject *)iterator));
    CHECK(NativeIterator_hasNext(iterator));CHECK(NativeIterator_next(iterator,&out)&&out==(MCObject *)collision);
    CHECK(!NativeIterator_hasNext(iterator));CHECK(NativeHashSet_size(set)==1);
    CHECK(!MCObjectHeap_collect(heap));MCObjectRootScope_end(&borrow);
    CHECK(NativeHashSet_clear(set));CHECK(NativeHashSet_size(set)==0);CHECK(NativeHashSet_clear(set));
    MCObjectHeap_free(heap);
}
static void previous_value(void) {
    MCObjectHeap *heap=MCObjectHeap_new(4u*1024u*1024u);CHECK(heap);
    NativeHashMap *map=NativeHashMap_newWithKeys(heap,&methods,NULL);CHECK(map);
    Key *first=new_key(heap,1,9),*same=new_key(heap,1,9),*value=new_key(heap,2,12);
    MCObject *previous=(MCObject *)value;
    CHECK(NativeHashMap_putWithPrevious(map,(MCObject *)first,(MCObject *)value,&previous)&&!previous);
    CHECK(first->hashes==1&&first->equals==0);
    CHECK(NativeHashMap_putWithPrevious(map,(MCObject *)same,NULL,&previous)&&previous==(MCObject *)value);
    CHECK(same->hashes==1&&same->equals==1);
    previous=(MCObject *)value;
    CHECK(NativeHashMap_putWithPrevious(map,(MCObject *)first,(MCObject *)value,&previous)&&!previous);
    CHECK(NativeHashMap_size(map)==1);CHECK(NativeHashMap_putWithPrevious(map,NULL,NULL,&previous)&&!previous);
    CHECK(NativeHashMap_putWithPrevious(map,NULL,(MCObject *)value,&previous)&&!previous);
    CHECK(NativeHashMap_putWithPrevious(map,NULL,NULL,&previous)&&previous==(MCObject *)value);
    MCObjectHeap_free(heap);
}
static void failures(void) {
    for(int mode=0;mode<8;mode++) {
        MCObjectHeap *heap=MCObjectHeap_new(4u*1024u*1024u);CHECK(heap);
        NativeHashSet *set=NativeHashSet_newWithKeys(heap,&methods,NULL);CHECK(set);
        Key *stored=new_key(heap,1,7),*query=new_key(heap,1,7),*marker=new_key(heap,2,11);
        bool changed=true;CHECK(NativeHashSet_add(set,(MCObject *)stored,&changed)&&changed);
        if(mode==0||mode==4)query->failHashAt=1;
        if(mode==1||mode==5)query->failEqualsAt=1;
        if(mode==2||mode==6)query->stickyHash=true;
        if(mode==3||mode==7)query->stickyEquals=true;
        if(mode<4) {
            changed=mode%2==0;
            CHECK(!NativeHashSet_add(set,(MCObject *)query,&changed));CHECK(changed==(mode%2==0));
        } else {
            MCObject *previous=(MCObject *)marker;
            CHECK(!NativeHashMap_putWithPrevious(set->map,(MCObject *)query,(MCObject *)marker,&previous));
            CHECK(previous==(MCObject *)marker);
        }
        CHECK(MCObjectHeap_failed(heap));CHECK(query->hashes==1);
        CHECK(query->equals==((mode%4==1||mode%4==3)?1u:0u));
        CHECK(stored->hashes==1&&stored->equals==0);
        MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap=MCObjectHeap_new(4u*1024u*1024u);CHECK(heap);
    NativeHashSet *set=NativeHashSet_newWithKeys(heap,&methods,NULL);CHECK(set);
    Key *key=new_key(heap,1,7);bool changed=false;CHECK(NativeHashSet_add(set,(MCObject *)key,&changed)&&changed);
    NativeIterator *iterator=NativeHashSet_iterator(set);CHECK(iterator);
    CHECK(NativeHashSet_add(set,NULL,&changed)&&changed);
    CHECK(NativeIterator_hasNext(iterator));MCObject *out=(MCObject *)key;
    CHECK(!NativeIterator_next(iterator,&out)&&out==(MCObject *)key);CHECK(MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
static void lifetime(void) {
    MCObjectHeap *heap=MCObjectHeap_new(4u*1024u*1024u);CHECK(heap);
    NativeHashSet *set=NativeHashSet_newWithKeys(heap,&methods,NULL);CHECK(set);
    Key *key=new_key(heap,1,3);bool changed;CHECK(NativeHashSet_add(set,(MCObject *)key,&changed)&&changed);
    CHECK(NativeHashSet_add(set,NULL,&changed)&&changed);
    NativeReferenceList *roots=NativeReferenceList_new(heap);CHECK(roots);
    CHECK(NativeReferenceList_add(roots,(MCObject *)set));CHECK(NativeReferenceList_add(roots,(MCObject *)key));
    CHECK(NativeReferenceList_add(roots,set->present));CHECK(NativeReferenceList_add(roots,(MCObject *)set));
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,heap,(MCObject *)roots));CHECK(MCObjectHeap_collect(heap));
    CHECK(NativeHashSet_contains(set,(MCObject *)key)&&NativeHashSet_contains(set,NULL));
    MCObjectHeap *clone=MCObjectHeap_clone(heap);CHECK(clone);MCObjectRoot copied={0};CHECK(MCObjectRoot_rebind(&copied,clone,&root));
    NativeReferenceList *list=(NativeReferenceList *)MCObjectRoot_get(&copied);
    NativeHashSet *copy=(NativeHashSet *)NativeReferenceList_get(list,0);Key *copyKey=(Key *)NativeReferenceList_get(list,1);
    CHECK(copy!=set&&copyKey!=key&&copy->object.heap==clone);
    CHECK(NativeReferenceList_get(list,2)==copy->present&&NativeReferenceList_get(list,3)==(MCObject *)copy);
    CHECK(NativeHashSet_contains(copy,(MCObject *)copyKey));CHECK(MCObjectHeap_collect(clone));
    CHECK(NativeHashSet_remove(copy,(MCObject *)copyKey,&changed)&&changed);CHECK(NativeHashSet_size(copy)==1);
    CHECK(NativeHashSet_size(set)==2);CHECK(MCObjectHeap_adopt(heap,clone));MCObjectHeap_free(clone);
    list=(NativeReferenceList *)MCObjectRoot_get(&root);set=(NativeHashSet *)NativeReferenceList_get(list,0);
    key=(Key *)NativeReferenceList_get(list,1);
    CHECK(set->object.heap==heap&&key->object.heap==heap);CHECK(NativeHashSet_size(set)==1);
    CHECK(!NativeHashSet_contains(set,(MCObject *)key)&&NativeHashSet_contains(set,NULL));CHECK(MCObjectHeap_collect(heap));
    CHECK(NativeHashSet_add(set,(MCObject *)key,&changed)&&changed);CHECK(NativeHashSet_size(set)==2);
    MCObjectRoot_drop(&root);MCObjectHeap_free(heap);
}
static void native_failure_boundaries(void) {
    const size_t budget=1024u*1024u;
    for(int method=0;method<2;method++) {
        MCObjectHeap *heap=MCObjectHeap_new(budget);CHECK(heap);
        NativeHashSet *set=NativeHashSet_newWithKeys(heap,&methods,NULL);CHECK(set);
        Key *key=new_key(heap,1,7),*query=new_key(heap,2,12);bool changed;
        CHECK(NativeHashSet_add(set,(MCObject *)key,&changed)&&changed);
        size_t remaining=budget-MCObjectHeap_liveBytes(heap);CHECK(remaining>sizeof(MCObject)*2);
        CHECK(MCObjectHeap_alloc(heap,remaining-sizeof(MCObject),&keyClass));
        if(method==0) {
            changed=false;CHECK(!NativeHashSet_add(set,(MCObject *)query,&changed));CHECK(!changed);
        } else {
            MCObject *previous=(MCObject *)key;
            CHECK(!NativeHashMap_putWithPrevious(set->map,(MCObject *)query,set->present,&previous));
            CHECK(previous==(MCObject *)key);
        }
        CHECK(MCObjectHeap_failed(heap));CHECK(query->hashes==1&&query->equals==0);
        MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap=MCObjectHeap_new(budget),*foreign=MCObjectHeap_new(budget);CHECK(heap&&foreign);
    NativeHashSet *set=NativeHashSet_newWithKeys(heap,&methods,NULL);CHECK(set);
    Key *key=new_key(foreign,1,7);bool changed=true;
    CHECK(!NativeHashSet_add(set,(MCObject *)key,&changed)&&changed);
    CHECK(MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(foreign));CHECK(key->hashes==0&&key->equals==0);
    MCObjectHeap_free(foreign);MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(budget);CHECK(heap);set=NativeHashSet_newWithKeys(heap,&methods,NULL);CHECK(set);
    NativeHashSet *undersized=(NativeHashSet *)MCObjectHeap_alloc(heap,sizeof(MCObject),set->object.klass);CHECK(undersized);
    CHECK(!NativeHashSet_isInstance((MCObject *)undersized));changed=true;
    CHECK(!NativeHashSet_add(undersized,NULL,&changed)&&changed);CHECK(MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
}
int main(void) {
    operations();previous_value();failures();lifetime();native_failure_boundaries();
    printf("Native HashSet: %u checks passed\n",checks);return 0;
}
