#include "util/Vec3i.h"
#include "util/BlockPosMutableBlockPos.h"
#include "util/MathHelper.h"

static const MCObjectClass klass = {"net.minecraft.util.Vec3i", MCObjectHeap_plainClone, NULL, NULL};
static const NativeJavaClassDescriptor *const parents[] = {&NativeJavaClass_ObjectClass};
const NativeJavaClassDescriptor Vec3i_Class = {"net.minecraft.util.Vec3i", parents, 1,
                                              Vec3i_isRuntimeClass};

static bool same_reference(const MCObject *object, void *expected) { return object == expected; }
static bool tracked(const MCObject *object) {
    return object && object->heap && object->klass &&
           MCObjectHeap_findObject(object->heap, object->klass, same_reference, (void *)object) ==
               object;
}
const MCObjectClass *Vec3i_nativeClass(void) { return &klass; }
bool Vec3i_isRuntimeClass(const MCObject *object) {
    return tracked(object) && object->klass == &klass &&
           MCObjectHeap_objectSize(object) >= sizeof(Vec3i);
}
bool Vec3i_isInstance(const MCObject *object) {
    return Vec3i_isRuntimeClass(object) || BlockPos_isRuntimeClass(object) ||
           BlockPosMutableBlockPos_isRuntimeClass(object);
}
static NativeArrayResult valid(Vec3i *self) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    if (!Vec3i_isInstance((MCObject *)self) || MCObjectHeap_failed(self->object.heap)) {
        MCObjectHeap_fail(self->object.heap);
        return NATIVE_ARRAY_FAILURE;
    }
    return NATIVE_ARRAY_OK;
}
static NativeArrayResult begin(Vec3i *self, MCObjectRootScope *scope) {
    NativeArrayResult result = valid(self);
    if (result != NATIVE_ARRAY_OK)
        return result;
    if (!MCObjectRootScope_begin(scope, self->object.heap) ||
        !MCObjectRootScope_pin(scope, (MCObject *)self)) {
        MCObjectHeap_fail(self->object.heap);
        MCObjectRootScope_end(scope);
        return NATIVE_ARRAY_FAILURE;
    }
    return NATIVE_ARRAY_OK;
}
Vec3i *Vec3i_nativeAllocate(MCObjectHeap *heap) {
    return (Vec3i *)MCObjectHeap_alloc(heap, sizeof(Vec3i), &klass);
}
NativeArrayResult Vec3i_constructInt(Vec3i *self, int32_t x, int32_t y, int32_t z) {
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(self, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    self->x = x;
    self->y = y;
    self->z = z;
    MCObjectHeap_touch(self->object.heap);
    MCObjectRootScope_end(&scope);
    return NATIVE_ARRAY_OK;
}
NativeArrayResult Vec3i_constructDouble(Vec3i *self, double x, double y, double z) {
    NativeArrayResult result = valid(self);
    if (result != NATIVE_ARRAY_OK)
        return result;
    int32_t ix = MathHelper_floor_double(x);
    int32_t iy = MathHelper_floor_double(y);
    int32_t iz = MathHelper_floor_double(z);
    return Vec3i_constructInt(self, ix, iy, iz);
}
Vec3i *Vec3i_newInt(MCObjectHeap *heap, int32_t x, int32_t y, int32_t z) {
    Vec3i *self = Vec3i_nativeAllocate(heap);
    return self && Vec3i_constructInt(self, x, y, z) == NATIVE_ARRAY_OK ? self : NULL;
}
Vec3i *Vec3i_newDouble(MCObjectHeap *heap, double x, double y, double z) {
    Vec3i *self = Vec3i_nativeAllocate(heap);
    return self && Vec3i_constructDouble(self, x, y, z) == NATIVE_ARRAY_OK ? self : NULL;
}
static NativeArrayResult coordinate(Vec3i *self, unsigned axis, int32_t *out) {
    if (!out) {
        MCObjectHeap_fail(self ? self->object.heap : NULL);
        return NATIVE_ARRAY_FAILURE;
    }
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(self, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    /* Exact known virtual receiver; no layout-based arbitrary subclass guess. */
    if (BlockPosMutableBlockPos_isRuntimeClass((MCObject *)self)) {
        BlockPosMutableBlockPos *mutable = (BlockPosMutableBlockPos *)self;
        result = axis == 0 ? BlockPosMutableBlockPos_getX(mutable, out) :
                 axis == 1 ? BlockPosMutableBlockPos_getY(mutable, out) :
                             BlockPosMutableBlockPos_getZ(mutable, out);
    } else {
        *out = axis == 0 ? self->x : axis == 1 ? self->y : self->z;
    }
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult Vec3i_getX(Vec3i *self, int32_t *out) { return coordinate(self, 0, out); }
NativeArrayResult Vec3i_getY(Vec3i *self, int32_t *out) { return coordinate(self, 1, out); }
NativeArrayResult Vec3i_getZ(Vec3i *self, int32_t *out) { return coordinate(self, 2, out); }
NativeArrayResult Vec3i_equals(Vec3i *self, MCObject *other, bool *out) {
    if (!out) {
        MCObjectHeap_fail(self ? self->object.heap : NULL);
        return NATIVE_ARRAY_FAILURE;
    }
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(self, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    bool equal = false;
    if ((MCObject *)self == other) {
        equal = true;
    } else if (other) {
        if (other->heap != self->object.heap || !tracked(other)) {
            MCObjectHeap_fail(self->object.heap);
            result = NATIVE_ARRAY_FAILURE;
        } else if (other->klass == &klass || other->klass == BlockPos_nativeClass() ||
                   other->klass == BlockPosMutableBlockPos_nativeClass()) {
            if (!Vec3i_isInstance(other) || !MCObjectRootScope_pin(&scope, other)) {
                MCObjectHeap_fail(self->object.heap);
                result = NATIVE_ARRAY_FAILURE;
            } else {
                int32_t left, right;
                result = Vec3i_getX(self, &left);
                if (result == NATIVE_ARRAY_OK)
                    result = Vec3i_getX((Vec3i *)other, &right);
                if (result == NATIVE_ARRAY_OK && left == right) {
                    result = Vec3i_getY(self, &left);
                    if (result == NATIVE_ARRAY_OK)
                        result = Vec3i_getY((Vec3i *)other, &right);
                    if (result == NATIVE_ARRAY_OK && left == right) {
                        result = Vec3i_getZ(self, &left);
                        if (result == NATIVE_ARRAY_OK)
                            result = Vec3i_getZ((Vec3i *)other, &right);
                        if (result == NATIVE_ARRAY_OK)
                            equal = left == right;
                    }
                }
            }
        }
    }
    if (result == NATIVE_ARRAY_OK)
        *out = equal;
    MCObjectRootScope_end(&scope);
    return result;
}
static int32_t signed_bits(uint32_t bits) {
    return bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)(UINT32_MAX - bits);
}
NativeArrayResult Vec3i_hashCode(Vec3i *self, int32_t *out) {
    if (!out) {
        MCObjectHeap_fail(self ? self->object.heap : NULL);
        return NATIVE_ARRAY_FAILURE;
    }
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(self, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    int32_t y, z, x;
    result = Vec3i_getY(self, &y);
    if (result == NATIVE_ARRAY_OK)
        result = Vec3i_getZ(self, &z);
    if (result == NATIVE_ARRAY_OK)
        result = Vec3i_getX(self, &x);
    if (result == NATIVE_ARRAY_OK)
        *out = signed_bits(((uint32_t)y + (uint32_t)z * 31u) * 31u + (uint32_t)x);
    MCObjectRootScope_end(&scope);
    return result;
}

typedef struct { MCObject object; Vec3i *NULL_VECTOR; } Statics;
static void trace_statics(MCObject *object, MCObjectVisitor visit, void *context) {
    /* Heap tracing supplies a tracked allocation, including a malformed one.
       Check its actual payload before following the class's reference field. */
    if (MCObjectHeap_objectSize(object) < sizeof(Statics)) {
        MCObjectHeap_fail(object->heap);
        return;
    }
    Statics *fields = (Statics *)object;
    fields->NULL_VECTOR = (Vec3i *)visit((MCObject *)fields->NULL_VECTOR, context);
}
static const MCObjectClass statics = {"native.Vec3i.statics", MCObjectHeap_plainClone,
                                     trace_statics, NULL};
static bool any(const MCObject *object, void *context) { (void)object; (void)context; return true; }
Vec3i *NativeVec3i_nullVector(MCObjectHeap *heap) {
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap)) {
        MCObjectHeap_fail(heap);
        return NULL;
    }
    Statics *fields = (Statics *)MCObjectHeap_findObject(heap, &statics, any, NULL);
    if (fields && (!tracked((MCObject *)fields) ||
                   MCObjectHeap_objectSize((MCObject *)fields) < sizeof(Statics))) {
        MCObjectHeap_fail(heap);
        MCObjectRootScope_end(&scope);
        return NULL;
    }
    if (!fields) {
        fields = (Statics *)MCObjectHeap_alloc(heap, sizeof(*fields), &statics);
        if (fields) {
            Vec3i *value = Vec3i_newInt(heap, 0, 0, 0);
            if (value)
                fields->NULL_VECTOR = value;
        }
        MCObjectRoot root = {0};
        if (!fields || !fields->NULL_VECTOR || !MCObjectRoot_init(&root, heap, (MCObject *)fields))
            fields = NULL;
    }
    Vec3i *value = fields ? fields->NULL_VECTOR : NULL;
    if (!Vec3i_isRuntimeClass((MCObject *)value) || value->object.heap != heap || value->x != 0 ||
        value->y != 0 || value->z != 0) {
        MCObjectHeap_fail(heap);
        value = NULL;
    }
    MCObjectRootScope_end(&scope);
    return value;
}
