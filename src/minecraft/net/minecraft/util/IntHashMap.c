#include "util/IntHashMap.h"
#include <math.h>
#include <string.h>

static bool fail(MCObjectHeap *heap) {MCObjectHeap_fail(heap);return false;}
static int32_t signed_bits(uint32_t bits) {int32_t value;memcpy(&value,&bits,sizeof(value));return value;}
static void entry_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    IntHashMapEntry *entry=(IntHashMapEntry *)object;
    entry->valueEntry=visitor(entry->valueEntry,context);
    entry->nextEntry=(IntHashMapEntry *)visitor((MCObject *)entry->nextEntry,context);
}
static const MCObjectClass entryClass={"net.minecraft.util.IntHashMap.Entry",MCObjectHeap_plainClone,entry_trace,NULL};
bool IntHashMapEntry_isInstance(const MCObject *object) {
    return object&&object->klass==&entryClass&&MCObjectHeap_objectSize(object)>=sizeof(IntHashMapEntry);
}
static bool slots_valid(const IntHashMapSlots *slots);
static void slots_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    IntHashMapSlots *slots=(IntHashMapSlots *)object;
    if(!slots_valid(slots)){fail(object->heap);return;}
    for(int32_t i=0;i<slots->length;i++)slots->values[i]=(IntHashMapEntry *)visitor((MCObject *)slots->values[i],context);
}
static const MCObjectClass slotsClass={"native.IntHashMap.EntryArray",MCObjectHeap_plainClone,slots_trace,NULL};
static bool slots_valid(const IntHashMapSlots *slots) {
    const MCObject *object=(const MCObject *)slots;
    size_t size=MCObjectHeap_objectSize(object);
    return object&&object->klass==&slotsClass&&size>=sizeof(*slots)&&slots->length>0&&
        slots->length<=1073741824&&(size_t)slots->length<=(size-sizeof(*slots))/sizeof(slots->values[0]);
}
static IntHashMapSlots *new_slots(MCObjectHeap *heap,int32_t length) {
    if(length<=0||(size_t)length>(SIZE_MAX-sizeof(IntHashMapSlots))/sizeof(IntHashMapEntry *)) {
        fail(heap);return NULL;
    }
    IntHashMapSlots *slots=(IntHashMapSlots *)MCObjectHeap_alloc(heap,
        sizeof(*slots)+(size_t)length*sizeof(slots->values[0]),&slotsClass);
    if(slots)slots->length=length;
    return slots;
}
static void map_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    IntHashMap *map=(IntHashMap *)object;
    map->slots=(IntHashMapSlots *)visitor((MCObject *)map->slots,context);
}
static const MCObjectClass klass={"net.minecraft.util.IntHashMap",MCObjectHeap_plainClone,map_trace,NULL};
bool IntHashMap_isInstance(const MCObject *object) {
    return object&&object->klass==&klass&&MCObjectHeap_objectSize(object)>=sizeof(IntHashMap);
}
static bool valid(IntHashMap *map) {
    MCObjectHeap *heap=map?map->object.heap:NULL;
    return (IntHashMap_isInstance((MCObject *)map)&&!MCObjectHeap_failed(heap)&&
        slots_valid(map->slots)&&map->slots->object.heap==heap&&map->count>=0)||fail(heap);
}
static bool entry_valid(IntHashMap *map,IntHashMapEntry *entry,int64_t visited) {
    return (IntHashMapEntry_isInstance((MCObject *)entry)&&entry->object.heap==map->object.heap&&
        (!entry->valueEntry||entry->valueEntry->heap==map->object.heap)&&visited<=map->count)||fail(map->object.heap);
}
IntHashMap *IntHashMap_nativeAllocate(MCObjectHeap *heap) {
    return (IntHashMap *)MCObjectHeap_alloc(heap,sizeof(IntHashMap),&klass);
}
bool IntHashMap_construct(IntHashMap *map) {
    MCObjectHeap *heap=map?map->object.heap:NULL;
    if(!IntHashMap_isInstance((MCObject *)map)||MCObjectHeap_failed(heap))return fail(heap);
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return false;
    IntHashMapSlots *slots=new_slots(heap,16);
    bool ok=slots!=NULL;
    if(ok){map->slots=slots;map->threshold=12;map->growFactor=0.75f;MCObjectHeap_touch(heap);}
    MCObjectRootScope_end(&scope);return ok;
}
IntHashMap *IntHashMap_new(MCObjectHeap *heap) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    IntHashMap *map=IntHashMap_nativeAllocate(heap);bool ok=map&&IntHashMap_construct(map);
    MCObjectRootScope_end(&scope);return ok?map:NULL;
}
int32_t IntHashMap_computeHash(int32_t key) {
    uint32_t hash=(uint32_t)key;hash=hash^(hash>>20)^(hash>>12);
    return signed_bits(hash^(hash>>7)^(hash>>4));
}
int32_t IntHashMap_getSlotIndex(int32_t hash,int32_t count) {
    return signed_bits((uint32_t)hash&((uint32_t)count-1u));
}
IntHashMapEntry *IntHashMap_lookupEntry(IntHashMap *map,int32_t key) {
    if(!valid(map))return NULL;
    int32_t index=IntHashMap_getSlotIndex(IntHashMap_computeHash(key),map->slots->length);
    int64_t visited=0;
    for(IntHashMapEntry *entry=map->slots->values[index];entry;entry=entry->nextEntry) {
        if(!entry_valid(map,entry,++visited))return NULL;
        if(entry->hashEntry==key)return entry;
    }
    return NULL;
}
MCObject *IntHashMap_lookup(IntHashMap *map,int32_t key) {
    IntHashMapEntry *entry=IntHashMap_lookupEntry(map,key);return entry?entry->valueEntry:NULL;
}
bool IntHashMap_containsItem(IntHashMap *map,int32_t key) {return IntHashMap_lookupEntry(map,key)!=NULL;}
static bool copy_to(IntHashMap *map,IntHashMapSlots *destination) {
    IntHashMapSlots *old=map->slots;int64_t visited=0;
    for(int32_t i=0;i<old->length;i++) {
        IntHashMapEntry *entry=old->values[i];
        if(entry) {
            old->values[i]=NULL;MCObjectHeap_touch(map->object.heap);
            while(entry) {
                if(!entry_valid(map,entry,++visited))return false;
                IntHashMapEntry *next=entry->nextEntry;
                int32_t index=IntHashMap_getSlotIndex(entry->slotHash,destination->length);
                entry->nextEntry=destination->values[index];destination->values[index]=entry;
                entry=next;
            }
        }
    }
    return true;
}
static int32_t float_to_int(float value) {
    if(isnan(value))return 0;
    if(value>=2147483648.0f)return INT32_MAX;
    if(value<=-2147483648.0f)return INT32_MIN;
    return (int32_t)value;
}
static bool grow(IntHashMap *map,int32_t capacity) {
    if(map->slots->length==1073741824){map->threshold=INT32_MAX;MCObjectHeap_touch(map->object.heap);return true;}
    IntHashMapSlots *slots=new_slots(map->object.heap,capacity);if(!slots)return false;
    if(!copy_to(map,slots))return false;
    map->slots=slots;map->threshold=float_to_int((float)capacity*map->growFactor);
    MCObjectHeap_touch(map->object.heap);return true;
}
bool IntHashMap_addKey(IntHashMap *map,int32_t key,MCObject *value) {
    if(!valid(map)||(value&&value->heap!=map->object.heap))return fail(map?map->object.heap:NULL);
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,map->object.heap))return false;
    int32_t hash=IntHashMap_computeHash(key),index=IntHashMap_getSlotIndex(hash,map->slots->length);
    IntHashMapEntry *entry=IntHashMap_lookupEntry(map,key);bool ok=!MCObjectHeap_failed(map->object.heap);
    if(ok&&entry){entry->valueEntry=value;MCObjectHeap_touch(map->object.heap);}
    else if(ok) {
        IntHashMapEntry *previous=map->slots->values[index];
        entry=(IntHashMapEntry *)MCObjectHeap_alloc(map->object.heap,sizeof(*entry),&entryClass);
        ok=entry!=NULL;
        if(ok) {
            entry->valueEntry=value;entry->nextEntry=previous;entry->hashEntry=key;entry->slotHash=hash;
            map->slots->values[index]=entry;
            int32_t count=map->count;map->count=signed_bits((uint32_t)count+1u);MCObjectHeap_touch(map->object.heap);
            if(count>=map->threshold)ok=grow(map,signed_bits((uint32_t)map->slots->length*2u));
        }
    }
    MCObjectRootScope_end(&scope);return ok;
}
IntHashMapEntry *IntHashMap_removeEntry(IntHashMap *map,int32_t key) {
    if(!valid(map))return NULL;
    int32_t index=IntHashMap_getSlotIndex(IntHashMap_computeHash(key),map->slots->length);
    IntHashMapEntry *previous=map->slots->values[index];int64_t visited=0;
    for(IntHashMapEntry *entry=previous;entry;) {
        if(!entry_valid(map,entry,++visited))return NULL;
        IntHashMapEntry *next=entry->nextEntry;
        if(entry->hashEntry==key) {
            map->count=signed_bits((uint32_t)map->count-1u);
            if(previous==entry)map->slots->values[index]=next;else previous->nextEntry=next;
            MCObjectHeap_touch(map->object.heap);return entry;
        }
        previous=entry;entry=next;
    }
    return NULL;
}
MCObject *IntHashMap_removeObject(IntHashMap *map,int32_t key) {
    IntHashMapEntry *entry=IntHashMap_removeEntry(map,key);return entry?entry->valueEntry:NULL;
}
bool IntHashMap_clearMap(IntHashMap *map) {
    if(!valid(map))return false;
    for(int32_t i=0;i<map->slots->length;i++)map->slots->values[i]=NULL;
    map->count=0;MCObjectHeap_touch(map->object.heap);return true;
}
static bool valid_entry(IntHashMapEntry *entry) {
    return (IntHashMapEntry_isInstance((MCObject *)entry)&&!MCObjectHeap_failed(entry->object.heap))||
        fail(entry?entry->object.heap:NULL);
}
int32_t IntHashMapEntry_getHash(IntHashMapEntry *entry){return valid_entry(entry)?entry->hashEntry:0;}
MCObject *IntHashMapEntry_getValue(IntHashMapEntry *entry){return valid_entry(entry)?entry->valueEntry:NULL;}
int32_t IntHashMapEntry_hashCode(IntHashMapEntry *entry){return valid_entry(entry)?IntHashMap_computeHash(entry->hashEntry):0;}
