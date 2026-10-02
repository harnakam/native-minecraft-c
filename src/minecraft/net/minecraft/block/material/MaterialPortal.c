#include "block/material/MaterialPortal.h"

static const MCObjectClass instanceClass;
static NativeArrayResult fail(MCObjectHeap *h) {
    MCObjectHeap_fail(h);
    return NATIVE_ARRAY_FAILURE;
}
static bool identity(const MCObject *o, void *ctx) { return o == ctx; }
bool MaterialPortal_isRuntimeClass(const MCObject *o) {
    return o && o->klass == &instanceClass && o->heap &&
           MCObjectHeap_findObject(o->heap, o->klass, identity, (void *)o) == o &&
           MCObjectHeap_objectSize(o) >= sizeof(MaterialPortal);
}
bool MaterialPortal_isInstance(const MCObject *o) { return MaterialPortal_isRuntimeClass(o); }
static const NativeJavaClassDescriptor *const parents[] = {&Material_Class};
const NativeJavaClassDescriptor MaterialPortal_Class = {
    "net.minecraft.block.material.MaterialPortal", parents, 1, MaterialPortal_isRuntimeClass};
static void trace(MCObject *o, MCObjectVisitor v, void *ctx) {
    Material_traceFields((Material *)o, v, ctx);
}
static const MCObjectClass instanceClass = {"net.minecraft.block.material.MaterialPortal",
                                            MCObjectHeap_plainClone, trace, NULL};
const MCObjectClass *MaterialPortal_nativeClass(void) { return &instanceClass; }
MaterialPortal *MaterialPortal_nativeAllocate(MCObjectHeap *h) {
    return (MaterialPortal *)MCObjectHeap_alloc(h, sizeof(MaterialPortal), &instanceClass);
}
NativeArrayResult MaterialPortal_construct(MaterialPortal *self, MapColor *color) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectHeap *h = self->material.object.heap;
    if (!MaterialPortal_isInstance((MCObject *)self) || MCObjectHeap_failed(h))
        return fail(h);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h) || !MCObjectRootScope_pin(&scope, (MCObject *)self)) {
        MCObjectRootScope_end(&scope);
        return fail(h);
    }
    NativeArrayResult result = Material_construct(&self->material, color);
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult MaterialPortal_new(MCObjectHeap *h, MapColor *color, MaterialPortal **out) {
    if (!h || !out || MCObjectHeap_failed(h))
        return fail(h);
    if (!Material_getStatics(h))
        return fail(h);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return fail(h);
    MaterialPortal *self = MaterialPortal_nativeAllocate(h);
    NativeArrayResult result = self ? MaterialPortal_construct(self, color) : fail(h);
    if (result == NATIVE_ARRAY_OK)
        *out = self;
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult MaterialPortal_isSolid(MaterialPortal *self, bool *out) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    if (!MaterialPortal_isInstance((MCObject *)self))
        return fail(self->material.object.heap);
    return Material_nativeReturnBoolean(&self->material, false, out);
}
NativeArrayResult MaterialPortal_blocksLight(MaterialPortal *self, bool *out) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    if (!MaterialPortal_isInstance((MCObject *)self))
        return fail(self->material.object.heap);
    return Material_nativeReturnBoolean(&self->material, false, out);
}
NativeArrayResult MaterialPortal_blocksMovement(MaterialPortal *self, bool *out) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    if (!MaterialPortal_isInstance((MCObject *)self))
        return fail(self->material.object.heap);
    return Material_nativeReturnBoolean(&self->material, false, out);
}
