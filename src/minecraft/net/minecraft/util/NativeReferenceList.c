#include "util/NativeReferenceList.h"
#include <limits.h>
#include <string.h>

static bool failed(MCObjectHeap *heap) {MCObjectHeap_fail(heap);return false;}
static const MCObjectClass arrayClass;
static bool array_valid(const NativeReferenceArray *a) {
    if(!a||a->object.klass!=&arrayClass||MCObjectHeap_objectSize((const MCObject *)a)<sizeof(*a))return false;
    return a->capacity>=0&&(size_t)a->capacity<=
        (MCObjectHeap_objectSize((const MCObject *)a)-sizeof(*a))/sizeof(*a->items);
}
static void array_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    NativeReferenceArray *a=(NativeReferenceArray *)object;
    if(!array_valid(a)){failed(object->heap);return;}
    for(int32_t i=0;i<a->capacity;i++)a->items[i]=visitor(a->items[i],context);
}
static const MCObjectClass arrayClass={"native.ReferenceList.Storage",MCObjectHeap_plainClone,array_trace,NULL};
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    NativeReferenceList *list=(NativeReferenceList *)object;
    if(MCObjectHeap_objectSize(object)<sizeof(*list)){failed(object->heap);return;}
    list->storage=(NativeReferenceArray *)visitor((MCObject *)list->storage,context);
}
static const MCObjectClass listClass={"native.ReferenceList",MCObjectHeap_plainClone,trace,NULL};
bool NativeReferenceList_isInstance(const MCObject *object) {
    return object&&object->klass==&listClass&&MCObjectHeap_objectSize(object)>=sizeof(NativeReferenceList);
}
static bool valid(const NativeReferenceList *list) {
    MCObjectHeap *heap=list?list->object.heap:NULL;
    if(!NativeReferenceList_isInstance((const MCObject *)list)||MCObjectHeap_failed(heap)||
       !array_valid(list->storage)||list->storage->object.heap!=heap||
       list->size<0||list->size>list->storage->capacity)return failed(heap);
    return true;
}
static bool begin(const NativeReferenceList *list,MCObjectRootScope *scope) {
    if(!valid(list)||!MCObjectRootScope_begin(scope,list->object.heap))return false;
    if(MCObjectRootScope_pin(scope,(MCObject *)list))return true;
    MCObjectRootScope_end(scope);return false;
}
static NativeReferenceArray *array_new(MCObjectHeap *heap,int32_t capacity) {
    if(capacity<0||(size_t)capacity>(SIZE_MAX-sizeof(NativeReferenceArray))/sizeof(MCObject *)) {
        failed(heap);return NULL;
    }
    NativeReferenceArray *a=(NativeReferenceArray *)MCObjectHeap_alloc(heap,
        sizeof(*a)+(size_t)capacity*sizeof(*a->items),&arrayClass);
    if(a)a->capacity=capacity;
    return a;
}
static NativeReferenceList *new_capacity(MCObjectHeap *heap,int32_t capacity) {
    NativeReferenceList *list=(NativeReferenceList *)MCObjectHeap_alloc(heap,sizeof(*list),&listClass);
    if(!list)return NULL;
    list->storage=array_new(heap,capacity);
    return list->storage?list:NULL;
}
NativeReferenceList *NativeReferenceList_new(MCObjectHeap *heap) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    NativeReferenceList *list=new_capacity(heap,0);
    MCObjectRootScope_end(&scope);return list;
}
int32_t NativeReferenceList_size(const NativeReferenceList *list) {return valid(list)?list->size:-1;}
static bool index_valid(const NativeReferenceList *list,int32_t index) {
    return valid(list)&&(index>=0&&index<list->size?true:failed(list->object.heap));
}
MCObject *NativeReferenceList_get(const NativeReferenceList *list,int32_t index) {
    if(!index_valid(list,index))return NULL;
    MCObject *value=list->storage->items[index];
    if(value&&value->heap!=list->object.heap){failed(list->object.heap);return NULL;}
    return value;
}
static bool grow(NativeReferenceList *list) {
    if(list->size<list->storage->capacity)return true;
    int32_t old=list->storage->capacity;
    if(old==INT32_MAX)return failed(list->object.heap);
    int32_t capacity=old==0?4:old>INT32_MAX/2?INT32_MAX:old*2;
    NativeReferenceArray *storage=array_new(list->object.heap,capacity);
    if(!storage)return false;
    if(list->size)memcpy(storage->items,list->storage->items,(size_t)list->size*sizeof(*storage->items));
    list->storage=storage;MCObjectHeap_touch(list->object.heap);return true;
}
bool NativeReferenceList_add(NativeReferenceList *list,MCObject *value) {
    MCObjectRootScope scope={0};if(!begin(list,&scope))return false;
    bool ok=MCObjectRootScope_pin(&scope,value)&&grow(list);
    if(ok) {
        list->storage->items[list->size++]=value;++list->modCount;MCObjectHeap_touch(list->object.heap);
    }
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(list->object.heap);
}
MCObject *NativeReferenceList_set(NativeReferenceList *list,int32_t index,MCObject *value) {
    MCObjectRootScope scope={0};if(!begin(list,&scope))return NULL;
    MCObject *previous=NULL;
    if(index_valid(list,index)&&MCObjectRootScope_pin(&scope,value)) {
        previous=NativeReferenceList_get(list,index);
        if(!MCObjectHeap_failed(list->object.heap)){list->storage->items[index]=value;MCObjectHeap_touch(list->object.heap);}
    }
    MCObjectRootScope_end(&scope);return previous;
}
MCObject *NativeReferenceList_remove(NativeReferenceList *list,int32_t index) {
    MCObjectRootScope scope={0};if(!begin(list,&scope))return NULL;
    MCObject *previous=NULL;
    if(index_valid(list,index)) {
        previous=NativeReferenceList_get(list,index);
        if(!MCObjectHeap_failed(list->object.heap)) {
            int32_t remaining=list->size-index-1;
            if(remaining)memmove(list->storage->items+index,list->storage->items+index+1,
                (size_t)remaining*sizeof(*list->storage->items));
            list->storage->items[--list->size]=NULL;++list->modCount;MCObjectHeap_touch(list->object.heap);
        }
    }
    MCObjectRootScope_end(&scope);return previous;
}
bool NativeReferenceList_clear(NativeReferenceList *list) {
    if(!valid(list))return false;
    for(int32_t i=0;i<list->size;i++)list->storage->items[i]=NULL;
    list->size=0;++list->modCount;MCObjectHeap_touch(list->object.heap);return true;
}
NativeReferenceList *NativeReferenceList_copy(const NativeReferenceList *list) {
    MCObjectRootScope scope={0};if(!begin(list,&scope))return NULL;
    NativeReferenceList *copy=NULL;
    for(int32_t i=0;i<list->size;i++) {
        MCObject *value=NativeReferenceList_get(list,i);
        if(MCObjectHeap_failed(list->object.heap)||!MCObjectRootScope_pin(&scope,value))goto done;
    }
    copy=new_capacity(list->object.heap,list->size);
    if(copy) {
        if(list->size)memcpy(copy->storage->items,list->storage->items,(size_t)list->size*sizeof(*list->storage->items));
        copy->size=list->size;MCObjectHeap_touch(list->object.heap);
    }
done:
    MCObjectRootScope_end(&scope);return copy;
}
