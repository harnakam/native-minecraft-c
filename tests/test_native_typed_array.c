#include "client/entity/EntityPlayerSP.h"
#include "nbt/NBTString.h"
#include "util/NativeReferenceList.h"
#include "util/NativeTypedObjectArray.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "typed array check %u line %d: %s\n", checks, __LINE__, #x);           \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static const MCObjectClass aClass = {"fixture.A", MCObjectHeap_plainClone, NULL, NULL};
static const MCObjectClass bClass = {"fixture.B", MCObjectHeap_plainClone, NULL, NULL};
static bool is_a(const MCObject *o) {
    return o && o->klass == &aClass && MCObjectHeap_objectSize(o) >= sizeof(MCObject);
}
static bool is_b(const MCObject *o) {
    return o && o->klass == &bClass && MCObjectHeap_objectSize(o) >= sizeof(MCObject);
}
static const NativeJavaClassDescriptor *const parents[] = {&NativeJavaClass_ObjectClass};
static const NativeJavaClassDescriptor aType = {"fixture.A", parents, 1, is_a};
static const NativeJavaClassDescriptor bType = {"fixture.B", parents, 1, is_b};
static void stores_and_covariance(void) {
    MCObjectHeap *h = MCObjectHeap_new(1000000);
    CHECK(h);
    NativeJavaClass *type = NativeJavaClass_literal(h, &aType),
                    *otherType = NativeJavaClass_literal(h, &bType),
                    *objectType = NativeJavaClass_Object(h);
    CHECK(type && otherType && objectType);
    MCObject *a = MCObjectHeap_alloc(h, sizeof(MCObject), &aClass),
             *b = MCObjectHeap_alloc(h, sizeof(MCObject), &bClass);
    CHECK(a && b);
    NativeTypedObjectArray *array = NULL;
    CHECK(NativeTypedObjectArray_new(h, type, 3, &array) == NATIVE_ARRAY_OK && array);
    CHECK(NativeTypedObjectArray_isInstance((MCObject *)array) &&
          NativeObjectArray_isInstance((MCObject *)array));
    CHECK(_Generic(array, NativeObjectArray *: true, default: false));
    CHECK(array->componentType == type && array->length == 3 && !array->values[0]);
    CHECK(NativeTypedObjectArray_set(array, 0, a) == NATIVE_ARRAY_OK);
    CHECK(NativeTypedObjectArray_set(array, 1, a) == NATIVE_ARRAY_OK);
    CHECK(NativeTypedObjectArray_set(array, 2, NULL) == NATIVE_ARRAY_OK);
    CHECK(NativeTypedObjectArray_set(array, 1, b) == NATIVE_ARRAY_EXCEPTION &&
          !MCObjectHeap_failed(h) && array->values[1] == a);
    MCObject *out = b;
    CHECK(NativeTypedObjectArray_get(array, 0, &out) == NATIVE_ARRAY_OK && out == a);
    CHECK(NativeTypedObjectArray_get(array, -1, &out) == NATIVE_ARRAY_EXCEPTION && out == a &&
          !MCObjectHeap_failed(h));
    CHECK(NativeTypedObjectArray_set(array, INT32_MAX, b) == NATIVE_ARRAY_EXCEPTION &&
          !MCObjectHeap_failed(h));
    bool compatible = false;
    CHECK(NativeTypedObjectArray_isAssignableTo(array, objectType, &compatible) && compatible);
    CHECK(NativeTypedObjectArray_isAssignableTo(array, type, &compatible) && compatible);
    CHECK(NativeTypedObjectArray_isAssignableTo(array, otherType, &compatible) && !compatible);
    NativeReferenceList *list = NativeReferenceList_new(h);
    CHECK(list);
    bool changed = false;
    CHECK(NativeReferenceList_addAllArray(list, array, &changed) && changed);
    CHECK(NativeReferenceList_size(list) == 3 && NativeReferenceList_get(list, 0) == a &&
          NativeReferenceList_get(list, 1) == a && !NativeReferenceList_get(list, 2));
    NativeObjectArray *generic = NativeObjectArray_new(h, 1);
    CHECK(generic && !generic->componentType);
    CHECK(NativeObjectArray_set(generic, 0, b));
    CHECK(NativeTypedObjectArray_isAssignableTo(generic, type, &compatible) && !compatible);
    CHECK(NativeTypedObjectArray_isAssignableTo(generic, objectType, &compatible) && compatible);
    CHECK(NativeTypedObjectArray_new(h, objectType, 2, &array) == NATIVE_ARRAY_OK);
    NBTString *unknown = NBTString_fromASCII(h, "no Class fact needed for Object");
    CHECK(unknown);
    CHECK(NativeTypedObjectArray_set(array, 0, (MCObject *)unknown) == NATIVE_ARRAY_OK);
    CHECK(NativeTypedObjectArray_set(array, 1, (MCObject *)array) == NATIVE_ARRAY_OK);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void result_and_lifetime(void) {
    MCObjectHeap *h = MCObjectHeap_new(1000000);
    CHECK(h);
    NativeJavaClass *type = NativeJavaClass_literal(h, &aType);
    CHECK(type);
    NativeTypedObjectArray *array = NULL;
    CHECK(NativeTypedObjectArray_new(h, type, 4, &array) == NATIVE_ARRAY_OK);
    NativeTypedObjectArray *untouched = array;
    CHECK(NativeTypedObjectArray_new(h, type, -1, &untouched) == NATIVE_ARRAY_EXCEPTION &&
          untouched == array && !MCObjectHeap_failed(h));
    MCObject *a = MCObjectHeap_alloc(h, sizeof(MCObject), &aClass);
    CHECK(a);
    CHECK(NativeTypedObjectArray_set(array, 0, a) == NATIVE_ARRAY_OK &&
          NativeTypedObjectArray_set(array, 2, a) == NATIVE_ARRAY_OK);
    MCObjectRoot root = {0}, objectRoot = {0};
    CHECK(MCObjectRoot_init(&root, h, (MCObject *)array) && MCObjectRoot_init(&objectRoot, h, a));
    CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *branch = MCObjectHeap_clone(h);
    CHECK(branch);
    MCObjectRoot br = {0}, bo = {0};
    CHECK(MCObjectRoot_rebind(&br, branch, &root) && MCObjectRoot_rebind(&bo, branch, &objectRoot));
    NativeTypedObjectArray *copy = (NativeTypedObjectArray *)MCObjectRoot_get(&br);
    CHECK(copy != array && copy->componentType != type &&
          copy->componentType == NativeJavaClass_literal(branch, &aType));
    CHECK(copy->values[0] == MCObjectRoot_get(&bo) && copy->values[2] == copy->values[0] &&
          !copy->values[1]);
    CHECK(MCObjectHeap_canAdopt(h, branch) && MCObjectHeap_adopt(h, branch));
    MCObjectHeap_free(branch);
    array = (NativeTypedObjectArray *)MCObjectRoot_get(&root);
    CHECK(NativeTypedObjectArray_isInstance((MCObject *)array) &&
          array->componentType == NativeJavaClass_literal(h, &aType));
    CHECK(array->values[0] == MCObjectRoot_get(&objectRoot) &&
          array->values[2] == array->values[0]);
    MCObjectRoot_drop(&objectRoot);
    CHECK(MCObjectHeap_collect(h) && array->values[0]);
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(h));
    MCObjectHeap_free(h);
}
static void native_failures(void) {
    MCObjectHeap *h = MCObjectHeap_new(100000), *f = MCObjectHeap_new(100000);
    CHECK(h && f);
    NativeJavaClass *type = NativeJavaClass_literal(h, &aType);
    NativeTypedObjectArray *a = NULL;
    CHECK(NativeTypedObjectArray_new(h, type, 1, &a) == NATIVE_ARRAY_OK);
    MCObject *foreign = MCObjectHeap_alloc(f, sizeof(MCObject), &aClass);
    CHECK(foreign);
    CHECK(NativeTypedObjectArray_set(a, 0, foreign) == NATIVE_ARRAY_FAILURE &&
          MCObjectHeap_failed(h) && !MCObjectHeap_failed(f) && !a->values[0]);
    MCObjectHeap_free(h);
    MCObjectHeap_free(f);
    h = MCObjectHeap_new(100000);
    CHECK(h);
    type = NativeJavaClass_literal(h, &aType);
    CHECK(NativeTypedObjectArray_new(h, type, 1, &a) == NATIVE_ARRAY_OK);
    MCObject *unknown = MCObjectHeap_alloc(h, sizeof(MCObject), &bClass);
    CHECK(unknown);
    CHECK(NativeTypedObjectArray_set(a, 0, unknown) == NATIVE_ARRAY_FAILURE &&
          MCObjectHeap_failed(h) && !a->values[0]);
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(100000);
    CHECK(h);
    type = NativeJavaClass_literal(h, &aType);
    CHECK(NativeTypedObjectArray_new(h, type, 1, &a) == NATIVE_ARRAY_OK);
    MCObjectHeap *limited = MCObjectHeap_new(100000);
    CHECK(limited);
    type = NativeJavaClass_literal(limited, &aType);
    CHECK(type);
    NativeTypedObjectArray *sentinel = a;
    CHECK(NativeTypedObjectArray_new(limited, type, INT32_MAX, &sentinel) == NATIVE_ARRAY_FAILURE &&
          sentinel == a && MCObjectHeap_failed(limited));
    MCObjectHeap_free(limited);
    MCObjectHeap_free(h);
    MCObject *out = NULL;
    CHECK(NativeTypedObjectArray_get(NULL, 0, &out) == NATIVE_ARRAY_EXCEPTION);
    CHECK(NativeTypedObjectArray_set(NULL, 0, NULL) == NATIVE_ARRAY_EXCEPTION);
}
static void rejected_store_has_no_graph_mutation(void) {
    MCObjectHeap *h = MCObjectHeap_new(100000);
    CHECK(h);
    NativeJavaClass *a = NativeJavaClass_literal(h, &aType),
                    *b = NativeJavaClass_literal(h, &bType);
    CHECK(a && b);
    NativeTypedObjectArray *array = NULL;
    CHECK(NativeTypedObjectArray_new(h, a, 1, &array) == NATIVE_ARRAY_OK);
    MCObject *wrong = MCObjectHeap_alloc(h, sizeof(MCObject), &bClass);
    CHECK(wrong);
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, h, (MCObject *)array));
    MCObjectHeap *branch = MCObjectHeap_clone(h);
    CHECK(branch);
    CHECK(NativeTypedObjectArray_set(array, 0, wrong) == NATIVE_ARRAY_EXCEPTION &&
          !array->values[0] && !MCObjectHeap_failed(h));
    CHECK(MCObjectHeap_canAdopt(h, branch));
    MCObjectHeap_free(branch);
    MCObjectRoot_drop(&root);
    MCObjectHeap_free(h);
}
static void builtin_rejected_store_allocates_no_class(void) {
    MCObjectHeap *h = MCObjectHeap_new(1000000);
    CHECK(h);
    NativeJavaClass *a = NativeJavaClass_literal(h, &aType);
    CHECK(a);
    NativeTypedObjectArray *array = NULL;
    CHECK(NativeTypedObjectArray_new(h, a, 1, &array) == NATIVE_ARRAY_OK);
    EntityPlayerSP *sp = EntityPlayerSP_nativeAllocate(h);
    CHECK(sp);
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, h, (MCObject *)array));
    MCObjectHeap *branch = MCObjectHeap_clone(h);
    CHECK(branch);
    size_t objects = MCObjectHeap_liveObjects(h);
    CHECK(NativeTypedObjectArray_set(array, 0, (MCObject *)sp) == NATIVE_ARRAY_EXCEPTION &&
          !array->values[0] && !MCObjectHeap_failed(h));
    CHECK(MCObjectHeap_liveObjects(h) == objects);
    CHECK(MCObjectHeap_canAdopt(h, branch));
    MCObjectHeap_free(branch);
    MCObjectRoot_drop(&root);
    MCObjectHeap_free(h);
}
int main(void) {
    stores_and_covariance();
    result_and_lifetime();
    native_failures();
    rejected_store_has_no_graph_mutation();
    builtin_rejected_store_allocates_no_class();
    printf("Native typed reference arrays: %u checks\n", checks);
    return 0;
}
