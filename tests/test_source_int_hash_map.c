#include "util/IntHashMap.h"
#include "util/NativePrimitiveArray.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(value) do{++checks;if(!(value)){fprintf(stderr,"IntHashMap check %u line %d\n",checks,__LINE__);exit(1);}}while(0)
static const MCObjectClass valueClass={"test.IntHashValue",MCObjectHeap_plainClone,NULL,NULL};
static MCObject *value(MCObjectHeap *heap) {
    MCObject *out=MCObjectHeap_alloc(heap,sizeof(MCObject),&valueClass);CHECK(out);return out;
}
static void construction_and_growth(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8u*1024u*1024u);CHECK(heap);
    IntHashMap *map=IntHashMap_new(heap);CHECK(map);
    CHECK(map->slots&&map->slots->length==16&&map->count==0&&map->threshold==12&&map->growFactor==0.75f);
    MCObject *shared=value(heap);
    for(int32_t i=0;i<12;i++)CHECK(IntHashMap_addKey(map,i,shared));
    CHECK(map->count==12&&map->slots->length==16);
    IntHashMapEntry *first=IntHashMap_lookupEntry(map,0);CHECK(first&&first->valueEntry==shared);
    CHECK(IntHashMap_addKey(map,12,NULL));
    CHECK(map->count==13&&map->slots->length==32&&map->threshold==24);
    CHECK(IntHashMap_containsItem(map,12)&&!IntHashMap_lookup(map,12));
    CHECK(IntHashMap_addKey(map,0,NULL)&&map->count==13&&IntHashMap_lookupEntry(map,0)==first&&!first->valueEntry);
    CHECK(IntHashMap_addKey(map,INT32_MIN,shared)&&IntHashMap_addKey(map,INT32_MAX,shared));
    CHECK(IntHashMap_lookup(map,INT32_MIN)==shared&&IntHashMap_lookup(map,INT32_MAX)==shared);
    for(int32_t i=13;i<1024;i++)CHECK(IntHashMap_addKey(map,i,shared));
    for(int32_t i=1;i<1024;i++)CHECK(IntHashMap_containsItem(map,i));
    CHECK(IntHashMap_removeEntry(map,0)==first&&!IntHashMap_containsItem(map,0));
    CHECK(IntHashMap_removeObject(map,INT32_MIN)==shared&&!IntHashMap_containsItem(map,INT32_MIN));
    CHECK(!IntHashMap_removeObject(map,-991)&&!MCObjectHeap_failed(heap));
    int32_t length=map->slots->length,threshold=map->threshold;
    CHECK(IntHashMap_clearMap(map)&&map->count==0&&map->slots->length==length&&map->threshold==threshold);
    for(int32_t i=0;i<length;i++)CHECK(!map->slots->values[i]);
    MCObjectHeap_free(heap);
}
static void aliases_and_arrays(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8u*1024u*1024u);CHECK(heap);
    IntHashMap *map=IntHashMap_new(heap);CHECK(map);
    MCObject *shared=value(heap);CHECK(IntHashMap_addKey(map,7,shared)&&IntHashMap_addKey(map,23,shared));
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,heap,(MCObject *)map));
    CHECK(MCObjectHeap_collect(heap));
    MCObjectHeap *copyHeap=MCObjectHeap_clone(heap);CHECK(copyHeap);
    MCObjectRoot copyRoot={0};CHECK(MCObjectRoot_rebind(&copyRoot,copyHeap,&root));
    IntHashMap *copy=(IntHashMap *)MCObjectRoot_get(&copyRoot);CHECK(copy&&copy!=map);
    CHECK(IntHashMap_lookup(copy,7)==IntHashMap_lookup(copy,23)&&IntHashMap_lookup(copy,7)!=shared);
    CHECK(MCObjectHeap_adopt(heap,copyHeap));MCObjectHeap_free(copyHeap);
    map=(IntHashMap *)MCObjectRoot_get(&root);CHECK(map&&IntHashMap_lookup(map,7)==IntHashMap_lookup(map,23));
    NativeIntArray *array=NativeIntArray_new(heap,32768);CHECK(array&&array->length==32768);
    int32_t out=42;CHECK(NativeIntArray_get(array,32767,&out)&&out==0);
    CHECK(NativeIntArray_set(array,0,INT32_MIN)&&NativeIntArray_get(array,0,&out)&&out==INT32_MIN);
    CHECK(NativeIntArray_set(array,32767,INT32_MAX)&&NativeIntArray_get(array,32767,&out)&&out==INT32_MAX);
    MCObjectHeap_free(heap);
}
int main(void) {
    construction_and_growth();aliases_and_arrays();
    printf("Source IntHashMap: %u checks passed\n",checks);return 0;
}
