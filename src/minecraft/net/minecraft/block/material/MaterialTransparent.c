#include "block/material/MaterialTransparent.h"

static const MCObjectClass instanceClass;
static NativeArrayResult fail(MCObjectHeap *h) {
    MCObjectHeap_fail(h);
    return NATIVE_ARRAY_FAILURE;
}
static bool identity(const MCObject *o, void *ctx) { return o == ctx; }
bool MaterialTransparent_isRuntimeClass(const MCObject *o) {
    return o && o->klass == &instanceClass && o->heap &&
           MCObjectHeap_findObject(o->heap, o->klass, identity, (void *)o) == o &&
           MCObjectHeap_objectSize(o) >= sizeof(MaterialTransparent);
}
bool MaterialTransparent_isInstance(const MCObject *o) {
    return MaterialTransparent_isRuntimeClass(o);
}
static const NativeJavaClassDescriptor *const parents[] = {&Material_Class};
const NativeJavaClassDescriptor MaterialTransparent_Class = {
    "net.minecraft.block.material.MaterialTransparent", parents, 1,
    MaterialTransparent_isRuntimeClass};
static void trace(MCObject *o, MCObjectVisitor v, void *ctx) {
    Material_traceFields((Material *)o, v, ctx);
}
static const MCObjectClass instanceClass = {"net.minecraft.block.material.MaterialTransparent",
                                            MCObjectHeap_plainClone, trace, NULL};
const MCObjectClass *MaterialTransparent_nativeClass(void) { return &instanceClass; }
MaterialTransparent *MaterialTransparent_nativeAllocate(MCObjectHeap *h) {
    return (MaterialTransparent *)MCObjectHeap_alloc(h, sizeof(MaterialTransparent),
                                                     &instanceClass);
}
NativeArrayResult MaterialTransparent_construct(MaterialTransparent *self, MapColor *color) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectHeap *h = self->material.object.heap;
    if (!MaterialTransparent_isInstance((MCObject *)self) || MCObjectHeap_failed(h))
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
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult MaterialTransparent_new(MCObjectHeap *h, MapColor *color,
                                          MaterialTransparent **out) {
    if (!h || !out || MCObjectHeap_failed(h))
        return fail(h);
    if (!Material_getStatics(h))
        return fail(h);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return fail(h);
    MaterialTransparent *self = MaterialTransparent_nativeAllocate(h);
    NativeArrayResult result = self ? MaterialTransparent_construct(self, color) : fail(h);
    if (result == NATIVE_ARRAY_OK)
        *out = self;
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult MaterialTransparent_isSolid(MaterialTransparent *self, bool *out) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    if (!MaterialTransparent_isInstance((MCObject *)self))
        return fail(self->material.object.heap);
    return Material_nativeReturnBoolean(&self->material, false, out);
}
NativeArrayResult MaterialTransparent_blocksLight(MaterialTransparent *self, bool *out) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    if (!MaterialTransparent_isInstance((MCObject *)self))
        return fail(self->material.object.heap);
    return Material_nativeReturnBoolean(&self->material, false, out);
}
NativeArrayResult MaterialTransparent_blocksMovement(MaterialTransparent *self, bool *out) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    if (!MaterialTransparent_isInstance((MCObject *)self))
        return fail(self->material.object.heap);
    return Material_nativeReturnBoolean(&self->material, false, out);
}
