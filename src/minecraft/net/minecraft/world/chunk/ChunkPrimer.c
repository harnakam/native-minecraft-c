#include "world/chunk/ChunkPrimer.h"
static const MCObjectClass klass;

static bool fail(MCObjectHeap *h) {
    MCObjectHeap_fail(h);
    return false;
}
bool ChunkPrimer_isInstance(const MCObject *o) {
    return o && o->klass == &klass && MCObjectHeap_objectSize(o) >= sizeof(ChunkPrimer);
}
static bool valid(ChunkPrimer *p) {
    return (ChunkPrimer_isInstance((MCObject *)p) && !MCObjectHeap_failed(p->object.heap)) ||
           fail(p ? p->object.heap : NULL);
}
static bool runtime_valid(ChunkPrimer *p) {
    return (NativeBlockStateRuntime_isInstance((MCObject *)p->nativeRuntime) &&
            p->nativeRuntime->object.heap == p->object.heap) ||
           fail(p->object.heap);
}
static bool array_valid(ChunkPrimer *p) {
    return (NativeShortArray_isInstance((MCObject *)p->data) &&
            p->data->object.heap == p->object.heap) ||
           fail(p->object.heap);
}
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
    if (!ChunkPrimer_isInstance(o)) {
        fail(o->heap);
        return;
    }
    ChunkPrimer *p = (ChunkPrimer *)o;
    p->data = (NativeShortArray *)v((MCObject *)p->data, c);
    p->defaultState = (NativeBlockState *)v((MCObject *)p->defaultState, c);
    p->nativeRuntime = (NativeBlockStateRuntime *)v((MCObject *)p->nativeRuntime, c);
}
static const MCObjectClass klass = {"ChunkPrimer", MCObjectHeap_plainClone, trace, NULL};

ChunkPrimer *ChunkPrimer_nativeAllocate(MCObjectHeap *h, NativeBlockStateRuntime *r) {
    if (!NativeBlockStateRuntime_isInstance((MCObject *)r) || r->object.heap != h) {
        fail(h);
        return NULL;
    }
    ChunkPrimer *p = (ChunkPrimer *)MCObjectHeap_alloc(h, sizeof(*p), &klass);

    if (p)
        p->nativeRuntime = r;
    return p;
}
bool ChunkPrimer_construct(ChunkPrimer *p) {
    if (!valid(p))
        return false;

    MCObjectRootScope scope = {0};

    if (!MCObjectRootScope_begin(&scope, p->object.heap) ||
        !MCObjectRootScope_pin(&scope, (MCObject *)p))
        return false;

    NativeShortArray *a = NativeShortArray_new(p->object.heap, 65536);
    bool ok = a != NULL;

    if (ok) {
        p->data = a;
        MCObjectHeap_touch(p->object.heap);
        ok = runtime_valid(p);
    }
    NativeBlockState *s = ok ? NativeBlock_getDefaultState(p->nativeRuntime->air) : NULL;
    ok = s != NULL;

    if (ok) {
        p->defaultState = s;
        MCObjectHeap_touch(p->object.heap);
    }
    MCObjectRootScope_end(&scope);
    return ok;
}
ChunkPrimer *ChunkPrimer_new(MCObjectHeap *h, NativeBlockStateRuntime *r) {
    ChunkPrimer *p = ChunkPrimer_nativeAllocate(h, r);
    return p && ChunkPrimer_construct(p) ? p : NULL;
}
static int32_t coordinates(int32_t x, int32_t y, int32_t z) {
    uint32_t v = ((uint32_t)x << 12) | ((uint32_t)z << 8) | (uint32_t)y;
    return v <= INT32_MAX ? (int32_t)v : (int32_t)((int64_t)v - INT64_C(4294967296));
}
NativeBlockState *ChunkPrimer_getBlockStateAt(ChunkPrimer *p, int32_t index) {
    if (!valid(p) || !array_valid(p))
        return NULL;

    int16_t raw = 0;

    if (!NativeShortArray_get(p->data, index, &raw) || !runtime_valid(p))
        return NULL;

    NativeBlockState *s = NativeBlockStateRuntime_state(p->nativeRuntime, raw);

    if (MCObjectHeap_failed(p->object.heap))
        return NULL;

    if (s)
        return s;

    s = p->defaultState;

    if (s && (!NativeBlockState_isInstance((MCObject *)s) || s->object.heap != p->object.heap)) {
        fail(p->object.heap);
        return NULL;
    }
    return s;
}
NativeBlockState *ChunkPrimer_getBlockState(ChunkPrimer *p, int32_t x, int32_t y, int32_t z) {
    return ChunkPrimer_getBlockStateAt(p, coordinates(x, y, z));
}
bool ChunkPrimer_setBlockStateAt(ChunkPrimer *p, int32_t index, NativeBlockState *s) {
    if (!valid(p) || !array_valid(p))
        return false;

    /* Java array bounds are evaluated before the ID lookup in the RHS. */
    if (index < 0 || index >= p->data->length)
        return fail(p->object.heap);

    if (s && (!NativeBlockState_isInstance((MCObject *)s) || s->object.heap != p->object.heap))
        return fail(p->object.heap);

    NativeShortArray *array = p->data;

    if (!runtime_valid(p))
        return false;
    int32_t id = ObjectIntIdentityMap_get(p->nativeRuntime->BLOCK_STATE_IDS, (MCObject *)s);

    if (MCObjectHeap_failed(p->object.heap))
        return false;

    uint32_t raw = (uint32_t)id & 65535;

    int16_t v = (int16_t)(raw < 32768 ? (int32_t)raw : (int32_t)raw - 65536);

    return NativeShortArray_set(array, index, v);
}
bool ChunkPrimer_setBlockState(ChunkPrimer *p, int32_t x, int32_t y, int32_t z,
                               NativeBlockState *s) {
    return ChunkPrimer_setBlockStateAt(p, coordinates(x, y, z), s);
}
