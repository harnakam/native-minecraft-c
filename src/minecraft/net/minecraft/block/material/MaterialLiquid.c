#include "block/material/MaterialLiquid.h"

static const MCObjectClass instanceClass;
static NativeArrayResult fail(MCObjectHeap *h) {
    MCObjectHeap_fail(h);
    return NATIVE_ARRAY_FAILURE;
}
static bool identity(const MCObject *o, void *ctx) { return o == ctx; }
bool MaterialLiquid_isRuntimeClass(const MCObject *o) {
    return o && o->klass == &instanceClass && o->heap &&
           MCObjectHeap_findObject(o->heap, o->klass, identity, (void *)o) == o &&
           MCObjectHeap_objectSize(o) >= sizeof(MaterialLiquid);
}
bool MaterialLiquid_isInstance(const MCObject *o) { return MaterialLiquid_isRuntimeClass(o); }
static const NativeJavaClassDescriptor *const parents[] = {&Material_Class};
const NativeJavaClassDescriptor MaterialLiquid_Class = {
    "net.minecraft.block.material.MaterialLiquid", parents, 1, MaterialLiquid_isRuntimeClass};
static void trace(MCObject *o, MCObjectVisitor v, void *ctx) {
    Material_traceFields((Material *)o, v, ctx);
}
static const MCObjectClass instanceClass = {"net.minecraft.block.material.MaterialLiquid",
                                            MCObjectHeap_plainClone, trace, NULL};
const MCObjectClass *MaterialLiquid_nativeClass(void) { return &instanceClass; }
MaterialLiquid *MaterialLiquid_nativeAllocate(MCObjectHeap *h) {
    return (MaterialLiquid *)MCObjectHeap_alloc(h, sizeof(MaterialLiquid), &instanceClass);
}
NativeArrayResult MaterialLiquid_construct(MaterialLiquid *self, MapColor *color) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectHeap *h = self->material.object.heap;
    if (!MaterialLiquid_isInstance((MCObject *)self) || MCObjectHeap_failed(h))
        return fail(h);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h) || !MCObjectRootScope_pin(&scope, (MCObject *)self)) {
        MCObjectRootScope_end(&scope);
        return fail(h);
    }
    NativeArrayResult result = Material_construct(&self->material, color);
    Material *receiver = &self->material;
    if (result == NATIVE_ARRAY_OK)
        result = Material_setReplaceable(receiver, &receiver);
    if (result == NATIVE_ARRAY_OK)
        result = Material_setNoPushMobility(receiver, &receiver);
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult MaterialLiquid_new(MCObjectHeap *h, MapColor *color, MaterialLiquid **out) {
    if (!h || !out || MCObjectHeap_failed(h))
        return fail(h);
    if (!Material_getStatics(h))
        return fail(h);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return fail(h);
    MaterialLiquid *self = MaterialLiquid_nativeAllocate(h);
    NativeArrayResult result = self ? MaterialLiquid_construct(self, color) : fail(h);
    if (result == NATIVE_ARRAY_OK)
        *out = self;
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult MaterialLiquid_isLiquid(MaterialLiquid *self, bool *out) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    if (!MaterialLiquid_isInstance((MCObject *)self))
        return fail(self->material.object.heap);
    return Material_nativeReturnBoolean(&self->material, true, out);
}
NativeArrayResult MaterialLiquid_blocksMovement(MaterialLiquid *self, bool *out) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    if (!MaterialLiquid_isInstance((MCObject *)self))
        return fail(self->material.object.heap);
    return Material_nativeReturnBoolean(&self->material, false, out);
}
NativeArrayResult MaterialLiquid_isSolid(MaterialLiquid *self, bool *out) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    if (!MaterialLiquid_isInstance((MCObject *)self))
        return fail(self->material.object.heap);
    return Material_nativeReturnBoolean(&self->material, false, out);
}
