#include "client/multiplayer/ChunkProviderClient.h"
#include "util/MCGameplayWorld.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { \
    fprintf(stderr, "WorldChunk check%u line%d: %s\n", checks, __LINE__, #value); \
    exit(1); } } while (0)

static void actual_cache(void) {
    MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
    CHECK(heap);
    World *world = World_nativeAllocate(heap, NULL, NULL);
    CHECK(world);
    ChunkProviderClient *provider = ChunkProviderClient_new(heap, world, NULL, NULL);
    CHECK(provider);
    world->chunkProvider = (MCObject *)provider;
    Chunk *loaded = ChunkProviderClient_loadChunk(provider, -2, 3);
    CHECK(loaded);
    BlockPos *pos = DataWatcher_blockPos(heap, -17, INT32_MIN, 63);
    CHECK(pos);
    size_t objects = MCObjectHeap_liveObjects(heap);
    int32_t count = NativeReferenceList_size(provider->chunkListing);
    CHECK(World_getChunkFromBlockCoords(world, pos) == loaded);
    CHECK(World_getChunkFromChunkCoords(world, -2, 3) == loaded);
    CHECK(!Chunk_isEmpty(loaded));
    CHECK(World_getChunkFromChunkCoords(world, 123, -456) == provider->blankChunk);
    CHECK(Chunk_isEmpty(provider->blankChunk));
    CHECK(World_getChunkFromBlockCoords_base(world, pos) == loaded);
    CHECK(World_getChunkFromChunkCoords_base(world, -2, 3) == loaded);
    CHECK(MCObjectHeap_liveObjects(heap) == objects);
    CHECK(NativeReferenceList_size(provider->chunkListing) == count);
    CHECK(ChunkProviderClient_getLoadedChunkCount(provider) == 1);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

typedef struct Witness {
    MCObject object;
    World *world;
    ChunkProviderClient *provider[2], *called, *replacementProvider;
    BlockPos *pos;
    MCObject *result, *replacementContext;
    const WorldDependencies *replacementMethods;
    MCObjectHeap *working;
    char events[32], failAt, changeContextAt, changeMethodsAt, changeProviderAt;
    unsigned calls;
    int32_t x, z, observedX, observedZ;
    bool sticky, probeScopes, mutateXAfterCapture, changeCapturedContext, reentrant;
} Witness;

static void witness_trace(MCObject *object, MCObjectVisitor visitor, void *context) {
    Witness *w = (Witness *)object;
    w->world = (World *)visitor((MCObject *)w->world, context);
    for (unsigned i = 0; i < 2; ++i)
        w->provider[i] = (ChunkProviderClient *)visitor((MCObject *)w->provider[i], context);
    w->called = (ChunkProviderClient *)visitor((MCObject *)w->called, context);
    w->replacementProvider =
        (ChunkProviderClient *)visitor((MCObject *)w->replacementProvider, context);
    w->pos = (BlockPos *)visitor((MCObject *)w->pos, context);
    w->result = visitor(w->result, context);
    w->replacementContext = visitor(w->replacementContext, context);
    /* working is an externally owned test-only heap, not a managed graph edge. */
}

static const MCObjectClass witnessClass = {
    "test.WorldChunkWitness", MCObjectHeap_plainClone, witness_trace, NULL
};
static const MCObjectClass opaqueClass = {
    "test.NonChunkObject", MCObjectHeap_plainClone, NULL, NULL
};
static bool any_object(const MCObject *object, void *context) {
    (void)object;
    (void)context;
    return true;
}

static bool event(Witness *w, char code) {
    CHECK(w->object.klass == &witnessClass);
    CHECK(MCObjectHeap_hasBorrowers(w->object.heap));
    CHECK(w->calls + 1 < sizeof(w->events));
    w->events[w->calls++] = code;
    w->events[w->calls] = 0;
    ++w->world->seaLevel;
    if (w->probeScopes) {
        CHECK(!MCObjectHeap_collect(w->object.heap));
        CHECK(w->working && !MCObjectHeap_canAdopt(w->object.heap, w->working));
        CHECK(!MCObjectHeap_failed(w->object.heap));
    }
    if (w->changeContextAt == code)
        w->world->dependencyContext = w->replacementContext;
    if (w->changeMethodsAt == code)
        w->world->dependencies = w->replacementMethods;
    if (w->changeProviderAt == code)
        w->world->chunkProvider = (MCObject *)w->replacementProvider;
    if (code == 'p' && w->changeCapturedContext)
        w->called->dependencyContext = w->replacementContext;
    if (w->failAt == code) {
        if (w->sticky)
            MCObjectHeap_fail(w->object.heap);
        return false;
    }
    return true;
}

static bool get_x(MCObject *context, BlockPos *pos, int32_t *out) {
    Witness *w = (Witness *)context;
    CHECK(pos == w->pos);
    if (!event(w, 'x'))
        return false;
    *out = w->x;
    return true;
}
static bool get_z(MCObject *context, BlockPos *pos, int32_t *out) {
    Witness *w = (Witness *)context;
    CHECK(pos == w->pos);
    if (!event(w, 'z'))
        return false;
    *out = w->z;
    if (w->mutateXAfterCapture)
        w->x = INT32_MAX;
    return true;
}
static bool get_z_changed(MCObject *context, BlockPos *pos, int32_t *out) {
    Witness *w = (Witness *)context;
    CHECK(pos == w->pos);
    if (!event(w, 'Z'))
        return false;
    *out = w->z;
    return true;
}
static MCObject *coords(MCObject *context, World *world, int32_t x, int32_t z) {
    Witness *w = (Witness *)context;
    CHECK(world == w->world);
    w->observedX = x;
    w->observedZ = z;
    if (!event(w, 'c'))
        return NULL;
    return w->reentrant ? (MCObject *)World_getChunkFromChunkCoords_base(world, x, z)
                        : w->result;
}
static MCObject *coords_changed(MCObject *context, World *world, int32_t x, int32_t z) {
    Witness *w = (Witness *)context;
    CHECK(world == w->world);
    w->observedX = x;
    w->observedZ = z;
    return event(w, 'C') ? w->result : NULL;
}
static MCObject *blocks(MCObject *context, World *world, BlockPos *pos) {
    Witness *w = (Witness *)context;
    CHECK(world == w->world && (pos == w->pos || !pos));
    return event(w, 'b') ? w->result : NULL;
}
static Chunk *provided(MCObject *context, ChunkProviderClient *provider, int32_t x, int32_t z) {
    Witness *w = (Witness *)context;
    w->called = provider;
    w->observedX = x;
    w->observedZ = z;
    return event(w, 'p') ? (Chunk *)w->result : NULL;
}

static const WorldDependencies coordinates = {
    .positionGetX = get_x, .positionGetZ = get_z, .getChunkFromChunkCoords = coords
};
static const WorldDependencies changedCoordinates = {
    .positionGetX = get_x, .positionGetZ = get_z_changed,
    .getChunkFromChunkCoords = coords_changed
};
static const WorldDependencies blockOverride = {.getChunkFromBlockCoords = blocks};
static const WorldDependencies positionOnly = {.positionGetX = get_x, .positionGetZ = get_z};
static const ChunkProviderClientDependencies providerOverride = {.provideChunk = provided};

static Witness *fixture(MCObjectHeap *heap) {
    Witness *w = (Witness *)MCObjectHeap_alloc(heap, sizeof(*w), &witnessClass);
    CHECK(w);
    w->world = World_nativeAllocate(heap, &coordinates, (MCObject *)w);
    CHECK(w->world);
    for (unsigned i = 0; i < 2; ++i) {
        w->provider[i] = ChunkProviderClient_new(heap, w->world, NULL, NULL);
        CHECK(w->provider[i]);
    }
    w->world->chunkProvider = (MCObject *)w->provider[0];
    w->pos = DataWatcher_blockPos(heap, 123, INT32_MIN, -456);
    CHECK(w->pos);
    w->x = -17;
    w->z = 63;
    w->result = (MCObject *)w->provider[0]->blankChunk;
    return w;
}

static void ordered_dispatch(void) {
    static const int32_t inputs[] = {
        INT32_MIN, -17, -16, -15, -1, 0, 15, 16, INT32_MAX
    };
    static const int32_t expected[] = {
        -134217728, -2, -1, -1, -1, 0, 0, 1, 134217727
    };
    for (unsigned i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
        CHECK(heap);
        Witness *w = fixture(heap);
        w->x = inputs[i];
        w->z = inputs[sizeof(inputs) / sizeof(inputs[0]) - 1 - i];
        w->mutateXAfterCapture = true;
        CHECK(World_getChunkFromBlockCoords(w->world, w->pos) == (Chunk *)w->result);
        CHECK(!strcmp(w->events, "xzc"));
        CHECK(w->observedX == expected[i]);
        CHECK(w->observedZ == expected[sizeof(inputs) / sizeof(inputs[0]) - 1 - i]);
        CHECK(!MCObjectHeap_hasBorrowers(heap) && !MCObjectHeap_failed(heap));
        MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
    Witness *w = fixture(heap);
    w->changeMethodsAt = 'x';
    w->replacementMethods = &changedCoordinates;
    CHECK(World_getChunkFromBlockCoords(w->world, w->pos) == (Chunk *)w->result);
    CHECK(!strcmp(w->events, "xZC"));
    CHECK(w->observedX == -2 && w->observedZ == 3);
    MCObjectHeap_free(heap);

    heap = MCObjectHeap_new(32u * 1024u * 1024u);
    w = fixture(heap);
    Witness *next = fixture(heap);
    next->world = w->world;
    next->pos = w->pos;
    next->z = -33;
    w->changeContextAt = 'x';
    w->replacementContext = (MCObject *)next;
    CHECK(World_getChunkFromBlockCoords(w->world, w->pos) == (Chunk *)next->result);
    CHECK(!strcmp(w->events, "x") && !strcmp(next->events, "zc"));
    CHECK(next->observedX == -2 && next->observedZ == -3);
    CHECK(!MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
}

static void provider_capture_and_null(void) {
    for (unsigned stage = 0; stage < 3; ++stage) {
        MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
        Witness *w = fixture(heap);
        w->world->dependencies = &positionOnly;
        for (unsigned i = 0; i < 2; ++i) {
            w->provider[i]->dependencies = &providerOverride;
            w->provider[i]->dependencyContext = (MCObject *)w;
        }
        w->changeProviderAt = stage == 0 ? 'x' : stage == 1 ? 'z' : 'p';
        w->replacementProvider = w->provider[1];
        CHECK(World_getChunkFromBlockCoords(w->world, w->pos) == (Chunk *)w->result);
        CHECK(!strcmp(w->events, "xzp"));
        CHECK(w->called == w->provider[stage == 2 ? 0 : 1]);
        CHECK(w->world->chunkProvider == (MCObject *)w->provider[1]);
        CHECK(w->observedX == -2 && w->observedZ == 3);
        CHECK(!MCObjectHeap_failed(heap));
        MCObjectHeap_free(heap);
    }
    for (unsigned mode = 0; mode < 3; ++mode) {
        MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
        Witness *w = fixture(heap);
        w->result = NULL;
        if (mode == 1) {
            w->world->dependencies = NULL;
            w->provider[0]->dependencies = &providerOverride;
            w->provider[0]->dependencyContext = (MCObject *)w;
        } else if (mode == 2) {
            w->world->dependencies = &blockOverride;
        }
        CHECK(!World_getChunkFromBlockCoords(w->world, mode == 2 ? NULL : w->pos));
        CHECK(!strcmp(w->events, mode == 0 ? "xzc" : mode == 1 ? "p" : "b"));
        CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
    Witness *w = fixture(heap);
    w->world->dependencies = NULL;
    CHECK(!World_getChunkFromBlockCoords(w->world, NULL));
    CHECK(MCObjectHeap_failed(heap) && !w->calls && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
    heap = MCObjectHeap_new(32u * 1024u * 1024u);
    w = fixture(heap);
    w->world->dependencies = &positionOnly;
    w->world->chunkProvider = NULL;
    CHECK(!World_getChunkFromBlockCoords(w->world, w->pos));
    CHECK(!strcmp(w->events, "xz") && MCObjectHeap_failed(heap));
    CHECK(!MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

static void repaired_edge_and_reentrant(void) {
    MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
    Witness *w = fixture(heap);
    /* The source arguments repair an initially unsupported/untracked provider
       before the field is reached; eagerly inspecting it would fail early. */
    MCObject untracked = {heap, w->provider[0]->object.klass};
    w->world->chunkProvider = &untracked;
    w->changeProviderAt = 'x';
    w->replacementProvider = w->provider[0];
    w->reentrant = true;
    CHECK(World_getChunkFromBlockCoords(w->world, w->pos) ==
          w->provider[0]->blankChunk);
    CHECK(!strcmp(w->events, "xzc"));
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);

    for (unsigned status = 0; status < 3; ++status) {
        heap = MCObjectHeap_new(32u * 1024u * 1024u);
        w = fixture(heap);
        MCObject context = {heap, &opaqueClass};
        w->changeContextAt = 'c';
        w->replacementContext = &context;
        if (status) {
            w->failAt = 'c';
            w->sticky = status == 2;
        }
        CHECK(!World_getChunkFromChunkCoords(w->world, 1, 2));
        CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
        CHECK(w->world->dependencyContext == &context);
        CHECK(w->world->seaLevel == 1);
        MCObjectHeap_free(heap);
    }

    heap = MCObjectHeap_new(32u * 1024u * 1024u);
    w = fixture(heap);
    w->world->dependencies = NULL;
    w->provider[0]->dependencies = &providerOverride;
    w->provider[0]->dependencyContext = (MCObject *)w;
    w->changeCapturedContext = true;
    w->replacementContext = NULL;
    CHECK(World_getChunkFromChunkCoords(w->world, 1, 2) == (Chunk *)w->result);
    CHECK(w->provider[0]->dependencyContext == NULL);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectHeap_free(heap);
}

static void failure_prefixes(void) {
    const char stages[] = {'x', 'z', 'c', 'b', 'p'};
    for (unsigned i = 0; i < sizeof(stages); ++i) {
        for (unsigned sticky = 0; sticky < 2; ++sticky) {
            MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
            Witness *w = fixture(heap);
            w->failAt = stages[i];
            w->sticky = sticky != 0;
            if (stages[i] == 'b')
                w->world->dependencies = &blockOverride;
            if (stages[i] == 'p') {
                w->world->dependencies = NULL;
                w->provider[0]->dependencies = &providerOverride;
                w->provider[0]->dependencyContext = (MCObject *)w;
            }
            CHECK(!World_getChunkFromBlockCoords(w->world, w->pos));
            CHECK(!strcmp(w->events, i == 0 ? "x" : i == 1 ? "xz" :
                                          i == 2 ? "xzc" : i == 3 ? "b" : "p"));
            /* Pointer callbacks can return healthy NULL; bool getter false
               is the native exception boundary even without sticky failure. */
            CHECK(MCObjectHeap_failed(heap) == (sticky != 0 || i < 2));
            CHECK(w->world->seaLevel == (int32_t)w->calls);
            CHECK(!MCObjectHeap_hasBorrowers(heap));
            MCObjectHeap_free(heap);
        }
    }
}

static void context_guards(void) {
    const char stages[] = {'x', 'z', 'c', 'b', 'p'};
    for (unsigned i = 0; i < sizeof(stages); ++i) {
        for (unsigned status = 0; status < 3; ++status) {
            MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
            MCObjectHeap *foreign = MCObjectHeap_new(1024);
            CHECK(heap && foreign);
            Witness *w = fixture(heap);
            w->replacementContext = MCObjectHeap_alloc(foreign, sizeof(MCObject), &opaqueClass);
            CHECK(w->replacementContext);
            w->changeContextAt = stages[i];
            if (status) {
                w->failAt = stages[i];
                w->sticky = status == 2;
            }
            if (stages[i] == 'b')
                w->world->dependencies = &blockOverride;
            if (stages[i] == 'p') {
                w->world->dependencies = NULL;
                w->provider[0]->dependencies = &providerOverride;
                w->provider[0]->dependencyContext = (MCObject *)w;
            }
            CHECK(!World_getChunkFromBlockCoords(w->world, w->pos));
            CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_failed(foreign));
            CHECK(w->world->dependencyContext == w->replacementContext);
            CHECK(w->world->seaLevel == (int32_t)w->calls);
            CHECK(!MCObjectHeap_hasBorrowers(heap));
            MCObjectHeap_free(heap);
            MCObjectHeap_free(foreign);
        }
    }
    for (unsigned status = 0; status < 3; ++status) {
        MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
        MCObjectHeap *foreign = MCObjectHeap_new(1024);
        Witness *w = fixture(heap);
        w->world->dependencies = NULL;
        w->provider[0]->dependencies = &providerOverride;
        w->provider[0]->dependencyContext = (MCObject *)w;
        w->replacementContext = MCObjectHeap_alloc(foreign, sizeof(MCObject), &opaqueClass);
        CHECK(w->replacementContext);
        w->changeCapturedContext = true;
        if (status) {
            w->failAt = 'p';
            w->sticky = status == 2;
        }
        CHECK(!World_getChunkFromChunkCoords(w->world, 0, 0));
        CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_failed(foreign));
        CHECK(w->provider[0]->dependencyContext == w->replacementContext);
        CHECK(w->world->dependencyContext == (MCObject *)w);
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
        MCObjectHeap_free(foreign);
    }
}

static void terminal_provider_edge(void) {
    const char stages[] = {'c', 'b', 'p'};
    for (unsigned i = 0; i < sizeof(stages); ++i) {
        for (unsigned replacement = 0; replacement < 3; ++replacement) {
            for (unsigned status = 0; status < 3; ++status) {
                MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
                MCObjectHeap *foreign = MCObjectHeap_new(1024);
                Witness *w = fixture(heap);
                w->changeProviderAt = stages[i];
                if (replacement)
                    w->replacementProvider = (ChunkProviderClient *)MCObjectHeap_alloc(
                        replacement == 1 ? heap : foreign, sizeof(MCObject), &opaqueClass);
                if (status) {
                    w->failAt = stages[i];
                    w->sticky = status == 2;
                }
                if (stages[i] == 'b')
                    w->world->dependencies = &blockOverride;
                if (stages[i] == 'p') {
                    w->world->dependencies = NULL;
                    w->provider[0]->dependencies = &providerOverride;
                    w->provider[0]->dependencyContext = (MCObject *)w;
                }
                Chunk *out = World_getChunkFromBlockCoords(w->world, w->pos);
                CHECK(out == (status || replacement == 2 ? NULL : (Chunk *)w->result));
                CHECK(MCObjectHeap_failed(heap) == (replacement == 2 || status == 2));
                CHECK(!MCObjectHeap_failed(foreign));
                CHECK(w->world->chunkProvider == (MCObject *)w->replacementProvider);
                if (stages[i] == 'p')
                    CHECK(w->called == w->provider[0]);
                CHECK(w->world->seaLevel == (int32_t)w->calls);
                CHECK(!MCObjectHeap_hasBorrowers(heap));
                MCObjectHeap_free(heap);
                MCObjectHeap_free(foreign);
            }
        }
    }
}

static void malformed_arguments(void) {
    for (unsigned mode = 0; mode < 8; ++mode) {
        MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
        MCObjectHeap *foreign = MCObjectHeap_new(32u * 1024u * 1024u);
        CHECK(heap && foreign);
        Witness *w = fixture(heap);
        w->world->dependencies = NULL;
        MCObject untracked = {heap, w->pos->object.klass};
        if (mode == 0)
            w->pos = (BlockPos *)MCObjectHeap_alloc(heap, sizeof(MCObject), w->pos->object.klass);
        else if (mode == 1)
            w->pos = DataWatcher_blockPos(foreign, 0, 0, 0);
        else if (mode == 2)
            w->pos = (BlockPos *)&untracked;
        else if (mode == 3)
            w->world->chunkProvider = MCObjectHeap_alloc(heap, sizeof(MCObject), w->provider[0]->object.klass);
        else if (mode == 4)
            w->world->chunkProvider = (MCObject *)ChunkProviderClient_new(foreign, NULL, NULL, NULL);
        else if (mode == 5) {
            untracked.klass = w->provider[0]->object.klass;
            w->world->chunkProvider = &untracked;
        } else if (mode == 6) {
            w->world->dependencyContext = &untracked;
        } else {
            World *short_world = (World *)MCObjectHeap_alloc(heap, sizeof(MCObject), w->world->object.klass);
            CHECK(short_world);
            CHECK(!World_getChunkFromChunkCoords(short_world, 0, 0));
        }
        if (mode < 7)
            CHECK(!World_getChunkFromBlockCoords(w->world, w->pos));
        CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_failed(foreign));
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
        MCObjectHeap_free(foreign);
    }
    for (unsigned mode = 0; mode < 4; ++mode) {
        MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
        MCObjectHeap *foreign = MCObjectHeap_new(32u * 1024u * 1024u);
        Witness *w = fixture(heap);
        MCObject untracked = {heap, w->result->klass};
        if (mode == 0)
            w->result = MCObjectHeap_alloc(heap, sizeof(MCObject), w->result->klass);
        else if (mode == 1)
            w->result = (MCObject *)EmptyChunk_new(foreign, NULL, 0, 0, NULL);
        else if (mode == 2)
            w->result = &untracked;
        else
            w->result = MCObjectHeap_alloc(heap, sizeof(MCObject), &opaqueClass);
        CHECK(w->result);
        CHECK(!World_getChunkFromChunkCoords(w->world, 0, 0));
        CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_failed(foreign));
        CHECK(!strcmp(w->events, "c") && !MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);
        MCObjectHeap_free(foreign);
    }
}

static void scope_and_graph(void) {
    MCObjectHeap *heap = MCObjectHeap_new(32u * 1024u * 1024u);
    Witness *w = fixture(heap);
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, heap, (MCObject *)w));
    w->probeScopes = true;
    w->working = MCObjectHeap_clone(heap);
    CHECK(w->working);
    CHECK(World_getChunkFromBlockCoords(w->world, w->pos) == (Chunk *)w->result);
    CHECK(!strcmp(w->events, "xzc"));
    MCObjectHeap_free(w->working);
    w->working = NULL;
    w->probeScopes = false;
    CHECK(MCObjectHeap_collect(heap));
    w = (Witness *)MCObjectRoot_get(&root);
    CHECK(w->provider[0]->worldObj == w->world);
    CHECK(w->provider[0]->blankChunk->worldObj == w->world);
    CHECK(w->result == (MCObject *)w->provider[0]->blankChunk);
    MCObjectHeap *copy = MCObjectHeap_clone(heap);
    CHECK(copy);
    MCObjectRoot copied = {0};
    CHECK(MCObjectRoot_rebind(&copied, copy, &root));
    Witness *cw = (Witness *)MCObjectRoot_get(&copied);
    CHECK(cw != w && cw->world != w->world);
    CHECK(cw->world->dependencyContext == (MCObject *)cw);
    CHECK(cw->world->chunkProvider == (MCObject *)cw->provider[0]);
    CHECK(cw->provider[0]->worldObj == cw->world);
    CHECK(cw->result == (MCObject *)cw->provider[0]->blankChunk);
    CHECK(MCObjectHeap_collect(copy));
    CHECK(MCObjectHeap_adopt(heap, copy));
    MCObjectHeap_free(copy);
    w = (Witness *)MCObjectRoot_get(&root);
    CHECK(World_getChunkFromBlockCoords(w->world, w->pos) == (Chunk *)w->result);
    CHECK(!MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(heap));
    /* Class-static Chunk dependencies may retain canonical roots. The test's
       nonstatic World/provider/cache cycle itself must no longer be reachable. */
    CHECK(MCObjectHeap_findObject(heap, &witnessClass, any_object, NULL) == NULL);
    MCObjectHeap_free(heap);
}

static void dense_is_not_source_chunk(void) {
    mc_world *terrain = (mc_world *)malloc(sizeof(*terrain));
    CHECK(terrain);
    mc_world_init(terrain, 919);
    CHECK(mc_world_chunk(terrain, 0, 0, true));
    MCGameplay game = {0};
    CHECK(MCGameplay_init(&game, 32u * 1024u * 1024u));
    World *world = MCGameplayWorld_new(game.heap, MCGameplay_get(&game), terrain, NULL);
    CHECK(world);
    CHECK(!world->chunkProvider);
    BlockPos *pos = DataWatcher_blockPos(game.heap, 0, 123, 0);
    CHECK(pos);
    BlockPos *height = World_getHeight(world, pos);
    CHECK(height && height->y == 0 && !MCObjectHeap_failed(game.heap));
    CHECK(!World_getChunkFromChunkCoords(world, 0, 0));
    CHECK(MCObjectHeap_failed(game.heap) && !MCObjectHeap_hasBorrowers(game.heap));
    MCGameplay_free(&game);
    mc_world_free(terrain);
    free(terrain);
}

int main(void) {
    actual_cache();
    ordered_dispatch();
    provider_capture_and_null();
    repaired_edge_and_reentrant();
    failure_prefixes();
    context_guards();
    malformed_arguments();
    scope_and_graph();
    dense_is_not_source_chunk();
    terminal_provider_edge();
    printf("Source World chunk lookup: %u checks passed\n", checks);
    return 0;
}
