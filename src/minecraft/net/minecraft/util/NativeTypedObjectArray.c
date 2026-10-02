#include "util/NativeTypedObjectArray.h"

static NativeArrayResult fail(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return NATIVE_ARRAY_FAILURE;
}
bool NativeTypedObjectArray_isInstance(const MCObject *object) {
    return NativeObjectArray_isInstance(object);
}
static bool ready(const NativeTypedObjectArray *array) {
    return NativeTypedObjectArray_isInstance((const MCObject *)array) &&
           !MCObjectHeap_failed(array->object.heap) &&
           (!array->componentType || array->componentType->object.heap == array->object.heap);
}
NativeArrayResult NativeTypedObjectArray_new(MCObjectHeap *heap, NativeJavaClass *component,
                                             int32_t length, NativeTypedObjectArray **out) {
    if (!heap || !out || !NativeJavaClass_isInstance((MCObject *)component) ||
        component->object.heap != heap || MCObjectHeap_failed(heap))
        return fail(heap);
    if (length < 0)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap))
        return fail(heap);
    NativeArrayResult result = NATIVE_ARRAY_FAILURE;
    if (!MCObjectRootScope_pin(&scope, (MCObject *)component))
        goto done;
    NativeTypedObjectArray *array = NativeObjectArray_new(heap, length);
    if (!array)
        goto done;
    array->componentType = component;
    MCObjectHeap_touch(heap);
    *out = array;
    result = NATIVE_ARRAY_OK;
done:
    if (result == NATIVE_ARRAY_FAILURE)
        fail(heap);
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult NativeTypedObjectArray_get(const NativeTypedObjectArray *array, int32_t index,
                                             MCObject **out) {
    if (!array)
        return NATIVE_ARRAY_EXCEPTION;
    if (!ready(array) || !out)
        return fail(array->object.heap);
    if (index < 0 || index >= array->length)
        return NATIVE_ARRAY_EXCEPTION;
    MCObject *value = NULL;
    if (!NativeObjectArray_get(array, index, &value))
        return NATIVE_ARRAY_FAILURE;
    *out = value;
    return NATIVE_ARRAY_OK;
}
NativeArrayResult NativeTypedObjectArray_set(NativeTypedObjectArray *array, int32_t index,
                                             MCObject *value) {
    if (!array)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectHeap *heap = array->object.heap;
    if (!ready(array))
        return fail(heap);
    if (index < 0 || index >= array->length)
        return NATIVE_ARRAY_EXCEPTION;
    if (value && (value->heap != heap || MCObjectHeap_objectSize(value) < sizeof(MCObject)))
        return fail(heap);
    MCObjectReadScope scope = {0};
    if (!MCObjectReadScope_begin(&scope, heap))
        return fail(heap);
    NativeArrayResult result = NATIVE_ARRAY_FAILURE;
    NativeJavaClass *component = array->componentType;
    /* Validation above establishes the same-heap owners. The read guard keeps
       them alive while runtime facts are resolved without inventing mutation
       when a healthy rejected store leaves the array unchanged. */
    if (value && component && component->descriptor != &NativeJavaClass_ObjectClass) {
        bool compatible = false;
        if (!NativeJavaClass_isInstanceOf(component, value, &compatible))
            goto done;
        if (!ready(array) || array->componentType != component)
            goto done;
        if (!compatible) {
            result = NATIVE_ARRAY_EXCEPTION;
            goto done;
        }
    }
    if (MCObjectHeap_failed(heap))
        goto done;
    MCObjectReadScope_end(&scope);
    array->values[index] = value;
    MCObjectHeap_touch(heap);
    result = NATIVE_ARRAY_OK;
done:
    if (result == NATIVE_ARRAY_FAILURE)
        fail(heap);
    MCObjectReadScope_end(&scope);
    return result;
}
bool NativeTypedObjectArray_isAssignableTo(NativeTypedObjectArray *array, NativeJavaClass *target,
                                           bool *out) {
    MCObjectHeap *heap = array ? array->object.heap : NULL;
    if (!ready(array) || !out || !NativeJavaClass_isInstance((MCObject *)target) ||
        target->object.heap != heap) {
        fail(heap);
        return false;
    }
    if (target->descriptor == &NativeJavaClass_ObjectClass) {
        *out = true;
        return true;
    }
    if (!array->componentType) {
        *out = false;
        return true;
    }
    return NativeJavaClass_isAssignableFrom(target, array->componentType, out);
}
