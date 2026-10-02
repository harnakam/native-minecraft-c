#include "util/NativeCollection.h"
#include "util/NativeCollectionTyped.h"
#include "util/NativeReferenceList.h"
#include "util/Vec4b.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "native typed collection check %u failed line %d: %s\n", checks,       \
                    __LINE__, #x);                                                                 \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

#ifndef NATIVE_TYPED_COLLECTION_PRIOR_RED
static const MCObjectClass leafClass = {"test.TypedCollectionOther", MCObjectHeap_plainClone, NULL,
                                        NULL};
static bool leaf_matches(const MCObject *object) {
    return object && object->klass == &leafClass &&
           MCObjectHeap_objectSize(object) >= sizeof(MCObject);
}
static const NativeJavaClassDescriptor *const leafParents[] = {&NativeJavaClass_ObjectClass};
static const NativeJavaClassDescriptor leafDescriptor = {"test.TypedCollectionOther", leafParents,
                                                         1, leaf_matches};

static NativeTypedObjectArray *typed(MCObjectHeap *heap, NativeJavaClass *component, int32_t length,
                                     MCObject *sentinel) {
    NativeTypedObjectArray *array = NULL;
    CHECK(NativeTypedObjectArray_new(heap, component, length, &array) == NATIVE_ARRAY_OK);
    for (int32_t i = 0; i < length; ++i)
        CHECK(NativeTypedObjectArray_set(array, i, sentinel) == NATIVE_ARRAY_OK);
    return array;
}
static NativeLinkedHashMap *map_values(MCObjectHeap *heap, MCObject **values, int count) {
    NativeLinkedHashMap *map = NativeLinkedHashMap_new(heap);
    CHECK(map);
    for (int i = 0; i < count; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "key-%d", i);
        NBTString *key = NBTString_fromASCII(heap, name);
        CHECK(key && NativeLinkedHashMap_put(map, (MCObject *)key, values[i]));
    }
    return map;
}
#endif

static NativeArrayResult default_to_array(MCObjectHeap *heap, MCObject *collection,
                                          NativeTypedObjectArray *requested, MCObject **out) {
#ifdef NATIVE_TYPED_COLLECTION_PRIOR_RED
    (void)heap;
    (void)requested;
    NativeObjectArray *old = NativeCollection_toArray(collection);
    if (!old)
        return NATIVE_ARRAY_FAILURE;
    *out = (MCObject *)old;
    return NATIVE_ARRAY_OK;
#else
    return NativeCollectionTyped_toArray(heap, collection, NULL, NULL, requested, out);
#endif
}

static void real_list_caller_reuse(void) {
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    NativeJavaClass *component = Vec4b_nativeClass(heap);
    Vec4b *value = Vec4b_new(heap, 1, 2, 3, 4), *sentinel = Vec4b_new(heap, 5, 6, 7, 8);
    NativeReferenceList *list = NativeReferenceList_new(heap);
    CHECK(heap && component && value && sentinel && list);
    CHECK(NativeReferenceList_add(list, (MCObject *)value) && NativeReferenceList_add(list, NULL));
    NativeTypedObjectArray *caller = NULL;
    CHECK(NativeTypedObjectArray_new(heap, component, 4, &caller) == NATIVE_ARRAY_OK);
    for (int i = 0; i < 4; ++i)
        CHECK(NativeTypedObjectArray_set(caller, i, (MCObject *)sentinel) == NATIVE_ARRAY_OK);
    MCObject *result = NULL;
    CHECK(default_to_array(heap, (MCObject *)list, caller, &result) == NATIVE_ARRAY_OK);
    CHECK(result == (MCObject *)caller);
    CHECK(caller->values[0] == (MCObject *)value && caller->values[1] == NULL &&
          caller->values[2] == NULL);
    CHECK(caller->values[3] == (MCObject *)sentinel && caller->componentType == component);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

#ifndef NATIVE_TYPED_COLLECTION_PRIOR_RED
static void default_list_lengths(void) {
    for (int size = 0; size <= 5; ++size)
        for (int length = 0; length <= 7; ++length) {
            MCObjectHeap *heap = MCObjectHeap_new(65536);
            NativeJavaClass *component = Vec4b_nativeClass(heap);
            Vec4b *a = Vec4b_new(heap, -128, -1, 0, 127), *b = Vec4b_new(heap, 5, 6, 7, 8),
                  *sentinel = Vec4b_new(heap, 9, 10, 11, 12);
            NativeReferenceList *list = NativeReferenceList_new(heap);
            CHECK(component && a && b && sentinel && list);
            MCObject *values[5] = {(MCObject *)a, NULL, (MCObject *)a, (MCObject *)b, NULL};
            for (int i = 0; i < size; ++i)
                CHECK(NativeReferenceList_add(list, values[i]));
            NativeTypedObjectArray *requested =
                typed(heap, component, length, (MCObject *)sentinel);
            size_t before = MCObjectHeap_liveObjects(heap);
            int32_t reported = -999;
            CHECK(NativeCollectionTyped_size(heap, (MCObject *)list, NULL, NULL, &reported) ==
                      NATIVE_ARRAY_OK &&
                  reported == size);
            MCObject *out = (MCObject *)sentinel;
            CHECK(default_to_array(heap, (MCObject *)list, requested, &out) == NATIVE_ARRAY_OK);
            NativeTypedObjectArray *returned = (NativeTypedObjectArray *)out;
            CHECK(returned->componentType == component &&
                  returned->length == (length >= size ? length : size));
            CHECK((length >= size) == (returned == requested));
            CHECK(MCObjectHeap_liveObjects(heap) == before + (length < size ? 1u : 0u));
            for (int i = 0; i < size; ++i)
                CHECK(returned->values[i] == values[i]);
            if (length >= size) {
                if (length > size)
                    CHECK(requested->values[size] == NULL);
                for (int i = size + 1; i < length; ++i)
                    CHECK(requested->values[i] == (MCObject *)sentinel);
            } else
                for (int i = 0; i < length; ++i)
                    CHECK(requested->values[i] == (MCObject *)sentinel);
            CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
            MCObjectHeap_free(heap);
        }
}

static void default_store_exception_prefix(void) {
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    NativeJavaClass *component = Vec4b_nativeClass(heap);
    CHECK(component && NativeJavaClass_literal(heap, &leafDescriptor));
    Vec4b *a = Vec4b_new(heap, 1, 2, 3, 4), *b = Vec4b_new(heap, 5, 6, 7, 8);
    MCObject *wrong = MCObjectHeap_alloc(heap, sizeof(MCObject), &leafClass);
    NativeReferenceList *list = NativeReferenceList_new(heap);
    CHECK(a && b && wrong && list);
    CHECK(NativeReferenceList_add(list, (MCObject *)a) && NativeReferenceList_add(list, wrong) &&
          NativeReferenceList_add(list, (MCObject *)a));
    NativeTypedObjectArray *requested = typed(heap, component, 4, (MCObject *)b);
    MCObject *out = wrong;
    size_t before = MCObjectHeap_liveObjects(heap);
    CHECK(default_to_array(heap, (MCObject *)list, requested, &out) == NATIVE_ARRAY_EXCEPTION);
    CHECK(out == wrong && requested->values[0] == (MCObject *)a &&
          requested->values[1] == (MCObject *)b && requested->values[2] == (MCObject *)b &&
          requested->values[3] == (MCObject *)b);
    CHECK(MCObjectHeap_liveObjects(heap) == before && !MCObjectHeap_failed(heap));
    NativeTypedObjectArray *shorter = typed(heap, component, 0, NULL);
    before = MCObjectHeap_liveObjects(heap);
    CHECK(default_to_array(heap, (MCObject *)list, shorter, &out) == NATIVE_ARRAY_EXCEPTION &&
          out == wrong);
    CHECK(MCObjectHeap_liveObjects(heap) == before + 1 && !MCObjectHeap_failed(heap));
    CHECK(!MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

static void linked_defaults_and_views(void) {
    for (int length = 0; length <= 5; ++length) {
        MCObjectHeap *heap = MCObjectHeap_new(65536);
        NativeJavaClass *component = Vec4b_nativeClass(heap);
        Vec4b *a = Vec4b_new(heap, 1, 2, 3, 4), *b = Vec4b_new(heap, 5, 6, 7, 8),
              *sentinel = Vec4b_new(heap, 9, 10, 11, 12);
        CHECK(component && a && b && sentinel);
        MCObject *values[3] = {(MCObject *)a, NULL, (MCObject *)a};
        NativeLinkedHashMap *map = map_values(heap, values, 3);
        NativeLinkedHashMapView *view = NativeLinkedHashMap_values(map);
        CHECK(view && view == NativeLinkedHashMap_values(map));
        CHECK(NativeLinkedHashMap_put(map, (MCObject *)NBTString_fromASCII(heap, "key-1"),
                                      (MCObject *)b));
        values[1] = (MCObject *)b;
        NativeTypedObjectArray *requested = typed(heap, component, length, (MCObject *)sentinel);
        int32_t count = 0;
        CHECK(NativeCollectionTyped_size(heap, (MCObject *)view, NULL, NULL, &count) ==
                  NATIVE_ARRAY_OK &&
              count == 3);
        MCObject *out = NULL;
        CHECK(default_to_array(heap, (MCObject *)view, requested, &out) == NATIVE_ARRAY_OK);
        NativeTypedObjectArray *returned = (NativeTypedObjectArray *)out;
        CHECK(returned->length == (length < 3 ? 3 : length) &&
              returned->componentType == component);
        CHECK((returned == requested) == (length >= 3));
        for (int i = 0; i < 3; ++i)
            CHECK(returned->values[i] == values[i]);
        if (length > 3)
            CHECK(requested->values[3] == NULL);
        if (length > 4)
            CHECK(requested->values[4] == (MCObject *)sentinel);
        NativeLinkedHashMapView *keys = NativeLinkedHashMap_keys(map);
        NativeObjectArray *objectArray = NativeObjectArray_new(heap, 4);
        CHECK(keys && objectArray);
        CHECK(default_to_array(heap, (MCObject *)keys, objectArray, &out) == NATIVE_ARRAY_OK &&
              out == (MCObject *)objectArray);
        for (int i = 0; i < 3; ++i) {
            char expected[16];
            snprintf(expected, sizeof(expected), "key-%d", i);
            CHECK(NBTString_equalsASCII((NBTString *)objectArray->values[i], expected));
        }
        CHECK(objectArray->values[3] == NULL && !MCObjectHeap_failed(heap) &&
              !MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
}

static void real_iterator_snapshot_sizes(void) {
    static const int cases[][3] = {{0, 5, 5}, {1, 2, 4}, {3, 1, 5}, {4, 1, 1},
                                   {3, 0, 0}, {7, 2, 2}, {8, 1, 1}};
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c) {
        int count = cases[c][0], callerLength = cases[c][1], workingLength = cases[c][2];
        MCObjectHeap *heap = MCObjectHeap_new(131072);
        NativeJavaClass *component = Vec4b_nativeClass(heap);
        Vec4b *a = Vec4b_new(heap, 1, 2, 3, 4), *b = Vec4b_new(heap, 5, 6, 7, 8),
              *sentinel = Vec4b_new(heap, 9, 10, 11, 12);
        CHECK(component && a && b && sentinel);
        MCObject *values[8] = {(MCObject *)a, (MCObject *)b, NULL,          (MCObject *)a,
                               (MCObject *)b, NULL,          (MCObject *)a, (MCObject *)b};
        NativeLinkedHashMap *map = map_values(heap, values, count);
        NativeTypedObjectArray *requested =
            typed(heap, component, callerLength, (MCObject *)sentinel);
        NativeTypedObjectArray *working =
            workingLength == callerLength ? requested : typed(heap, component, workingLength, NULL);
        NativeLinkedHashMapIterator *iterator =
            NativeLinkedHashMapIterator_fromView(NativeLinkedHashMap_values(map));
        CHECK(iterator);
        MCObject *out = (MCObject *)sentinel;
        CHECK(NativeCollectionTyped_fromLinkedIterator(heap, iterator, working, requested, &out) ==
              NATIVE_ARRAY_OK);
        NativeTypedObjectArray *returned = (NativeTypedObjectArray *)out;
        bool reuse = count <= workingLength && count <= callerLength;
        CHECK((returned == requested) == reuse);
        CHECK(returned->length == (reuse ? callerLength : count) &&
              returned->componentType == component);
        for (int i = 0; i < count; ++i)
            CHECK(returned->values[i] == values[i]);
        if (reuse && callerLength > count) {
            CHECK(requested->values[count] == NULL);
            for (int i = count + 1; i < callerLength; ++i)
                CHECK(requested->values[i] == (MCObject *)sentinel);
        }
        CHECK(!NativeLinkedHashMapIterator_hasNext(iterator) && !MCObjectHeap_failed(heap));
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
}

static void real_iterator_exception_and_live_replacement(void) {
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    NativeJavaClass *component = Vec4b_nativeClass(heap);
    CHECK(component && NativeJavaClass_literal(heap, &leafDescriptor));
    Vec4b *a = Vec4b_new(heap, 1, 2, 3, 4), *sentinel = Vec4b_new(heap, 5, 6, 7, 8);
    MCObject *wrong = MCObjectHeap_alloc(heap, sizeof(MCObject), &leafClass);
    MCObject *values[3] = {(MCObject *)a, wrong, (MCObject *)a};
    NativeLinkedHashMap *map = map_values(heap, values, 3);
    NativeLinkedHashMapView *view = NativeLinkedHashMap_values(map);
    NativeTypedObjectArray *requested = typed(heap, component, 4, (MCObject *)sentinel);
    NativeLinkedHashMapIterator *iterator = NativeLinkedHashMapIterator_fromView(view);
    MCObject *out = wrong;
    CHECK(iterator && NativeCollectionTyped_fromLinkedIterator(heap, iterator, requested, requested,
                                                               &out) == NATIVE_ARRAY_EXCEPTION);
    CHECK(out == wrong && requested->values[0] == (MCObject *)a &&
          requested->values[1] == (MCObject *)sentinel);
    CHECK(requested->values[2] == (MCObject *)sentinel &&
          requested->values[3] == (MCObject *)sentinel);
    MCObject *next = NULL;
    CHECK(NativeLinkedHashMapIterator_nextSource(iterator, &next) == NATIVE_LINKED_ITERATOR_OK &&
          next == (MCObject *)a);
    CHECK(!MCObjectHeap_failed(heap));
    iterator = NativeLinkedHashMapIterator_fromView(view);
    CHECK(iterator);
    CHECK(NativeLinkedHashMap_put(map, (MCObject *)NBTString_fromASCII(heap, "key-1"),
                                  (MCObject *)sentinel));
    CHECK(NativeCollectionTyped_fromLinkedIterator(heap, iterator, requested, requested, &out) ==
          NATIVE_ARRAY_OK);
    CHECK(requested->values[1] == (MCObject *)sentinel && out == (MCObject *)requested);
    iterator = NativeLinkedHashMapIterator_fromView(view);
    CHECK(iterator && NativeLinkedHashMap_put(map, (MCObject *)NBTString_fromASCII(heap, "new"),
                                              (MCObject *)a));
    out = wrong;
    CHECK(NativeCollectionTyped_fromLinkedIterator(heap, iterator, requested, requested, &out) ==
          NATIVE_ARRAY_EXCEPTION);
    CHECK(out == wrong && !MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

typedef struct CallbackOwner CallbackOwner;
typedef struct CallbackContext {
    MCObject object;
    CallbackOwner *owner;
    int identity;
} CallbackContext;
struct CallbackOwner {
    MCObject object;
    MCObject *context;
    CallbackContext *nextContext;
    MCObject *receiver;
    MCObject *returnValue;
    MCObject *extra;
    NativeTypedObjectArray *requested;
    MCObject *result;
    int mode, terminalStatus, events[8], eventCount, partial;
};
enum {
    CB_DELEGATE,
    CB_GROW_AND_CONTEXT,
    CB_RETURN_NULL,
    CB_RETURN_VALUE,
    CB_REPLACE_CONTEXT,
    CB_STATUS,
    CB_BAD_ENUM
};
static void callback_owner_trace(MCObject *object, MCObjectVisitor visit, void *context) {
    CallbackOwner *owner = (CallbackOwner *)object;
    owner->context = visit(owner->context, context);
    owner->nextContext = (CallbackContext *)visit((MCObject *)owner->nextContext, context);
    owner->receiver = visit(owner->receiver, context);
    owner->returnValue = visit(owner->returnValue, context);
    owner->extra = visit(owner->extra, context);
    owner->requested = (NativeTypedObjectArray *)visit((MCObject *)owner->requested, context);
    owner->result = visit(owner->result, context);
}
static void callback_context_trace(MCObject *object, MCObjectVisitor visit, void *context) {
    CallbackContext *value = (CallbackContext *)object;
    value->owner = (CallbackOwner *)visit((MCObject *)value->owner, context);
}
static const MCObjectClass callbackOwnerClass = {"test.CollectionCaller", MCObjectHeap_plainClone,
                                                 callback_owner_trace, NULL};
static const MCObjectClass callbackContextClass = {
    "test.CollectionContext", MCObjectHeap_plainClone, callback_context_trace, NULL};
static CallbackOwner *callback_fixture(MCObjectHeap *heap, MCObject *receiver) {
    CallbackOwner *owner =
        (CallbackOwner *)MCObjectHeap_alloc(heap, sizeof(*owner), &callbackOwnerClass);
    CallbackContext *first = (CallbackContext *)MCObjectHeap_alloc(heap, sizeof(*first),
                                                                   &callbackContextClass),
                    *second = (CallbackContext *)MCObjectHeap_alloc(heap, sizeof(*second),
                                                                    &callbackContextClass);
    CHECK(owner && first && second);
    owner->receiver = receiver;
    owner->context = (MCObject *)first;
    owner->nextContext = second;
    first->owner = owner;
    first->identity = 1;
    second->owner = owner;
    second->identity = 2;
    return owner;
}
static CallbackOwner *callback_event(MCObject *context, MCObject *receiver, int operation) {
    CHECK(context && context->klass == &callbackContextClass);
    CallbackContext *state = (CallbackContext *)context;
    CallbackOwner *owner = state->owner;
    CHECK(owner && receiver == owner->receiver && context == owner->context);
    CHECK(owner->eventCount < 8 && MCObjectHeap_hasBorrowers(context->heap));
    CHECK(!MCObjectHeap_collect(context->heap) && !MCObjectHeap_failed(context->heap));
    owner->events[owner->eventCount++] = operation * 10 + state->identity;
    ++owner->partial;
    MCObjectHeap_touch(context->heap);
    return owner;
}
static NativeArrayResult custom_size(MCObject *context, MCObject *receiver, int32_t *out) {
    CallbackOwner *owner = callback_event(context, receiver, 1);
    int32_t count = 0;
    NativeArrayResult result =
        NativeCollectionTyped_size(context->heap, receiver, NULL, NULL, &count);
    if (result != NATIVE_ARRAY_OK)
        return result;
    if (owner->mode == CB_GROW_AND_CONTEXT) {
        CHECK(NativeReferenceList_add((NativeReferenceList *)receiver, owner->extra));
        owner->context = (MCObject *)owner->nextContext;
    } else if (owner->mode == CB_REPLACE_CONTEXT)
        owner->context = owner->returnValue;
    *out = count;
    if (owner->mode == CB_BAD_ENUM)
        return (NativeArrayResult)99;
    if (owner->mode == CB_REPLACE_CONTEXT || owner->mode == CB_STATUS)
        return (NativeArrayResult)owner->terminalStatus;
    return NATIVE_ARRAY_OK;
}
static NativeArrayResult custom_to_array(MCObject *context, MCObject *receiver,
                                         NativeTypedObjectArray *requested, MCObject **out) {
    CallbackOwner *owner = callback_event(context, receiver, 2);
    if (owner->mode == CB_RETURN_NULL) {
        *out = NULL;
        return NATIVE_ARRAY_OK;
    }
    if (owner->mode == CB_RETURN_VALUE) {
        *out = owner->returnValue;
        return NATIVE_ARRAY_OK;
    }
    if (owner->mode == CB_REPLACE_CONTEXT || owner->mode == CB_STATUS ||
        owner->mode == CB_BAD_ENUM) {
        if (requested && requested->length)
            CHECK(NativeTypedObjectArray_set(requested, 0, owner->extra) == NATIVE_ARRAY_OK);
        *out = (MCObject *)requested;
        if (owner->mode == CB_REPLACE_CONTEXT)
            owner->context = owner->returnValue;
        return owner->mode == CB_BAD_ENUM ? (NativeArrayResult)99
                                          : (NativeArrayResult)owner->terminalStatus;
    }
    return NativeCollectionTyped_toArray(context->heap, receiver, NULL, NULL, requested, out);
}
static const NativeCollectionTypedMethods callbacks = {custom_size, custom_to_array};
static NativeArrayResult null_context_size(MCObject *context, MCObject *receiver, int32_t *out) {
    CHECK(context == NULL);
    return NativeCollectionTyped_size(receiver->heap, receiver, NULL, NULL, out);
}
static NativeArrayResult null_context_array(MCObject *context, MCObject *receiver,
                                            NativeTypedObjectArray *requested, MCObject **out) {
    CHECK(context == NULL);
    return NativeCollectionTyped_toArray(receiver->heap, receiver, NULL, NULL, requested, out);
}
static const NativeCollectionTypedMethods nullContextCallbacks = {null_context_size,
                                                                  null_context_array};

static void callback_order_and_outputs(void) {
    MCObjectHeap *heap = MCObjectHeap_new(131072);
    NativeJavaClass *component = Vec4b_nativeClass(heap);
    Vec4b *a = Vec4b_new(heap, 1, 2, 3, 4), *b = Vec4b_new(heap, 5, 6, 7, 8);
    NativeReferenceList *list = NativeReferenceList_new(heap);
    CHECK(component && a && b && list && NativeReferenceList_add(list, (MCObject *)a));
    CallbackOwner *owner = callback_fixture(heap, (MCObject *)list);
    owner->extra = (MCObject *)b;
    owner->mode = CB_GROW_AND_CONTEXT;
    int32_t reported = -1;
    CHECK(NativeCollectionTyped_size(heap, owner->receiver, &callbacks, &owner->context,
                                     &reported) == NATIVE_ARRAY_OK &&
          reported == 1);
    CHECK(owner->context == (MCObject *)owner->nextContext && list->size == 2 &&
          owner->partial == 1);
    owner->requested = typed(heap, component, reported, (MCObject *)b);
    MCObject *out = (MCObject *)a;
    CHECK(NativeCollectionTyped_toArray(heap, owner->receiver, &callbacks, &owner->context,
                                        owner->requested, &out) == NATIVE_ARRAY_OK);
    CHECK(owner->eventCount == 2 && owner->events[0] == 11 && owner->events[1] == 22 &&
          owner->partial == 2);
    CHECK(out != (MCObject *)owner->requested && ((NativeTypedObjectArray *)out)->length == 2);
    CHECK(((NativeTypedObjectArray *)out)->values[0] == (MCObject *)a &&
          ((NativeTypedObjectArray *)out)->values[1] == (MCObject *)b);
    CHECK(owner->requested->values[0] == (MCObject *)b);
    owner->mode = CB_RETURN_NULL;
    CHECK(NativeCollectionTyped_toArray(heap, owner->receiver, &callbacks, &owner->context, NULL,
                                        &out) == NATIVE_ARRAY_OK &&
          out == NULL);
    owner->mode = CB_RETURN_VALUE;
    owner->returnValue = (MCObject *)a;
    CHECK(NativeCollectionTyped_toArray(heap, owner->receiver, &callbacks, &owner->context,
                                        owner->requested, &out) == NATIVE_ARRAY_OK &&
          out == (MCObject *)a);
    CHECK(!NativeTypedObjectArray_isInstance(out) && !MCObjectHeap_failed(heap));
    NativeObjectArray *objectArray = NativeObjectArray_new(heap, 2);
    CHECK(objectArray && NativeObjectArray_set(objectArray, 0, (MCObject *)a));
    owner->returnValue = (MCObject *)objectArray;
    CHECK(NativeCollectionTyped_toArray(heap, owner->receiver, &callbacks, &owner->context,
                                        owner->requested, &out) == NATIVE_ARRAY_OK &&
          out == (MCObject *)objectArray);
    bool assignable = true;
    CHECK(NativeTypedObjectArray_isAssignableTo((NativeTypedObjectArray *)out, component,
                                                &assignable) &&
          !assignable);
    CHECK(!MCObjectHeap_failed(heap));
    MCObject *nullContext = NULL;
    CHECK(NativeCollectionTyped_size(heap, (MCObject *)list, &nullContextCallbacks, &nullContext,
                                     &reported) == NATIVE_ARRAY_OK &&
          reported == 2);
    CHECK(NativeCollectionTyped_toArray(heap, (MCObject *)list, &nullContextCallbacks, NULL,
                                        objectArray, &out) == NATIVE_ARRAY_OK &&
          out == (MCObject *)objectArray);
    CHECK(!MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

static void terminal_context_and_statuses(void) {
    for (int operation = 0; operation < 2; ++operation)
        for (int status = 0; status < 3; ++status) {
            MCObjectHeap *heap = MCObjectHeap_new(65536), *foreign = MCObjectHeap_new(4096);
            NativeJavaClass *component = Vec4b_nativeClass(heap);
            Vec4b *a = Vec4b_new(heap, 1, 2, 3, 4), *b = Vec4b_new(heap, 5, 6, 7, 8);
            NativeReferenceList *list = NativeReferenceList_new(heap);
            CHECK(component && a && b && list && NativeReferenceList_add(list, (MCObject *)a));
            CallbackOwner *owner = callback_fixture(heap, (MCObject *)list);
            owner->mode = CB_REPLACE_CONTEXT;
            owner->terminalStatus = status;
            owner->extra = (MCObject *)a;
            owner->returnValue = MCObjectHeap_alloc(foreign, sizeof(MCObject), &leafClass);
            CHECK(owner->returnValue);
            owner->requested = typed(heap, component, 2, (MCObject *)b);
            MCObject *out = (MCObject *)b;
            int32_t count = 73;
            NativeArrayResult result =
                operation ? NativeCollectionTyped_toArray(heap, owner->receiver, &callbacks,
                                                          &owner->context, owner->requested, &out)
                          : NativeCollectionTyped_size(heap, owner->receiver, &callbacks,
                                                       &owner->context, &count);
            CHECK(result == NATIVE_ARRAY_FAILURE && MCObjectHeap_failed(heap) &&
                  !MCObjectHeap_failed(foreign));
            CHECK(out == (MCObject *)b && count == 73 && owner->partial == 1 &&
                  owner->context == owner->returnValue);
            if (operation)
                CHECK(owner->requested->values[0] == (MCObject *)a &&
                      owner->requested->values[1] == (MCObject *)b);
            CHECK(!MCObjectHeap_hasBorrowers(heap));
            MCObjectHeap_free(heap);
            MCObjectHeap_free(foreign);
        }
    for (int operation = 0; operation < 2; ++operation)
        for (int status = 0; status < 4; ++status) {
            MCObjectHeap *heap = MCObjectHeap_new(65536);
            NativeJavaClass *component = Vec4b_nativeClass(heap);
            Vec4b *a = Vec4b_new(heap, 1, 2, 3, 4), *b = Vec4b_new(heap, 5, 6, 7, 8);
            NativeReferenceList *list = NativeReferenceList_new(heap);
            CHECK(component && a && b && list && NativeReferenceList_add(list, (MCObject *)a));
            CallbackOwner *owner = callback_fixture(heap, (MCObject *)list);
            owner->mode = status == 3 ? CB_BAD_ENUM : CB_STATUS;
            owner->terminalStatus = status;
            owner->extra = (MCObject *)a;
            owner->requested = typed(heap, component, 2, (MCObject *)b);
            MCObject *out = (MCObject *)b;
            int32_t count = 73;
            NativeArrayResult result =
                operation ? NativeCollectionTyped_toArray(heap, owner->receiver, &callbacks,
                                                          &owner->context, owner->requested, &out)
                          : NativeCollectionTyped_size(heap, owner->receiver, &callbacks,
                                                       &owner->context, &count);
            CHECK(result == (status >= 2 ? NATIVE_ARRAY_FAILURE : (NativeArrayResult)status));
            CHECK(MCObjectHeap_failed(heap) == (status >= 2) && owner->partial == 1);
            if (status == 0)
                CHECK(operation ? out == (MCObject *)owner->requested : count == 1);
            else
                CHECK(out == (MCObject *)b && count == 73);
            if (operation)
                CHECK(owner->requested->values[0] == (MCObject *)a);
            CHECK(!MCObjectHeap_hasBorrowers(heap));
            MCObjectHeap_free(heap);
        }
}

static void malformed_known_array_result(void) {
    for (int which = 0; which < 2; ++which) {
        MCObjectHeap *heap = MCObjectHeap_new(65536);
        NativeReferenceList *list = NativeReferenceList_new(heap);
        CallbackOwner *owner = callback_fixture(heap, (MCObject *)list);
        NativeObjectArray *model = NativeObjectArray_new(heap, 1);
        CHECK(list && model);
        if (which == 0) {
            owner->returnValue = MCObjectHeap_alloc(heap, sizeof(MCObject), model->object.klass);
        } else {
            model->length = 2;
            owner->returnValue = (MCObject *)model;
        }
        CHECK(owner->returnValue && NativeObjectArray_isRuntimeClass(owner->returnValue) &&
              !NativeObjectArray_isInstance(owner->returnValue));
        owner->mode = CB_RETURN_VALUE;
        MCObject *out = (MCObject *)list;
        CHECK(NativeCollectionTyped_toArray(heap, owner->receiver, &callbacks, &owner->context,
                                            NULL, &out) == NATIVE_ARRAY_FAILURE);
        CHECK(out == (MCObject *)list && MCObjectHeap_failed(heap) && owner->partial == 1);
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
}

static void untracked_callback_edges(void) {
    /* A readable native header with matching heap/class is not a registered
       graph owner. The padding makes an old unchecked metadata read readable;
       the assertion is about membership, not the allocator's metadata layout. */
    for (int which = 0; which < 7; ++which) {
        MCObjectHeap *heap = MCObjectHeap_new(65536);
        NativeReferenceList *list = NativeReferenceList_new(heap);
        CallbackOwner *owner = callback_fixture(heap, (MCObject *)list);
        struct {
            max_align_t padding[32];
            MCObject object;
        } untracked;
        memset(&untracked, 0x7f, sizeof(untracked));
        untracked.object = (MCObject){heap, &leafClass};
        owner->returnValue = &untracked.object;
        owner->mode = which == 0 ? CB_RETURN_VALUE : CB_REPLACE_CONTEXT;
        owner->terminalStatus = which == 0 ? 0 : (which - 1) % 3;
        MCObject *out = (MCObject *)list;
        int32_t count = 77;
        NativeArrayResult result =
            which > 3 ? NativeCollectionTyped_size(heap, owner->receiver, &callbacks,
                                                   &owner->context, &count)
                      : NativeCollectionTyped_toArray(heap, owner->receiver, &callbacks,
                                                      &owner->context, NULL, &out);
        CHECK(result == NATIVE_ARRAY_FAILURE && MCObjectHeap_failed(heap));
        CHECK(out == (MCObject *)list && count == 77 && owner->partial == 1);
        if (which)
            CHECK(owner->context == &untracked.object);
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
}

static void invalid_array_component(void) {
    for (int which = 0; which < 3; ++which) {
        MCObjectHeap *heap = MCObjectHeap_new(65536), *foreign = MCObjectHeap_new(65536);
        NativeReferenceList *list = NativeReferenceList_new(heap);
        CallbackOwner *owner = callback_fixture(heap, (MCObject *)list);
        NativeJavaClass *model = Vec4b_nativeClass(heap), *other = Vec4b_nativeClass(foreign);
        NativeObjectArray *returned = NativeObjectArray_new(heap, 1);
        struct {
            max_align_t padding[32];
            NativeJavaClass object;
        } untracked;
        memset(&untracked, 0x7f, sizeof(untracked));
        untracked.object = *model;
        CHECK(list && model && other && returned);
        returned->componentType = which == 0   ? other
                                  : which == 1 ? &untracked.object
                                               : (NativeJavaClass *)MCObjectHeap_alloc(
                                                     heap, sizeof(MCObject), model->object.klass);
        CHECK(returned->componentType);
        owner->mode = CB_RETURN_VALUE;
        owner->returnValue = (MCObject *)returned;
        MCObject *out = (MCObject *)list;
        CHECK(NativeCollectionTyped_toArray(heap, owner->receiver, &callbacks, &owner->context,
                                            NULL, &out) == NATIVE_ARRAY_FAILURE);
        CHECK(out == (MCObject *)list && owner->partial == 1 && MCObjectHeap_failed(heap) &&
              !MCObjectHeap_failed(foreign));
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
        MCObjectHeap_free(foreign);
    }
}

static void clone_adopt_gc_aliases(void) {
    MCObjectHeap *heap = MCObjectHeap_new(131072);
    NativeJavaClass *component = Vec4b_nativeClass(heap);
    CHECK(component);
    size_t baseline = MCObjectHeap_liveObjects(heap);
    Vec4b *a = Vec4b_new(heap, 1, 2, 3, 4), *sentinel = Vec4b_new(heap, 5, 6, 7, 8);
    CHECK(a && sentinel);
    MCObject *values[3] = {(MCObject *)a, NULL, (MCObject *)a};
    NativeLinkedHashMap *map = map_values(heap, values, 3);
    NativeLinkedHashMapView *view = NativeLinkedHashMap_values(map);
    CallbackOwner *owner = callback_fixture(heap, (MCObject *)view);
    owner->extra = (MCObject *)a;
    owner->requested = typed(heap, component, 4, (MCObject *)sentinel);
    CHECK(NativeCollectionTyped_toArray(heap, owner->receiver, &callbacks, &owner->context,
                                        owner->requested, &owner->result) == NATIVE_ARRAY_OK);
    CHECK(owner->result == (MCObject *)owner->requested &&
          owner->requested->values[0] == owner->extra &&
          owner->requested->values[2] == owner->extra && owner->requested->values[1] == NULL &&
          owner->requested->values[3] == NULL);
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)owner) && MCObjectHeap_collect(heap));
    MCObjectHeap *clone = MCObjectHeap_clone(heap);
    MCObjectRoot cloneRoot = {0};
    CHECK(clone && MCObjectRoot_rebind(&cloneRoot, clone, &root));
    CallbackOwner *copy = (CallbackOwner *)MCObjectRoot_get(&cloneRoot);
    CHECK(copy != owner && copy->requested != owner->requested && copy->extra != owner->extra);
    CHECK(((CallbackContext *)copy->context)->owner == copy && copy->nextContext->owner == copy);
    CHECK(copy->result == (MCObject *)copy->requested &&
          copy->requested->values[0] == copy->extra && copy->requested->values[2] == copy->extra &&
          copy->requested->values[1] == NULL);
    CHECK(copy->requested->componentType == Vec4b_nativeClass(clone));
    CHECK(NativeCollectionTyped_toArray(clone, copy->receiver, &callbacks, &copy->context,
                                        copy->requested, &copy->result) == NATIVE_ARRAY_OK);
    CHECK(copy->partial == 2 && owner->partial == 1 && copy->requested->values[0] == copy->extra);
    CHECK(MCObjectHeap_collect(clone) && MCObjectHeap_canAdopt(heap, clone) &&
          MCObjectHeap_adopt(heap, clone));
    owner = (CallbackOwner *)MCObjectRoot_get(&root);
    CHECK(owner->partial == 2 && owner->result == (MCObject *)owner->requested &&
          ((CallbackContext *)owner->context)->owner == owner &&
          owner->requested->values[2] == owner->extra);
    CHECK(NativeCollectionTyped_toArray(heap, owner->receiver, &callbacks, &owner->context,
                                        owner->requested, &owner->result) == NATIVE_ARRAY_OK);
    MCObjectHeap_free(clone);
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == baseline);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

static NativeLinkedHashMapView *oom_setup(MCObjectHeap *heap, NativeTypedObjectArray **requested) {
    NativeJavaClass *component = Vec4b_nativeClass(heap);
    Vec4b *a = Vec4b_new(heap, 1, 2, 3, 4);
    CHECK(component && a);
    MCObject *values[1] = {(MCObject *)a};
    NativeLinkedHashMapView *view = NativeLinkedHashMap_values(map_values(heap, values, 1));
    *requested = typed(heap, component, 0, NULL);
    CHECK(view);
    return view;
}
static void allocation_failure_order(void) {
    MCObjectHeap *probe = MCObjectHeap_new(65536);
    NativeTypedObjectArray *requested = NULL;
    CHECK(oom_setup(probe, &requested));
    size_t setupBytes = MCObjectHeap_liveBytes(probe),
           setupObjects = MCObjectHeap_liveObjects(probe);
    size_t arrayBytes = sizeof(NativeObjectArray) + sizeof(MCObject *);
    MCObjectHeap_free(probe);
    for (int phase = 0; phase < 2; ++phase) {
        MCObjectHeap *heap = MCObjectHeap_new(setupBytes + arrayBytes - (phase ? 0u : 1u));
        NativeLinkedHashMapView *view = oom_setup(heap, &requested);
        CHECK(MCObjectHeap_liveBytes(heap) == setupBytes &&
              MCObjectHeap_liveObjects(heap) == setupObjects);
        MCObject *out = (MCObject *)requested;
        CHECK(default_to_array(heap, (MCObject *)view, requested, &out) == NATIVE_ARRAY_FAILURE);
        CHECK(out == (MCObject *)requested && requested->length == 0 && MCObjectHeap_failed(heap));
        /* Initial component-typed array failure does not create an iterator;
           the next budget succeeds at that array, then fails at iterator. */
        CHECK(MCObjectHeap_liveObjects(heap) == setupObjects + (phase ? 1u : 0u));
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
}

static void growth_oom_preserves_caller_prefix(void) {
    size_t setupBytes = 0, setupObjects = 0;
    for (int phase = -1; phase < 2; ++phase) {
        size_t growthBytes = sizeof(NativeObjectArray) + 2 * sizeof(MCObject *);
        MCObjectHeap *heap =
            MCObjectHeap_new(phase < 0 ? 65536 : setupBytes + growthBytes - (phase ? 0u : 1u));
        NativeJavaClass *component = Vec4b_nativeClass(heap);
        Vec4b *value = Vec4b_new(heap, 1, 2, 3, 4), *sentinel = Vec4b_new(heap, 5, 6, 7, 8);
        CHECK(component && value && sentinel);
        MCObject *values[4] = {(MCObject *)value, NULL, (MCObject *)value, (MCObject *)value};
        NativeLinkedHashMap *map = map_values(heap, values, 4);
        NativeTypedObjectArray *requested = typed(heap, component, 1, (MCObject *)sentinel);
        NativeLinkedHashMapIterator *iterator =
            NativeLinkedHashMapIterator_fromView(NativeLinkedHashMap_values(map));
        CHECK(iterator);
        if (phase < 0) {
            setupBytes = MCObjectHeap_liveBytes(heap);
            setupObjects = MCObjectHeap_liveObjects(heap);
        } else {
            CHECK(MCObjectHeap_liveBytes(heap) == setupBytes);
            MCObject *out = (MCObject *)sentinel;
            CHECK(NativeCollectionTyped_fromLinkedIterator(heap, iterator, requested, requested,
                                                           &out) == NATIVE_ARRAY_FAILURE);
            CHECK(out == (MCObject *)sentinel && requested->values[0] == (MCObject *)value &&
                  MCObjectHeap_failed(heap));
            CHECK(MCObjectHeap_liveObjects(heap) == setupObjects + (phase ? 1u : 0u));
            CHECK(!MCObjectHeap_hasBorrowers(heap));
        }
        MCObjectHeap_free(heap);
    }
}

static void nullable_and_native_failures(void) {
    MCObjectHeap *heap = MCObjectHeap_new(65536);
    NativeReferenceList *list = NativeReferenceList_new(heap);
    CallbackOwner *owner = callback_fixture(heap, (MCObject *)list);
    int32_t size = 77;
    MCObject *out = (MCObject *)list;
    CHECK(NativeCollectionTyped_size(heap, NULL, &callbacks, &owner->context, &size) ==
              NATIVE_ARRAY_EXCEPTION &&
          size == 77);
    CHECK(NativeCollectionTyped_toArray(heap, NULL, &callbacks, &owner->context, NULL, &out) ==
              NATIVE_ARRAY_EXCEPTION &&
          out == (MCObject *)list);
    CHECK(owner->eventCount == 0 && !MCObjectHeap_failed(heap));
    size_t before = MCObjectHeap_liveObjects(heap);
    CHECK(default_to_array(heap, (MCObject *)list, NULL, &out) == NATIVE_ARRAY_EXCEPTION &&
          out == (MCObject *)list);
    NativeLinkedHashMapView *empty = NativeLinkedHashMap_values(map_values(heap, NULL, 0));
    CHECK(empty);
    before = MCObjectHeap_liveObjects(heap);
    CHECK(default_to_array(heap, (MCObject *)empty, NULL, &out) == NATIVE_ARRAY_EXCEPTION &&
          out == (MCObject *)list);
    CHECK(MCObjectHeap_liveObjects(heap) == before && !MCObjectHeap_failed(heap));
    CHECK(NativeCollectionTyped_fromLinkedIterator(heap, NULL, NULL, NULL, &out) ==
          NATIVE_ARRAY_EXCEPTION);
    MCObjectHeap_free(heap);

    for (int which = 0; which < 8; ++which) {
        heap = MCObjectHeap_new(65536);
        MCObjectHeap *foreign = MCObjectHeap_new(65536);
        NativeJavaClass *component = Vec4b_nativeClass(heap),
                        *foreignComponent = Vec4b_nativeClass(foreign);
        Vec4b *a = Vec4b_new(heap, 1, 2, 3, 4);
        list = NativeReferenceList_new(heap);
        CHECK(component && foreignComponent && a && list);
        owner = callback_fixture(heap, (MCObject *)list);
        NativeTypedObjectArray *requested = typed(heap, component, 1, (MCObject *)a),
                               *foreignArray = typed(foreign, foreignComponent, 1, NULL);
        MCObject *foreignLeaf = MCObjectHeap_alloc(foreign, sizeof(MCObject), &leafClass);
        out = (MCObject *)a;
        size = 77;
        static const NativeCollectionTypedMethods missing = {NULL, NULL};
        NativeArrayResult result = NATIVE_ARRAY_OK;
        switch (which) {
        case 0:
            result = NativeCollectionTyped_size(heap, (MCObject *)a, NULL, NULL, &size);
            break;
        case 1:
            result =
                NativeCollectionTyped_toArray(heap, (MCObject *)a, NULL, NULL, requested, &out);
            break;
        case 2:
            result = NativeCollectionTyped_size(heap, (MCObject *)list, &missing, &owner->context,
                                                &size);
            break;
        case 3:
            result = NativeCollectionTyped_toArray(heap, (MCObject *)list, &missing,
                                                   &owner->context, requested, &out);
            break;
        case 4:
            result = NativeCollectionTyped_toArray(heap, (MCObject *)list, &callbacks,
                                                   &owner->context, foreignArray, &out);
            break;
        case 5:
            result =
                NativeCollectionTyped_size(heap, foreignLeaf, &callbacks, &owner->context, &size);
            break;
        case 6:
            owner->context = foreignLeaf;
            result = NativeCollectionTyped_size(heap, (MCObject *)list, &callbacks, &owner->context,
                                                &size);
            break;
        case 7:
            owner->mode = CB_RETURN_VALUE;
            owner->returnValue = foreignLeaf;
            result = NativeCollectionTyped_toArray(heap, (MCObject *)list, &callbacks,
                                                   &owner->context, requested, &out);
            break;
        }
        CHECK(result == NATIVE_ARRAY_FAILURE && MCObjectHeap_failed(heap) &&
              !MCObjectHeap_failed(foreign));
        CHECK(out == (MCObject *)a && size == 77 && requested->values[0] == (MCObject *)a);
        CHECK(owner->eventCount == (which == 7 ? 1 : 0) && !MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
        MCObjectHeap_free(foreign);
    }
}
#endif

int main(void) {
    real_list_caller_reuse();
#ifndef NATIVE_TYPED_COLLECTION_PRIOR_RED
    default_list_lengths();
    default_store_exception_prefix();
    linked_defaults_and_views();
    real_iterator_snapshot_sizes();
    real_iterator_exception_and_live_replacement();
    callback_order_and_outputs();
    terminal_context_and_statuses();
    malformed_known_array_result();
    untracked_callback_edges();
    invalid_array_component();
    clone_adopt_gc_aliases();
    allocation_failure_order();
    growth_oom_preserves_caller_prefix();
    nullable_and_native_failures();
#endif
    printf("native typed collection: %u checks passed\n", checks);
    return 0;
}
