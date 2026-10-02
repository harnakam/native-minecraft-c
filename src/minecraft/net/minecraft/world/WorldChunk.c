#include "world/World.h"
#include "client/multiplayer/ChunkProviderClient.h"
#include <string.h>

/* The two Source World lookup bodies. The reached IChunkProvider interface
   dispatch below supports the translated client provider; other concrete
   providers are unported dependencies, not dense views or fabricated chunks.
   NULL invocation/native dependency errors use the existing sticky-heap
   boundary. A successful nullable Chunk result stays nullable. */
static bool lookup_fail(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return false;
}

static bool same_object(const MCObject *candidate, void *context) {
    return candidate == (const MCObject *)context;
}

/* objectSize is an allocation-prefix accessor, not a membership predicate.
   Check the actual heap's tracked object first, including callback contexts. */
static bool tracked(MCObjectHeap *heap, MCObject *object, size_t minimum) {
    return object && object->heap == heap && object->klass &&
           MCObjectHeap_findObject(heap, object->klass, same_object, object) == object &&
           MCObjectHeap_objectSize(object) >= minimum;
}

static bool world_shape(World *world) {
    MCObjectHeap *heap = world ? world->object.heap : NULL;
    return tracked(heap, (MCObject *)world, sizeof(*world)) &&
           World_isInstance((MCObject *)world);
}

static bool pin_reference(MCObjectHeap *heap, MCObjectRootScope *scope, MCObject *object) {
    if (object && !tracked(heap, object, sizeof(*object)))
        return lookup_fail(heap);
    return MCObjectRootScope_pin(scope, object) || lookup_fail(heap);
}

/* Called before each native callback and after EVERY completion status.
   Final guards do not erase callback effects or clear an existing failure. */
static bool current_context(World *world, MCObjectRootScope *scope) {
    if (!world_shape(world))
        return lookup_fail(world ? world->object.heap : NULL);
    return pin_reference(world->object.heap, scope, world->dependencyContext);
}

static bool begin_lookup(World *world, MCObjectRootScope *scope) {
    MCObjectHeap *heap = world ? world->object.heap : NULL;
    if (!world_shape(world) || MCObjectHeap_failed(heap) ||
        !MCObjectRootScope_begin(scope, heap))
        return lookup_fail(heap);
    if (pin_reference(heap, scope, (MCObject *)world) && current_context(world, scope))
        return true;
    MCObjectRootScope_end(scope);
    return false;
}

static bool finish_lookup(World *world, MCObjectRootScope *scope, bool ok,
                          bool check_provider_edge) {
    /* Do not short-circuit the current-context check on callback failure. */
    bool context_ok = current_context(world, scope);
    /* Only after a reached chunk invocation: retaining an unrelated-heap
       replacement would corrupt the managed graph. Do not redispatch, demand
       a supported concrete type, or read this field during coordinate args. */
    bool provider_ok = !check_provider_edge ||
                       (world_shape(world) &&
                        pin_reference(scope->heap, scope, world->chunkProvider));
    ok = ok && context_ok && provider_ok && !MCObjectHeap_failed(scope->heap);
    if (!ok)
        lookup_fail(scope->heap);
    MCObjectRootScope_end(scope);
    return ok;
}

static bool position(World *world, MCObjectRootScope *scope, BlockPos *pos, bool nullable) {
    if (!pos)
        return nullable || lookup_fail(world->object.heap);
    return pin_reference(world->object.heap, scope, (MCObject *)pos) &&
           (BlockPos_isInstance((MCObject *)pos) || lookup_fail(world->object.heap));
}

static bool chunk_result(World *world, MCObjectRootScope *scope, Chunk *chunk) {
    return !chunk || (pin_reference(world->object.heap, scope, (MCObject *)chunk) &&
                     (Chunk_isInstance((MCObject *)chunk) || lookup_fail(world->object.heap)));
}

static bool coordinate(World *world, MCObjectRootScope *scope, BlockPos *pos,
                       bool x_axis, int32_t *out) {
    if (!position(world, scope, pos, false) || !current_context(world, scope))
        return false;
    const WorldDependencies *methods = world->dependencies;
    bool (*method)(MCObject *, BlockPos *, int32_t *) =
        methods ? (x_axis ? methods->positionGetX : methods->positionGetZ) : NULL;
    if (!method) {
        NativeArrayResult r = x_axis ? Vec3i_getX(&pos->vec3i, out)
                                     : Vec3i_getZ(&pos->vec3i, out);
        return r == NATIVE_ARRAY_OK || lookup_fail(world->object.heap);
    }
    MCObject *context = world->dependencyContext;
    bool ok = method(context, pos, out);
    bool context_ok = current_context(world, scope);
    return (ok && context_ok && !MCObjectHeap_failed(world->object.heap)) ||
           lookup_fail(world->object.heap);
}

static int32_t shift_four(int32_t value) {
    uint32_t bits = (uint32_t)value;
    bits = (bits >> 4) | (value < 0 ? UINT32_C(0xf0000000) : 0);
    int32_t result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

Chunk *World_getChunkFromBlockCoords_base(World *world, BlockPos *pos) {
    MCObjectRootScope scope = {0};
    if (!begin_lookup(world, &scope))
        return NULL;
    bool ok = false, invoked = false;
    Chunk *out = NULL;
    int32_t x, z;
    /* Source captures this/pos, evaluates X then Z, and only then dispatches
       the virtual chunk-coordinate call. Provider/table must not be cached. */
    if (!coordinate(world, &scope, pos, true, &x))
        goto done;
    x = shift_four(x);
    if (!coordinate(world, &scope, pos, false, &z))
        goto done;
    z = shift_four(z);
    invoked = true;
    out = World_getChunkFromChunkCoords(world, x, z);
    ok = chunk_result(world, &scope, out);
done:
    return finish_lookup(world, &scope, ok, invoked) ? out : NULL;
}

Chunk *World_getChunkFromBlockCoords(World *world, BlockPos *pos) {
    MCObjectRootScope scope = {0};
    if (!begin_lookup(world, &scope))
        return NULL;
    Chunk *out = NULL;
    bool invoked = false;
    bool ok = position(world, &scope, pos, true);
    if (ok) {
        const WorldDependencies *methods = world->dependencies;
        if (methods && methods->getChunkFromBlockCoords) {
            if (current_context(world, &scope)) {
                MCObject *context = world->dependencyContext;
                invoked = true;
                out = (Chunk *)methods->getChunkFromBlockCoords(context, world, pos);
                /* Callback status is pointer plus sticky heap, not Throwable. */
                ok = current_context(world, &scope);
            } else {
                ok = false;
            }
        } else {
            out = World_getChunkFromBlockCoords_base(world, pos);
        }
        bool result_ok = chunk_result(world, &scope, out);
        ok = ok && result_ok;
    }
    return finish_lookup(world, &scope, ok, invoked) ? out : NULL;
}

static bool provider_context(World *world, MCObjectRootScope *scope,
                             ChunkProviderClient *provider) {
    MCObjectHeap *heap = world->object.heap;
    if (!tracked(heap, (MCObject *)provider, sizeof(*provider)) ||
        !ChunkProviderClient_isInstance((MCObject *)provider))
        return lookup_fail(heap);
    return pin_reference(heap, scope, (MCObject *)provider) &&
           pin_reference(heap, scope, provider->dependencyContext);
}

Chunk *World_getChunkFromChunkCoords_base(World *world, int32_t x, int32_t z) {
    MCObjectRootScope scope = {0};
    if (!begin_lookup(world, &scope))
        return NULL;
    /* Source interface invocation captures CURRENT field receiver here. */
    ChunkProviderClient *provider = (ChunkProviderClient *)world->chunkProvider;
    Chunk *out = NULL;
    bool invoked = false;
    bool ok = provider_context(world, &scope, provider);
    if (ok) {
        invoked = true;
        out = ChunkProviderClient_provideChunk(provider, x, z);
        /* Check both current contexts even after failed native invocation.
           A replacement world->chunkProvider does not retarget this receiver. */
        bool provider_ok = provider_context(world, &scope, provider);
        bool world_ok = current_context(world, &scope);
        bool result_ok = chunk_result(world, &scope, out);
        ok = provider_ok && world_ok && result_ok;
    }
    return finish_lookup(world, &scope, ok, invoked) ? out : NULL;
}

Chunk *World_getChunkFromChunkCoords(World *world, int32_t x, int32_t z) {
    MCObjectRootScope scope = {0};
    if (!begin_lookup(world, &scope))
        return NULL;
    const WorldDependencies *methods = world->dependencies;
    Chunk *out = NULL;
    bool ok = true, invoked = false;
    if (methods && methods->getChunkFromChunkCoords) {
        if (current_context(world, &scope)) {
            MCObject *context = world->dependencyContext;
            invoked = true;
            out = (Chunk *)methods->getChunkFromChunkCoords(context, world, x, z);
            ok = current_context(world, &scope);
        } else {
            ok = false;
        }
    } else {
        out = World_getChunkFromChunkCoords_base(world, x, z);
    }
    bool result_ok = chunk_result(world, &scope, out);
    ok = ok && result_ok;
    return finish_lookup(world, &scope, ok, invoked) ? out : NULL;
}
