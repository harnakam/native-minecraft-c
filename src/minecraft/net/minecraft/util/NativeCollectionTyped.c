#include "util/NativeCollectionTyped.h"
#include "util/NativeReferenceList.h"
#include <limits.h>

static NativeArrayResult fail(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return NATIVE_ARRAY_FAILURE;
}
static bool same_reference(const MCObject *object, void *expected) { return object == expected; }
static bool reference(MCObjectHeap *heap, MCObject *object) {
    /* Native callbacks may supply a readable but unregistered header. Verify
       actual graph membership before using the allocator's fast size query. */
    return !object ||
           (object->heap == heap && object->klass &&
            MCObjectHeap_findObject(heap, object->klass, same_reference, object) == object &&
            MCObjectHeap_objectSize(object) >= sizeof(MCObject));
}
static bool array(MCObjectHeap *heap, NativeTypedObjectArray *value) {
    return value && reference(heap, (MCObject *)value) &&
           NativeObjectArray_isRuntimeClass((MCObject *)value) &&
           MCObjectHeap_objectSize((MCObject *)value) >= sizeof(*value) &&
           reference(heap, (MCObject *)value->componentType) &&
           NativeTypedObjectArray_isInstance((MCObject *)value) &&
           (!value->componentType || value->componentType->object.heap == heap);
}
static bool context(MCObjectHeap *heap, MCObject **field, MCObjectRootScope *scope,
                    MCObject **captured) {
    MCObject *value = field ? *field : NULL;
    if (!reference(heap, value) || !MCObjectRootScope_pin(scope, value)) {
        fail(heap);
        return false;
    }
    if (captured)
        *captured = value;
    return true;
}
static NativeArrayResult terminal(MCObjectHeap *heap, MCObject **field, MCObjectRootScope *scope,
                                  NativeArrayResult result) {
    /* Run the final current-field check even for a Source exception or sticky
       callback failure; it must not hide a newly installed foreign edge. */
    if (!context(heap, field, scope, NULL) ||
        (result != NATIVE_ARRAY_OK && result != NATIVE_ARRAY_EXCEPTION &&
         result != NATIVE_ARRAY_FAILURE) ||
        result == NATIVE_ARRAY_FAILURE || MCObjectHeap_failed(heap))
        return fail(heap);
    return result;
}
static NativeArrayResult allocate_like(MCObjectHeap *heap, NativeTypedObjectArray *source,
                                       int32_t length, NativeTypedObjectArray **out) {
    if (source->componentType)
        return NativeTypedObjectArray_new(heap, source->componentType, length, out);
    if (length < 0)
        return NATIVE_ARRAY_EXCEPTION;
    NativeTypedObjectArray *created = NativeObjectArray_new(heap, length);
    if (!created)
        return fail(heap);
    *out = created;
    return NATIVE_ARRAY_OK;
}
static NativeArrayResult copy_cells(MCObjectHeap *heap, NativeTypedObjectArray *source,
                                    NativeTypedObjectArray *destination, int32_t count) {
    for (int32_t i = 0; i < count; ++i) {
        MCObject *value = NULL;
        NativeArrayResult result = NativeTypedObjectArray_get(source, i, &value);
        if (result != NATIVE_ARRAY_OK)
            return result;
        if (!reference(heap, value))
            return fail(heap);
        result = NativeTypedObjectArray_set(destination, i, value);
        if (result != NATIVE_ARRAY_OK)
            return result;
    }
    return NATIVE_ARRAY_OK;
}
static NativeArrayResult resize(MCObjectHeap *heap, NativeTypedObjectArray *source, int32_t length,
                                int32_t copied, NativeTypedObjectArray **out) {
    NativeTypedObjectArray *created = NULL;
    NativeArrayResult result = allocate_like(heap, source, length, &created);
    if (result == NATIVE_ARRAY_OK)
        result = copy_cells(heap, source, created, copied);
    if (result == NATIVE_ARRAY_OK)
        *out = created;
    return result;
}
static NativeArrayResult list_to_array(MCObjectHeap *heap, NativeReferenceList *list,
                                       NativeTypedObjectArray *requested, MCObject **out) {
    /* ArrayList directly copies elementData: no artificial iterator allocation
       or a sequence of live-list getters is added to this branch. */
    if (!requested)
        return NATIVE_ARRAY_EXCEPTION;
    int32_t size = NativeReferenceList_size(list);
    if (MCObjectHeap_failed(heap))
        return NATIVE_ARRAY_FAILURE;
    NativeReferenceArray *storage = list->storage;
    NativeTypedObjectArray *destination = requested;
    NativeArrayResult result = NATIVE_ARRAY_OK;
    if (requested->length < size)
        result = allocate_like(heap, requested, size, &destination);
    if (result != NATIVE_ARRAY_OK)
        return result;
    for (int32_t i = 0; i < size; ++i) {
        MCObject *value = storage->items[i];
        if (!reference(heap, value))
            return fail(heap);
        result = NativeTypedObjectArray_set(destination, i, value);
        if (result != NATIVE_ARRAY_OK)
            return result;
    }
    if (destination == requested && requested->length > size) {
        result = NativeTypedObjectArray_set(requested, size, NULL);
        if (result != NATIVE_ARRAY_OK)
            return result;
    }
    *out = (MCObject *)destination;
    return NATIVE_ARRAY_OK;
}
static NativeArrayResult consume(MCObjectHeap *heap, NativeLinkedHashMapIterator *iterator,
                                 NativeTypedObjectArray *working, NativeTypedObjectArray *requested,
                                 MCObject **out) {
    int32_t index = 0;
    while (index < working->length) {
        bool more = NativeLinkedHashMapIterator_hasNext(iterator);
        if (MCObjectHeap_failed(heap))
            return NATIVE_ARRAY_FAILURE;
        if (!more) {
            NativeArrayResult result = NATIVE_ARRAY_OK;
            NativeTypedObjectArray *returned = working;
            if (working == requested) {
                result = NativeTypedObjectArray_set(working, index, NULL);
            } else if (requested->length < index) {
                result = resize(heap, working, index, index, &returned);
            } else {
                result = copy_cells(heap, working, requested, index);
                if (result == NATIVE_ARRAY_OK && requested->length > index)
                    result = NativeTypedObjectArray_set(requested, index, NULL);
                returned = requested;
            }
            if (result == NATIVE_ARRAY_OK)
                *out = (MCObject *)returned;
            return result;
        }
        MCObject *value = NULL;
        NativeLinkedHashMapIteratorResult next =
            NativeLinkedHashMapIterator_nextSource(iterator, &value);
        if (next == NATIVE_LINKED_ITERATOR_EXCEPTION)
            return NATIVE_ARRAY_EXCEPTION;
        if (next != NATIVE_LINKED_ITERATOR_OK)
            return fail(heap);
        if (!reference(heap, value))
            return fail(heap);
        NativeArrayResult result = NativeTypedObjectArray_set(working, index, value);
        if (result != NATIVE_ARRAY_OK)
            return result;
        ++index;
    }
    bool more = NativeLinkedHashMapIterator_hasNext(iterator);
    if (MCObjectHeap_failed(heap))
        return NATIVE_ARRAY_FAILURE;
    if (!more) {
        *out = (MCObject *)working;
        return NATIVE_ARRAY_OK;
    }
    /* AbstractCollection's finishToArray: component-preserving 1.5x+1
       growth and exact final trimming, with healthy iterator/store prefixes. */
    while (more) {
        if (index == working->length) {
            int32_t capacity = working->length;
            int64_t grown = (int64_t)capacity + (capacity >> 1) + 1;
            if (grown > INT32_MAX - 8) {
                if (capacity == INT32_MAX)
                    return fail(heap);
                grown = capacity + 1 > INT32_MAX - 8 ? INT32_MAX : INT32_MAX - 8;
            }
            NativeTypedObjectArray *replacement = NULL;
            NativeArrayResult result = resize(heap, working, (int32_t)grown, index, &replacement);
            if (result != NATIVE_ARRAY_OK)
                return result;
            working = replacement;
        }
        MCObject *value = NULL;
        NativeLinkedHashMapIteratorResult next =
            NativeLinkedHashMapIterator_nextSource(iterator, &value);
        if (next == NATIVE_LINKED_ITERATOR_EXCEPTION)
            return NATIVE_ARRAY_EXCEPTION;
        if (next != NATIVE_LINKED_ITERATOR_OK)
            return fail(heap);
        if (!reference(heap, value))
            return fail(heap);
        NativeArrayResult result = NativeTypedObjectArray_set(working, index, value);
        if (result != NATIVE_ARRAY_OK)
            return result;
        ++index;
        more = NativeLinkedHashMapIterator_hasNext(iterator);
        if (MCObjectHeap_failed(heap))
            return NATIVE_ARRAY_FAILURE;
    }
    if (index != working->length) {
        NativeTypedObjectArray *trimmed = NULL;
        NativeArrayResult result = resize(heap, working, index, index, &trimmed);
        if (result != NATIVE_ARRAY_OK)
            return result;
        working = trimmed;
    }
    *out = (MCObject *)working;
    return NATIVE_ARRAY_OK;
}
NativeArrayResult NativeCollectionTyped_fromLinkedIterator(MCObjectHeap *heap,
                                                           NativeLinkedHashMapIterator *iterator,
                                                           NativeTypedObjectArray *working,
                                                           NativeTypedObjectArray *requested,
                                                           MCObject **out) {
    MCObjectRootScope scope = {0};
    if (!heap || !out || MCObjectHeap_failed(heap) || !MCObjectRootScope_begin(&scope, heap))
        return fail(heap);
    NativeArrayResult result = NATIVE_ARRAY_FAILURE;
    MCObject *returned = NULL;
    if (!iterator || !working || !requested) {
        result = NATIVE_ARRAY_EXCEPTION;
        goto done;
    }
    if (!reference(heap, (MCObject *)iterator) ||
        !NativeLinkedHashMapIterator_isInstance((MCObject *)iterator) || !array(heap, working) ||
        !array(heap, requested))
        goto done;
    const NativeJavaClassDescriptor *workingClass =
        working->componentType ? working->componentType->descriptor : &NativeJavaClass_ObjectClass;
    const NativeJavaClassDescriptor *requestedClass = requested->componentType
                                                          ? requested->componentType->descriptor
                                                          : &NativeJavaClass_ObjectClass;
    if (workingClass != requestedClass)
        goto done;
    if (!MCObjectRootScope_pin(&scope, (MCObject *)iterator) ||
        !MCObjectRootScope_pin(&scope, (MCObject *)working) ||
        !MCObjectRootScope_pin(&scope, (MCObject *)requested))
        goto done;
    result = consume(heap, iterator, working, requested, &returned);
done:
    if (result == NATIVE_ARRAY_FAILURE || MCObjectHeap_failed(heap))
        result = fail(heap);
    if (result == NATIVE_ARRAY_OK)
        *out = returned;
    MCObjectRootScope_end(&scope);
    return result;
}
static NativeArrayResult view_to_array(MCObjectHeap *heap, NativeLinkedHashMapView *view,
                                       NativeTypedObjectArray *requested, MCObject **out) {
    int32_t size = NativeLinkedHashMapView_size(view);
    if (MCObjectHeap_failed(heap))
        return NATIVE_ARRAY_FAILURE;
    if (!requested)
        return NATIVE_ARRAY_EXCEPTION;
    NativeTypedObjectArray *working = requested;
    NativeArrayResult result = NATIVE_ARRAY_OK;
    if (requested->length < size)
        result = allocate_like(heap, requested, size, &working);
    if (result != NATIVE_ARRAY_OK)
        return result;
    /* The actual iterator is constructed AFTER the size/array branch. */
    NativeLinkedHashMapIterator *iterator = NativeLinkedHashMapIterator_fromView(view);
    if (!iterator)
        return fail(heap);
    return NativeCollectionTyped_fromLinkedIterator(heap, iterator, working, requested, out);
}
NativeArrayResult NativeCollectionTyped_size(MCObjectHeap *heap, MCObject *receiver,
                                             const NativeCollectionTypedMethods *methods,
                                             MCObject **field, int32_t *out) {
    MCObjectRootScope scope = {0};
    if (!heap || !out || MCObjectHeap_failed(heap) || !MCObjectRootScope_begin(&scope, heap))
        return fail(heap);
    NativeArrayResult result = NATIVE_ARRAY_FAILURE;
    int32_t value = 0;
    MCObject *captured = NULL;
    if (!reference(heap, receiver) || !MCObjectRootScope_pin(&scope, receiver) ||
        !context(heap, field, &scope, &captured))
        goto done;
    if (!receiver) {
        result = NATIVE_ARRAY_EXCEPTION;
        goto done;
    }
    if (methods) {
        if (!methods->size)
            goto done;
        result = methods->size(captured, receiver, &value);
    } else if (NativeReferenceList_isInstance(receiver)) {
        value = NativeReferenceList_size((NativeReferenceList *)receiver);
        result = MCObjectHeap_failed(heap) ? NATIVE_ARRAY_FAILURE : NATIVE_ARRAY_OK;
    } else if (NativeLinkedHashMapView_isInstance(receiver)) {
        value = NativeLinkedHashMapView_size((NativeLinkedHashMapView *)receiver);
        result = MCObjectHeap_failed(heap) ? NATIVE_ARRAY_FAILURE : NATIVE_ARRAY_OK;
    }
done:
    result = terminal(heap, field, &scope, result);
    if (result == NATIVE_ARRAY_OK)
        *out = value;
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult NativeCollectionTyped_toArray(MCObjectHeap *heap, MCObject *receiver,
                                                const NativeCollectionTypedMethods *methods,
                                                MCObject **field, NativeTypedObjectArray *requested,
                                                MCObject **out) {
    MCObjectRootScope scope = {0};
    if (!heap || !out || MCObjectHeap_failed(heap) || !MCObjectRootScope_begin(&scope, heap))
        return fail(heap);
    NativeArrayResult result = NATIVE_ARRAY_FAILURE;
    MCObject *returned = NULL, *captured = NULL;
    if (!reference(heap, receiver) || !MCObjectRootScope_pin(&scope, receiver) ||
        (requested && !array(heap, requested)) ||
        !MCObjectRootScope_pin(&scope, (MCObject *)requested) ||
        !context(heap, field, &scope, &captured))
        goto done;
    if (!receiver) {
        result = NATIVE_ARRAY_EXCEPTION;
        goto done;
    }
    if (methods) {
        if (!methods->toArray)
            goto done;
        result = methods->toArray(captured, receiver, requested, &returned);
    } else if (NativeReferenceList_isInstance(receiver)) {
        result = list_to_array(heap, (NativeReferenceList *)receiver, requested, &returned);
    } else if (NativeLinkedHashMapView_isInstance(receiver)) {
        result = view_to_array(heap, (NativeLinkedHashMapView *)receiver, requested, &returned);
    }
done:
    result = terminal(heap, field, &scope, result);
    if (result == NATIVE_ARRAY_OK) {
        if (!reference(heap, returned) ||
            (NativeObjectArray_isRuntimeClass(returned) &&
             !array(heap, (NativeTypedObjectArray *)returned)) ||
            !MCObjectRootScope_pin(&scope, returned))
            result = fail(heap);
        else
            *out = returned;
    }
    MCObjectRootScope_end(&scope);
    return result;
}
