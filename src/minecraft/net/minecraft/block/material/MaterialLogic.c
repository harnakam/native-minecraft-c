#include "block/material/MaterialLogic.h"

static const MCObjectClass instanceClass;
static NativeArrayResult fail(MCObjectHeap *h) {
    MCObjectHeap_fail(h);
    return NATIVE_ARRAY_FAILURE;
}
static bool identity(const MCObject *o, void *ctx) { return o == ctx; }
bool MaterialLogic_isRuntimeClass(const MCObject *o) {
    return o && o->klass == &instanceClass && o->heap &&
           MCObjectHeap_findObject(o->heap, o->klass, identity, (void *)o) == o &&
           MCObjectHeap_objectSize(o) >= sizeof(MaterialLogic);
}
bool MaterialLogic_isInstance(const MCObject *o) { return MaterialLogic_isRuntimeClass(o); }
static const NativeJavaClassDescriptor *const parents[] = {&Material_Class};
const NativeJavaClassDescriptor MaterialLogic_Class = {"net.minecraft.block.material.MaterialLogic",
                                                       parents, 1, MaterialLogic_isRuntimeClass};
static void trace(MCObject *o, MCObjectVisitor v, void *ctx) {
    Material_traceFields((Material *)o, v, ctx);
}
static const MCObjectClass instanceClass = {"net.minecraft.block.material.MaterialLogic",
                                            MCObjectHeap_plainClone, trace, NULL};
const MCObjectClass *MaterialLogic_nativeClass(void) { return &instanceClass; }
MaterialLogic *MaterialLogic_nativeAllocate(MCObjectHeap *h) {
    return (MaterialLogic *)MCObjectHeap_alloc(h, sizeof(MaterialLogic), &instanceClass);
}
NativeArrayResult MaterialLogic_construct(MaterialLogic *self, MapColor *color) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectHeap *h = self->material.object.heap;
    if (!MaterialLogic_isInstance((MCObject *)self) || MCObjectHeap_failed(h))
        return fail(h);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h) || !MCObjectRootScope_pin(&scope, (MCObject *)self)) {
        MCObjectRootScope_end(&scope);
        return fail(h);
    }
    NativeArrayResult result = Material_construct(&self->material, color);
    Material *receiver = &self->material;
    if (result == NATIVE_ARRAY_OK)
        result = Material_setAdventureModeExempt(receiver, &receiver);
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult MaterialLogic_new(MCObjectHeap *h, MapColor *color, MaterialLogic **out) {
    if (!h || !out || MCObjectHeap_failed(h))
        return fail(h);
    if (!Material_getStatics(h))
        return fail(h);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return fail(h);
    MaterialLogic *self = MaterialLogic_nativeAllocate(h);
    NativeArrayResult result = self ? MaterialLogic_construct(self, color) : fail(h);
    if (result == NATIVE_ARRAY_OK)
        *out = self;
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult MaterialLogic_isSolid(MaterialLogic *self, bool *out) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    if (!MaterialLogic_isInstance((MCObject *)self))
        return fail(self->material.object.heap);
    return Material_nativeReturnBoolean(&self->material, false, out);
}
NativeArrayResult MaterialLogic_blocksLight(MaterialLogic *self, bool *out) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    if (!MaterialLogic_isInstance((MCObject *)self))
        return fail(self->material.object.heap);
    return Material_nativeReturnBoolean(&self->material, false, out);
}
NativeArrayResult MaterialLogic_blocksMovement(MaterialLogic *self, bool *out) {
    if (!self)
        return NATIVE_ARRAY_EXCEPTION;
    if (!MaterialLogic_isInstance((MCObject *)self))
        return fail(self->material.object.heap);
    return Material_nativeReturnBoolean(&self->material, false, out);
}
