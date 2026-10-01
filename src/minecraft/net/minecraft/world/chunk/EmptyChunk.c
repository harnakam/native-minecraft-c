#include "world/chunk/EmptyChunk.h"
#include "world/EnumSkyBlock.h"
static void trace(MCObject *o, MCObjectVisitor v, void *ctx) {
    Chunk_traceFields((Chunk *)o, v, ctx);
}
static const MCObjectClass emptyClass = {"net.minecraft.world.chunk.EmptyChunk",
                                         MCObjectHeap_plainClone, trace, NULL};
bool EmptyChunk_isInstance(const MCObject *o) {
    return o && o->klass == &emptyClass && MCObjectHeap_objectSize(o) >= sizeof(EmptyChunk);
}
static bool valid(EmptyChunk *c) {
    if (EmptyChunk_isInstance((MCObject *)c) && !MCObjectHeap_failed(c->super.object.heap))
        return true;
    MCObjectHeap_fail(c ? c->super.object.heap : NULL);
    return false;
}
EmptyChunk *EmptyChunk_new(MCObjectHeap *h, World *w, int32_t x, int32_t z,
                           NativeBlockStateRuntime *r) {
    if (!h || MCObjectHeap_failed(h) ||
        (r && (!NativeBlockStateRuntime_isInstance((MCObject *)r) || r->object.heap != h))) {
        MCObjectHeap_fail(h);
        return NULL;
    }
    EmptyChunk *c = (EmptyChunk *)MCObjectHeap_alloc(h, sizeof *c, &emptyClass);
    if (!c)
        return NULL;
    c->super.nativeRuntime = r;
    return Chunk_construct(&c->super, w, x, z) ? c : NULL;
}
int32_t EmptyChunk_getHeightValue(EmptyChunk *c, int32_t x, int32_t z) {
    (void)x;
    (void)z;
    valid(c);
    return 0;
}
/* Empty bodies below are genuine original overrides, not pending behavior. */
bool EmptyChunk_generateHeightMap(EmptyChunk *c) { return valid(c); }
NativeBlock *EmptyChunk_getBlock(EmptyChunk *c, BlockPos *p) {
    (void)p;
    if (!valid(c))
        return NULL;
    NativeBlockStateRuntime *r = c->super.nativeRuntime;
    if (!r) {
        r = NativeBlockStateRuntime_get(c->super.object.heap);
        if (!r)
            return NULL;
        c->super.nativeRuntime = r;
        MCObjectHeap_touch(c->super.object.heap);
    }
    if (!NativeBlockStateRuntime_isInstance((MCObject *)r) ||
        r->object.heap != c->super.object.heap) {
        MCObjectHeap_fail(c->super.object.heap);
        return NULL;
    }
    return r->air;
}
int32_t EmptyChunk_getBlockLightOpacity(EmptyChunk *c, BlockPos *p) {
    (void)p;
    return valid(c) ? 255 : 0;
}
int32_t EmptyChunk_getBlockMetadata(EmptyChunk *c, BlockPos *p) {
    (void)p;
    valid(c);
    return 0;
}
int32_t EmptyChunk_getLightFor(EmptyChunk *c, EnumSkyBlock *kind, BlockPos *p) {
    (void)p;
    if (!valid(c))
        return 0;
    if (!EnumSkyBlock_isInstance((MCObject *)kind) || kind->object.heap != c->super.object.heap) {
        MCObjectHeap_fail(c->super.object.heap);
        return 0;
    }
    return kind->defaultLightValue;
}
int32_t EmptyChunk_getLightSubtracted(EmptyChunk *c, BlockPos *p, int32_t amount) {
    (void)p;
    (void)amount;
    valid(c);
    return 0;
}
bool EmptyChunk_canSeeSky(EmptyChunk *c, BlockPos *p) {
    (void)p;
    valid(c);
    return false;
}
bool EmptyChunk_getAreLevelsEmpty(EmptyChunk *c, int32_t a, int32_t b) {
    (void)a;
    (void)b;
    return valid(c);
}
bool EmptyChunk_onChunkUnload(EmptyChunk *c) { return valid(c); }
bool EmptyChunk_setChunkModified(EmptyChunk *c) { return valid(c); }
