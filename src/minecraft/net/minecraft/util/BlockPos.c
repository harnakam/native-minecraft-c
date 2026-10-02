#include "util/BlockPos.h"
#include "util/BlockPosMutableBlockPos.h"

static const MCObjectClass klass = {"net.minecraft.util.BlockPos", MCObjectHeap_plainClone, NULL, NULL};
static const NativeJavaClassDescriptor *const parents[] = {&Vec3i_Class};
const NativeJavaClassDescriptor BlockPos_Class = {"net.minecraft.util.BlockPos", parents, 1,
                                                 BlockPos_isRuntimeClass};
static bool same_reference(const MCObject *object, void *expected) { return object == expected; }
const MCObjectClass *BlockPos_nativeClass(void) { return &klass; }
bool BlockPos_isRuntimeClass(const MCObject *object) {
    return object && object->heap && object->klass == &klass &&
           MCObjectHeap_findObject(object->heap, &klass, same_reference, (void *)object) == object &&
           MCObjectHeap_objectSize(object) >= sizeof(BlockPos);
}
bool BlockPos_isInstance(const MCObject *object) {
    return BlockPos_isRuntimeClass(object) || BlockPosMutableBlockPos_isRuntimeClass(object);
}
static NativeArrayResult begin(BlockPos *self, MCObjectRootScope *scope) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectHeap *heap = self->vec3i.object.heap;
    if (!BlockPos_isInstance((MCObject *)self) || MCObjectHeap_failed(heap) ||
        !MCObjectRootScope_begin(scope, heap) || !MCObjectRootScope_pin(scope, (MCObject *)self)) {
        MCObjectHeap_fail(heap);
        MCObjectRootScope_end(scope);
        return NATIVE_ARRAY_FAILURE;
    }
    return NATIVE_ARRAY_OK;
}
BlockPos *BlockPos_nativeAllocate(MCObjectHeap *heap) {
    return (BlockPos *)MCObjectHeap_alloc(heap, sizeof(BlockPos), &klass);
}
NativeArrayResult BlockPos_constructInt(BlockPos *self, int32_t x, int32_t y, int32_t z) {
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(self, &scope);
    if (result == NATIVE_ARRAY_OK)
        result = Vec3i_constructInt(&self->vec3i, x, y, z);
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult BlockPos_constructDouble(BlockPos *self, double x, double y, double z) {
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(self, &scope);
    if (result == NATIVE_ARRAY_OK)
        result = Vec3i_constructDouble(&self->vec3i, x, y, z);
    MCObjectRootScope_end(&scope);
    return result;
}
BlockPos *BlockPos_newInt(MCObjectHeap *heap, int32_t x, int32_t y, int32_t z) {
    BlockPos *self = BlockPos_nativeAllocate(heap);
    return self && BlockPos_constructInt(self, x, y, z) == NATIVE_ARRAY_OK ? self : NULL;
}
BlockPos *BlockPos_newDouble(MCObjectHeap *heap, double x, double y, double z) {
    BlockPos *self = BlockPos_nativeAllocate(heap);
    return self && BlockPos_constructDouble(self, x, y, z) == NATIVE_ARRAY_OK ? self : NULL;
}
BlockPos *NativeBlockPos_allocate(MCObjectHeap *heap) { return BlockPos_nativeAllocate(heap); }
bool NativeBlockPos_constructCoordinates(BlockPos *self, int32_t x, int32_t y, int32_t z) {
    /* Legacy bool caller has no healthy exception result channel. */
    NativeArrayResult result = BlockPos_constructInt(self, x, y, z);
    if (result != NATIVE_ARRAY_OK)
        MCObjectHeap_fail(self ? self->vec3i.object.heap : NULL);
    return result == NATIVE_ARRAY_OK;
}
static int32_t signed_bits(uint32_t bits) {
    return bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)(UINT32_MAX - bits);
}
BlockPos *BlockPos_downN(BlockPos *self, int32_t n) {
    /* Original DOWN offsets (0,-1,0); all products/additions Java-wrap. */
    return BlockPos_add(self, 0, signed_bits(UINT32_C(0) - (uint32_t)n), 0);
}
BlockPos *BlockPos_down(BlockPos *self) { return BlockPos_downN(self, 1); }
BlockPos *BlockPos_add(BlockPos *self, int32_t x, int32_t y, int32_t z) {
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(self, &scope);
    if (result != NATIVE_ARRAY_OK) {
        MCObjectHeap_fail(self ? self->vec3i.object.heap : NULL);
        return NULL;
    }
    BlockPos *value = self;
    if (x != 0 || y != 0 || z != 0) {
        /* Java NEW precedes each argument getter. The scope prevents native
           GC/adoption throughout NEW, virtual getters and constructor. */
        value = BlockPos_nativeAllocate(self->vec3i.object.heap);
        int32_t ix, iy, iz;
        if (!value) {
            result = NATIVE_ARRAY_FAILURE;
        } else {
            result = Vec3i_getX(&self->vec3i, &ix);
            if (result == NATIVE_ARRAY_OK) {
                ix = signed_bits((uint32_t)ix + (uint32_t)x);
                result = Vec3i_getY(&self->vec3i, &iy);
            }
            if (result == NATIVE_ARRAY_OK) {
                iy = signed_bits((uint32_t)iy + (uint32_t)y);
                result = Vec3i_getZ(&self->vec3i, &iz);
            }
            if (result == NATIVE_ARRAY_OK) {
                iz = signed_bits((uint32_t)iz + (uint32_t)z);
                result = BlockPos_constructInt(value, ix, iy, iz);
            }
        }
    }
    if (result != NATIVE_ARRAY_OK) {
        MCObjectHeap_fail(self->vec3i.object.heap);
        value = NULL;
    }
    MCObjectRootScope_end(&scope);
    return value;
}
NativeArrayResult BlockPos_toLong(BlockPos *self, int64_t *out) {
    if (!out) {
        MCObjectHeap_fail(self ? self->vec3i.object.heap : NULL);
        return NATIVE_ARRAY_FAILURE;
    }
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(self, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    int32_t x, y, z;
    result = Vec3i_getX(&self->vec3i, &x);
    if (result == NATIVE_ARRAY_OK)
        result = Vec3i_getY(&self->vec3i, &y);
    if (result == NATIVE_ARRAY_OK)
        result = Vec3i_getZ(&self->vec3i, &z);
    if (result == NATIVE_ARRAY_OK) {
        uint64_t bits = ((uint64_t)(uint32_t)x & UINT64_C(0x3ffffff)) << 38;
        bits |= ((uint64_t)(uint32_t)y & UINT64_C(0xfff)) << 26;
        bits |= (uint64_t)(uint32_t)z & UINT64_C(0x3ffffff);
        *out = bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(UINT64_MAX - bits);
    }
    MCObjectRootScope_end(&scope);
    return result;
}
static int32_t sign_extend(uint32_t bits, unsigned width) {
    uint32_t sign = UINT32_C(1) << (width - 1u);
    return (bits & sign) ? (int32_t)bits - (int32_t)(UINT32_C(1) << width) : (int32_t)bits;
}
BlockPos *BlockPos_fromLong(MCObjectHeap *heap, int64_t value) {
    /* Extraction's signed shifts are modeled without implementation-defined
       right shift or signed-left-shift UB. Original locals precede NEW. */
    uint64_t bits = (uint64_t)value;
    int32_t x = sign_extend((uint32_t)(bits >> 38) & UINT32_C(0x3ffffff), 26);
    int32_t y = sign_extend((uint32_t)(bits >> 26) & UINT32_C(0xfff), 12);
    int32_t z = sign_extend((uint32_t)bits & UINT32_C(0x3ffffff), 26);
    return BlockPos_newInt(heap, x, y, z);
}

typedef struct { MCObject object; BlockPos *ORIGIN; } Statics;
static void trace_statics(MCObject *object, MCObjectVisitor visit, void *context) {
    if (MCObjectHeap_objectSize(object) < sizeof(Statics)) {
        MCObjectHeap_fail(object->heap);
        return;
    }
    Statics *fields = (Statics *)object;
    fields->ORIGIN = (BlockPos *)visit((MCObject *)fields->ORIGIN, context);
}
static const MCObjectClass statics = {"native.BlockPos.statics", MCObjectHeap_plainClone,
                                     trace_statics, NULL};
static bool any(const MCObject *object, void *context) { (void)object; (void)context; return true; }
BlockPos *NativeBlockPos_origin(MCObjectHeap *heap) {
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap)) {
        MCObjectHeap_fail(heap);
        return NULL;
    }
    Statics *fields = (Statics *)MCObjectHeap_findObject(heap, &statics, any, NULL);
    if (fields && (MCObjectHeap_findObject(heap, &statics, same_reference, fields) !=
                      (MCObject *)fields ||
                   MCObjectHeap_objectSize((MCObject *)fields) < sizeof(Statics))) {
        MCObjectHeap_fail(heap);
        MCObjectRootScope_end(&scope);
        return NULL;
    }
    if (!fields) {
        fields = (Statics *)MCObjectHeap_alloc(heap, sizeof(*fields), &statics);
        if (fields) {
            BlockPos *value = BlockPos_newInt(heap, 0, 0, 0);
            if (value)
                fields->ORIGIN = value;
        }
        MCObjectRoot root = {0};
        if (!fields || !fields->ORIGIN || !MCObjectRoot_init(&root, heap, (MCObject *)fields))
            fields = NULL;
    }
    BlockPos *value = fields ? fields->ORIGIN : NULL;
    if (!BlockPos_isRuntimeClass((MCObject *)value) || value->vec3i.object.heap != heap ||
        value->vec3i.x != 0 || value->vec3i.y != 0 || value->vec3i.z != 0) {
        MCObjectHeap_fail(heap);
        value = NULL;
    }
    MCObjectRootScope_end(&scope);
    return value;
}
