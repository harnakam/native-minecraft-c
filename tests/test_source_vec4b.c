#include "util/Vec4b.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "source Vec4b check %u failed line %d: %s\n", checks, __LINE__, #x);   \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static const MCObjectClass unrelated = {"net.minecraft.util.Vec4b", MCObjectHeap_plainClone, NULL,
                                        NULL};
/* A test-only traced owner exposes duplicate references without depending on
   the separately migrating production Object[] representation. */
typedef struct TestOwner {
    MCObject object;
    Vec4b *values[4];
} TestOwner;
static void owner_trace(MCObject *object, MCObjectVisitor visitor, void *context) {
    TestOwner *owner = (TestOwner *)object;
    for (int i = 0; i < 4; ++i)
        owner->values[i] = (Vec4b *)visitor((MCObject *)owner->values[i], context);
}
static const MCObjectClass ownerClass = {"test.Vec4bReferenceOwner", MCObjectHeap_plainClone,
                                         owner_trace, NULL};
static int8_t signed_byte(unsigned bits) {
    return (int8_t)(bits < 128u ? (int)bits : (int)bits - 256);
}
static bool fields(const Vec4b *v, int8_t a, int8_t b, int8_t c, int8_t d) {
    return v->field_176117_a == a && v->field_176115_b == b && v->field_176116_c == c &&
           v->field_176114_d == d;
}
static int32_t expected_hash(int8_t a, int8_t b, int8_t c, int8_t d) {
    /* An independent weighted sum; four signed bytes cannot exceed int32. */
    int64_t value = (int64_t)a * 29791 + (int64_t)b * 961 + (int64_t)c * 31 + d;
    return (int32_t)value;
}

static void source_constructor(void) {
    MCObjectHeap *heap = MCObjectHeap_new(4096);
    CHECK(heap);
    Vec4b *value = Vec4b_nativeAllocate(heap);
    CHECK(value);
    CHECK(value->field_176117_a == 0 && value->field_176115_b == 0 && value->field_176116_c == 0 &&
          value->field_176114_d == 0);
    CHECK(Vec4b_construct(value, INT8_MIN, -1, 0, INT8_MAX));
    CHECK(Vec4b_func_176110_a(value) == INT8_MIN);
    CHECK(Vec4b_func_176112_b(value) == -1);
    CHECK(Vec4b_func_176113_c(value) == 0);
    CHECK(Vec4b_func_176111_d(value) == INT8_MAX);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

static void signed_fields_and_hash(void) {
    MCObjectHeap *heap = MCObjectHeap_new(4096);
    Vec4b *value = Vec4b_new(heap, 0, 0, 0, 0), *copy = Vec4b_new(heap, 1, 2, 3, 4);
    CHECK(value && copy);
    CHECK(sizeof(value->field_176117_a) == 1 && sizeof(value->field_176115_b) == 1 &&
          sizeof(value->field_176116_c) == 1 && sizeof(value->field_176114_d) == 1);
    for (unsigned i = 0; i < 256; ++i)
        for (unsigned j = 0; j < 256; ++j) {
            int8_t a = signed_byte(i), b = signed_byte(j), c = signed_byte(i ^ j),
                   d = signed_byte((i + j) & 255u);
            CHECK(Vec4b_construct(value, a, b, c, d));
            CHECK(Vec4b_func_176110_a(value) == a && Vec4b_func_176112_b(value) == b &&
                  Vec4b_func_176113_c(value) == c && Vec4b_func_176111_d(value) == d);
            CHECK(Vec4b_hashCode(value) == expected_hash(a, b, c, d));
            CHECK(Vec4b_constructCopy(copy, value) == VEC4B_OK);
            CHECK(fields(copy, a, b, c, d) && Vec4b_equals(value, (MCObject *)copy) &&
                  Vec4b_equals(copy, (MCObject *)value));
        }
    CHECK(Vec4b_construct(value, -128, -128, -128, -128) && Vec4b_hashCode(value) == -3940352);
    CHECK(Vec4b_construct(value, 127, 127, 127, 127) && Vec4b_hashCode(value) == 3909568);
    CHECK(Vec4b_construct(value, -128, -1, 0, 127) && Vec4b_hashCode(value) == -3814082);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

static void source_copy_and_equality(void) {
    MCObjectHeap *heap = MCObjectHeap_new(4096);
    Vec4b *left = Vec4b_new(heap, -128, -1, 2, 127), *right = NULL;
    CHECK(left);
    CHECK(Vec4b_newCopy(heap, left, &right) == VEC4B_OK && right && right != left);
    CHECK(Vec4b_equals(left, (MCObject *)left));
    CHECK(Vec4b_equals(left, (MCObject *)right) && Vec4b_hashCode(left) == Vec4b_hashCode(right));
    CHECK(!Vec4b_equals(left, NULL) && !MCObjectHeap_failed(heap));
    MCObject *other = MCObjectHeap_alloc(heap, sizeof(Vec4b), &unrelated);
    CHECK(other && !Vec4b_isInstance(other));
    CHECK(!Vec4b_equals(left, other) && !MCObjectHeap_failed(heap));
    /* Source comparisons read fields directly in a,d,b,c order. These
       independent mismatches also exclude accidental reference equality. */
    CHECK(Vec4b_construct(right, -127, -1, 2, 127) && !Vec4b_equals(left, (MCObject *)right));
    CHECK(Vec4b_construct(right, -128, -1, 2, 126) && !Vec4b_equals(left, (MCObject *)right));
    CHECK(Vec4b_construct(right, -128, 0, 2, 127) && !Vec4b_equals(left, (MCObject *)right));
    CHECK(Vec4b_construct(right, -128, -1, 3, 127) && !Vec4b_equals(left, (MCObject *)right));
    CHECK(Vec4b_constructCopy(right, left) == VEC4B_OK && Vec4b_equals(left, (MCObject *)right));
    CHECK(Vec4b_constructCopy(left, left) == VEC4B_OK && fields(left, -128, -1, 2, 127));
    CHECK(Vec4b_construct(right, 0, 0, 0, 31) && Vec4b_construct(left, 0, 0, 1, 0));
    CHECK(Vec4b_hashCode(left) == Vec4b_hashCode(right) && !Vec4b_equals(left, (MCObject *)right));
    CHECK(!MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}

static void null_copy_exception_prefix(void) {
    MCObjectHeap *heap = MCObjectHeap_new(4096);
    Vec4b *target = Vec4b_new(heap, 11, 22, 33, 44);
    CHECK(target);
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)target));
    size_t before = MCObjectHeap_liveObjects(heap), bytes = MCObjectHeap_liveBytes(heap);
    CHECK(Vec4b_constructCopy(target, NULL) == VEC4B_EXCEPTION);
    CHECK(fields(target, 11, 22, 33, 44) && !MCObjectHeap_failed(heap));
    CHECK(MCObjectHeap_liveObjects(heap) == before && MCObjectHeap_liveBytes(heap) == bytes);
    Vec4b *out = target;
    CHECK(Vec4b_newCopy(heap, NULL, &out) == VEC4B_EXCEPTION);
    CHECK(out == target && fields(target, 11, 22, 33, 44) && !MCObjectHeap_failed(heap));
    CHECK(MCObjectHeap_liveObjects(heap) == before + 1 &&
          MCObjectHeap_liveBytes(heap) == bytes + sizeof(Vec4b));
    CHECK(!MCObjectHeap_hasBorrowers(heap));
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == before);
    CHECK(MCObjectRoot_get(&root) == (MCObject *)target);
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == 0);
    MCObjectHeap_free(heap);

    /* New-object OOM precedes the copy constructor's NULL dereference. */
    heap = MCObjectHeap_new(sizeof(Vec4b) - 1);
    out = NULL;
    CHECK(Vec4b_newCopy(heap, NULL, &out) == VEC4B_FAILURE && out == NULL);
    CHECK(MCObjectHeap_failed(heap) && MCObjectHeap_liveObjects(heap) == 0 &&
          !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

static void native_class_facts(void) {
    MCObjectHeap *heap = MCObjectHeap_new(4096);
    NativeJavaClass *object = NativeJavaClass_Object(heap), *type = Vec4b_nativeClass(heap);
    Vec4b *value = Vec4b_new(heap, -1, 0, 1, 2);
    CHECK(object && type && value);
    CHECK(type == Vec4b_nativeClass(heap) && type->descriptor == &Vec4b_Class);
    CHECK(object->descriptor == &NativeJavaClass_ObjectClass);
    CHECK(Vec4b_Class.supertypeCount == 1 &&
          Vec4b_Class.supertypes[0] == &NativeJavaClass_ObjectClass);
    CHECK(NativeJavaClass_getClass(heap, (MCObject *)value) == type);
    bool result = false;
    CHECK(NativeJavaClass_isAssignableFrom(object, type, &result) && result);
    CHECK(NativeJavaClass_isAssignableFrom(type, type, &result) && result);
    CHECK(NativeJavaClass_isAssignableFrom(type, object, &result) && !result);
    CHECK(NativeJavaClass_isInstanceOf(object, (MCObject *)value, &result) && result);
    CHECK(NativeJavaClass_isInstanceOf(type, (MCObject *)value, &result) && result);
    CHECK(NativeJavaClass_isInstanceOf(type, NULL, &result) && !result);
    CHECK(!MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);

    /* Unknown facts remain unsupported; a convincing name/layout is not a
       registration, and no global reflection matcher is changed. */
    heap = MCObjectHeap_new(4096);
    type = Vec4b_nativeClass(heap);
    MCObject *unknown = MCObjectHeap_alloc(heap, sizeof(Vec4b), &unrelated);
    CHECK(type && unknown);
    result = true;
    CHECK(!NativeJavaClass_isInstanceOf(type, unknown, &result) && result &&
          MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}

static void reads_and_source_exception_preserve_snapshot(void) {
    MCObjectHeap *heap = MCObjectHeap_new(4096);
    Vec4b *left = Vec4b_new(heap, -1, 2, -3, 4), *right = NULL;
    CHECK(left && Vec4b_newCopy(heap, left, &right) == VEC4B_OK);
    MCObjectRoot root = {0}, otherRoot = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)left));
    CHECK(MCObjectRoot_init(&otherRoot, heap, (MCObject *)right));
    size_t before = MCObjectHeap_liveObjects(heap), bytes = MCObjectHeap_liveBytes(heap);
    MCObjectHeap *snapshot = MCObjectHeap_clone(heap);
    CHECK(snapshot);
    CHECK(Vec4b_func_176110_a(left) == -1 && Vec4b_func_176112_b(left) == 2 &&
          Vec4b_func_176113_c(left) == -3 && Vec4b_func_176111_d(left) == 4);
    CHECK(Vec4b_equals(left, (MCObject *)right) &&
          Vec4b_hashCode(left) == expected_hash(-1, 2, -3, 4));
    CHECK(Vec4b_constructCopy(left, NULL) == VEC4B_EXCEPTION);
    CHECK(MCObjectHeap_liveObjects(heap) == before && MCObjectHeap_liveBytes(heap) == bytes);
    CHECK(MCObjectHeap_canAdopt(heap, snapshot));
    MCObjectHeap_free(snapshot);
    snapshot = MCObjectHeap_clone(heap);
    CHECK(snapshot);
    CHECK(Vec4b_construct(left, 5, 6, 7, 8));
    CHECK(!MCObjectHeap_canAdopt(heap, snapshot) && !MCObjectHeap_failed(heap));
    MCObjectHeap_free(snapshot);
    MCObjectRoot_drop(&root);
    MCObjectRoot_drop(&otherRoot);
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == 0);
    MCObjectHeap_free(heap);
}

static void graph_aliases_gc_clone_adopt(void) {
    MCObjectHeap *heap = MCObjectHeap_new(64u * 1024u);
    NativeJavaClass *type = Vec4b_nativeClass(heap), *object = NativeJavaClass_Object(heap);
    CHECK(type && object);
    size_t classBaseline = MCObjectHeap_liveObjects(heap);
    CHECK(classBaseline == 2);
    TestOwner *array = (TestOwner *)MCObjectHeap_alloc(heap, sizeof(TestOwner), &ownerClass);
    Vec4b *value = Vec4b_new(heap, -128, 127, -1, 1), *copy = NULL;
    CHECK(array && value && Vec4b_newCopy(heap, value, &copy) == VEC4B_OK);
    array->values[0] = value;
    array->values[1] = value;
    array->values[3] = copy;
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)array));
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == classBaseline + 3);
    MCObjectHeap *clone = MCObjectHeap_clone(heap);
    MCObjectRoot cloneRoot = {0};
    CHECK(clone && MCObjectRoot_rebind(&cloneRoot, clone, &root));
    TestOwner *cloned = (TestOwner *)MCObjectRoot_get(&cloneRoot);
    Vec4b *clonedValue = cloned->values[0], *clonedCopy = cloned->values[3];
    CHECK(cloned != array && clonedValue != value && clonedValue == (Vec4b *)cloned->values[1]);
    CHECK(cloned->values[2] == NULL && clonedCopy != clonedValue && clonedCopy != copy);
    CHECK(Vec4b_isInstance((MCObject *)clonedValue) &&
          Vec4b_equals(clonedValue, (MCObject *)clonedCopy));
    CHECK(NativeJavaClass_getClass(clone, (MCObject *)clonedValue) == Vec4b_nativeClass(clone));
    CHECK(Vec4b_construct(clonedValue, 1, 2, 3, 4) && fields(value, -128, 127, -1, 1));
    CHECK(fields((Vec4b *)cloned->values[1], 1, 2, 3, 4) && fields(clonedCopy, -128, 127, -1, 1));
    CHECK(MCObjectHeap_canAdopt(heap, clone) && MCObjectHeap_adopt(heap, clone));
    array = (TestOwner *)MCObjectRoot_get(&root);
    CHECK(array->values[0] == array->values[1] && array->values[0] != array->values[3]);
    CHECK(fields((Vec4b *)array->values[0], 1, 2, 3, 4));
    CHECK(NativeJavaClass_getClass(heap, (MCObject *)array->values[0]) == Vec4b_nativeClass(heap));
    MCObjectHeap_free(clone);
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == classBaseline);
    /* Class-literal roots legitimately survive. No stale collected instance
       address is compared with a subsequent newly allocated object. */
    for (int i = 0; i < 1000; ++i)
        CHECK(Vec4b_new(heap, -1, 0, 1, 2));
    CHECK(MCObjectHeap_collect(heap) && MCObjectHeap_liveObjects(heap) == classBaseline);
    CHECK(!MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}

static void native_failures(void) {
    for (int which = 0; which < 7; ++which) {
        MCObjectHeap *heap = MCObjectHeap_new(4096), *foreign = MCObjectHeap_new(4096);
        Vec4b *value = Vec4b_new(heap, 1, 2, 3, 4), *other = Vec4b_new(foreign, 5, 6, 7, 8);
        CHECK(value && other);
        size_t before = MCObjectHeap_liveObjects(heap);
        Vec4b *out = value;
        switch (which) {
        case 0:
            CHECK(Vec4b_constructCopy(value, other) == VEC4B_FAILURE);
            break;
        case 1:
            CHECK(!Vec4b_equals(value, (MCObject *)other));
            break;
        case 2:
            CHECK(Vec4b_newCopy(heap, other, &out) == VEC4B_FAILURE && out == value);
            CHECK(MCObjectHeap_liveObjects(heap) == before + 1);
            break;
        case 3:
            CHECK(Vec4b_newCopy(heap, NULL, NULL) == VEC4B_FAILURE);
            break;
        case 4: {
            MCObject *small = MCObjectHeap_alloc(heap, sizeof(MCObject), value->object.klass);
            CHECK(small && !Vec4b_isInstance(small));
            CHECK(!Vec4b_equals(value, small));
            break;
        }
        case 5: {
            MCObject *small = MCObjectHeap_alloc(heap, sizeof(MCObject), value->object.klass);
            CHECK(small && !Vec4b_isInstance(small));
            CHECK(!Vec4b_construct((Vec4b *)small, 5, 6, 7, 8));
            break;
        }
        case 6: {
            MCObject *wrong = MCObjectHeap_alloc(heap, sizeof(Vec4b), &unrelated);
            CHECK(wrong && !Vec4b_isInstance(wrong));
            CHECK(Vec4b_constructCopy(value, (Vec4b *)wrong) == VEC4B_FAILURE);
            break;
        }
        }
        CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_failed(foreign));
        CHECK(fields(value, 1, 2, 3, 4) && fields(other, 5, 6, 7, 8));
        CHECK(Vec4b_constructCopy(value, NULL) == VEC4B_FAILURE);
        CHECK(!MCObjectHeap_hasBorrowers(heap) && !MCObjectHeap_hasBorrowers(foreign));
        MCObjectHeap_free(foreign);
        MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap = MCObjectHeap_new(sizeof(Vec4b));
    CHECK(Vec4b_new(heap, -128, -1, 0, 127));
    CHECK(Vec4b_new(heap, 1, 2, 3, 4) == NULL && MCObjectHeap_failed(heap));
    CHECK(MCObjectHeap_liveObjects(heap) == 1 && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
    CHECK(Vec4b_nativeAllocate(NULL) == NULL && !Vec4b_construct(NULL, 1, 2, 3, 4));
    CHECK(Vec4b_constructCopy(NULL, NULL) == VEC4B_FAILURE);
    CHECK(Vec4b_newCopy(NULL, NULL, NULL) == VEC4B_FAILURE);
}

int main(void) {
    source_constructor();
    signed_fields_and_hash();
    source_copy_and_equality();
    null_copy_exception_prefix();
    native_class_facts();
    reads_and_source_exception_preserve_snapshot();
    graph_aliases_gc_clone_adopt();
    native_failures();
    printf("source Vec4b: %u checks passed\n", checks);
    return 0;
}
