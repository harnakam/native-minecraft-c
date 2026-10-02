#include "util/Vec4b.h"
#include <string.h>

static const MCObjectClass klass = {"net.minecraft.util.Vec4b", MCObjectHeap_plainClone, NULL,
                                    NULL};
static const NativeJavaClassDescriptor *const parents[] = {&NativeJavaClass_ObjectClass};
const NativeJavaClassDescriptor Vec4b_Class = {"net.minecraft.util.Vec4b", parents, 1,
                                               Vec4b_isInstance};

static bool fail(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return false;
}
bool Vec4b_isInstance(const MCObject *object) {
    return object && object->klass == &klass && MCObjectHeap_objectSize(object) >= sizeof(Vec4b);
}
static bool valid(const Vec4b *self) {
    MCObjectHeap *heap = self ? self->object.heap : NULL;
    if (!Vec4b_isInstance((const MCObject *)self) || MCObjectHeap_failed(heap))
        return fail(heap);
    return true;
}
NativeJavaClass *Vec4b_nativeClass(MCObjectHeap *heap) {
    return NativeJavaClass_literal(heap, &Vec4b_Class);
}
Vec4b *Vec4b_nativeAllocate(MCObjectHeap *heap) {
    return (Vec4b *)MCObjectHeap_alloc(heap, sizeof(Vec4b), &klass);
}
bool Vec4b_construct(Vec4b *self, int8_t a, int8_t b, int8_t c, int8_t d) {
    if (!valid(self))
        return false;
    self->field_176117_a = a;
    self->field_176115_b = b;
    self->field_176116_c = c;
    self->field_176114_d = d;
    MCObjectHeap_touch(self->object.heap);
    return true;
}
Vec4bResult Vec4b_constructCopy(Vec4b *self, const Vec4b *other) {
    if (!valid(self))
        return VEC4B_FAILURE;
    /* Object's base initialization has no reached field/effect dependency.
       The first source-field read throws before this receiver's first write. */
    if (!other)
        return VEC4B_EXCEPTION;
    if (other->object.heap != self->object.heap || !Vec4b_isInstance((const MCObject *)other)) {
        fail(self->object.heap);
        return VEC4B_FAILURE;
    }
    self->field_176117_a = other->field_176117_a;
    self->field_176115_b = other->field_176115_b;
    self->field_176116_c = other->field_176116_c;
    self->field_176114_d = other->field_176114_d;
    MCObjectHeap_touch(self->object.heap);
    return VEC4B_OK;
}
Vec4b *Vec4b_new(MCObjectHeap *heap, int8_t a, int8_t b, int8_t c, int8_t d) {
    Vec4b *self = Vec4b_nativeAllocate(heap);
    return self && Vec4b_construct(self, a, b, c, d) ? self : NULL;
}
Vec4bResult Vec4b_newCopy(MCObjectHeap *heap, const Vec4b *other, Vec4b **out) {
    if (!out) {
        fail(heap);
        return VEC4B_FAILURE;
    }
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap)) {
        fail(heap);
        return VEC4B_FAILURE;
    }
    /* The borrow guard protects a live source across allocation. Type/owner
       validation remains inside the constructor, after the new allocation. */
    Vec4b *self = Vec4b_nativeAllocate(heap);
    Vec4bResult result = self ? Vec4b_constructCopy(self, other) : VEC4B_FAILURE;
    if (result == VEC4B_OK)
        *out = self;
    MCObjectRootScope_end(&scope);
    return result;
}
int8_t Vec4b_func_176110_a(const Vec4b *self) { return valid(self) ? self->field_176117_a : 0; }
int8_t Vec4b_func_176112_b(const Vec4b *self) { return valid(self) ? self->field_176115_b : 0; }
int8_t Vec4b_func_176113_c(const Vec4b *self) { return valid(self) ? self->field_176116_c : 0; }
int8_t Vec4b_func_176111_d(const Vec4b *self) { return valid(self) ? self->field_176114_d : 0; }
bool Vec4b_equals(Vec4b *self, MCObject *other) {
    if (!valid(self))
        return false;
    if ((MCObject *)self == other)
        return true;
    if (!other)
        return false;
    if (other->heap != self->object.heap || MCObjectHeap_objectSize(other) < sizeof(MCObject))
        return fail(self->object.heap);
    if (other->klass != &klass)
        return false;
    if (!Vec4b_isInstance(other))
        return fail(self->object.heap);
    const Vec4b *right = (const Vec4b *)other;
    if (self->field_176117_a != right->field_176117_a)
        return false;
    if (self->field_176114_d != right->field_176114_d)
        return false;
    if (self->field_176115_b != right->field_176115_b)
        return false;
    return self->field_176116_c == right->field_176116_c;
}
static int32_t signed_bits(uint32_t bits) {
    int32_t value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
int32_t Vec4b_hashCode(const Vec4b *self) {
    if (!valid(self))
        return 0;
    uint32_t i = (uint32_t)(int32_t)self->field_176117_a;
    i = 31u * i + (uint32_t)(int32_t)self->field_176115_b;
    i = 31u * i + (uint32_t)(int32_t)self->field_176116_c;
    i = 31u * i + (uint32_t)(int32_t)self->field_176114_d;
    return signed_bits(i);
}
