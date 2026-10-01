#include "util/LongHashMap.h"
#include <math.h>
#include <string.h>

static bool fail(MCObjectHeap *heap) { MCObjectHeap_fail(heap); return false; }
static int32_t signed_bits(uint32_t bits) {
    int32_t value;memcpy(&value,&bits,sizeof(value));return value;
}
static bool same_heap(MCObjectHeap *heap,const MCObject *value) {
    return !value||(value->heap==heap&&MCObjectHeap_objectSize(value)>=sizeof(MCObject));
}
static void entry_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    if(MCObjectHeap_objectSize(object)<sizeof(LongHashMapEntry)){fail(object->heap);return;}
    LongHashMapEntry *entry=(LongHashMapEntry *)object;
    entry->value=visitor(entry->value,context);
    entry->nextEntry=(LongHashMapEntry *)visitor((MCObject *)entry->nextEntry,context);
}
static const MCObjectClass entryClass={"net.minecraft.util.LongHashMap.Entry",MCObjectHeap_plainClone,entry_trace,NULL};
bool LongHashMapEntry_isInstance(const MCObject *object) {
    return object&&object->klass==&entryClass&&MCObjectHeap_objectSize(object)>=sizeof(LongHashMapEntry);
}
static bool array_valid(const LongHashMapEntryArray *array);
static void array_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    LongHashMapEntryArray *array=(LongHashMapEntryArray *)object;
    if(!array_valid(array)){fail(object->heap);return;}
    for(int32_t i=0;i<array->length;i++)
        array->values[i]=(LongHashMapEntry *)visitor((MCObject *)array->values[i],context);
}
static const MCObjectClass arrayClass={"native.LongHashMap.EntryArray",MCObjectHeap_plainClone,array_trace,NULL};
static bool array_valid(const LongHashMapEntryArray *array) {
    if(!array)return false;
    const MCObject *object=(const MCObject *)array;
    size_t bytes=MCObjectHeap_objectSize(object);
    return object->klass==&arrayClass&&bytes>=sizeof(*array)&&array->length>=0&&
        (size_t)array->length<=(bytes-sizeof(*array))/sizeof(array->values[0]);
}
bool LongHashMapEntryArray_isInstance(const MCObject *object) {
    return array_valid((const LongHashMapEntryArray *)object);
}
LongHashMapEntryArray *LongHashMapEntryArray_nativeNew(MCObjectHeap *heap,int32_t length) {
    if(length<0||(size_t)length>(SIZE_MAX-sizeof(LongHashMapEntryArray))/sizeof(LongHashMapEntry *)) {
        fail(heap);return NULL;
    }
    LongHashMapEntryArray *array=(LongHashMapEntryArray *)MCObjectHeap_alloc(heap,
        sizeof(*array)+(size_t)length*sizeof(array->values[0]),&arrayClass);
    if(array){array->length=length;MCObjectHeap_touch(heap);}
    return array;
}
static void map_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    if(MCObjectHeap_objectSize(object)<sizeof(LongHashMap)){fail(object->heap);return;}
    LongHashMap *map=(LongHashMap *)object;
    map->hashArray=(LongHashMapEntryArray *)visitor((MCObject *)map->hashArray,context);
}
static const MCObjectClass mapClass={"net.minecraft.util.LongHashMap",MCObjectHeap_plainClone,map_trace,NULL};
bool LongHashMap_isInstance(const MCObject *object) {
    return object&&object->klass==&mapClass&&MCObjectHeap_objectSize(object)>=sizeof(LongHashMap);
}
static bool receiver_valid(LongHashMap *map) {
    MCObjectHeap *heap=map?map->object.heap:NULL;
    return (LongHashMap_isInstance((MCObject *)map)&&!MCObjectHeap_failed(heap))||fail(heap);
}
static bool valid(LongHashMap *map) {
    return receiver_valid(map)&&((array_valid(map->hashArray)&&map->hashArray->object.heap==map->object.heap)||fail(map->object.heap));
}
static bool array_index(LongHashMap *map,LongHashMapEntryArray *array,int32_t index) {
    return (array_valid(array)&&array->object.heap==map->object.heap&&index>=0&&index<array->length)||fail(map->object.heap);
}
static bool entry_valid(LongHashMap *map,LongHashMapEntry *entry,size_t visited) {
    return (LongHashMapEntry_isInstance((MCObject *)entry)&&entry->object.heap==map->object.heap&&
        same_heap(map->object.heap,entry->value)&&visited<=MCObjectHeap_liveObjects(map->object.heap))||fail(map->object.heap);
}
LongHashMap *LongHashMap_nativeAllocate(MCObjectHeap *heap) {
    return (LongHashMap *)MCObjectHeap_alloc(heap,sizeof(LongHashMap),&mapClass);
}
bool LongHashMap_construct(LongHashMap *map) {
    if(!receiver_valid(map))return false;
    MCObjectHeap *heap=map->object.heap;
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return false;
    /* The executable constructor stores these before allocating hashArray.
       This preserves its observable allocation-failure prefix. */
    map->percentUseable=0.75f;map->capacity=3072;MCObjectHeap_touch(heap);
    LongHashMapEntryArray *array=LongHashMapEntryArray_nativeNew(heap,4096);
    bool ok=array!=NULL;
    if(ok){map->hashArray=array;map->mask=signed_bits((uint32_t)map->hashArray->length-1u);MCObjectHeap_touch(heap);}
    MCObjectRootScope_end(&scope);return ok;
}
LongHashMap *LongHashMap_new(MCObjectHeap *heap) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    LongHashMap *map=LongHashMap_nativeAllocate(heap);bool ok=map&&LongHashMap_construct(map);
    MCObjectRootScope_end(&scope);return ok?map:NULL;
}
LongHashMapEntry *LongHashMapEntry_new(MCObjectHeap *heap,int32_t hash,int64_t key,
                                     MCObject *value,LongHashMapEntry *next) {
    if(!same_heap(heap,value)||(next&&(!LongHashMapEntry_isInstance((MCObject *)next)||next->object.heap!=heap))) {
        fail(heap);return NULL;
    }
    LongHashMapEntry *entry=(LongHashMapEntry *)MCObjectHeap_alloc(heap,sizeof(*entry),&entryClass);
    if(entry){entry->value=value;entry->nextEntry=next;entry->key=key;entry->hash=hash;MCObjectHeap_touch(heap);}
    return entry;
}
int32_t LongHashMap_hash(int32_t integer) {
    uint32_t hash=(uint32_t)integer;hash=hash^(hash>>20)^(hash>>12);
    return signed_bits(hash^(hash>>7)^(hash>>4));
}
int32_t LongHashMap_getHashedKey(int64_t key) {
    uint64_t bits=(uint64_t)key;
    return LongHashMap_hash(signed_bits((uint32_t)(bits^(bits>>32))));
}
int32_t LongHashMap_getHashIndex(int32_t hash,int32_t mask) {
    return signed_bits((uint32_t)hash&(uint32_t)mask);
}
int32_t LongHashMap_getNumHashElements(LongHashMap *map) {
    return receiver_valid(map)?map->numHashElements:0;
}
LongHashMapEntry *LongHashMap_getEntry(LongHashMap *map,int64_t key) {
    if(!valid(map))return NULL;
    int32_t hash=LongHashMap_getHashedKey(key),index=LongHashMap_getHashIndex(hash,map->mask);
    LongHashMapEntryArray *array=map->hashArray;
    if(!array_index(map,array,index))return NULL;
    size_t visited=0;
    for(LongHashMapEntry *entry=array->values[index];entry;entry=entry->nextEntry) {
        if(!entry_valid(map,entry,++visited))return NULL;
        if(entry->key==key)return entry;
    }
    return NULL;
}
MCObject *LongHashMap_getValueByKey(LongHashMap *map,int64_t key) {
    LongHashMapEntry *entry=LongHashMap_getEntry(map,key);return entry?entry->value:NULL;
}
bool LongHashMap_containsItem(LongHashMap *map,int64_t key) {return LongHashMap_getEntry(map,key)!=NULL;}
static int32_t float_to_int(float value) {
    if(isnan(value))return 0;
    if(value>=2147483648.0f)return INT32_MAX;
    if(value<=-2147483648.0f)return INT32_MIN;
    return (int32_t)value;
}
bool LongHashMap_copyHashTableTo(LongHashMap *map,LongHashMapEntryArray *destination) {
    if(!valid(map))return false;
    LongHashMapEntryArray *old=map->hashArray;
    if(!array_valid(destination)||destination->object.heap!=map->object.heap)return fail(map->object.heap);
    int32_t length=destination->length;
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,map->object.heap))return false;
    bool ok=true;size_t visited=0;
    for(int32_t i=0;ok&&i<old->length;i++) {
        LongHashMapEntry *entry=old->values[i];
        if(entry) {
            old->values[i]=NULL;MCObjectHeap_touch(map->object.heap);
            while(entry) {
                if(!entry_valid(map,entry,++visited)){ok=false;break;}
                LongHashMapEntry *next=entry->nextEntry;
                int32_t index=LongHashMap_getHashIndex(entry->hash,signed_bits((uint32_t)length-1u));
                if(!array_index(map,destination,index)){ok=false;break;}
                entry->nextEntry=destination->values[index];destination->values[index]=entry;
                MCObjectHeap_touch(map->object.heap);entry=next;
            }
        }
    }
    MCObjectRootScope_end(&scope);return ok;
}
bool LongHashMap_resizeTable(LongHashMap *map,int32_t newSize) {
    if(!valid(map))return false;
    LongHashMapEntryArray *old=map->hashArray;
    int32_t length=old->length;
    if(length==1073741824){map->capacity=INT32_MAX;MCObjectHeap_touch(map->object.heap);return true;}
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,map->object.heap))return false;
    LongHashMapEntryArray *array=LongHashMapEntryArray_nativeNew(map->object.heap,newSize);
    bool ok=array&&LongHashMap_copyHashTableTo(map,array);
    if(ok) {
        map->hashArray=array;map->mask=signed_bits((uint32_t)map->hashArray->length-1u);
        volatile float capacity=(float)newSize*map->percentUseable;
        map->capacity=float_to_int(capacity);MCObjectHeap_touch(map->object.heap);
    }
    MCObjectRootScope_end(&scope);return ok;
}
bool LongHashMap_createKey(LongHashMap *map,int32_t hash,int64_t key,MCObject *value,int32_t index) {
    if(!valid(map)||!same_heap(map->object.heap,value))return fail(map?map->object.heap:NULL);
    LongHashMapEntryArray *array=map->hashArray;
    if(!array_index(map,array,index))return false;
    LongHashMapEntry *previous=array->values[index];
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,map->object.heap))return false;
    LongHashMapEntry *entry=LongHashMapEntry_new(map->object.heap,hash,key,value,previous);
    bool ok=entry!=NULL;
    if(ok) {
        array->values[index]=entry;
        int32_t count=map->numHashElements;map->numHashElements=signed_bits((uint32_t)count+1u);
        MCObjectHeap_touch(map->object.heap);
        if(count>=map->capacity)ok=LongHashMap_resizeTable(map,signed_bits((uint32_t)map->hashArray->length*2u));
    }
    MCObjectRootScope_end(&scope);return ok;
}
bool LongHashMap_add(LongHashMap *map,int64_t key,MCObject *value) {
    if(!valid(map)||!same_heap(map->object.heap,value))return fail(map?map->object.heap:NULL);
    int32_t hash=LongHashMap_getHashedKey(key),index=LongHashMap_getHashIndex(hash,map->mask);
    LongHashMapEntryArray *array=map->hashArray;
    if(!array_index(map,array,index))return false;
    size_t visited=0;
    for(LongHashMapEntry *entry=array->values[index];entry;entry=entry->nextEntry) {
        if(!entry_valid(map,entry,++visited))return false;
        if(entry->key==key){entry->value=value;MCObjectHeap_touch(map->object.heap);return true;}
    }
    map->modCount=signed_bits((uint32_t)map->modCount+1u);MCObjectHeap_touch(map->object.heap);
    return LongHashMap_createKey(map,hash,key,value,index);
}
LongHashMapEntry *LongHashMap_removeKey(LongHashMap *map,int64_t key) {
    if(!valid(map))return NULL;
    int32_t hash=LongHashMap_getHashedKey(key),index=LongHashMap_getHashIndex(hash,map->mask);
    LongHashMapEntryArray *array=map->hashArray;
    if(!array_index(map,array,index))return NULL;
    LongHashMapEntry *previous=array->values[index];size_t visited=0;
    for(LongHashMapEntry *entry=previous;entry;) {
        if(!entry_valid(map,entry,++visited))return NULL;
        LongHashMapEntry *next=entry->nextEntry;
        if(entry->key==key) {
            map->modCount=signed_bits((uint32_t)map->modCount+1u);
            map->numHashElements=signed_bits((uint32_t)map->numHashElements-1u);
            if(previous==entry)array->values[index]=next;else previous->nextEntry=next;
            MCObjectHeap_touch(map->object.heap);return entry;
        }
        previous=entry;entry=next;
    }
    return NULL;
}
MCObject *LongHashMap_remove(LongHashMap *map,int64_t key) {
    LongHashMapEntry *entry=LongHashMap_removeKey(map,key);return entry?entry->value:NULL;
}
static bool valid_entry(LongHashMapEntry *entry) {
    MCObjectHeap *heap=entry?entry->object.heap:NULL;
    return (LongHashMapEntry_isInstance((MCObject *)entry)&&!MCObjectHeap_failed(heap))||fail(heap);
}
int64_t LongHashMapEntry_getKey(LongHashMapEntry *entry) {return valid_entry(entry)?entry->key:0;}
MCObject *LongHashMapEntry_getValue(LongHashMapEntry *entry) {return valid_entry(entry)?entry->value:NULL;}
int32_t LongHashMapEntry_hashCode(LongHashMapEntry *entry) {return valid_entry(entry)?LongHashMap_getHashedKey(entry->key):0;}
