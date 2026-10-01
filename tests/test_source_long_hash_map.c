#include "util/LongHashMap.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { fprintf(stderr,"LongHashMap check %u line %d\n",checks,__LINE__); exit(1); } } while (0)
static const MCObjectClass valueClass={"test.LongMapValue",MCObjectHeap_plainClone,NULL,NULL};
static MCObject *value(MCObjectHeap *heap) {
    MCObject *out=MCObjectHeap_alloc(heap,sizeof(MCObject),&valueClass);CHECK(out);return out;
}
static size_t array_bytes(int32_t length) {
    return sizeof(LongHashMapEntryArray)+(size_t)length*sizeof(LongHashMapEntry *);
}

static void original_constructor(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8u*1024u*1024u);CHECK(heap);
    LongHashMap *map=LongHashMap_new(heap);CHECK(map);
    CHECK(map->hashArray&&map->hashArray->length==4096);
    CHECK(map->numHashElements==0&&map->mask==4095&&map->capacity==3072);
    CHECK(map->percentUseable==0.75f&&map->modCount==0);
    for(int32_t i=0;i<4096;i++)CHECK(!map->hashArray->values[i]);
    MCObjectHeap_free(heap);
}
static void null_values_collision_replacement_and_removal(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8u*1024u*1024u);CHECK(heap);
    LongHashMap *map=LongHashMap_new(heap);CHECK(map);
    MCObject *shared=value(heap),*replacement=value(heap);
    const int64_t keys[]={0,INT64_C(4294967297),INT64_C(8589934594),-1};
    CHECK(LongHashMap_add(map,keys[0],shared));
    LongHashMapEntry *a=LongHashMap_getEntry(map,keys[0]);CHECK(a);
    CHECK(LongHashMap_add(map,keys[1],NULL));
    LongHashMapEntry *b=LongHashMap_getEntry(map,keys[1]);CHECK(b);
    CHECK(LongHashMap_containsItem(map,keys[1])&&!LongHashMap_getValueByKey(map,keys[1]));
    CHECK(LongHashMap_add(map,keys[2],shared));
    LongHashMapEntry *c=LongHashMap_getEntry(map,keys[2]);CHECK(c);
    CHECK(map->hashArray->values[0]==c&&c->nextEntry==b&&b->nextEntry==a&&!a->nextEntry);
    CHECK(map->modCount==3&&LongHashMap_getNumHashElements(map)==3);
    CHECK(LongHashMap_add(map,keys[1],replacement));
    CHECK(map->modCount==3&&map->numHashElements==3&&LongHashMap_getEntry(map,keys[1])==b);
    CHECK(LongHashMapEntry_getKey(b)==keys[1]&&LongHashMapEntry_getValue(b)==replacement);
    CHECK(LongHashMap_removeKey(map,keys[1])==b);
    CHECK(c->nextEntry==a&&b->nextEntry==a&&map->modCount==4&&map->numHashElements==2);
    CHECK(LongHashMap_remove(map,keys[2])==shared&&map->hashArray->values[0]==a);
    CHECK(LongHashMap_remove(map,keys[0])==shared&&!map->hashArray->values[0]);
    CHECK(map->modCount==6&&map->numHashElements==0);
    CHECK(!LongHashMap_removeKey(map,999)&&map->modCount==6&&map->numHashElements==0);
    CHECK(LongHashMap_add(map,keys[3],NULL));
    CHECK(LongHashMap_containsItem(map,-1)&&!LongHashMap_remove(map,-1));
    CHECK(!LongHashMap_containsItem(map,-1)&&map->modCount==8&&!MCObjectHeap_failed(heap));
    CHECK(LongHashMap_getHashedKey(0)==0&&LongHashMap_getHashedKey(-1)==0);
    CHECK(LongHashMap_getHashedKey(keys[1])==0&&LongHashMap_getHashedKey(keys[2])==0);
    CHECK(LongHashMap_getHashedKey(INT64_MIN)==-1995925360&&LongHashMap_getHashedKey(INT64_MAX)==-1995925360);
    CHECK(LongHashMap_getHashedKey(-INT64_C(4294967296))==-235868385);
    CHECK(LongHashMap_getHashIndex(LongHashMap_getHashedKey(INT64_MIN),4095)==2192);
    CHECK(LongHashMap_getHashIndex(INT32_MIN,-1)==INT32_MIN);
    CHECK(LongHashMap_getHashIndex(-1,4095)==4095);
    MCObjectHeap_free(heap);
}
static void original_growth_threshold_and_held_array(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8u*1024u*1024u);CHECK(heap);
    LongHashMap *map=LongHashMap_new(heap);CHECK(map);
    MCObject *shared=value(heap);LongHashMapEntryArray *old=map->hashArray;
    for(int64_t i=0;i<3072;i++)CHECK(LongHashMap_add(map,i,shared));
    CHECK(map->numHashElements==3072&&map->modCount==3072&&map->hashArray==old);
    LongHashMapEntry *held=LongHashMap_getEntry(map,17);CHECK(held);
    CHECK(LongHashMap_add(map,3072,NULL));
    CHECK(map->hashArray!=old&&map->hashArray->length==8192&&map->mask==8191&&map->capacity==6144);
    CHECK(map->numHashElements==3073&&map->modCount==3073&&LongHashMap_getEntry(map,17)==held);
    for(int32_t i=0;i<old->length;i++)CHECK(!old->values[i]);
    for(int64_t i=0;i<3072;i++)CHECK(LongHashMap_getValueByKey(map,i)==shared);
    CHECK(LongHashMap_containsItem(map,3072)&&!LongHashMap_getValueByKey(map,3072));
    CHECK(LongHashMap_add(map,INT64_MIN,shared)&&LongHashMap_add(map,INT64_MAX,shared));
    CHECK(LongHashMap_getValueByKey(map,INT64_MIN)==shared&&LongHashMap_getValueByKey(map,INT64_MAX)==shared);
    CHECK(LongHashMap_remove(map,INT64_MIN)==shared&&LongHashMap_remove(map,INT64_MAX)==shared);
    MCObjectHeap_free(heap);
}
static void rehash_relinks_original_entries(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8u*1024u*1024u);CHECK(heap);
    LongHashMap *map=LongHashMap_new(heap);CHECK(map);
    CHECK(LongHashMap_resizeTable(map,4));
    CHECK(map->mask==3&&map->capacity==3&&map->modCount==0);
    CHECK(LongHashMap_add(map,0,NULL)&&LongHashMap_add(map,INT64_C(4294967297),NULL)&&
          LongHashMap_add(map,INT64_C(8589934594),NULL));
    LongHashMapEntry *a=LongHashMap_getEntry(map,0),*b=LongHashMap_getEntry(map,INT64_C(4294967297));
    LongHashMapEntry *c=LongHashMap_getEntry(map,INT64_C(8589934594));
    LongHashMapEntryArray *old=map->hashArray;CHECK(old->values[0]==c&&c->nextEntry==b&&b->nextEntry==a);
    CHECK(LongHashMap_resizeTable(map,8));
    CHECK(!old->values[0]&&map->hashArray->values[0]==a&&a->nextEntry==b&&b->nextEntry==c&&!c->nextEntry);
    CHECK(map->numHashElements==3&&map->modCount==3&&map->capacity==6);
    LongHashMapEntry *separate=LongHashMapEntry_new(heap,99,INT64_C(4294967297),(MCObject *)map,a);CHECK(separate);
    CHECK(separate->hash==99&&LongHashMapEntry_hashCode(separate)==0&&separate->nextEntry==a);
    LongHashMapEntryArray *destination=LongHashMapEntryArray_nativeNew(heap,16);CHECK(destination);
    old=map->hashArray;CHECK(LongHashMap_copyHashTableTo(map,destination));
    CHECK(map->hashArray==old&&!old->values[0]&&destination->values[0]==c&&c->nextEntry==b&&b->nextEntry==a);
    CHECK(map->mask==7&&map->capacity==6&&map->numHashElements==3&&map->modCount==3);
    MCObjectHeap_free(heap);
}
static void allocation_failures_retain_source_prefix(void) {
    MCObjectHeap *heap=MCObjectHeap_new(sizeof(LongHashMap));CHECK(heap);
    LongHashMap *map=LongHashMap_nativeAllocate(heap);CHECK(map);
    CHECK(!LongHashMap_construct(map)&&MCObjectHeap_failed(heap));
    CHECK(map->percentUseable==0.75f&&map->capacity==3072&&!map->hashArray&&map->mask==0);
    MCObjectHeap_free(heap);

    size_t initial=sizeof(LongHashMap)+array_bytes(4096);
    heap=MCObjectHeap_new(initial);CHECK(heap);map=LongHashMap_new(heap);CHECK(map);
    CHECK(!LongHashMap_add(map,0,NULL)&&MCObjectHeap_failed(heap));
    CHECK(map->modCount==1&&map->numHashElements==0&&!map->hashArray->values[0]);
    MCObjectHeap_free(heap);

    heap=MCObjectHeap_new(initial+sizeof(LongHashMapEntry));CHECK(heap);map=LongHashMap_new(heap);CHECK(map);
    LongHashMapEntryArray *old=map->hashArray;map->capacity=0;MCObjectHeap_touch(heap);
    CHECK(!LongHashMap_add(map,0,NULL)&&MCObjectHeap_failed(heap));
    CHECK(map->modCount==1&&map->numHashElements==1&&map->hashArray==old&&map->mask==4095&&map->capacity==0);
    CHECK(old->values[0]&&old->values[0]->key==0&&old->values[0]->hash==0);
    MCObjectHeap_free(heap);

    heap=MCObjectHeap_new(8u*1024u*1024u);CHECK(heap);map=LongHashMap_new(heap);CHECK(map);
    CHECK(LongHashMap_add(map,0,NULL));old=map->hashArray;
    LongHashMapEntry *held=old->values[0];
    LongHashMapEntryArray *zero=LongHashMapEntryArray_nativeNew(heap,0);CHECK(zero);
    CHECK(!LongHashMap_copyHashTableTo(map,zero)&&MCObjectHeap_failed(heap));
    CHECK(!old->values[0]&&held->key==0&&!held->nextEntry&&map->numHashElements==1&&map->modCount==1);
    MCObjectHeap_free(heap);

    heap=MCObjectHeap_new(8u*1024u*1024u);CHECK(heap);map=LongHashMap_new(heap);CHECK(map);old=map->hashArray;
    CHECK(!LongHashMap_resizeTable(map,-1)&&MCObjectHeap_failed(heap));
    CHECK(map->hashArray==old&&map->mask==4095&&map->capacity==3072&&map->modCount==0);
    MCObjectHeap_free(heap);
}
static void signed_counters_and_float_capacity(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8u*1024u*1024u);CHECK(heap);
    LongHashMap *map=LongHashMap_new(heap);CHECK(map);
    map->modCount=INT32_MAX;map->numHashElements=INT32_MAX;map->capacity=INT32_MAX;MCObjectHeap_touch(heap);
    CHECK(LongHashMap_add(map,0,NULL));
    CHECK(map->modCount==INT32_MIN&&map->numHashElements==INT32_MIN&&map->hashArray->length==8192);
    CHECK(LongHashMap_removeKey(map,0));
    CHECK(map->modCount==INT32_MIN+1&&map->numHashElements==INT32_MAX);
    const float factors[]={NAN,INFINITY,-INFINITY,-0.25f,0.75f};
    const int32_t capacities[]={0,INT32_MAX,INT32_MIN,-2,6};
    for(size_t i=0;i<sizeof(factors)/sizeof(factors[0]);i++) {
        map->percentUseable=factors[i];MCObjectHeap_touch(heap);
        CHECK(LongHashMap_resizeTable(map,8)&&map->capacity==capacities[i]);
    }
    CHECK(LongHashMap_resizeTable(map,0)&&map->hashArray->length==0&&map->mask==-1&&map->capacity==0);
    CHECK(!LongHashMap_getEntry(map,0)&&MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}
static void graph_aliases_collection_clone_adopt_and_abort(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8u*1024u*1024u);CHECK(heap);
    LongHashMap *map=LongHashMap_new(heap);CHECK(map);CHECK(LongHashMap_resizeTable(map,4));
    MCObject *shared=value(heap);
    CHECK(LongHashMap_add(map,0,shared)&&LongHashMap_add(map,INT64_C(4294967297),shared));
    CHECK(LongHashMap_add(map,INT64_C(8589934594),(MCObject *)map));
    LongHashMapEntry *removed=LongHashMap_removeKey(map,INT64_C(4294967297));CHECK(removed&&removed->nextEntry);
    LongHashMapEntryArray *old=map->hashArray;CHECK(LongHashMap_resizeTable(map,8));
    MCObjectRoot root={0},removedRoot={0},arrayRoot={0};
    CHECK(MCObjectRoot_init(&root,heap,(MCObject *)map)&&MCObjectRoot_init(&removedRoot,heap,(MCObject *)removed)&&
          MCObjectRoot_init(&arrayRoot,heap,(MCObject *)old));
    CHECK(MCObjectHeap_collect(heap));
    MCObjectHeap *copyHeap=MCObjectHeap_clone(heap);CHECK(copyHeap);
    MCObjectRoot copyRoot={0},copyRemovedRoot={0},copyArrayRoot={0};
    CHECK(MCObjectRoot_rebind(&copyRoot,copyHeap,&root)&&MCObjectRoot_rebind(&copyRemovedRoot,copyHeap,&removedRoot)&&
          MCObjectRoot_rebind(&copyArrayRoot,copyHeap,&arrayRoot));
    LongHashMap *copy=(LongHashMap *)MCObjectRoot_get(&copyRoot);
    LongHashMapEntry *copyRemoved=(LongHashMapEntry *)MCObjectRoot_get(&copyRemovedRoot);
    LongHashMapEntryArray *copyOld=(LongHashMapEntryArray *)MCObjectRoot_get(&copyArrayRoot);
    CHECK(copy&&copy!=map&&copy->hashArray!=map->hashArray&&copyOld!=old&&copyRemoved!=removed);
    CHECK(copyRemoved->value==LongHashMap_getValueByKey(copy,0)&&copyRemoved->value!=shared);
    CHECK(copyRemoved->nextEntry==LongHashMap_getEntry(copy,0));
    CHECK(LongHashMap_getValueByKey(copy,INT64_C(8589934594))==(MCObject *)copy);
    CHECK(copyOld->length==4&&!copyOld->values[0]&&copy->hashArray->length==8);
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,copyHeap));
    CHECK(!MCObjectHeap_collect(copyHeap)&&!MCObjectHeap_clone(copyHeap)&&!MCObjectHeap_failed(copyHeap));
    CHECK(!MCObjectHeap_adopt(heap,copyHeap));MCObjectRootScope_end(&scope);
    /* A refused adoption is a native guard, not a mutation of the live graph. */
    CHECK(LongHashMap_getValueByKey(map,0)==shared);
    CHECK(MCObjectHeap_adopt(heap,copyHeap));MCObjectHeap_free(copyHeap);
    map=(LongHashMap *)MCObjectRoot_get(&root);removed=(LongHashMapEntry *)MCObjectRoot_get(&removedRoot);
    CHECK(map&&removed->value==LongHashMap_getValueByKey(map,0)&&removed->nextEntry==LongHashMap_getEntry(map,0));
    copyHeap=MCObjectHeap_clone(heap);CHECK(copyHeap);CHECK(MCObjectRoot_rebind(&copyRoot,copyHeap,&root));
    copy=(LongHashMap *)MCObjectRoot_get(&copyRoot);
    MCObject *copyValue=LongHashMap_getValueByKey(copy,0);CHECK(copyValue&&copyValue!=removed->value);
    CHECK(LongHashMap_remove(copy,0)==copyValue&&!LongHashMap_containsItem(copy,0));
    CHECK(!LongHashMap_resizeTable(copy,-1)&&MCObjectHeap_failed(copyHeap));
    CHECK(!MCObjectHeap_adopt(heap,copyHeap));MCObjectHeap_free(copyHeap);
    CHECK(LongHashMap_containsItem(map,0)&&LongHashMap_getValueByKey(map,INT64_C(8589934594))==(MCObject *)map);
    MCObjectRoot_drop(&removedRoot);MCObjectRoot_drop(&arrayRoot);CHECK(MCObjectHeap_collect(heap));
    MCObjectHeap_free(heap);
}
static void native_guards_reject_foreign_and_malformed_nodes(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8u*1024u*1024u),*foreign=MCObjectHeap_new(8u*1024u*1024u);CHECK(heap&&foreign);
    LongHashMap *map=LongHashMap_new(heap);CHECK(map);MCObject *other=value(foreign);
    CHECK(!LongHashMap_add(map,7,other)&&MCObjectHeap_failed(heap));
    CHECK(map->modCount==0&&map->numHashElements==0);MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
    heap=MCObjectHeap_new(8u*1024u*1024u);CHECK(heap);map=LongHashMap_new(heap);CHECK(map);
    map->hashArray->length=INT32_MAX;MCObjectHeap_touch(heap);
    CHECK(!LongHashMap_containsItem(map,0)&&MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(8u*1024u*1024u);CHECK(heap);map=LongHashMap_new(heap);CHECK(map);
    CHECK(LongHashMap_add(map,0,NULL));LongHashMapEntry *entry=LongHashMap_getEntry(map,0);CHECK(entry);
    entry->nextEntry=entry;MCObjectHeap_touch(heap);
    CHECK(!LongHashMap_getEntry(map,INT64_C(4294967297))&&MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
}
int main(void) {
    original_constructor();
    null_values_collision_replacement_and_removal();original_growth_threshold_and_held_array();
    rehash_relinks_original_entries();allocation_failures_retain_source_prefix();
    signed_counters_and_float_capacity();graph_aliases_collection_clone_adopt_and_abort();
    native_guards_reject_foreign_and_malformed_nodes();
    printf("Source LongHashMap: %u checks passed\n",checks);return 0;
}
