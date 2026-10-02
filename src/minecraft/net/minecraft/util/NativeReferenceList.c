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
bool NativeReferenceList_addAllArray(NativeReferenceList *list,NativeObjectArray *array,bool *changed) {
    MCObjectRootScope scope={0};if(!begin(list,&scope))return false;
    bool ok=false;
    if(!changed||!NativeObjectArray_isInstance((MCObject *)array)||
       array->object.heap!=list->object.heap||!MCObjectRootScope_pin(&scope,(MCObject *)array))goto done;
    int32_t count=array->length;
    for(int32_t i=0;i<count;i++)
        if(!MCObjectRootScope_pin(&scope,array->values[i]))goto done;
    /* ArrayList.ensureExplicitCapacity increments before any growth, even
       when numNew is zero. Allocation failure retains this source prefix. */
    ++list->modCount;MCObjectHeap_touch(list->object.heap);
    int64_t required=(int64_t)list->size+count;
    if(required>INT32_MAX)goto done;
    if(required>list->storage->capacity) {
        int32_t old=list->storage->capacity;
        int32_t capacity=old==0?4:old>INT32_MAX/2?INT32_MAX:old*2;
        if(capacity<required)capacity=(int32_t)required;
        NativeReferenceArray *storage=array_new(list->object.heap,capacity);
        if(!storage)goto done;
        if(list->size)memcpy(storage->items,list->storage->items,(size_t)list->size*sizeof(*storage->items));
        list->storage=storage;MCObjectHeap_touch(list->object.heap);
    }
    if(count)memcpy(list->storage->items+list->size,array->values,(size_t)count*sizeof(*array->values));
    list->size=(int32_t)required;MCObjectHeap_touch(list->object.heap);
    *changed=count!=0;ok=true;
done:
    if(!ok)failed(list->object.heap);
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

static NativeArrayResult source_failure(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return NATIVE_ARRAY_FAILURE;
}
static bool source_same_reference(const MCObject *object,void *expected) {
    return object==expected;
}
static bool source_reference(MCObjectHeap *heap,const MCObject *object) {
    return !object||(object->heap==heap&&object->klass&&
        MCObjectHeap_findObject(heap,object->klass,source_same_reference,(void *)object)==object&&
        MCObjectHeap_objectSize(object)>=sizeof(MCObject));
}
static NativeArrayResult source_list(const NativeReferenceList *list,MCObjectHeap *heap) {
    if(!list)return NATIVE_ARRAY_EXCEPTION;
    if(MCObjectHeap_failed(heap)||!source_reference(heap,(const MCObject *)list)||
       list->object.klass!=&listClass||MCObjectHeap_objectSize((const MCObject *)list)<sizeof(*list))
        return source_failure(heap);
    return NATIVE_ARRAY_OK;
}
static NativeArrayResult source_array(NativeReferenceArray *array,MCObjectHeap *heap) {
    if(!array)return NATIVE_ARRAY_EXCEPTION;
    if(!source_reference(heap,(MCObject *)array)||!array_valid(array))
        return source_failure(heap);
    return NATIVE_ARRAY_OK;
}
static NativeArrayResult source_cell(NativeReferenceArray *array,int32_t index,
                                    MCObjectHeap *heap,MCObject **out) {
    NativeArrayResult result=source_array(array,heap);
    if(result!=NATIVE_ARRAY_OK)return result;
    if(index<0||index>=array->capacity)return NATIVE_ARRAY_EXCEPTION;
    MCObject *value=array->items[index];
    if(!source_reference(heap,value))return source_failure(heap);
    *out=value;
    return NATIVE_ARRAY_OK;
}
NativeArrayResult NativeReferenceList_sizeSource(const NativeReferenceList *list,int32_t *out) {
    MCObjectHeap *heap=list?list->object.heap:NULL;
    if(!out)return source_failure(heap);
    NativeArrayResult result=source_list(list,heap);
    /* A negative size can be an actual fastRemove exception prefix. size()
       reads that field without visiting elementData or imposing an invariant. */
    if(result==NATIVE_ARRAY_OK)*out=list->size;
    return result;
}
NativeArrayResult NativeReferenceList_getSource(const NativeReferenceList *list,int32_t index,
                                               MCObject **out) {
    MCObjectHeap *heap=list?list->object.heap:NULL;
    if(!out)return source_failure(heap);
    NativeArrayResult result=source_list(list,heap);
    if(result!=NATIVE_ARRAY_OK)return result;
    /* ArrayList.rangeCheck has only the upper test; negative indices reach
       the subsequent array access. Neither exception poisons the owner. */
    if(index>=list->size)return NATIVE_ARRAY_EXCEPTION;
    return source_cell(list->storage,index,heap,out);
}
static int32_t source_int_bits(uint32_t bits) {
    return bits<=INT32_MAX?(int32_t)bits:(int32_t)((int64_t)bits-INT64_C(4294967296));
}
static NativeArrayResult source_fast_remove(NativeReferenceList *list,int32_t index,
                                            MCObjectHeap *heap) {
    ++list->modCount;
    MCObjectHeap_touch(heap);
    int32_t moved=source_int_bits((uint32_t)list->size-(uint32_t)index-1u);
    if(moved>0) {
        NativeReferenceArray *storage=list->storage;
        NativeArrayResult result=source_array(storage,heap);
        if(result!=NATIVE_ARRAY_OK)return result;
        int64_t from=(int64_t)index+1,to=index,length=moved;
        if(from<0||to<0||from+length>storage->capacity||to+length>storage->capacity)
            return NATIVE_ARRAY_EXCEPTION;
        for(int32_t i=0;i<moved;++i)
            if(!source_reference(heap,storage->items[(int32_t)from+i]))
                return source_failure(heap);
        memmove(storage->items+index,storage->items+index+1,(size_t)moved*sizeof(*storage->items));
        MCObjectHeap_touch(heap);
    }
    /* The assignment evaluates elementData before --size. In particular,
       equals may have cleared the list: size becomes -1 before the array
       index exception, after fastRemove has already incremented modCount. */
    NativeReferenceArray *storage=list->storage;
    list->size=source_int_bits((uint32_t)list->size-1u);
    MCObjectHeap_touch(heap);
    NativeArrayResult result=source_array(storage,heap);
    if(result!=NATIVE_ARRAY_OK)return result;
    if(list->size<0||list->size>=storage->capacity)return NATIVE_ARRAY_EXCEPTION;
    storage->items[list->size]=NULL;
    MCObjectHeap_touch(heap);
    return NATIVE_ARRAY_OK;
}
NativeArrayResult NativeReferenceList_removeObjectSource(NativeReferenceList *list,MCObject *query,
    const NativeReferenceListEqualsMethods *methods,MCObject *context,bool *removed) {
    MCObjectHeap *heap=list?list->object.heap:NULL;
    if(!removed)return source_failure(heap);
    NativeArrayResult result=source_list(list,heap);
    if(result!=NATIVE_ARRAY_OK)return result;
    MCObjectRootScope scope={0};
    if(!source_reference(heap,query)||!source_reference(heap,context)||
       !MCObjectRootScope_begin(&scope,heap))return source_failure(heap);
    bool found=false;
    if(!MCObjectRootScope_pin(&scope,(MCObject *)list)||!MCObjectRootScope_pin(&scope,query)||
       !MCObjectRootScope_pin(&scope,context)) {
        result=source_failure(heap);
        goto done;
    }
    /* Both size and current storage are read again after every equals call.
       This is the Source indexed loop, not a snapshot or an iterator. */
    for(int32_t index=0;index<list->size;++index) {
        MCObject *stored=NULL;
        result=source_cell(list->storage,index,heap,&stored);
        if(result!=NATIVE_ARRAY_OK)goto done;
        bool equal=stored==NULL;
        if(query) {
            if(!methods||!methods->equals||!MCObjectRootScope_pin(&scope,stored)) {
                result=source_failure(heap);
                goto done;
            }
            result=methods->equals(context,query,stored,&equal);
            /* Native ownership is checked after every callback status; a
               foreign context cannot be hidden by a healthy Source exception. */
            if(!source_reference(heap,context)||!source_reference(heap,query)||
               source_list(list,heap)!=NATIVE_ARRAY_OK||MCObjectHeap_failed(heap)||
               (list->storage&&source_array(list->storage,heap)!=NATIVE_ARRAY_OK)||
               (result!=NATIVE_ARRAY_OK&&result!=NATIVE_ARRAY_EXCEPTION&&
                result!=NATIVE_ARRAY_FAILURE)||result==NATIVE_ARRAY_FAILURE) {
                result=source_failure(heap);
                goto done;
            }
            if(result!=NATIVE_ARRAY_OK)goto done;
        }
        if(equal) {
            result=source_fast_remove(list,index,heap);
            if(result==NATIVE_ARRAY_OK)found=true;
            goto done;
        }
    }
    result=NATIVE_ARRAY_OK;
done:
    if(result==NATIVE_ARRAY_FAILURE||MCObjectHeap_failed(heap))result=source_failure(heap);
    if(result==NATIVE_ARRAY_OK)*removed=found;
    MCObjectRootScope_end(&scope);
    return result;
}
