#include "util/NativeFloatArray.h"
#include <stddef.h>
static const MCObjectClass arrayClass={"native.float[]",MCObjectHeap_plainClone,NULL,NULL};
NativeFloatArray *NativeFloatArray_new(MCObjectHeap *heap,int32_t length) {
    if(length<0||(size_t)length>(SIZE_MAX-sizeof(NativeFloatArray))/sizeof(float)) {
        MCObjectHeap_fail(heap);return NULL;
    }
    NativeFloatArray *array=(NativeFloatArray *)MCObjectHeap_alloc(heap,sizeof(*array)+(size_t)length*sizeof(float),&arrayClass);
    if(array)array->length=length;
    return array;
}
bool NativeFloatArray_isInstance(const MCObject *object) {
    if(!object||object->klass!=&arrayClass||MCObjectHeap_objectSize(object)<sizeof(NativeFloatArray))return false;
    const NativeFloatArray *array=(const NativeFloatArray *)object;
    return array->length>=0&&(size_t)array->length<=(MCObjectHeap_objectSize(object)-sizeof(*array))/sizeof(float);
}
