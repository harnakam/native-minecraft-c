#include "util/NativeCollection.h"
#include "util/NativeReferenceList.h"
#include "util/ClassInheritanceMultiMap.h"
#include <limits.h>
#include <string.h>

static NativeObjectArray *resize(NativeObjectArray *source,int32_t count,int32_t copied) {
    NativeObjectArray *out=NativeObjectArray_new(source->object.heap,count);
    if(out&&copied)memcpy(out->values,source->values,(size_t)copied*sizeof(*out->values));
    return out;
}
NativeObjectArray *NativeCollection_toArray(MCObject *collection) {
    MCObjectHeap *heap=collection?collection->heap:NULL;
    MCObjectRootScope scope={0};NativeObjectArray *array=NULL;
    if(!collection||!MCObjectRootScope_begin(&scope,heap))goto failed;
    if(!MCObjectRootScope_pin(&scope,collection))goto done;
    if(NativeReferenceList_isInstance(collection)) {
        NativeReferenceList *list=(NativeReferenceList *)collection;
        int32_t size=NativeReferenceList_size(list);
        if(size<0)goto done;
        NativeReferenceArray *storage=list->storage;
        if(!MCObjectRootScope_pin(&scope,(MCObject *)storage))goto done;
        array=NativeObjectArray_new(heap,size);
        if(!array)goto done;
        for(int32_t i=0;i<size;i++) {
            MCObject *value=storage->items[i];
            if(!MCObjectRootScope_pin(&scope,value)){array=NULL;goto done;}
            array->values[i]=value;
        }
        MCObjectHeap_touch(heap);
    } else if(ClassInheritanceMultiMap_isInstance(collection)) {
        ClassInheritanceMultiMap *map=(ClassInheritanceMultiMap *)collection;
        int32_t size=ClassInheritanceMultiMap_size(map);
        if(size<0||MCObjectHeap_failed(heap))goto done;
        array=NativeObjectArray_new(heap,size);
        if(!array)goto done;
        ClassInheritanceMultiMapIterator *it=ClassInheritanceMultiMap_iterator(map);
        if(!it||!MCObjectRootScope_pin(&scope,(MCObject *)it)){array=NULL;goto done;}
        int32_t index=0;
        for(;;) {
            bool more=ClassInheritanceMultiMapIterator_hasNext(it);
            if(MCObjectHeap_failed(heap)){array=NULL;goto done;}
            if(!more)break;
            if(index==array->length) {
                int64_t capacity=(int64_t)array->length+(array->length>>1)+1;
                if(capacity>INT32_MAX){array=NULL;goto done;}
                NativeObjectArray *grown=resize(array,(int32_t)capacity,index);
                if(!grown){array=NULL;goto done;}array=grown;
            }
            MCObject *value=NULL;
            if(!ClassInheritanceMultiMapIterator_next(it,&value)||
               !MCObjectRootScope_pin(&scope,value)){array=NULL;goto done;}
            array->values[index++]=value;MCObjectHeap_touch(heap);
        }
        if(index!=array->length)array=resize(array,index,index);
    }
done:
    if(!array||MCObjectHeap_failed(heap)){array=NULL;MCObjectHeap_fail(heap);}
    MCObjectRootScope_end(&scope);return array;
failed:MCObjectHeap_fail(heap);return NULL;
}
