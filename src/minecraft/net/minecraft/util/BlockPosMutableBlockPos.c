#include "util/BlockPosMutableBlockPos.h"

static const MCObjectClass klass = {"net.minecraft.util.BlockPos$MutableBlockPos",
                                    MCObjectHeap_plainClone, NULL, NULL};
static const NativeJavaClassDescriptor *const parents[] = {&BlockPos_Class};
const NativeJavaClassDescriptor BlockPosMutableBlockPos_Class = {
    "net.minecraft.util.BlockPos$MutableBlockPos", parents, 1,
    BlockPosMutableBlockPos_isRuntimeClass};
static bool same_reference(const MCObject *object, void *expected) { return object == expected; }
const MCObjectClass *BlockPosMutableBlockPos_nativeClass(void) { return &klass; }
bool BlockPosMutableBlockPos_isRuntimeClass(const MCObject *object) {
    return object && object->heap && object->klass == &klass &&
           MCObjectHeap_findObject(object->heap, &klass, same_reference, (void *)object) == object &&
           MCObjectHeap_objectSize(object) >= sizeof(BlockPosMutableBlockPos);
}
bool BlockPosMutableBlockPos_isInstance(const MCObject *object) {
    return BlockPosMutableBlockPos_isRuntimeClass(object);
}
BlockPosMutableBlockPos *BlockPosMutableBlockPos_nativeAllocate(MCObjectHeap *heap) {
    return (BlockPosMutableBlockPos *)MCObjectHeap_alloc(heap, sizeof(BlockPosMutableBlockPos), &klass);
}
static NativeArrayResult begin(BlockPosMutableBlockPos *self, MCObjectRootScope *scope) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectHeap *heap = self->blockPos.vec3i.object.heap;
    if (!BlockPosMutableBlockPos_isInstance((MCObject *)self) || MCObjectHeap_failed(heap) ||
        !MCObjectRootScope_begin(scope, heap) || !MCObjectRootScope_pin(scope, (MCObject *)self)) {
        MCObjectHeap_fail(heap);
        MCObjectRootScope_end(scope);
        return NATIVE_ARRAY_FAILURE;
    }
    return NATIVE_ARRAY_OK;
}
NativeArrayResult BlockPosMutableBlockPos_constructInt(BlockPosMutableBlockPos *self, int32_t x,
                                                       int32_t y, int32_t z) {
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(self, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    result = BlockPos_constructInt(&self->blockPos, 0, 0, 0);
    if (result == NATIVE_ARRAY_OK) {
        self->x = x;
        self->y = y;
        self->z = z;
        MCObjectHeap_touch(self->blockPos.vec3i.object.heap);
    }
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult BlockPosMutableBlockPos_constructEmpty(BlockPosMutableBlockPos *self) {
    return BlockPosMutableBlockPos_constructInt(self, 0, 0, 0);
}
BlockPosMutableBlockPos *BlockPosMutableBlockPos_newInt(MCObjectHeap *heap, int32_t x, int32_t y,
                                                       int32_t z) {
    BlockPosMutableBlockPos *self = BlockPosMutableBlockPos_nativeAllocate(heap);
    return self && BlockPosMutableBlockPos_constructInt(self, x, y, z) == NATIVE_ARRAY_OK ? self : NULL;
}
BlockPosMutableBlockPos *BlockPosMutableBlockPos_newEmpty(MCObjectHeap *heap) {
    BlockPosMutableBlockPos *self = BlockPosMutableBlockPos_nativeAllocate(heap);
    return self && BlockPosMutableBlockPos_constructEmpty(self) == NATIVE_ARRAY_OK ? self : NULL;
}
NativeArrayResult BlockPosMutableBlockPos_getX(BlockPosMutableBlockPos *self, int32_t *out) {
    if (!out) {
        MCObjectHeap_fail(self ? self->blockPos.vec3i.object.heap : NULL);
        return NATIVE_ARRAY_FAILURE;
    }
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(self, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    *out = self->x;
    MCObjectRootScope_end(&scope);
    return NATIVE_ARRAY_OK;
}
NativeArrayResult BlockPosMutableBlockPos_getY(BlockPosMutableBlockPos *self, int32_t *out) {
    if (!out) {
        MCObjectHeap_fail(self ? self->blockPos.vec3i.object.heap : NULL);
        return NATIVE_ARRAY_FAILURE;
    }
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(self, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    *out = self->y;
    MCObjectRootScope_end(&scope);
    return NATIVE_ARRAY_OK;
}
NativeArrayResult BlockPosMutableBlockPos_getZ(BlockPosMutableBlockPos *self, int32_t *out) {
    if (!out) {
        MCObjectHeap_fail(self ? self->blockPos.vec3i.object.heap : NULL);
        return NATIVE_ARRAY_FAILURE;
    }
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(self, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    *out = self->z;
    MCObjectRootScope_end(&scope);
    return NATIVE_ARRAY_OK;
}
NativeArrayResult BlockPosMutableBlockPos_set(BlockPosMutableBlockPos *self, int32_t x, int32_t y,
                                             int32_t z, BlockPosMutableBlockPos **out) {
    if (!out) {
        MCObjectHeap_fail(self ? self->blockPos.vec3i.object.heap : NULL);
        return NATIVE_ARRAY_FAILURE;
    }
    MCObjectRootScope scope = {0};
    NativeArrayResult result = begin(self, &scope);
    if (result != NATIVE_ARRAY_OK)
        return result;
    self->x = x;
    self->y = y;
    self->z = z;
    MCObjectHeap_touch(self->blockPos.vec3i.object.heap);
    *out = self;
    MCObjectRootScope_end(&scope);
    return NATIVE_ARRAY_OK;
}
