#include "util/NativePrimitiveArray.h"

static const MCObjectClass klass={"native.IntArray",MCObjectHeap_plainClone,NULL,NULL};
static bool failed(const NativeIntArray *array) {
    MCObjectHeap_fail(array?array->object.heap:NULL);return false;
}
bool NativeIntArray_isInstance(const MCObject *object) {
    if(!object||object->klass!=&klass||MCObjectHeap_objectSize(object)<sizeof(NativeIntArray))return false;
    const NativeIntArray *array=(const NativeIntArray *)object;
    return array->length>=0&&(size_t)array->length<=
        (MCObjectHeap_objectSize(object)-sizeof(*array))/sizeof(*array->values);
}
NativeIntArray *NativeIntArray_new(MCObjectHeap *heap,int32_t length) {
    if(length<0||(size_t)length>(SIZE_MAX-sizeof(NativeIntArray))/sizeof(int32_t)) {
        MCObjectHeap_fail(heap);return NULL;
    }
    NativeIntArray *array=(NativeIntArray *)MCObjectHeap_alloc(heap,
        sizeof(*array)+(size_t)length*sizeof(int32_t),&klass);
    if(array)array->length=length;
    return array;
}
bool NativeIntArray_get(const NativeIntArray *array,int32_t index,int32_t *out) {
    if(!NativeIntArray_isInstance((const MCObject *)array)||!out||index<0||index>=array->length||
       MCObjectHeap_failed(array->object.heap))return failed(array);
    *out=array->values[index];return true;
}
bool NativeIntArray_set(NativeIntArray *array,int32_t index,int32_t value) {
    if(!NativeIntArray_isInstance((const MCObject *)array)||index<0||index>=array->length||
       MCObjectHeap_failed(array->object.heap))return failed(array);
    array->values[index]=value;MCObjectHeap_touch(array->object.heap);return true;
}
