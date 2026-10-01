#include "world/chunk/storage/ExtendedBlockStorage.h"
static const MCObjectClass klass;

static bool fail(MCObjectHeap *h) {
    MCObjectHeap_fail(h);
    return false;
}
bool ExtendedBlockStorage_isInstance(const MCObject *o) {
    return o && o->klass == &klass && MCObjectHeap_objectSize(o) >= sizeof(ExtendedBlockStorage);
}
static bool valid(ExtendedBlockStorage *e) {
    return (ExtendedBlockStorage_isInstance((MCObject *)e) &&
            !MCObjectHeap_failed(e->object.heap)) ||
           fail(e ? e->object.heap : NULL);
}
static bool runtime_valid(ExtendedBlockStorage *e) {
    if (!e->nativeRuntime) {
        NativeBlockStateRuntime *runtime = NativeBlockStateRuntime_get(e->object.heap);
        if (!runtime)
            return false;
        e->nativeRuntime = runtime;
        MCObjectHeap_touch(e->object.heap);
    }
    return (NativeBlockStateRuntime_isInstance((MCObject *)e->nativeRuntime) &&
            e->nativeRuntime->object.heap == e->object.heap) ||
           fail(e->object.heap);
}
static bool array_valid(ExtendedBlockStorage *e, NativeCharArray *a) {
    return (NativeCharArray_isInstance((MCObject *)a) && a->object.heap == e->object.heap) ||
           fail(e->object.heap);
}
static bool nibble_valid(ExtendedBlockStorage *e, NibbleArray *n) {
    return (NibbleArray_isInstance((MCObject *)n) && n->object.heap == e->object.heap) ||
           fail(e->object.heap);
}
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
    if (!ExtendedBlockStorage_isInstance(o)) {
        fail(o->heap);
        return;
    }
    ExtendedBlockStorage *e = (ExtendedBlockStorage *)o;
    e->data = (NativeCharArray *)v((MCObject *)e->data, c);
    e->blocklightArray = (NibbleArray *)v((MCObject *)e->blocklightArray, c);
    e->skylightArray = (NibbleArray *)v((MCObject *)e->skylightArray, c);
    e->nativeRuntime = (NativeBlockStateRuntime *)v((MCObject *)e->nativeRuntime, c);
}
static const MCObjectClass klass = {"ExtendedBlockStorage", MCObjectHeap_plainClone, trace, NULL};

static int32_t signed32(uint32_t v) {
    return v <= INT32_MAX ? (int32_t)v : (int32_t)((int64_t)v - INT64_C(4294967296));
}
static int32_t coordinate(int32_t x, int32_t y, int32_t z) {
    return signed32(((uint32_t)y << 8) | ((uint32_t)z << 4) | (uint32_t)x);
}
static void add(int32_t *p, int32_t d) { *p = signed32((uint32_t)*p + (uint32_t)d); }
ExtendedBlockStorage *ExtendedBlockStorage_nativeAllocate(MCObjectHeap *h,
                                                          NativeBlockStateRuntime *r) {
    if (r && (!NativeBlockStateRuntime_isInstance((MCObject *)r) || r->object.heap != h)) {
        fail(h);
        return NULL;
    }
    ExtendedBlockStorage *e = (ExtendedBlockStorage *)MCObjectHeap_alloc(h, sizeof(*e), &klass);

    if (e)
        e->nativeRuntime = r;
    return e;
}
bool ExtendedBlockStorage_construct(ExtendedBlockStorage *e, int32_t y, bool sky) {
    if (!valid(e))
        return false;

    MCObjectRootScope scope = {0};

    if (!MCObjectRootScope_begin(&scope, e->object.heap) ||
        !MCObjectRootScope_pin(&scope, (MCObject *)e))
        return false;

    e->yBase = y;
    MCObjectHeap_touch(e->object.heap);
    NativeCharArray *a = NativeCharArray_new(e->object.heap, 4096);
    bool ok = a != NULL;

    if (ok) {
        e->data = a;
        MCObjectHeap_touch(e->object.heap);
        NibbleArray *n = NibbleArray_new(e->object.heap);
        ok = n != NULL;

        if (ok) {
            e->blocklightArray = n;
            MCObjectHeap_touch(e->object.heap);
        }
    }
    if (ok && sky) {
        NibbleArray *n = NibbleArray_new(e->object.heap);
        ok = n != NULL;

        if (ok) {
            e->skylightArray = n;
            MCObjectHeap_touch(e->object.heap);
        }
    }
    MCObjectRootScope_end(&scope);
    return ok;
}
ExtendedBlockStorage *ExtendedBlockStorage_new(MCObjectHeap *h, int32_t y, bool sky,
                                               NativeBlockStateRuntime *r) {
    ExtendedBlockStorage *e = ExtendedBlockStorage_nativeAllocate(h, r);
    return e && ExtendedBlockStorage_construct(e, y, sky) ? e : NULL;
}
NativeBlockState *ExtendedBlockStorage_get(ExtendedBlockStorage *e, int32_t x, int32_t y,
                                           int32_t z) {
    if (!valid(e) || !runtime_valid(e) || !array_valid(e, e->data))
        return NULL;

    uint16_t raw = 0;

    if (!NativeCharArray_get(e->data, coordinate(x, y, z), &raw))
        return NULL;

    NativeBlockState *s = NativeBlockStateRuntime_state(e->nativeRuntime, raw);

    if (MCObjectHeap_failed(e->object.heap))
        return NULL;

    if (s)
        return s;
    NativeBlock *air = e->nativeRuntime->air;
    if (!NativeBlock_isInstance((MCObject *)air) || air->object.heap != e->object.heap) {
        fail(e->object.heap);
        return NULL;
    }
    return NativeBlock_getDefaultState(air);
}
bool ExtendedBlockStorage_set(ExtendedBlockStorage *e, int32_t x, int32_t y, int32_t z,
                              NativeBlockState *state) {
    if (!valid(e))
        return false;

    MCObjectRootScope scope = {0};

    if (!MCObjectRootScope_begin(&scope, e->object.heap) ||
        !MCObjectRootScope_pin(&scope, (MCObject *)e))
        return false;

    NativeBlockState *old = ExtendedBlockStorage_get(e, x, y, z);
    NativeBlock *block = old ? NativeBlockState_getBlock(old) : NULL;
    bool ok = block != NULL;

    NativeBlock *next = NULL;

    if (ok) {
        if (!NativeBlockState_isInstance((MCObject *)state) ||
            state->object.heap != e->object.heap) {
            ok = fail(e->object.heap);
        } else
            next = NativeBlockState_getBlock(state);
        ok = ok && next != NULL;
    }
    if (ok) {
        ok = runtime_valid(e);

        if (ok && block != e->nativeRuntime->air) {
            add(&e->blockRefCount, -1);
            MCObjectHeap_touch(e->object.heap);
            bool tick = false;
            ok = NativeBlock_getTickRandomly(block, &tick);

            if (ok && tick) {
                add(&e->tickRefCount, -1);
                MCObjectHeap_touch(e->object.heap);
            }
        }
    }
    if (ok) {
        ok = runtime_valid(e);

        if (ok && next != e->nativeRuntime->air) {
            add(&e->blockRefCount, 1);
            MCObjectHeap_touch(e->object.heap);
            bool tick = false;
            ok = NativeBlock_getTickRandomly(next, &tick);

            if (ok && tick) {
                add(&e->tickRefCount, 1);
                MCObjectHeap_touch(e->object.heap);
            }
        }
    }
    if (ok) {
        /* The array reference/index are captured before the ID RHS; the JVM
           array-store bounds/null check occurs after that RHS evaluates. */
        NativeCharArray *array = e->data;
        int32_t index = coordinate(x, y, z);
        ok = runtime_valid(e);
        if (ok) {
            int32_t id =
                ObjectIntIdentityMap_get(e->nativeRuntime->BLOCK_STATE_IDS, (MCObject *)state);
            ok = !MCObjectHeap_failed(e->object.heap);
            if (ok)
                ok = array_valid(e, array) &&
                     NativeCharArray_set(array, index, (uint16_t)((uint32_t)id & 65535));
        }
    }
    MCObjectRootScope_end(&scope);
    return ok;
}
NativeBlock *ExtendedBlockStorage_getBlockByExtId(ExtendedBlockStorage *e, int32_t x, int32_t y,
                                                  int32_t z) {
    NativeBlockState *s = ExtendedBlockStorage_get(e, x, y, z);

    if (!s) {
        if (e)
            fail(e->object.heap);
        return NULL;
    }
    return NativeBlockState_getBlock(s);
}
int32_t ExtendedBlockStorage_getExtBlockMetadata(ExtendedBlockStorage *e, int32_t x, int32_t y,
                                                 int32_t z) {
    if (!valid(e))
        return 0;

    MCObjectRootScope scope = {0};

    if (!MCObjectRootScope_begin(&scope, e->object.heap) ||
        !MCObjectRootScope_pin(&scope, (MCObject *)e))
        return 0;

    NativeBlockState *s = ExtendedBlockStorage_get(e, x, y, z);
    NativeBlock *b = s ? NativeBlockState_getBlock(s) : NULL;
    int32_t value = 0;

    if (b)
        (void)NativeBlock_getMetaFromState(b, s, &value);
    else
        fail(e->object.heap);
    MCObjectRootScope_end(&scope);
    return value;
}
bool ExtendedBlockStorage_isEmpty(ExtendedBlockStorage *e) {
    return valid(e) && e->blockRefCount == 0;
}
bool ExtendedBlockStorage_getNeedsRandomTick(ExtendedBlockStorage *e) {
    return valid(e) && e->tickRefCount > 0;
}
int32_t ExtendedBlockStorage_getYLocation(ExtendedBlockStorage *e) {
    return valid(e) ? e->yBase : 0;
}
bool ExtendedBlockStorage_setExtSkylightValue(ExtendedBlockStorage *e, int32_t x, int32_t y,
                                              int32_t z, int32_t v) {
    return valid(e) && nibble_valid(e, e->skylightArray) &&
           NibbleArray_set(e->skylightArray, x, y, z, v);
}
int32_t ExtendedBlockStorage_getExtSkylightValue(ExtendedBlockStorage *e, int32_t x, int32_t y,
                                                 int32_t z) {
    return valid(e) && nibble_valid(e, e->skylightArray)
               ? NibbleArray_get(e->skylightArray, x, y, z)
               : 0;
}
bool ExtendedBlockStorage_setExtBlocklightValue(ExtendedBlockStorage *e, int32_t x, int32_t y,
                                                int32_t z, int32_t v) {
    return valid(e) && nibble_valid(e, e->blocklightArray) &&
           NibbleArray_set(e->blocklightArray, x, y, z, v);
}
int32_t ExtendedBlockStorage_getExtBlocklightValue(ExtendedBlockStorage *e, int32_t x, int32_t y,
                                                   int32_t z) {
    return valid(e) && nibble_valid(e, e->blocklightArray)
               ? NibbleArray_get(e->blocklightArray, x, y, z)
               : 0;
}
bool ExtendedBlockStorage_removeInvalidBlocks(ExtendedBlockStorage *e) {
    if (!valid(e))
        return false;

    MCObjectRootScope scope = {0};

    if (!MCObjectRootScope_begin(&scope, e->object.heap) ||
        !MCObjectRootScope_pin(&scope, (MCObject *)e))
        return false;

    e->blockRefCount = 0;
    e->tickRefCount = 0;
    MCObjectHeap_touch(e->object.heap);
    bool ok = true;

    for (int32_t x = 0; ok && x < 16; x++)
        for (int32_t y = 0; ok && y < 16; y++)
            for (int32_t z = 0; ok && z < 16; z++) {
                NativeBlock *b = ExtendedBlockStorage_getBlockByExtId(e, x, y, z);
                ok = b != NULL && runtime_valid(e);

                if (ok && b != e->nativeRuntime->air) {
                    add(&e->blockRefCount, 1);
                    MCObjectHeap_touch(e->object.heap);
                    bool tick = false;
                    ok = NativeBlock_getTickRandomly(b, &tick);

                    if (ok && tick) {
                        add(&e->tickRefCount, 1);
                        MCObjectHeap_touch(e->object.heap);
                    }
                }
            }
    MCObjectRootScope_end(&scope);
    return ok;
}
NativeCharArray *ExtendedBlockStorage_getData(ExtendedBlockStorage *e) {
    if (!valid(e))
        return NULL;

    if (e->data && !array_valid(e, e->data))
        return NULL;
    return e->data;
}
bool ExtendedBlockStorage_setData(ExtendedBlockStorage *e, NativeCharArray *a) {
    if (!valid(e) || (a && !array_valid(e, a)))
        return false;
    e->data = a;
    MCObjectHeap_touch(e->object.heap);
    return true;
}
NibbleArray *ExtendedBlockStorage_getBlocklightArray(ExtendedBlockStorage *e) {
    if (!valid(e))
        return NULL;

    if (e->blocklightArray && !nibble_valid(e, e->blocklightArray))
        return NULL;
    return e->blocklightArray;
}
NibbleArray *ExtendedBlockStorage_getSkylightArray(ExtendedBlockStorage *e) {
    if (!valid(e))
        return NULL;

    if (e->skylightArray && !nibble_valid(e, e->skylightArray))
        return NULL;
    return e->skylightArray;
}
bool ExtendedBlockStorage_setBlocklightArray(ExtendedBlockStorage *e, NibbleArray *n) {
    if (!valid(e) || (n && !nibble_valid(e, n)))
        return false;
    e->blocklightArray = n;
    MCObjectHeap_touch(e->object.heap);
    return true;
}
bool ExtendedBlockStorage_setSkylightArray(ExtendedBlockStorage *e, NibbleArray *n) {
    if (!valid(e) || (n && !nibble_valid(e, n)))
        return false;
    e->skylightArray = n;
    MCObjectHeap_touch(e->object.heap);
    return true;
}
