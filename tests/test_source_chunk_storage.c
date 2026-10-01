#include "world/chunk/ChunkPrimer.h"
#include "world/chunk/storage/ExtendedBlockStorage.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        checks++;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "chunk storage check %u failed line %d: %s\n", checks, __LINE__, #x);  \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static void registry_and_storage(void) {
    MCObjectHeap *h = MCObjectHeap_new(16u * 1024u * 1024u);
    CHECK(h);
    NativeBlockStateRuntime *r = NativeBlockStateRuntime_get(h);
    CHECK(r);
    CHECK(r == NativeBlockStateRuntime_get(h));
    NativeBlockState *air = NativeBlock_getDefaultState(r->air),
                     *stone = NativeBlockStateRuntime_state(r, 16);
    CHECK(air && stone);
    CHECK(NativeBlockStateRuntime_state(r, 22) != NULL &&
          NativeBlockStateRuntime_state(r, 23) == NULL);
    NativeBlockState *grass0 = NativeBlockStateRuntime_validState(r, 2, 0),
                     *grass1 = NativeBlockStateRuntime_validState(r, 2, 1);
    CHECK(grass0 && grass1 && grass0 != grass1 &&
          ObjectIntIdentityMap_get(r->BLOCK_STATE_IDS, (MCObject *)grass0) == 32);
    CHECK(NativeBlockStateRuntime_state(r, 32) == grass1);
    ExtendedBlockStorage *e = ExtendedBlockStorage_new(h, 32, true, r);
    CHECK(e);
    CHECK(e->yBase == 32 && e->data->length == 4096 && e->blocklightArray->data->length == 2048 &&
          e->skylightArray->data->length == 2048);
    CHECK(ExtendedBlockStorage_isEmpty(e) && !ExtendedBlockStorage_getNeedsRandomTick(e));
    CHECK(ExtendedBlockStorage_set(e, 0, 0, 0, stone));
    CHECK(e->blockRefCount == 1 && e->tickRefCount == 0);
    CHECK(ExtendedBlockStorage_get(e, 0, 0, 0) == stone &&
          ExtendedBlockStorage_getBlockByExtId(e, 0, 0, 0) == stone->block);
    CHECK(ExtendedBlockStorage_set(e, 1, 0, 0, grass0));
    CHECK(ExtendedBlockStorage_get(e, 1, 0, 0) == grass1 && e->blockRefCount == 2 &&
          e->tickRefCount == 1);
    CHECK(ExtendedBlockStorage_set(e, 0, 0, 0, air));
    CHECK(e->blockRefCount == 1);
    e->data->values[0] = 23;
    CHECK(ExtendedBlockStorage_get(e, 0, 0, 0) == air);
    CHECK(ExtendedBlockStorage_removeInvalidBlocks(e) && e->blockRefCount == 1);
    NativeBlockState *foreign = NativeBlockState_nativeNew(r, stone->block, 0);
    CHECK(foreign);
    CHECK(ExtendedBlockStorage_set(e, 2, 0, 0, foreign));
    CHECK(e->data->values[2] == 65535 && e->blockRefCount == 2);
    CHECK(ExtendedBlockStorage_get(e, 2, 0, 0) == air);
    CHECK(ExtendedBlockStorage_removeInvalidBlocks(e) && e->blockRefCount == 1);
    CHECK(ExtendedBlockStorage_setExtBlocklightValue(e, 0, 0, 0, 31) &&
          ExtendedBlockStorage_getExtBlocklightValue(e, 0, 0, 0) == 15);
    CHECK(ExtendedBlockStorage_setExtSkylightValue(e, 1, 0, 0, -1) &&
          ExtendedBlockStorage_getExtSkylightValue(e, 1, 0, 0) == 15);
    NibbleArray *same = ExtendedBlockStorage_getBlocklightArray(e);
    CHECK(ExtendedBlockStorage_setSkylightArray(e, same) && e->skylightArray == same);
    CHECK(NibbleArray_set(same, 1, 0, 0, 7) &&
          ExtendedBlockStorage_getExtSkylightValue(e, 1, 0, 0) == 7);
    ChunkPrimer *p = ChunkPrimer_new(h, r);
    CHECK(p && p->data->length == 65536 && p->defaultState == air);
    CHECK(ChunkPrimer_setBlockState(p, 0, 0, 16, stone) &&
          ChunkPrimer_getBlockState(p, 1, 0, 0) == stone);
    CHECK(ChunkPrimer_setBlockStateAt(p, 0, foreign) && p->data->values[0] == -1 &&
          ChunkPrimer_getBlockStateAt(p, 0) == air);
    p->data->values[1] = INT16_MIN;
    CHECK(ChunkPrimer_getBlockStateAt(p, 1) == air);
    NativeBlockState *saved = r->air->defaultState;
    r->air->defaultState = stone;
    CHECK(ExtendedBlockStorage_get(e, 0, 0, 0) == stone &&
          ChunkPrimer_getBlockStateAt(p, 0) == saved);
    r->air->defaultState = saved;
    NativeObjectArray *roots = NativeObjectArray_new(h, 2);
    CHECK(roots);
    CHECK(NativeObjectArray_set(roots, 0, (MCObject *)e) &&
          NativeObjectArray_set(roots, 1, (MCObject *)p));
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, h, (MCObject *)roots));
    CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *clone = MCObjectHeap_clone(h);
    CHECK(clone);
    MCObjectRoot cr = {0};
    CHECK(MCObjectRoot_rebind(&cr, clone, &root));
    roots = (NativeObjectArray *)MCObjectRoot_get(&cr);
    ExtendedBlockStorage *ce = (ExtendedBlockStorage *)roots->values[0];
    ChunkPrimer *cp = (ChunkPrimer *)roots->values[1];
    CHECK(ce != e && ce->blocklightArray == ce->skylightArray &&
          ce->nativeRuntime == cp->nativeRuntime);
    CHECK(cp->defaultState == ce->nativeRuntime->air->defaultState && cp->defaultState != air);
    CHECK(ExtendedBlockStorage_get(ce, 1, 0, 0) ==
          NativeBlockStateRuntime_state(ce->nativeRuntime, 32));
    CHECK(MCObjectHeap_canAdopt(h, clone) && MCObjectHeap_adopt(h, clone));
    CHECK(MCObjectHeap_collect(h));
    MCObjectRoot_drop(&root);
    MCObjectHeap_free(clone);
    MCObjectHeap_free(h);
}
static void nibble_and_failure_prefixes(void) {
    MCObjectHeap *h = MCObjectHeap_new(65536);
    NativeByteArray *bytes = NativeByteArray_new(h, 2048);
    CHECK(bytes);
    NibbleArray *n = NibbleArray_newWithArray(h, bytes);
    CHECK(n && NibbleArray_getData(n) == bytes);
    CHECK(NibbleArray_setIndex(n, 0, 10) && NibbleArray_setIndex(n, 1, 11) &&
          bytes->values[0] == -70);
    CHECK(NibbleArray_getFromIndex(n, 0) == 10 && NibbleArray_getFromIndex(n, 1) == 11);
    CHECK(NibbleArray_set(n, 16, 0, 0, 5) && NibbleArray_get(n, 0, 0, 1) == 5);
    CHECK(NibbleArray_set(n, 0, 16, 0, 9) == false && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(65536);
    n = NibbleArray_nativeAllocate(h);
    bytes = NativeByteArray_new(h, 1);
    CHECK(n && bytes);
    CHECK(!NibbleArray_constructWithArray(n, bytes) && n->data == bytes && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(16u * 1024u * 1024u);
    NativeBlockStateRuntime *r = NativeBlockStateRuntime_get(h);
    CHECK(r);
    ExtendedBlockStorage *e = ExtendedBlockStorage_new(h, 0, false, r);
    CHECK(e && !e->skylightArray);
    CHECK(ExtendedBlockStorage_setData(e, NULL) && e->data == NULL && !MCObjectHeap_failed(h));
    CHECK(ExtendedBlockStorage_get(e, 0, 0, 0) == NULL && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(16u * 1024u * 1024u);
    r = NativeBlockStateRuntime_get(h);
    e = ExtendedBlockStorage_new(h, 0, true, r);
    CHECK(e);
    CHECK(!ExtendedBlockStorage_set(e, 0, 0, 0, NULL) && e->blockRefCount == 0 &&
          e->data->values[0] == 0 && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}

typedef struct CallbackContext {
    MCObject object;
    ExtendedBlockStorage *storage;
    NativeCharArray *replacement;
    NativeBlockState *oldState, *newState;
    int events[8], used, failAt;
    bool swapArray;
    int recountIndex;
} CallbackContext;
static void context_trace(MCObject *o, MCObjectVisitor v, void *c) {
    CallbackContext *t = (CallbackContext *)o;
    t->storage = (ExtendedBlockStorage *)v((MCObject *)t->storage, c);
    t->replacement = (NativeCharArray *)v((MCObject *)t->replacement, c);
    t->oldState = (NativeBlockState *)v((MCObject *)t->oldState, c);
    t->newState = (NativeBlockState *)v((MCObject *)t->newState, c);
}
static const MCObjectClass contextClass = {"test.storage.callbacks", MCObjectHeap_plainClone,
                                           context_trace, NULL};
static bool event(CallbackContext *c, int value) {
    if (c->used < 8)
        c->events[c->used] = value;
    c->used++;
    CHECK(MCObjectHeap_hasBorrowers(c->object.heap));
    if (c->used == c->failAt) {
        MCObjectHeap_fail(c->object.heap);
        return false;
    }
    return true;
}
static NativeBlock *get_block(MCObject *o, NativeBlockState *s) {
    CallbackContext *c = (CallbackContext *)o;
    if (c->recountIndex >= 0) {
        if (c->recountIndex < 32) {
            int i = c->recountIndex;
            int index = (i / 16) * 256 + (i % 16) * 16;
            CHECK(s == NativeBlockStateRuntime_state(s->runtime, c->storage->data->values[index]));
        }
        c->recountIndex++;
        return s->block;
    }
    int e = s == c->oldState ? 1 : 2;
    if (!event(c, e))
        return NULL;
    return s->block;
}
static bool get_tick(MCObject *o, NativeBlock *b, bool *out) {
    CallbackContext *c = (CallbackContext *)o;
    bool old = b == c->oldState->block;
    CHECK(c->storage->blockRefCount == (old ? 0 : 1));
    CHECK(c->storage->tickRefCount == (old ? 1 : 0));
    if (!event(c, old ? 3 : 4))
        return false;
    if (!old && c->swapArray)
        c->storage->data = c->replacement;
    *out = b->tickRandomly;
    return true;
}
static const NativeBlockStateDependencies callbacks = {.getBlock = get_block,
                                                       .getTickRandomly = get_tick};
static void callback_order(void) {
    for (int failure = 0; failure <= 4; failure++) {
        MCObjectHeap *h = MCObjectHeap_new(16u * 1024u * 1024u);
        NativeBlockStateRuntime *r = NativeBlockStateRuntime_get(h);
        ExtendedBlockStorage *e = ExtendedBlockStorage_new(h, 0, true, r);
        CHECK(e);
        NativeBlockState *old = NativeBlockStateRuntime_state(r, 32),
                         *next = NativeBlockStateRuntime_state(r, 16);
        CHECK(ExtendedBlockStorage_set(e, 2, 0, 0, old));
        CallbackContext *c = (CallbackContext *)MCObjectHeap_alloc(h, sizeof(*c), &contextClass);
        CHECK(c);
        c->storage = e;
        c->oldState = old;
        c->newState = next;
        c->failAt = failure;
        c->recountIndex = -1;
        CHECK(NativeBlockStateRuntime_bindDependencies(r, &callbacks, (MCObject *)c));
        bool ok = ExtendedBlockStorage_set(e, 2, 0, 0, next);
        CHECK(ok == (failure == 0));
        int expected = failure ? failure : 4;
        CHECK(c->used == expected);
        for (int i = 0; i < expected; i++)
            CHECK(c->events[i] == i + 1);
        if (failure == 1 || failure == 2) {
            CHECK(e->blockRefCount == 1 && e->tickRefCount == 1 && e->data->values[2] == 32);
        }
        if (failure == 3) {
            CHECK(e->blockRefCount == 0 && e->tickRefCount == 1 && e->data->values[2] == 32);
        }
        if (failure == 4) {
            CHECK(e->blockRefCount == 1 && e->tickRefCount == 0 && e->data->values[2] == 32);
        }
        if (!failure) {
            CHECK(e->blockRefCount == 1 && e->tickRefCount == 0 && e->data->values[2] == 16);
        }
        CHECK(!MCObjectHeap_hasBorrowers(h));
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h = MCObjectHeap_new(16u * 1024u * 1024u);
    NativeBlockStateRuntime *r = NativeBlockStateRuntime_get(h);
    ExtendedBlockStorage *e = ExtendedBlockStorage_new(h, 0, true, r);
    CHECK(e);
    NativeBlockState *old = NativeBlockStateRuntime_state(r, 32),
                     *next = NativeBlockStateRuntime_state(r, 16);
    CHECK(ExtendedBlockStorage_set(e, 2, 0, 0, old));
    NativeCharArray *original = e->data, *replacement = NativeCharArray_new(h, 4096);
    CHECK(replacement);
    CallbackContext *c = (CallbackContext *)MCObjectHeap_alloc(h, sizeof(*c), &contextClass);
    CHECK(c);
    c->storage = e;
    c->oldState = old;
    c->newState = next;
    c->replacement = replacement;
    c->swapArray = true;
    c->recountIndex = -1;
    CHECK(NativeBlockStateRuntime_bindDependencies(r, &callbacks, (MCObject *)c));
    CHECK(ExtendedBlockStorage_set(e, 2, 0, 0, next));
    CHECK(e->data == replacement && original->values[2] == 32 && replacement->values[2] == 16);
    NativeBlockStateDependencies orderOnly = {.getBlock = get_block};
    c->recountIndex = 0;
    CHECK(NativeBlockStateRuntime_bindDependencies(r, &orderOnly, (MCObject *)c));
    for (int i = 0; i < 4096; i++)
        replacement->values[i] = (uint16_t)(16 + (i % 7));
    CHECK(ExtendedBlockStorage_removeInvalidBlocks(e));
    CHECK(c->recountIndex == 4096 && e->blockRefCount == 4096);
    CHECK(NativeBlockStateRuntime_bindDependencies(r, NULL, NULL));
    e->blockRefCount = INT32_MIN;
    e->tickRefCount = INT32_MIN;
    CHECK(ExtendedBlockStorage_set(e, 0, 0, 0, r->air->defaultState));
    CHECK(e->blockRefCount == INT32_MAX && e->tickRefCount == INT32_MIN);
    e->blockRefCount = INT32_MAX;
    CHECK(ExtendedBlockStorage_set(e, 0, 0, 0, next));
    CHECK(e->blockRefCount == INT32_MIN);
    MCObjectHeap_free(h);
}
static void constructor_allocation_prefix(void) {
    MCObjectHeap *h = MCObjectHeap_new(16u * 1024u * 1024u);
    NativeBlockStateRuntime *r = NativeBlockStateRuntime_get(h);
    CHECK(r);
    size_t base = MCObjectHeap_liveBytes(h);
    MCObjectHeap_free(h);
    size_t byteArray = sizeof(NativeByteArray) + 2048, nibble = sizeof(NibbleArray) + byteArray,
           chars = sizeof(NativeCharArray) + 8192;
    /* Last child array allocation fails; already assigned data/blocklight stay,
       while the original skylight reference survives the failed new expression. */
    size_t budget = base + sizeof(ExtendedBlockStorage) + sizeof(NativeCharArray) + 2 + 2 * nibble +
                    chars + nibble + sizeof(NibbleArray) + byteArray - 1;
    h = MCObjectHeap_new(budget);
    r = NativeBlockStateRuntime_get(h);
    CHECK(r);
    ExtendedBlockStorage *e = ExtendedBlockStorage_nativeAllocate(h, r);
    CHECK(e);
    NativeCharArray *oldData = NativeCharArray_new(h, 1);
    NibbleArray *oldBlock = NibbleArray_new(h), *oldSky = NibbleArray_new(h);
    CHECK(oldData && oldBlock && oldSky);
    e->data = oldData;
    e->blocklightArray = oldBlock;
    e->skylightArray = oldSky;
    e->blockRefCount = 7;
    e->tickRefCount = -9;
    CHECK(!ExtendedBlockStorage_construct(e, 123, true) && MCObjectHeap_failed(h));
    CHECK(e->yBase == 123 && e->data != oldData && e->data->length == 4096 &&
          e->blocklightArray != oldBlock && e->skylightArray == oldSky);
    CHECK(e->blockRefCount == 7 && e->tickRefCount == -9 && !MCObjectHeap_hasBorrowers(h));
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(16u * 1024u * 1024u);
    r = NativeBlockStateRuntime_get(h);
    e = ExtendedBlockStorage_new(h, 1, true, r);
    CHECK(e);
    oldSky = e->skylightArray;
    e->blockRefCount = 3;
    CHECK(ExtendedBlockStorage_construct(e, -99, false));
    CHECK(e->skylightArray == oldSky && e->blockRefCount == 3);
    MCObjectHeap_free(h);
}
static void unknown_registry_and_guards(void) {
    MCObjectHeap *h = MCObjectHeap_new(16u * 1024u * 1024u);
    NativeBlockStateRuntime *r = NativeBlockStateRuntime_get(h);
    ChunkPrimer *p = ChunkPrimer_new(h, r);
    CHECK(p);
    CHECK(ChunkPrimer_setBlockStateAt(p, 0, NULL) && p->data->values[0] == -1 &&
          ChunkPrimer_getBlockStateAt(p, 0) == p->defaultState);
    NativeBlockState *stone = NativeBlockStateRuntime_state(r, 16);
    CHECK(ObjectIntIdentityMap_put(r->BLOCK_STATE_IDS, (MCObject *)stone, 32768));
    ExtendedBlockStorage *e = ExtendedBlockStorage_new(h, 0, false, r);
    CHECK(e);
    CHECK(ExtendedBlockStorage_set(e, 0, 0, 0, stone) && e->data->values[0] == 32768 &&
          ExtendedBlockStorage_get(e, 0, 0, 0) == stone);
    CHECK(ChunkPrimer_setBlockStateAt(p, 1, stone) && p->data->values[1] == INT16_MIN &&
          ChunkPrimer_getBlockStateAt(p, 1) == p->defaultState);
    CHECK(!ExtendedBlockStorage_setExtSkylightValue(e, 0, 0, 0, 1) && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(16u * 1024u * 1024u);
    MCObjectHeap *f = MCObjectHeap_new(65536);
    r = NativeBlockStateRuntime_get(h);
    e = ExtendedBlockStorage_new(h, 0, true, r);
    NativeCharArray *bad = NativeCharArray_new(f, 4096);
    CHECK(e && bad);
    NativeCharArray *saved = e->data;
    CHECK(!ExtendedBlockStorage_setData(e, bad) && e->data == saved && MCObjectHeap_failed(h) &&
          !MCObjectHeap_failed(f));
    MCObjectHeap_free(h);
    MCObjectHeap_free(f);
    h = MCObjectHeap_new(16u * 1024u * 1024u);
    r = NativeBlockStateRuntime_get(h);
    e = ExtendedBlockStorage_new(h, 0, true, r);
    CHECK(e);
    e->data->length = 4097;
    CHECK(ExtendedBlockStorage_get(e, 0, 0, 0) == NULL && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}

static void all_registered_identity_facts(void) {
    MCObjectHeap *h = MCObjectHeap_new(16u * 1024u * 1024u);
    NativeBlockStateRuntime *r = NativeBlockStateRuntime_get(h);
    CHECK(r);
    int states = 0, overwritten = 0, finalIds = 0;
    for (int id = 0; id < 198; id++) {
        NativeBlock *b = NativeBlockStateRuntime_block(r, id);
        CHECK(b && b->registeredId == id);
        int ordinal = 0;
        for (;; ordinal++) {
            NativeBlockState *s = NativeBlockStateRuntime_validState(r, id, ordinal);
            if (!s)
                break;
            CHECK(s->block == b && s->runtime == r && s->ordinal == ordinal);
            int raw = ObjectIntIdentityMap_get(r->BLOCK_STATE_IDS, (MCObject *)s);
            CHECK(raw == ((id << 4) | s->metadata));
            if (NativeBlockStateRuntime_state(r, raw) != s)
                overwritten++;
            states++;
        }
        CHECK(b->defaultState && b->defaultState->ordinal >= 0 &&
              b->defaultState->ordinal < ordinal);
    }
    for (int raw = 0; raw < 65536; raw++)
        if (NativeBlockStateRuntime_state(r, raw))
            finalIds++;
    CHECK(states == 7806 && overwritten == 6412 && finalIds == 1394);
    CHECK(r->materials->length == 34 && r->airMaterial == r->air->material &&
          r->leavesMaterial == NativeBlockStateRuntime_block(r, 18)->material);
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, h, (MCObject *)r));
    CHECK(MCObjectHeap_collect(h));
    size_t baseline = MCObjectHeap_liveObjects(h);
    for (int i = 0; i < 100; i++) {
        CHECK(ExtendedBlockStorage_new(h, i, true, r) && ChunkPrimer_new(h, r));
    }
    CHECK(MCObjectHeap_collect(h) && MCObjectHeap_liveObjects(h) == baseline &&
          NativeBlockStateRuntime_get(h) == r);
    MCObjectHeap_free(h);
}
static void malformed_descriptor_trace(void) {
    MCObjectHeap *h = MCObjectHeap_new(16u * 1024u * 1024u);
    NativeBlockStateRuntime *r = NativeBlockStateRuntime_get(h);
    CHECK(r);
    MCObject *shortBlock = MCObjectHeap_alloc(h, sizeof(MCObject), r->air->object.klass);
    CHECK(shortBlock && !NativeBlock_isInstance(shortBlock));
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, h, shortBlock));
    CHECK(!MCObjectHeap_collect(h) && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}

static void lazy_static_dependency(void) {
    MCObjectHeap *h = MCObjectHeap_new(16u * 1024u * 1024u);
    ExtendedBlockStorage *e = ExtendedBlockStorage_new(h, 4, true, NULL);
    CHECK(e && !e->nativeRuntime && MCObjectHeap_liveObjects(h) == 6);
    CHECK(ExtendedBlockStorage_getYLocation(e) == 4 && ExtendedBlockStorage_isEmpty(e) &&
          !ExtendedBlockStorage_getNeedsRandomTick(e));
    CHECK(ExtendedBlockStorage_setExtBlocklightValue(e, 0, 0, 0, 3) &&
          ExtendedBlockStorage_getExtBlocklightValue(e, 0, 0, 0) == 3 && !e->nativeRuntime);
    NativeBlockState *air = ExtendedBlockStorage_get(e, 0, 0, 0);
    CHECK(air && e->nativeRuntime && air == e->nativeRuntime->air->defaultState);
    CHECK(e->nativeRuntime == NativeBlockStateRuntime_get(h));
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(16u * 1024u * 1024u);
    e = ExtendedBlockStorage_new(h, 0, true, NULL);
    CHECK(e);
    e->blockRefCount = 9;
    e->tickRefCount = -9;
    CHECK(ExtendedBlockStorage_removeInvalidBlocks(e));
    CHECK(e->nativeRuntime && e->blockRefCount == 0 && e->tickRefCount == 0);
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(65536);
    e = ExtendedBlockStorage_new(h, 0, false, NULL);
    CHECK(e && !e->nativeRuntime);
    NativeCharArray *a = NativeCharArray_new(h, 1);
    CHECK(a && ExtendedBlockStorage_setData(e, a));
    CHECK(!NativeCharArray_set(ExtendedBlockStorage_getData(e), 1, 16) && MCObjectHeap_failed(h) &&
          !e->nativeRuntime);
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(16u * 1024u * 1024u);
    e = ExtendedBlockStorage_new(h, 0, false, NULL);
    CHECK(e && ExtendedBlockStorage_setData(e, NULL));
    CHECK(!ExtendedBlockStorage_get(e, 0, 0, 0) && MCObjectHeap_failed(h) && e->nativeRuntime);
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(65536);
    e = ExtendedBlockStorage_new(h, 0, false, NULL);
    CHECK(e);
    CHECK(!ExtendedBlockStorage_get(e, 0, 0, 0) && MCObjectHeap_failed(h) && !e->nativeRuntime &&
          e->data->values[0] == 0 && !MCObjectHeap_hasBorrowers(h));
    MCObjectHeap_free(h);
}

static void malformed_registry_dependency(void) {
    MCObjectHeap *h = MCObjectHeap_new(16u * 1024u * 1024u);
    NativeBlockStateRuntime *r = NativeBlockStateRuntime_get(h);
    CHECK(r);
    MCObject *fake = MCObjectHeap_alloc(h, sizeof(NativeBlockStateRuntime), r->object.klass);
    CHECK(fake && NativeBlockStateRuntime_isInstance(fake));
    CHECK(NativeBlockStateRuntime_get(h) == NULL && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(16u * 1024u * 1024u);
    r = NativeBlockStateRuntime_get(h);
    CHECK(r);
    r->BLOCK_STATE_IDS = NULL;
    CHECK(!NativeBlockStateRuntime_state(r, 0) && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(16u * 1024u * 1024u);
    MCObjectHeap *f = MCObjectHeap_new(65536);
    r = NativeBlockStateRuntime_get(h);
    CHECK(r);
    r->blocks = NativeObjectArray_new(f, 198);
    CHECK(r->blocks);
    CHECK(!NativeBlockStateRuntime_block(r, 0) && MCObjectHeap_failed(h) &&
          !MCObjectHeap_failed(f));
    MCObjectHeap_free(h);
    MCObjectHeap_free(f);
}
int main(void) {
    registry_and_storage();
    nibble_and_failure_prefixes();
    callback_order();
    constructor_allocation_prefix();
    unknown_registry_and_guards();
    all_registered_identity_facts();
    malformed_descriptor_trace();
    lazy_static_dependency();
    malformed_registry_dependency();
    printf("source chunk storage: %u checks passed\n", checks);
    return 0;
}
