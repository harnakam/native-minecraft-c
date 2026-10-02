#include "world/chunk/Chunk.h"
#include "world/EnumSkyBlock.h"
#include "world/chunk/EmptyChunk.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static const MCObjectClass chunkClass;
bool Chunk_isInstance(const MCObject *o) {
    return o && (o->klass == &chunkClass || EmptyChunk_isInstance(o)) &&
           MCObjectHeap_objectSize(o) >= sizeof(Chunk);
}
static bool valid(Chunk *c) {
    if (Chunk_isInstance((MCObject *)c) && !MCObjectHeap_failed(c->object.heap))
        return true;
    MCObjectHeap_fail(c ? c->object.heap : NULL);
    return false;
}
static bool ref(Chunk *c, MCObject *o) {
    if (!o || o->heap == c->object.heap)
        return true;
    MCObjectHeap_fail(c->object.heap);
    return false;
}
static bool required(Chunk *c, MCObject *o, bool type) {
    if (o && ref(c, o) && type)
        return true;
    MCObjectHeap_fail(c->object.heap);
    return false;
}
static bool start(Chunk *c, MCObjectRootScope *s) {
    return valid(c) && MCObjectRootScope_begin(s, c->object.heap) &&
           MCObjectRootScope_pin(s, (MCObject *)c);
}
static bool context(Chunk *c, MCObjectRootScope *s) {
    return ref(c, c->dependencyContext) && MCObjectRootScope_pin(s, c->dependencyContext);
}
static bool complete(Chunk *c, bool ok) {
    if (!ok)
        MCObjectHeap_fail(c->object.heap);
    return ok && !MCObjectHeap_failed(c->object.heap);
}
static bool cast_tile(Chunk *c, MCObjectRootScope *s, MCObject *tile) {
    if (!ref(c, tile) || !MCObjectRootScope_pin(s, tile))
        return false;
    /* Java checkcast accepts NULL. The subsequent method call decides
       whether that NULL receiver/argument is legal. */
    if (!tile)
        return true;
    if (!c->dependencies || !c->dependencies->tileEntityIsInstance || !context(c, s)) {
        MCObjectHeap_fail(c->object.heap);
        return false;
    }
    bool matches = false;
    return complete(c,
                    c->dependencies->tileEntityIsInstance(c->dependencyContext, tile, &matches)) &&
           complete(c, matches);
}
static int32_t from_bits(uint32_t u) {
    int32_t i;
    memcpy(&i, &u, sizeof i);
    return i;
}
static int32_t add(int32_t a, int32_t b) { return from_bits((uint32_t)a + (uint32_t)b); }
static int32_t sar4(int32_t x) {
    uint32_t u = (uint32_t)x;
    return from_bits((u >> 4) | (x < 0 ? UINT32_C(0xf0000000) : 0));
}
static int32_t column(int32_t x, int32_t z) { return from_bits(((uint32_t)z << 4) | (uint32_t)x); }
static bool selected(int32_t mask, int32_t i) {
    return ((uint32_t)mask & (UINT32_C(1) << ((uint32_t)i & 31))) != 0;
}
void Chunk_traceFields(Chunk *c, MCObjectVisitor v, void *ctx) {
    c->storageArrays = (NativeObjectArray *)v((MCObject *)c->storageArrays, ctx);
    c->blockBiomeArray = (NativeByteArray *)v((MCObject *)c->blockBiomeArray, ctx);
    c->precipitationHeightMap = (NativeIntArray *)v((MCObject *)c->precipitationHeightMap, ctx);
    c->updateSkylightColumns = (NativeBooleanArray *)v((MCObject *)c->updateSkylightColumns, ctx);
    c->worldObj = (World *)v((MCObject *)c->worldObj, ctx);
    c->heightMap = (NativeIntArray *)v((MCObject *)c->heightMap, ctx);
    c->chunkTileEntityMap = (NativeHashMap *)v((MCObject *)c->chunkTileEntityMap, ctx);
    c->entityLists = (NativeObjectArray *)v((MCObject *)c->entityLists, ctx);
    c->tileEntityPosQueue = (NativeConcurrentQueue *)v((MCObject *)c->tileEntityPosQueue, ctx);
    c->dependencyContext = v(c->dependencyContext, ctx);
    c->nativeRuntime = (NativeBlockStateRuntime *)v((MCObject *)c->nativeRuntime, ctx);
}
static void trace(MCObject *o, MCObjectVisitor v, void *ctx) {
    Chunk_traceFields((Chunk *)o, v, ctx);
}
static const MCObjectClass chunkClass = {"net.minecraft.world.chunk.Chunk", MCObjectHeap_plainClone,
                                         trace, NULL};
Chunk *Chunk_nativeAllocate(MCObjectHeap *h, const ChunkDependencies *d, MCObject *ctx,
                            NativeBlockStateRuntime *r) {
    if (!h || MCObjectHeap_failed(h) || (ctx && ctx->heap != h) ||
        (r && (!NativeBlockStateRuntime_isInstance((MCObject *)r) || r->object.heap != h))) {
        MCObjectHeap_fail(h);
        return NULL;
    }
    Chunk *c = (Chunk *)MCObjectHeap_alloc(h, sizeof *c, &chunkClass);
    if (c) {
        c->dependencies = d;
        c->dependencyContext = ctx;
        c->nativeRuntime = r;
    }
    return c;
}
/* Position keys keep their Source reference and inherited virtual methods. */
static bool position_hash(MCObject *ctx, MCObject *o, int32_t *out) {
    (void)ctx;
    if (!BlockPos_isInstance(o) || !out)
        return false;
    return Vec3i_hashCode(&((BlockPos *)o)->vec3i, out) == NATIVE_ARRAY_OK;
}
static bool position_equals(MCObject *ctx, MCObject *a, MCObject *b, bool *out) {
    (void)ctx;
    if (!BlockPos_isInstance(a) || !out)
        return false;
    return Vec3i_equals(&((BlockPos *)a)->vec3i, b, out) == NATIVE_ARRAY_OK;
}
static const NativeHashKeyMethods positionKeys = {position_hash, position_equals};
#define CONSTRUCT_FIELD(field, Type, predicate, hook, expr)                                        \
    do {                                                                                           \
        Type *value;                                                                               \
        if (c->dependencies && c->dependencies->hook) {                                            \
            if (!context(c, &scope))                                                               \
                goto done;                                                                         \
            value = c->dependencies->hook(c->dependencyContext, c, 256);                           \
        } else                                                                                     \
            value = (expr);                                                                        \
        if (!required(c, (MCObject *)value, predicate((MCObject *)value)) ||                       \
            MCObjectHeap_failed(h))                                                                \
            goto done;                                                                             \
        c->field = value;                                                                          \
        MCObjectHeap_touch(h);                                                                     \
    } while (0)
bool Chunk_construct(Chunk *c, World *w, int32_t x, int32_t z) {
    MCObjectRootScope scope = {0};
    if (!start(c, &scope))
        return false;
    MCObjectHeap *h = c->object.heap;
    bool ok = false;
    if (!ref(c, (MCObject *)w) || !MCObjectRootScope_pin(&scope, (MCObject *)w))
        goto done;
    NativeObjectArray *array;
    if (c->dependencies && c->dependencies->newStorageArrays) {
        if (!context(c, &scope))
            goto done;
        array = c->dependencies->newStorageArrays(c->dependencyContext, c, 16);
    } else
        array = NativeObjectArray_new(h, 16);
    if (!required(c, (MCObject *)array, NativeObjectArray_isInstance((MCObject *)array)) ||
        MCObjectHeap_failed(h))
        goto done;
    c->storageArrays = array;
    MCObjectHeap_touch(h);
    CONSTRUCT_FIELD(blockBiomeArray, NativeByteArray, NativeByteArray_isInstance, newBiomeArray,
                    NativeByteArray_new(h, 256));
    CONSTRUCT_FIELD(precipitationHeightMap, NativeIntArray, NativeIntArray_isInstance,
                    newPrecipitationHeightMap, NativeIntArray_new(h, 256));
    CONSTRUCT_FIELD(updateSkylightColumns, NativeBooleanArray, NativeBooleanArray_isInstance,
                    newSkylightColumns, NativeBooleanArray_new(h, 256));
    NativeHashMap *map;
    if (c->dependencies && c->dependencies->newTileEntityMap) {
        if (!context(c, &scope))
            goto done;
        map = c->dependencies->newTileEntityMap(c->dependencyContext, c);
    } else
        map = NativeHashMap_newWithKeys(h, &positionKeys, NULL);
    if (!required(c, (MCObject *)map, NativeHashMap_isInstance((MCObject *)map)) ||
        MCObjectHeap_failed(h))
        goto done;
    c->chunkTileEntityMap = map;
    c->queuedLightChecks = 4096;
    MCObjectHeap_touch(h);
    NativeConcurrentQueue *queue;
    if (c->dependencies && c->dependencies->newTileEntityQueue) {
        if (!context(c, &scope))
            goto done;
        queue = c->dependencies->newTileEntityQueue(c->dependencyContext, c);
    } else
        queue = NativeConcurrentQueue_new(h);
    if (!required(c, (MCObject *)queue, NativeConcurrentQueue_isInstance((MCObject *)queue)) ||
        MCObjectHeap_failed(h))
        goto done;
    c->tileEntityPosQueue = queue;
    MCObjectHeap_touch(h);
    if (c->dependencies && c->dependencies->newEntityLists) {
        if (!context(c, &scope))
            goto done;
        array = c->dependencies->newEntityLists(c->dependencyContext, c, 16);
    } else
        array = NativeObjectArray_new(h, 16);
    if (!required(c, (MCObject *)array, NativeObjectArray_isInstance((MCObject *)array)) ||
        MCObjectHeap_failed(h))
        goto done;
    c->entityLists = array;
    c->worldObj = w;
    c->xPosition = x;
    c->zPosition = z;
    MCObjectHeap_touch(h);
    CONSTRUCT_FIELD(heightMap, NativeIntArray, NativeIntArray_isInstance, newHeightMap,
                    NativeIntArray_new(h, 256));
    for (int32_t i = 0;; i = add(i, 1)) {
        array = c->entityLists;
        if (!required(c, (MCObject *)array, NativeObjectArray_isInstance((MCObject *)array)))
            goto done;
        if (i >= array->length)
            break;
        /* Java captures the assignment array/index before the child ctor. */
        if (!MCObjectRootScope_pin(&scope, (MCObject *)array))
            goto done;
        NativeJavaClass *clazz = NativeJavaClass_Entity(h);
        if (!clazz)
            goto done;
        ClassInheritanceMultiMap *list;
        if (c->dependencies && c->dependencies->newEntityList) {
            if (!context(c, &scope))
                goto done;
            list = c->dependencies->newEntityList(c->dependencyContext, c, clazz);
        } else
            list = ClassInheritanceMultiMap_new(h, clazz);
        if (!required(c, (MCObject *)list, ClassInheritanceMultiMap_isInstance((MCObject *)list)) ||
            MCObjectHeap_failed(h) || !NativeObjectArray_set(array, i, (MCObject *)list))
            goto done;
    }
    if (!required(c, (MCObject *)c->precipitationHeightMap,
                  NativeIntArray_isInstance((MCObject *)c->precipitationHeightMap)))
        goto done;
    for (int32_t i = 0; i < c->precipitationHeightMap->length; i++)
        c->precipitationHeightMap->values[i] = -999;
    if (!required(c, (MCObject *)c->blockBiomeArray,
                  NativeByteArray_isInstance((MCObject *)c->blockBiomeArray)))
        goto done;
    memset(c->blockBiomeArray->values, 255, (size_t)c->blockBiomeArray->length);
    MCObjectHeap_touch(h);
    ok = true;
done:
    MCObjectRootScope_end(&scope);
    return complete(c, ok);
}
#undef CONSTRUCT_FIELD
Chunk *Chunk_new(MCObjectHeap *h, World *w, int32_t x, int32_t z, NativeBlockStateRuntime *r) {
    Chunk *c = Chunk_nativeAllocate(h, NULL, NULL, r);
    return c && Chunk_construct(c, w, x, z) ? c : NULL;
}
static NativeBlockStateRuntime *runtime(Chunk *c) {
    if (!c->nativeRuntime) {
        NativeBlockStateRuntime *r = NativeBlockStateRuntime_get(c->object.heap);
        if (!r)
            return NULL;
        c->nativeRuntime = r;
        MCObjectHeap_touch(c->object.heap);
    }
    return required(c, (MCObject *)c->nativeRuntime,
                    NativeBlockStateRuntime_isInstance((MCObject *)c->nativeRuntime))
               ? c->nativeRuntime
               : NULL;
}
static bool no_sky(Chunk *c, MCObjectRootScope *s, bool *out) {
    World *w = c->worldObj;
    if (!required(c, (MCObject *)w, World_isInstance((MCObject *)w)))
        return false;
    WorldProvider *p = w->provider;
    if (!required(c, (MCObject *)p, WorldProvider_isInstance((MCObject *)p)) ||
        !MCObjectRootScope_pin(s, (MCObject *)p))
        return false;
    if (c->dependencies && c->dependencies->providerGetHasNoSky) {
        if (!context(c, s))
            return false;
        return complete(c, c->dependencies->providerGetHasNoSky(c->dependencyContext, p, out));
    }
    *out = WorldProvider_getHasNoSky(p);
    return !MCObjectHeap_failed(c->object.heap);
}
static bool position(Chunk *c, MCObjectRootScope *s, BlockPos *p, unsigned axis, int32_t *out) {
    if (!required(c, (MCObject *)p, BlockPos_isInstance((MCObject *)p)) ||
        !MCObjectRootScope_pin(s, (MCObject *)p))
        return false;
    bool (*fn)(MCObject *, BlockPos *, int32_t *) = NULL;
    if (c->dependencies)
        fn = axis == 0   ? c->dependencies->positionGetX
             : axis == 1 ? c->dependencies->positionGetY
                         : c->dependencies->positionGetZ;
    if (fn) {
        if (!context(c, s))
            return false;
        return complete(c, fn(c->dependencyContext, p, out));
    }
    NativeArrayResult r = axis == 0 ? Vec3i_getX(&p->vec3i, out)
                          : axis == 1 ? Vec3i_getY(&p->vec3i, out)
                                      : Vec3i_getZ(&p->vec3i, out);
    return complete(c, r == NATIVE_ARRAY_OK);
}
static bool object_array(Chunk *c, NativeObjectArray *a) {
    return required(c, (MCObject *)a, NativeObjectArray_isInstance((MCObject *)a));
}
static bool section_at(Chunk *c, NativeObjectArray *array, int32_t index,
                       ExtendedBlockStorage **out) {
    if (!object_array(c, array))
        return false;
    MCObject *o = NULL;
    if (!NativeObjectArray_get(array, index, &o))
        return false;
    if (o && !required(c, o, ExtendedBlockStorage_isInstance(o)))
        return false;
    *out = (ExtendedBlockStorage *)o;
    return true;
}
static bool section(Chunk *c, int32_t index, ExtendedBlockStorage **out) {
    return section_at(c, c->storageArrays, index, out);
}
static ExtendedBlockStorage *new_section(Chunk *c, MCObjectRootScope *s, int32_t y, bool sky) {
    ExtendedBlockStorage *e;
    if (c->dependencies && c->dependencies->newSection) {
        if (!context(c, s))
            return NULL;
        e = c->dependencies->newSection(c->dependencyContext, c, y, sky);
    } else
        e = ExtendedBlockStorage_new(c->object.heap, y, sky, c->nativeRuntime);
    return required(c, (MCObject *)e, ExtendedBlockStorage_isInstance((MCObject *)e)) &&
                   !MCObjectHeap_failed(c->object.heap)
               ? e
               : NULL;
}
bool Chunk_constructPrimer(Chunk *c, World *w, ChunkPrimer *p, int32_t x, int32_t z) {
    if (!Chunk_construct(c, w, x, z))
        return false;
    MCObjectRootScope scope = {0};
    if (!start(c, &scope))
        return false;
    bool ok = false, noSky;
    if (!ref(c, (MCObject *)p) || !MCObjectRootScope_pin(&scope, (MCObject *)p) ||
        !no_sky(c, &scope, &noSky))
        goto done;
    for (int32_t j = 0; j < 16; j++)
        for (int32_t k = 0; k < 16; k++)
            for (int32_t l = 0; l < 256; l++) {
                if (!required(c, (MCObject *)p, ChunkPrimer_isInstance((MCObject *)p)))
                    goto done;
                NativeBlockState *state = ChunkPrimer_getBlockStateAt(p, (j << 12) | (k << 8) | l);
                if (!state)
                    goto done;
                NativeBlock *block = NativeBlockState_getBlock(state);
                if (!block)
                    goto done;
                NativeMaterial *material = NativeBlock_getMaterial(block);
                NativeBlockStateRuntime *r = runtime(c);
                if (!material || !r)
                    goto done;
                if (material != r->airMaterial) {
                    ExtendedBlockStorage *e;
                    if (!section(c, l >> 4, &e))
                        goto done;
                    if (!e) {
                        NativeObjectArray *a = c->storageArrays;
                        if (!MCObjectRootScope_pin(&scope, (MCObject *)a))
                            goto done;
                        e = new_section(c, &scope, (l >> 4) << 4, !noSky);
                        if (!e || !NativeObjectArray_set(a, l >> 4, (MCObject *)e))
                            goto done;
                    }
                    if (!section(c, l >> 4, &e) || !e ||
                        !ExtendedBlockStorage_set(e, j, l & 15, k, state))
                        goto done;
                }
            }
    ok = true;
done:
    MCObjectRootScope_end(&scope);
    return complete(c, ok);
}
bool Chunk_isAtLocation(Chunk *c, int32_t x, int32_t z) {
    return valid(c) && x == c->xPosition && z == c->zPosition;
}
int32_t Chunk_getHeightValue_base(Chunk *c, int32_t x, int32_t z) {
    int32_t out = 0;
    if (!valid(c))
        return 0;
    if (!required(c, (MCObject *)c->heightMap,
                  NativeIntArray_isInstance((MCObject *)c->heightMap)) ||
        !NativeIntArray_get(c->heightMap, column(x, z), &out))
        return 0;
    return out;
}
int32_t Chunk_getHeightValue(Chunk *c, int32_t x, int32_t z) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return 0;
    int32_t out = 0;
    if (c->dependencies && c->dependencies->getHeightValue) {
        if (context(c, &s))
            complete(c, c->dependencies->getHeightValue(c->dependencyContext, c, x, z, &out));
    } else if (EmptyChunk_isInstance((MCObject *)c))
        out = EmptyChunk_getHeightValue((EmptyChunk *)c, x, z);
    else
        out = Chunk_getHeightValue_base(c, x, z);
    MCObjectRootScope_end(&s);
    return out;
}
int32_t Chunk_getHeight(Chunk *c, BlockPos *p) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return 0;
    int32_t x, z, out = 0;
    if (position(c, &s, p, 0, &x) && position(c, &s, p, 2, &z))
        out = Chunk_getHeightValue(c, x & 15, z & 15);
    MCObjectRootScope_end(&s);
    return out;
}
int32_t Chunk_getTopFilledSegment_base(Chunk *c) {
    if (!valid(c) || !object_array(c, c->storageArrays))
        return 0;
    for (int32_t i = c->storageArrays->length - 1; i >= 0; i--) {
        ExtendedBlockStorage *e;
        if (!section(c, i, &e))
            return 0;
        if (e)
            return ExtendedBlockStorage_getYLocation(e);
    }
    return 0;
}
int32_t Chunk_getTopFilledSegment(Chunk *c) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return 0;
    int32_t out = 0;
    if (c->dependencies && c->dependencies->getTopFilledSegment) {
        if (context(c, &s))
            complete(c, c->dependencies->getTopFilledSegment(c->dependencyContext, c, &out));
    } else
        out = Chunk_getTopFilledSegment_base(c);
    MCObjectRootScope_end(&s);
    return out;
}
NativeBlock *Chunk_getBlock0(Chunk *c, int32_t x, int32_t y, int32_t z) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return NULL;
    NativeBlockStateRuntime *r = runtime(c);
    NativeBlock *out = r ? r->air : NULL;
    if (!out)
        goto done;
    if (y >= 0) {
        if (!object_array(c, c->storageArrays))
            goto done;
        if (sar4(y) < c->storageArrays->length) {
            ExtendedBlockStorage *e;
            if (!section(c, sar4(y), &e))
                goto done;
            if (e)
                out = ExtendedBlockStorage_getBlockByExtId(e, x, y & 15, z);
        }
    }
done:
    MCObjectRootScope_end(&s);
    return MCObjectHeap_failed(c->object.heap) ? NULL : out;
}
NativeBlock *Chunk_getBlockXYZ(Chunk *c, int32_t x, int32_t y, int32_t z) {
    return Chunk_getBlock0(c, x & 15, y, z & 15);
}
NativeBlock *Chunk_getBlock_base(Chunk *c, BlockPos *p) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return NULL;
    int32_t x, y, z;
    NativeBlock *out = NULL;
    if (position(c, &s, p, 0, &x) && position(c, &s, p, 1, &y) && position(c, &s, p, 2, &z))
        out = Chunk_getBlock0(c, x & 15, y, z & 15);
    MCObjectRootScope_end(&s);
    return out;
}
NativeBlock *Chunk_getBlock(Chunk *c, BlockPos *p) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return NULL;
    NativeBlock *out = NULL;
    if (c->dependencies && c->dependencies->getBlock) {
        if (ref(c, (MCObject *)p) && MCObjectRootScope_pin(&s, (MCObject *)p) && context(c, &s))
            out = c->dependencies->getBlock(c->dependencyContext, c, p);
        if (out && !required(c, (MCObject *)out, NativeBlock_isInstance((MCObject *)out)))
            out = NULL;
    } else if (EmptyChunk_isInstance((MCObject *)c))
        out = EmptyChunk_getBlock((EmptyChunk *)c, p);
    else
        out = Chunk_getBlock_base(c, p);
    MCObjectRootScope_end(&s);
    return MCObjectHeap_failed(c->object.heap) ? NULL : out;
}
bool Chunk_generateHeightMap_base(Chunk *c) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return false;
    bool ok = false;
    int32_t top = Chunk_getTopFilledSegment(c);
    if (MCObjectHeap_failed(c->object.heap))
        goto done;
    c->heightMapMinimum = INT_MAX;
    MCObjectHeap_touch(c->object.heap);
    for (int32_t x = 0; x < 16; x++)
        for (int32_t z = 0; z < 16; z++) {
            if (!required(c, (MCObject *)c->precipitationHeightMap,
                          NativeIntArray_isInstance((MCObject *)c->precipitationHeightMap)) ||
                !NativeIntArray_set(c->precipitationHeightMap, column(x, z), -999))
                goto done;
            for (int32_t y = add(top, 16); y > 0; y = add(y, -1)) {
                NativeBlock *b = Chunk_getBlock0(c, x, add(y, -1), z);
                int32_t opacity;
                if (!b || !NativeBlock_getLightOpacity(b, &opacity))
                    goto done;
                if (opacity != 0) {
                    if (!required(c, (MCObject *)c->heightMap,
                                  NativeIntArray_isInstance((MCObject *)c->heightMap)) ||
                        !NativeIntArray_set(c->heightMap, column(x, z), y))
                        goto done;
                    if (y < c->heightMapMinimum)
                        c->heightMapMinimum = y;
                    break;
                }
            }
        }
    c->isModified = true;
    MCObjectHeap_touch(c->object.heap);
    ok = true;
done:
    MCObjectRootScope_end(&s);
    return complete(c, ok);
}
bool Chunk_generateHeightMap(Chunk *c) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return false;
    bool ok;
    if (c->dependencies && c->dependencies->generateHeightMap)
        ok = context(c, &s) && c->dependencies->generateHeightMap(c->dependencyContext, c);
    else if (EmptyChunk_isInstance((MCObject *)c))
        ok = EmptyChunk_generateHeightMap((EmptyChunk *)c);
    else
        ok = Chunk_generateHeightMap_base(c);
    MCObjectRootScope_end(&s);
    return complete(c, ok);
}
NativeBlockState *Chunk_getBlockState(Chunk *c, BlockPos *p) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return NULL;
    NativeBlockState *out = NULL;
    World *w = c->worldObj;
    if (!required(c, (MCObject *)w, World_isInstance((MCObject *)w)) ||
        !MCObjectRootScope_pin(&s, (MCObject *)w))
        goto done;
    WorldType *type;
    if (c->dependencies && c->dependencies->worldGetWorldType) {
        if (!context(c, &s))
            goto done;
        type = c->dependencies->worldGetWorldType(c->dependencyContext, w);
    } else
        type = World_getWorldType(w);
    if (!ref(c, (MCObject *)type) || (type && !WorldType_isInstance((MCObject *)type)) ||
        MCObjectHeap_failed(c->object.heap)) {
        MCObjectHeap_fail(c->object.heap);
        goto done;
    }
    WorldTypeStatics *types = WorldType_getStatics(c->object.heap);
    if (!types)
        goto done;
    int32_t x, y, z;
    if (type == types->DEBUG_WORLD) {
        if (!position(c, &s, p, 1, &y))
            goto done;
        if (y == 60) {
            NativeBlockStateRuntime *r = runtime(c);
            if (!r)
                goto done;
            out = NativeBlock_getDefaultState(r->barrier);
            if (MCObjectHeap_failed(c->object.heap))
                goto done;
        }
        if (!position(c, &s, p, 1, &y))
            goto done;
        if (y == 70) {
            if (!position(c, &s, p, 0, &x) || !position(c, &s, p, 2, &z) || !c->dependencies ||
                !c->dependencies->debugGetState || !context(c, &s)) {
                MCObjectHeap_fail(c->object.heap);
                goto done;
            }
            out = c->dependencies->debugGetState(c->dependencyContext, c, x, z);
            if (!ref(c, (MCObject *)out) ||
                (out && !NativeBlockState_isInstance((MCObject *)out))) {
                MCObjectHeap_fail(c->object.heap);
                goto done;
            }
        }
    } else {
        if (!position(c, &s, p, 1, &y))
            goto done;
        if (y >= 0) {
            if (!position(c, &s, p, 1, &y) || !object_array(c, c->storageArrays))
                goto done;
            if (sar4(y) < c->storageArrays->length) {
                NativeObjectArray *captured = c->storageArrays;
                if (!ref(c, (MCObject *)captured) ||
                    !MCObjectRootScope_pin(&s, (MCObject *)captured) || !position(c, &s, p, 1, &y))
                    goto done;
                ExtendedBlockStorage *e;
                if (!section_at(c, captured, sar4(y), &e))
                    goto done;
                if (e) {
                    if (!MCObjectRootScope_pin(&s, (MCObject *)e) || !position(c, &s, p, 0, &x) ||
                        !position(c, &s, p, 1, &y) || !position(c, &s, p, 2, &z))
                        goto done;
                    out = ExtendedBlockStorage_get(e, x & 15, y & 15, z & 15);
                    goto done;
                }
            }
        }
    }
    if (!out && !MCObjectHeap_failed(c->object.heap)) {
        NativeBlockStateRuntime *r = runtime(c);
        if (r)
            out = NativeBlock_getDefaultState(r->air);
    }
done:
    MCObjectRootScope_end(&s);
    return MCObjectHeap_failed(c->object.heap) ? NULL : out;
}
int32_t Chunk_getBlockMetadataXYZ(Chunk *c, int32_t x, int32_t y, int32_t z) {
    if (!valid(c) || !object_array(c, c->storageArrays))
        return 0;
    if (sar4(y) >= c->storageArrays->length)
        return 0;
    ExtendedBlockStorage *e;
    if (!section(c, sar4(y), &e))
        return 0;
    return e ? ExtendedBlockStorage_getExtBlockMetadata(e, x, y & 15, z) : 0;
}
int32_t Chunk_getBlockMetadata(Chunk *c, BlockPos *p) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return 0;
    int32_t x, y, z, out = 0;
    if (EmptyChunk_isInstance((MCObject *)c))
        out = EmptyChunk_getBlockMetadata((EmptyChunk *)c, p);
    else if (position(c, &s, p, 0, &x) && position(c, &s, p, 1, &y) && position(c, &s, p, 2, &z))
        out = Chunk_getBlockMetadataXYZ(c, x & 15, y, z & 15);
    MCObjectRootScope_end(&s);
    return out;
}
int32_t Chunk_getBlockLightOpacity(Chunk *c, BlockPos *p) {
    if (!valid(c))
        return 0;
    if (EmptyChunk_isInstance((MCObject *)c))
        return EmptyChunk_getBlockLightOpacity((EmptyChunk *)c, p);
    NativeBlock *b = Chunk_getBlock(c, p);
    int32_t out = 0;
    if (required(c, (MCObject *)b, NativeBlock_isInstance((MCObject *)b)))
        NativeBlock_getLightOpacity(b, &out);
    return out;
}
bool Chunk_canSeeSky_base(Chunk *c, BlockPos *p) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return false;
    int32_t x, y, z, height;
    bool out = false;
    if (position(c, &s, p, 0, &x) && position(c, &s, p, 1, &y) && position(c, &s, p, 2, &z) &&
        required(c, (MCObject *)c->heightMap,
                 NativeIntArray_isInstance((MCObject *)c->heightMap)) &&
        NativeIntArray_get(c->heightMap, column(x & 15, z & 15), &height))
        out = y >= height;
    MCObjectRootScope_end(&s);
    return out;
}
bool Chunk_canSeeSky(Chunk *c, BlockPos *p) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return false;
    bool out = false;
    if (c->dependencies && c->dependencies->canSeeSky) {
        if (ref(c, (MCObject *)p) && MCObjectRootScope_pin(&s, (MCObject *)p) && context(c, &s))
            complete(c, c->dependencies->canSeeSky(c->dependencyContext, c, p, &out));
    } else if (EmptyChunk_isInstance((MCObject *)c))
        out = EmptyChunk_canSeeSky((EmptyChunk *)c, p);
    else
        out = Chunk_canSeeSky_base(c, p);
    MCObjectRootScope_end(&s);
    return out;
}
int32_t Chunk_getLightFor(Chunk *c, EnumSkyBlock *kind, BlockPos *p) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return 0;
    int32_t x, y, z, out = 0;
    bool noSky;
    if (EmptyChunk_isInstance((MCObject *)c)) {
        out = EmptyChunk_getLightFor((EmptyChunk *)c, kind, p);
        goto done;
    }
    if (!position(c, &s, p, 0, &x) || !position(c, &s, p, 1, &y) || !position(c, &s, p, 2, &z))
        goto done;
    ExtendedBlockStorage *e;
    if (!section(c, sar4(y), &e))
        goto done;
    if (!e) {
        bool visible = Chunk_canSeeSky(c, p);
        if (MCObjectHeap_failed(c->object.heap))
            goto done;
        if (visible) {
            if (!required(c, (MCObject *)kind, EnumSkyBlock_isInstance((MCObject *)kind)))
                goto done;
            out = kind->defaultLightValue;
        }
        goto done;
    }
    EnumSkyBlockStatics *kinds = EnumSkyBlock_getStatics(c->object.heap);
    if (!kinds)
        goto done;
    if (kind == kinds->SKY) {
        if (no_sky(c, &s, &noSky) && !noSky)
            out = ExtendedBlockStorage_getExtSkylightValue(e, x & 15, y & 15, z & 15);
    } else if (kind == kinds->BLOCK)
        out = ExtendedBlockStorage_getExtBlocklightValue(e, x & 15, y & 15, z & 15);
    else if (required(c, (MCObject *)kind, EnumSkyBlock_isInstance((MCObject *)kind)))
        out = kind->defaultLightValue;
done:
    MCObjectRootScope_end(&s);
    return out;
}
static NativeCharArray *section_data(Chunk *c, MCObjectRootScope *s, ExtendedBlockStorage *e) {
    if (!required(c, (MCObject *)e, ExtendedBlockStorage_isInstance((MCObject *)e)) ||
        !MCObjectRootScope_pin(s, (MCObject *)e))
        return NULL;
    NativeCharArray *a;
    if (c->dependencies && c->dependencies->sectionGetData) {
        if (!context(c, s))
            return NULL;
        a = c->dependencies->sectionGetData(c->dependencyContext, e);
    } else
        a = ExtendedBlockStorage_getData(e);
    if (!ref(c, (MCObject *)a) || (a && !NativeCharArray_isInstance((MCObject *)a))) {
        MCObjectHeap_fail(c->object.heap);
        return NULL;
    }
    return a;
}
static NibbleArray *section_nibble(Chunk *c, MCObjectRootScope *s, ExtendedBlockStorage *e,
                                   bool sky) {
    if (!required(c, (MCObject *)e, ExtendedBlockStorage_isInstance((MCObject *)e)) ||
        !MCObjectRootScope_pin(s, (MCObject *)e))
        return NULL;
    NibbleArray *(*fn)(MCObject *, ExtendedBlockStorage *) = NULL;
    if (c->dependencies)
        fn = sky ? c->dependencies->sectionGetSkylightArray
                 : c->dependencies->sectionGetBlocklightArray;
    NibbleArray *a;
    if (fn) {
        if (!context(c, s))
            return NULL;
        a = fn(c->dependencyContext, e);
    } else
        a = sky ? ExtendedBlockStorage_getSkylightArray(e)
                : ExtendedBlockStorage_getBlocklightArray(e);
    if (!ref(c, (MCObject *)a) || (a && !NibbleArray_isInstance((MCObject *)a))) {
        MCObjectHeap_fail(c->object.heap);
        return NULL;
    }
    return a;
}
static NativeByteArray *nibble_data(Chunk *c, MCObjectRootScope *s, NibbleArray *n) {
    if (!required(c, (MCObject *)n, NibbleArray_isInstance((MCObject *)n)) ||
        !MCObjectRootScope_pin(s, (MCObject *)n))
        return NULL;
    NativeByteArray *a;
    if (c->dependencies && c->dependencies->nibbleGetData) {
        if (!context(c, s))
            return NULL;
        a = c->dependencies->nibbleGetData(c->dependencyContext, n);
    } else
        a = NibbleArray_getData(n);
    if (!ref(c, (MCObject *)a) || (a && !NativeByteArray_isInstance((MCObject *)a))) {
        MCObjectHeap_fail(c->object.heap);
        return NULL;
    }
    return a;
}
static bool copy_bytes(Chunk *c, NativeByteArray *src, int32_t offset, NativeByteArray *dst,
                       int32_t length) {
    if (!required(c, (MCObject *)src, NativeByteArray_isInstance((MCObject *)src)) ||
        !required(c, (MCObject *)dst, NativeByteArray_isInstance((MCObject *)dst)))
        return false;
    if (offset < 0 || length < 0 || length > dst->length || offset > src->length ||
        length > src->length - offset) {
        MCObjectHeap_fail(c->object.heap);
        return false;
    }
    memmove(dst->values, src->values + offset, (size_t)length);
    MCObjectHeap_touch(c->object.heap);
    return true;
}
bool Chunk_fillChunk(Chunk *c, NativeByteArray *bytes, int32_t mask, bool full) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return false;
    bool ok = false, noSky;
    int32_t offset = 0;
    if (!ref(c, (MCObject *)bytes) || !MCObjectRootScope_pin(&s, (MCObject *)bytes) ||
        !no_sky(c, &s, &noSky))
        goto done;
    for (int32_t j = 0;; j = add(j, 1)) {
        if (!object_array(c, c->storageArrays))
            goto done;
        if (j >= c->storageArrays->length)
            break;
        if (selected(mask, j)) {
            ExtendedBlockStorage *e;
            if (!section(c, j, &e))
                goto done;
            if (!e) {
                NativeObjectArray *a = c->storageArrays;
                if (!MCObjectRootScope_pin(&s, (MCObject *)a))
                    goto done;
                e = new_section(c, &s, from_bits((uint32_t)j << 4), !noSky);
                if (!e || !NativeObjectArray_set(a, j, (MCObject *)e))
                    goto done;
            }
            if (!section(c, j, &e))
                goto done;
            NativeCharArray *data = section_data(c, &s, e);
            if (!required(c, (MCObject *)data, NativeCharArray_isInstance((MCObject *)data)) ||
                !MCObjectRootScope_pin(&s, (MCObject *)data))
                goto done;
            for (int32_t k = 0; k < data->length; k = add(k, 1)) {
                int8_t hi, lo;
                if (!required(c, (MCObject *)bytes,
                              NativeByteArray_isInstance((MCObject *)bytes)) ||
                    !NativeByteArray_get(bytes, add(offset, 1), &hi) ||
                    !NativeByteArray_get(bytes, offset, &lo))
                    goto done;
                if (!NativeCharArray_set(
                        data, k, (uint16_t)(((uint32_t)(uint8_t)hi << 8) | (uint32_t)(uint8_t)lo)))
                    goto done;
                offset = add(offset, 2);
            }
        } else if (full) {
            ExtendedBlockStorage *e;
            if (!section(c, j, &e))
                goto done;
            if (e && !NativeObjectArray_set(c->storageArrays, j, NULL))
                goto done;
        }
    }
    for (unsigned phase = 0; phase < 2; phase++) {
        if (phase == 1 && noSky)
            break;
        for (int32_t j = 0;; j = add(j, 1)) {
            if (!object_array(c, c->storageArrays))
                goto done;
            if (j >= c->storageArrays->length)
                break;
            if (!selected(mask, j))
                continue;
            ExtendedBlockStorage *e;
            if (!section(c, j, &e))
                goto done;
            if (!e)
                continue;
            NibbleArray *n = section_nibble(c, &s, e, phase == 1);
            if (MCObjectHeap_failed(c->object.heap))
                goto done;
            NativeByteArray *target = nibble_data(c, &s, n);
            if (MCObjectHeap_failed(c->object.heap) ||
                !MCObjectRootScope_pin(&s, (MCObject *)target))
                goto done;
            NativeByteArray *lengthArray = nibble_data(c, &s, n);
            if (!required(c, (MCObject *)lengthArray,
                          NativeByteArray_isInstance((MCObject *)lengthArray)))
                goto done;
            if (!copy_bytes(c, bytes, offset, target, lengthArray->length))
                goto done;
            lengthArray = nibble_data(c, &s, n);
            if (!required(c, (MCObject *)lengthArray,
                          NativeByteArray_isInstance((MCObject *)lengthArray)))
                goto done;
            offset = add(offset, lengthArray->length);
        }
    }
    if (full) {
        NativeByteArray *target = c->blockBiomeArray;
        if (!ref(c, (MCObject *)target) || !MCObjectRootScope_pin(&s, (MCObject *)target) ||
            !required(c, (MCObject *)c->blockBiomeArray,
                      NativeByteArray_isInstance((MCObject *)c->blockBiomeArray)) ||
            !copy_bytes(c, bytes, offset, target, c->blockBiomeArray->length))
            goto done;
        /* The original's final k1 local is unused, but its length read occurs. */
        if (!required(c, (MCObject *)c->blockBiomeArray,
                      NativeByteArray_isInstance((MCObject *)c->blockBiomeArray)))
            goto done;
        (void)add(offset, c->blockBiomeArray->length);
    }
    for (int32_t j = 0;; j = add(j, 1)) {
        if (!object_array(c, c->storageArrays))
            goto done;
        if (j >= c->storageArrays->length)
            break;
        ExtendedBlockStorage *e;
        if (!section(c, j, &e))
            goto done;
        if (e && selected(mask, j)) {
            if (!MCObjectRootScope_pin(&s, (MCObject *)e))
                goto done;
            if (c->dependencies && c->dependencies->sectionRemoveInvalidBlocks) {
                if (!context(c, &s) || !complete(c, c->dependencies->sectionRemoveInvalidBlocks(
                                                        c->dependencyContext, e)))
                    goto done;
            } else if (!ExtendedBlockStorage_removeInvalidBlocks(e))
                goto done;
        }
    }
    c->isLightPopulated = true;
    c->isTerrainPopulated = true;
    MCObjectHeap_touch(c->object.heap);
    if (!Chunk_generateHeightMap(c) ||
        !required(c, (MCObject *)c->chunkTileEntityMap,
                  NativeHashMap_isInstance((MCObject *)c->chunkTileEntityMap)))
        goto done;
    NativeHashMapView *view = NativeHashMap_values(c->chunkTileEntityMap);
    NativeIterator *it = view ? NativeIterator_fromView(view) : NULL;
    if (!it || !MCObjectRootScope_pin(&s, (MCObject *)it))
        goto done;
    while (NativeIterator_hasNext(it)) {
        MCObject *tile = NULL;
        if (!NativeIterator_next(it, &tile) || !cast_tile(c, &s, tile) ||
            !required(c, tile, true) || !c->dependencies ||
            !c->dependencies->tileUpdateContainingBlockInfo || !context(c, &s)) {
            MCObjectHeap_fail(c->object.heap);
            goto done;
        }
        if (!complete(c,
                      c->dependencies->tileUpdateContainingBlockInfo(c->dependencyContext, tile)))
            goto done;
    }
    ok = !MCObjectHeap_failed(c->object.heap);
done:
    MCObjectRootScope_end(&s);
    return complete(c, ok);
}
static MCObject *plains(Chunk *c, MCObjectRootScope *s) {
    if (!c->dependencies || !c->dependencies->biomePlains || !context(c, s)) {
        MCObjectHeap_fail(c->object.heap);
        return NULL;
    }
    MCObject *out = c->dependencies->biomePlains(c->dependencyContext, c);
    return required(c, out, true) && !MCObjectHeap_failed(c->object.heap) ? out : NULL;
}
MCObject *Chunk_getBiome(Chunk *c, BlockPos *p, MCObject *manager) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return NULL;
    int32_t x, z, id;
    int8_t byte;
    MCObject *out = NULL;
    if (!ref(c, manager) || !MCObjectRootScope_pin(&s, manager) || !position(c, &s, p, 0, &x) ||
        !position(c, &s, p, 2, &z) ||
        !required(c, (MCObject *)c->blockBiomeArray,
                  NativeByteArray_isInstance((MCObject *)c->blockBiomeArray)) ||
        !NativeByteArray_get(c->blockBiomeArray, column(x & 15, z & 15), &byte))
        goto done;
    id = (uint8_t)byte;
    if (id == 255) {
        MCObject *fallback = plains(c, &s);
        if (!fallback || !MCObjectRootScope_pin(&s, fallback) || !required(c, manager, true) ||
            !c->dependencies->managerGetBiomeGenerator || !context(c, &s)) {
            MCObjectHeap_fail(c->object.heap);
            goto done;
        }
        MCObject *biome =
            c->dependencies->managerGetBiomeGenerator(c->dependencyContext, manager, p, fallback);
        if (!required(c, biome, true) || !MCObjectRootScope_pin(&s, biome) ||
            !c->dependencies->biomeGetId || !context(c, &s) ||
            !complete(c, c->dependencies->biomeGetId(c->dependencyContext, biome, &id)))
            goto done;
        int8_t narrowed;
        uint8_t raw = (uint8_t)((uint32_t)id & 255);
        memcpy(&narrowed, &raw, 1);
        if (!required(c, (MCObject *)c->blockBiomeArray,
                      NativeByteArray_isInstance((MCObject *)c->blockBiomeArray)) ||
            !NativeByteArray_set(c->blockBiomeArray, column(x & 15, z & 15), narrowed))
            goto done;
    }
    if (!c->dependencies || !c->dependencies->biomeById || !context(c, &s)) {
        MCObjectHeap_fail(c->object.heap);
        goto done;
    }
    out = c->dependencies->biomeById(c->dependencyContext, c, id);
    if (!ref(c, out) || MCObjectHeap_failed(c->object.heap))
        goto done;
    if (!out)
        out = plains(c, &s);
done:
    MCObjectRootScope_end(&s);
    return MCObjectHeap_failed(c->object.heap) ? NULL : out;
}
static bool warn_length(Chunk *c, MCObjectRootScope *s, const char *what, int32_t actual,
                        int32_t expected) {
    if (c->dependencies && c->dependencies->warnArrayLength)
        return context(c, s) && complete(c, c->dependencies->warnArrayLength(
                                                c->dependencyContext, c, what, actual, expected));
    fprintf(stderr, "Could not set level chunk %s, array length is %d instead of %d\n", what,
            (int)actual, (int)expected);
    return true;
}
bool Chunk_setStorageArrays(Chunk *c, NativeObjectArray *a) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return false;
    bool ok = false;
    if (!object_array(c, c->storageArrays) || !object_array(c, a) ||
        !MCObjectRootScope_pin(&s, (MCObject *)a))
        goto done;
    if (c->storageArrays->length != a->length) {
        ok = warn_length(c, &s, "sections", a->length, c->storageArrays->length);
        goto done;
    }
    for (int32_t i = 0; i < c->storageArrays->length; i++) {
        MCObject *e;
        if (!NativeObjectArray_get(a, i, &e) ||
            (e && !required(c, e, ExtendedBlockStorage_isInstance(e))) ||
            !NativeObjectArray_set(c->storageArrays, i, e))
            goto done;
    }
    ok = true;
done:
    MCObjectRootScope_end(&s);
    return complete(c, ok);
}
bool Chunk_setBiomeArray(Chunk *c, NativeByteArray *a) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return false;
    bool ok = false;
    if (!required(c, (MCObject *)c->blockBiomeArray,
                  NativeByteArray_isInstance((MCObject *)c->blockBiomeArray)) ||
        !required(c, (MCObject *)a, NativeByteArray_isInstance((MCObject *)a)) ||
        !MCObjectRootScope_pin(&s, (MCObject *)a))
        goto done;
    if (c->blockBiomeArray->length != a->length) {
        ok = warn_length(c, &s, "biomes", a->length, c->blockBiomeArray->length);
        goto done;
    }
    for (int32_t i = 0; i < c->blockBiomeArray->length; i++)
        if (!NativeByteArray_set(c->blockBiomeArray, i, a->values[i]))
            goto done;
    ok = true;
done:
    MCObjectRootScope_end(&s);
    return complete(c, ok);
}
bool Chunk_setHeightMap(Chunk *c, NativeIntArray *a) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return false;
    bool ok = false;
    if (!required(c, (MCObject *)c->heightMap,
                  NativeIntArray_isInstance((MCObject *)c->heightMap)) ||
        !required(c, (MCObject *)a, NativeIntArray_isInstance((MCObject *)a)) ||
        !MCObjectRootScope_pin(&s, (MCObject *)a))
        goto done;
    if (c->heightMap->length != a->length) {
        ok = warn_length(c, &s, "heightmap", a->length, c->heightMap->length);
        goto done;
    }
    for (int32_t i = 0; i < c->heightMap->length; i++)
        if (!NativeIntArray_set(c->heightMap, i, a->values[i]))
            goto done;
    ok = true;
done:
    MCObjectRootScope_end(&s);
    return complete(c, ok);
}
bool Chunk_getAreLevelsEmpty(Chunk *c, int32_t startY, int32_t endY) {
    if (!valid(c))
        return false;
    if (EmptyChunk_isInstance((MCObject *)c))
        return EmptyChunk_getAreLevelsEmpty((EmptyChunk *)c, startY, endY);
    if (startY < 0)
        startY = 0;
    if (endY >= 256)
        endY = 255;
    for (int32_t i = startY; i <= endY; i = add(i, 16)) {
        ExtendedBlockStorage *e;
        if (!section(c, sar4(i), &e))
            return false;
        if (e && !ExtendedBlockStorage_isEmpty(e))
            return false;
    }
    return true;
}
bool Chunk_isEmpty(Chunk *c) { return valid(c) && EmptyChunk_isInstance((MCObject *)c); }
bool Chunk_onChunkUnload_base(Chunk *c) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return false;
    bool ok = false;
    c->isChunkLoaded = false;
    MCObjectHeap_touch(c->object.heap);
    if (!required(c, (MCObject *)c->chunkTileEntityMap,
                  NativeHashMap_isInstance((MCObject *)c->chunkTileEntityMap)))
        goto done;
    NativeHashMapView *view = NativeHashMap_values(c->chunkTileEntityMap);
    NativeIterator *it = view ? NativeIterator_fromView(view) : NULL;
    if (!it || !MCObjectRootScope_pin(&s, (MCObject *)it))
        goto done;
    while (NativeIterator_hasNext(it)) {
        MCObject *tile = NULL;
        if (!NativeIterator_next(it, &tile) || !cast_tile(c, &s, tile))
            goto done;
        World *w = c->worldObj;
        if (!required(c, (MCObject *)w, World_isInstance((MCObject *)w)) ||
            !MCObjectRootScope_pin(&s, (MCObject *)w))
            goto done;
        if (c->dependencies && c->dependencies->worldMarkTileEntityForRemoval) {
            if (!context(c, &s) || !complete(c, c->dependencies->worldMarkTileEntityForRemoval(
                                                    c->dependencyContext, w, tile)))
                goto done;
        } else if (!World_markTileEntityForRemoval(w, tile))
            goto done;
    }
    if (MCObjectHeap_failed(c->object.heap))
        goto done;
    for (int32_t i = 0;; i = add(i, 1)) {
        if (!object_array(c, c->entityLists))
            goto done;
        if (i >= c->entityLists->length)
            break;
        /* Capture the World receiver before evaluating the collection argument. */
        World *w = c->worldObj;
        MCObject *collection = NULL;
        if (!ref(c, (MCObject *)w) || !MCObjectRootScope_pin(&s, (MCObject *)w) ||
            !NativeObjectArray_get(c->entityLists, i, &collection) || !ref(c, collection) ||
            !MCObjectRootScope_pin(&s, collection))
            goto done;
        if (!required(c, (MCObject *)w, World_isInstance((MCObject *)w)))
            goto done;
        if (collection && !required(c, collection, ClassInheritanceMultiMap_isInstance(collection)))
            goto done;
        if (c->dependencies && c->dependencies->worldUnloadEntities) {
            if (!context(c, &s) || !complete(c, c->dependencies->worldUnloadEntities(
                                                    c->dependencyContext, w, collection)))
                goto done;
        } else if (!World_unloadEntities(w, collection))
            goto done;
    }
    ok = true;
done:
    MCObjectRootScope_end(&s);
    return complete(c, ok);
}
bool Chunk_onChunkUnload(Chunk *c) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return false;
    bool ok;
    if (c->dependencies && c->dependencies->onChunkUnload)
        ok = context(c, &s) && c->dependencies->onChunkUnload(c->dependencyContext, c);
    else if (EmptyChunk_isInstance((MCObject *)c))
        ok = EmptyChunk_onChunkUnload((EmptyChunk *)c);
    else
        ok = Chunk_onChunkUnload_base(c);
    MCObjectRootScope_end(&s);
    return complete(c, ok);
}
bool Chunk_setChunkModified(Chunk *c) {
    if (!valid(c))
        return false;
    if (EmptyChunk_isInstance((MCObject *)c))
        return EmptyChunk_setChunkModified((EmptyChunk *)c);
    c->isModified = true;
    MCObjectHeap_touch(c->object.heap);
    return true;
}
#define GET_REF(name, Type, field)                                                                 \
    Type *Chunk_##name(Chunk *c) { return valid(c) ? c->field : NULL; }
GET_REF(getBlockStorageArray, NativeObjectArray, storageArrays)
GET_REF(getBiomeArray, NativeByteArray, blockBiomeArray)
GET_REF(getWorld, World, worldObj)
GET_REF(getHeightMap, NativeIntArray, heightMap)
GET_REF(getTileEntityMap, NativeHashMap, chunkTileEntityMap)
GET_REF(getEntityLists, NativeObjectArray, entityLists)
#undef GET_REF
#define GET_VALUE(name, Type, field)                                                               \
    Type Chunk_##name(Chunk *c) { return valid(c) ? c->field : 0; }
GET_VALUE(isLoaded, bool, isChunkLoaded)
GET_VALUE(isTerrainPopulated, bool, isTerrainPopulated)
GET_VALUE(isLightPopulated, bool, isLightPopulated)
GET_VALUE(getLowestHeight, int32_t, heightMapMinimum)
GET_VALUE(getInhabitedTime, int64_t, inhabitedTime)
#undef GET_VALUE
#define SET_VALUE(name, Type, field)                                                               \
    bool Chunk_##name(Chunk *c, Type v) {                                                          \
        if (!valid(c))                                                                             \
            return false;                                                                          \
        c->field = v;                                                                              \
        MCObjectHeap_touch(c->object.heap);                                                        \
        return true;                                                                               \
    }
SET_VALUE(setChunkLoaded, bool, isChunkLoaded)
SET_VALUE(setTerrainPopulated, bool, isTerrainPopulated)
SET_VALUE(setLightPopulated, bool, isLightPopulated)
SET_VALUE(setModified, bool, isModified)
SET_VALUE(setHasEntities, bool, hasEntities)
SET_VALUE(setLastSaveTime, int64_t, lastSaveTime)
SET_VALUE(setInhabitedTime, int64_t, inhabitedTime)
#undef SET_VALUE
bool Chunk_resetRelightChecks(Chunk *c) {
    if (!valid(c))
        return false;
    c->queuedLightChecks = 0;
    MCObjectHeap_touch(c->object.heap);
    return true;
}
int32_t Chunk_getLightSubtracted(Chunk *c, BlockPos *p, int32_t amount) {
    MCObjectRootScope s = {0};
    if (!start(c, &s))
        return 0;
    int32_t x, y, z, out = 0;
    bool noSky;
    if (EmptyChunk_isInstance((MCObject *)c)) {
        out = EmptyChunk_getLightSubtracted((EmptyChunk *)c, p, amount);
        goto done;
    }
    if (!position(c, &s, p, 0, &x) || !position(c, &s, p, 1, &y) || !position(c, &s, p, 2, &z))
        goto done;
    ExtendedBlockStorage *e;
    if (!section(c, sar4(y), &e) || !no_sky(c, &s, &noSky))
        goto done;
    if (!e) {
        if (!noSky) {
            EnumSkyBlockStatics *k = EnumSkyBlock_getStatics(c->object.heap);
            if (k && amount < k->SKY->defaultLightValue)
                out = from_bits((uint32_t)k->SKY->defaultLightValue - (uint32_t)amount);
        }
        goto done;
    }
    int32_t light = noSky ? 0 : ExtendedBlockStorage_getExtSkylightValue(e, x & 15, y & 15, z & 15);
    if (MCObjectHeap_failed(c->object.heap))
        goto done;
    light = from_bits((uint32_t)light - (uint32_t)amount);
    int32_t block = ExtendedBlockStorage_getExtBlocklightValue(e, x & 15, y & 15, z & 15);
    if (!MCObjectHeap_failed(c->object.heap))
        out = block > light ? block : light;
done:
    MCObjectRootScope_end(&s);
    return out;
}
