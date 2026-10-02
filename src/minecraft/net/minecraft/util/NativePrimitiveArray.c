#include "util/NativePrimitiveArray.h"
#include "util/NativeTypedObjectArray.h"

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

static bool fail_heap(MCObjectHeap *heap){MCObjectHeap_fail(heap);return false;}
#define NATIVE_PRIMITIVE_BODY(Name,Type,Label) \
static const MCObjectClass Name##_class={Label,MCObjectHeap_plainClone,NULL,NULL}; \
bool Name##_isInstance(const MCObject *object) { \
    if(!object||object->klass!=&Name##_class||MCObjectHeap_objectSize(object)<sizeof(Name))return false; \
    const Name *array=(const Name *)object; \
    return array->length>=0&&(size_t)array->length<=(MCObjectHeap_objectSize(object)-sizeof(*array))/sizeof(Type); \
} \
Name *Name##_new(MCObjectHeap *heap,int32_t length) { \
    if(length<0||(size_t)length>(SIZE_MAX-sizeof(Name))/sizeof(Type)){fail_heap(heap);return NULL;} \
    Name *array=(Name *)MCObjectHeap_alloc(heap,sizeof(*array)+(size_t)length*sizeof(Type),&Name##_class); \
    if(array){array->length=length;}return array; \
} \
bool Name##_get(const Name *array,int32_t index,Type *out) { \
    if(!Name##_isInstance((const MCObject *)array)||!out||index<0||index>=array->length||MCObjectHeap_failed(array->object.heap))return fail_heap(array?array->object.heap:NULL); \
    *out=array->values[index];return true; \
} \
bool Name##_set(Name *array,int32_t index,Type value) { \
    if(!Name##_isInstance((const MCObject *)array)||index<0||index>=array->length||MCObjectHeap_failed(array->object.heap))return fail_heap(array?array->object.heap:NULL); \
    array->values[index]=value;MCObjectHeap_touch(array->object.heap);return true; \
}
NATIVE_PRIMITIVE_BODY(NativeByteArray,int8_t,"native.ByteArray")
NATIVE_PRIMITIVE_BODY(NativeCharArray,uint16_t,"native.CharArray")
NATIVE_PRIMITIVE_BODY(NativeShortArray,int16_t,"native.ShortArray")
NATIVE_PRIMITIVE_BODY(NativeBooleanArray,bool,"native.BooleanArray")
#undef NATIVE_PRIMITIVE_BODY

static const MCObjectClass objectArrayClass;
bool NativeObjectArray_isRuntimeClass(const MCObject *object) {
    return object && object->klass == &objectArrayClass;
}
bool NativeObjectArray_isInstance(const MCObject *object) {
    if(!object||object->klass!=&objectArrayClass||MCObjectHeap_objectSize(object)<sizeof(NativeObjectArray))return false;
    const NativeObjectArray *array=(const NativeObjectArray *)object;
    return array->length>=0&&(size_t)array->length<=
        (MCObjectHeap_objectSize(object)-sizeof(*array))/sizeof(*array->values)&&
        (!array->componentType||NativeJavaClass_isInstance((MCObject *)array->componentType));
}
static void object_array_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    if(!NativeObjectArray_isInstance(object)){fail_heap(object->heap);return;}
    NativeObjectArray *array=(NativeObjectArray *)object;
    array->componentType=(NativeJavaClass *)visitor((MCObject *)array->componentType,context);
    for(int32_t i=0;i<array->length;i++)array->values[i]=visitor(array->values[i],context);
}
static const MCObjectClass objectArrayClass={"native.ObjectArray",MCObjectHeap_plainClone,object_array_trace,NULL};
NativeObjectArray *NativeObjectArray_new(MCObjectHeap *heap,int32_t length) {
    if(length<0||(size_t)length>(SIZE_MAX-sizeof(NativeObjectArray))/sizeof(MCObject *)){fail_heap(heap);return NULL;}
    NativeObjectArray *array=(NativeObjectArray *)MCObjectHeap_alloc(heap,
        sizeof(*array)+(size_t)length*sizeof(*array->values),&objectArrayClass);
    if(array)array->length=length;
    return array;
}
bool NativeObjectArray_get(const NativeObjectArray *array,int32_t index,MCObject **out) {
    if(!NativeObjectArray_isInstance((const MCObject *)array)||!out||index<0||index>=array->length||
       MCObjectHeap_failed(array->object.heap)||(array->componentType&&array->componentType->object.heap!=array->object.heap))return fail_heap(array?array->object.heap:NULL);
    MCObject *value=array->values[index];
    if(value&&(value->heap!=array->object.heap||MCObjectHeap_objectSize(value)<sizeof(MCObject)))return fail_heap(array->object.heap);
    *out=value;return true;
}
bool NativeObjectArray_set(NativeObjectArray *array,int32_t index,MCObject *value) {
    if(NativeObjectArray_isInstance((MCObject *)array)&&array->componentType) {
        NativeArrayResult result=NativeTypedObjectArray_set(array,index,value);
        if(result!=NATIVE_ARRAY_OK)return fail_heap(array->object.heap);
        return true;
    }
    if(!NativeObjectArray_isInstance((const MCObject *)array)||index<0||index>=array->length||
       MCObjectHeap_failed(array->object.heap)||(value&&(value->heap!=array->object.heap||MCObjectHeap_objectSize(value)<sizeof(MCObject))))return fail_heap(array?array->object.heap:NULL);
    array->values[index]=value;MCObjectHeap_touch(array->object.heap);return true;
}
