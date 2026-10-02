#include "util/NativeBlockStateRuntime.h"
#include "util/NativeBlockStateFacts.h"
static const MCObjectClass runtimeClass, blockClass, stateClass;

static bool fail(MCObjectHeap *h) {
    MCObjectHeap_fail(h);
    return false;
}
static bool any(const MCObject *o, void *c) {
    (void)o;
    (void)c;
    return true;
}
static bool identity(const MCObject *o, void *c) { return o == c; }
static bool tracked(const MCObject *o) {
    return o && o->heap && o->klass &&
           MCObjectHeap_findObject(o->heap, o->klass, identity, (void *)o) == o;
}
bool NativeBlockStateRuntime_isInstance(const MCObject *o) {
    return o && o->klass == &runtimeClass && tracked(o) &&
           MCObjectHeap_objectSize(o) >= sizeof(NativeBlockStateRuntime);
}
bool NativeBlock_isInstance(const MCObject *o) {
    return o && o->klass == &blockClass && tracked(o) && MCObjectHeap_objectSize(o) >= sizeof(NativeBlock);
}
bool NativeBlockState_isInstance(const MCObject *o) {
    return o && o->klass == &stateClass && tracked(o) && MCObjectHeap_objectSize(o) >= sizeof(NativeBlockState);
}


static bool runtime_valid(NativeBlockStateRuntime *r) {
    return (NativeBlockStateRuntime_isInstance((MCObject *)r) &&
            !MCObjectHeap_failed(r->object.heap)) ||
           fail(r ? r->object.heap : NULL);
}
static bool same(MCObjectHeap *h, MCObject *o) {
    return (!o || (o->heap == h && tracked(o))) || fail(h);
}
static bool begin(MCObject *o, NativeBlockStateRuntime *r, MCObjectRootScope *scope) {
    if (!runtime_valid(r) || !o || !same(r->object.heap, o) ||
        !MCObjectRootScope_begin(scope, r->object.heap))
        return false;
    if (!MCObjectRootScope_pin(scope, o) || !MCObjectRootScope_pin(scope, (MCObject *)r)) {
        MCObjectRootScope_end(scope);
        return false;
    }
    return true;
}
static void runtime_trace(MCObject *o, MCObjectVisitor v, void *c) {
    if (!NativeBlockStateRuntime_isInstance(o)) {
        fail(o->heap);
        return;
    }
    NativeBlockStateRuntime *r = (NativeBlockStateRuntime *)o;

    r->BLOCK_STATE_IDS = (ObjectIntIdentityMap *)v((MCObject *)r->BLOCK_STATE_IDS, c);

    r->blocks = (NativeObjectArray *)v((MCObject *)r->blocks, c);
    r->validStates = (NativeObjectArray *)v((MCObject *)r->validStates, c);
    r->materials = (NativeObjectArray *)v((MCObject *)r->materials, c);

    r->air = (NativeBlock *)v((MCObject *)r->air, c);
    r->barrier = (NativeBlock *)v((MCObject *)r->barrier, c);
    r->airMaterial = (Material *)v((MCObject *)r->airMaterial, c);
    r->leavesMaterial = (Material *)v((MCObject *)r->leavesMaterial, c);
    r->context = v(r->context, c);
}
static void block_trace(MCObject *o, MCObjectVisitor v, void *c) {
    if (!NativeBlock_isInstance(o)) {
        fail(o->heap);
        return;
    }
    NativeBlock *b = (NativeBlock *)o;
    b->runtime = (NativeBlockStateRuntime *)v((MCObject *)b->runtime, c);
    b->defaultState = (NativeBlockState *)v((MCObject *)b->defaultState, c);
    b->material = (Material *)v((MCObject *)b->material, c);
}
static void state_trace(MCObject *o, MCObjectVisitor v, void *c) {
    if (!NativeBlockState_isInstance(o)) {
        fail(o->heap);
        return;
    }
    NativeBlockState *s = (NativeBlockState *)o;
    s->runtime = (NativeBlockStateRuntime *)v((MCObject *)s->runtime, c);
    s->block = (NativeBlock *)v((MCObject *)s->block, c);
}
static const MCObjectClass runtimeClass = {"native.BlockStateRegistry", MCObjectHeap_plainClone,
                                           runtime_trace, NULL};

static const MCObjectClass blockClass = {"native.BlockRegistryIdentity", MCObjectHeap_plainClone,
                                         block_trace, NULL};

static const MCObjectClass stateClass = {"native.BlockStateIdentity", MCObjectHeap_plainClone,
                                         state_trace, NULL};

static bool owned_array(NativeBlockStateRuntime *r, NativeObjectArray *a) {
    return (a && same(r->object.heap, (MCObject *)a) && NativeObjectArray_isInstance((MCObject *)a)) ||
           fail(r->object.heap);
}
static bool owned_ids(NativeBlockStateRuntime *r) {
    return (r->BLOCK_STATE_IDS && same(r->object.heap, (MCObject *)r->BLOCK_STATE_IDS) &&
            ObjectIntIdentityMap_isInstance((MCObject *)r->BLOCK_STATE_IDS) &&
            r->BLOCK_STATE_IDS->object.heap == r->object.heap) ||
           fail(r->object.heap);
}
static bool initialized_registry(NativeBlockStateRuntime *r) {
    return owned_ids(r) && owned_array(r, r->blocks) && owned_array(r, r->validStates) &&
           owned_array(r, r->materials) && r->blocks->length == 198 &&
           r->validStates->length == 7806 && r->materials->length == 34 &&
           NativeBlock_isInstance((MCObject *)r->air) && r->air->object.heap == r->object.heap &&
           NativeBlock_isInstance((MCObject *)r->barrier) &&
           r->barrier->object.heap == r->object.heap;
}
/* One binding selection shared by the native registry and dense raw-ID view.
   The subset stores only these actual Source references, never scalar flags. */
static Material *named_material(MaterialStatics *s,int32_t group) {
    Material *bindings[34] = {
        s->air, s->rock, s->grass, s->ground, s->wood, s->plants,
        s->water, s->lava, s->sand, s->leaves, s->sponge, s->glass,
        s->iron, s->cloth, s->circuits, s->piston, s->web, s->vine,
        s->tnt, s->fire, s->snow, s->ice, s->craftedSnow, s->cactus,
        s->clay, s->gourd, s->portal, s->cake, s->dragonEgg,
        s->redstoneLight, s->anvil, s->barrier, s->carpet, s->packedIce
    };
    return group>=0 && group<34?bindings[group]:NULL;
}
Material *NativeBlockStateRuntime_materialForBlockId(MCObjectHeap *h,int32_t id) {
    if(!h||MCObjectHeap_failed(h)){fail(h);return NULL;}
    if(id<0||id>=198)return NULL;
    MCObjectRootScope scope={0};
    if(!MCObjectRootScope_begin(&scope,h))return NULL;
    MaterialStatics *s=Material_getStatics(h);
    Material *out=NULL;
    if(s&&MCObjectRootScope_pin(&scope,(MCObject *)s)) {
        out=named_material(s,registryFacts[id].material);
        if(!out||!same(h,(MCObject *)out)||!Material_isInstance((MCObject *)out)||
            !MCObjectRootScope_pin(&scope,(MCObject *)out))out=NULL;
    }
    if(!out)fail(h);
    MCObjectRootScope_end(&scope);
    return MCObjectHeap_failed(h)?NULL:out;
}
NativeBlockStateRuntime *NativeBlockStateRuntime_get(MCObjectHeap *h) {
    if (!h || MCObjectHeap_failed(h)) {
        fail(h);
        return NULL;
    }
    NativeBlockStateRuntime *r =
        (NativeBlockStateRuntime *)MCObjectHeap_findObject(h, &runtimeClass, any, NULL);

    if (r) {
        if (!NativeBlockStateRuntime_isInstance((MCObject *)r) || !initialized_registry(r)) {
            fail(h);
            return NULL;
        }
        return r;
    }

    MCObjectRootScope scope = {0};

    if (!MCObjectRootScope_begin(&scope, h))
        return NULL;

    r = (NativeBlockStateRuntime *)MCObjectHeap_alloc(h, sizeof(*r), &runtimeClass);

    bool ok = r && MCObjectRootScope_pin(&scope, (MCObject *)r);

    if (ok) {
        r->BLOCK_STATE_IDS = ObjectIntIdentityMap_new(h);
        ok = r->BLOCK_STATE_IDS != NULL;
    }
    if (ok) {
        r->blocks = NativeObjectArray_new(h, 198);
        ok = r->blocks != NULL;
    }
    if (ok) {
        r->validStates = NativeObjectArray_new(h, 7806);
        ok = r->validStates != NULL;
    }
    if (ok) {
        r->materials = NativeObjectArray_new(h, 34);
        ok = r->materials != NULL;
    }
    /* Native named binding of the closed target registry. These refs were
       resolved from supplied constructor chains before joining retained
       material identity groups. Numeric groups are not Source Material IDs. */
    MaterialStatics *statics = ok ? Material_getStatics(h) : NULL;
    ok = ok && statics && MCObjectRootScope_pin(&scope, (MCObject *)statics);
    if (ok) {
        for (int32_t i = 0; ok && i < 34; i++) {
            Material *binding=named_material(statics,i);
            ok = same(h,(MCObject *)binding) && Material_isInstance((MCObject *)binding) &&
                 NativeObjectArray_set(r->materials, i, (MCObject *)binding);
        }
    }
    for (int32_t id = 0; ok && id < 198; id++) {
        const RegistryFact *f = &registryFacts[id];
        Material *m = (Material *)r->materials->values[f->material];
        NativeBlock *b = ok ? (NativeBlock *)MCObjectHeap_alloc(h, sizeof(*b), &blockClass) : NULL;
        ok = b != NULL;

        if (ok) {
            b->runtime = r;
            b->registeredId = id;
            b->material = m;
            b->tickRandomly = f->tick != 0;
            b->lightOpacity = f->opacity;
            b->lightValue = f->light;
            r->blocks->values[id] = (MCObject *)b;
        }
        for (int32_t ordinal = 0; ok && ordinal < f->count; ordinal++) {
            NativeBlockState *s =
                (NativeBlockState *)MCObjectHeap_alloc(h, sizeof(*s), &stateClass);
            ok = s != NULL;

            if (ok) {
                s->runtime = r;
                s->block = b;
                s->ordinal = ordinal;
                s->metadata = registryMetadata[f->offset + ordinal];
                r->validStates->values[f->offset + ordinal] = (MCObject *)s;
                ok = ObjectIntIdentityMap_put(r->BLOCK_STATE_IDS, (MCObject *)s,
                                              (id << 4) | s->metadata);

                if (ordinal == f->defaultOrdinal)
                    b->defaultState = s;
            }
        }
    }
    if (ok) {
        r->air = (NativeBlock *)r->blocks->values[0];
        r->barrier = (NativeBlock *)r->blocks->values[166];
        r->airMaterial = r->air->material;
        r->leavesMaterial = ((NativeBlock *)r->blocks->values[18])->material;
        MCObjectRoot root = {0};
        ok = MCObjectRoot_init(&root, h, (MCObject *)r);
    }
    if (!ok)
        fail(h);
    MCObjectRootScope_end(&scope);
    return ok ? r : NULL;
}
bool NativeBlockStateRuntime_bindDependencies(NativeBlockStateRuntime *r,
                                              const NativeBlockStateDependencies *deps,
                                              MCObject *ctx) {
    if (!runtime_valid(r) || !same(r->object.heap, ctx))
        return false;
    r->dependencies = deps;
    r->context = ctx;
    MCObjectHeap_touch(r->object.heap);
    return true;
}
NativeBlock *NativeBlockStateRuntime_block(NativeBlockStateRuntime *r, int32_t id) {
    if (!runtime_valid(r))
        return NULL;

    if (id < 0 || id >= 198)
        return NULL;

    MCObject *o = NULL;

    if (!owned_array(r, r->blocks) || !NativeObjectArray_get(r->blocks, id, &o))
        return NULL;

    if (!NativeBlock_isInstance(o) || o->heap != r->object.heap) {
        fail(r->object.heap);
        return NULL;
    }
    return (NativeBlock *)o;
}
NativeBlockState *NativeBlockStateRuntime_validState(NativeBlockStateRuntime *r, int32_t id,
                                                     int32_t ordinal) {
    if (!runtime_valid(r))
        return NULL;

    if (id < 0 || id >= 198 || ordinal < 0 || ordinal >= registryFacts[id].count)
        return NULL;

    MCObject *o = NULL;

    if (!owned_array(r, r->validStates) ||
        !NativeObjectArray_get(r->validStates, registryFacts[id].offset + ordinal, &o))
        return NULL;

    if (!NativeBlockState_isInstance(o) || o->heap != r->object.heap) {
        fail(r->object.heap);
        return NULL;
    }
    return (NativeBlockState *)o;
}
NativeBlockState *NativeBlockStateRuntime_state(NativeBlockStateRuntime *r, int32_t id) {
    if (!runtime_valid(r))
        return NULL;

    if (!owned_ids(r))
        return NULL;
    MCObject *o = ObjectIntIdentityMap_getByValue(r->BLOCK_STATE_IDS, id);

    if (o && (!NativeBlockState_isInstance(o) || o->heap != r->object.heap)) {
        fail(r->object.heap);
        return NULL;
    }
    return (NativeBlockState *)o;
}
NativeBlockState *NativeBlockState_nativeNew(NativeBlockStateRuntime *r, NativeBlock *b,
                                             int32_t metadata) {
    if (!runtime_valid(r) || !NativeBlock_isInstance((MCObject *)b) ||
        b->object.heap != r->object.heap || b->runtime != r) {
        fail(r ? r->object.heap : NULL);
        return NULL;
    }
    NativeBlockState *s =
        (NativeBlockState *)MCObjectHeap_alloc(r->object.heap, sizeof(*s), &stateClass);

    if (s) {
        s->runtime = r;
        s->block = b;
        s->metadata = metadata;
        s->ordinal = -1;
    }
    return s;
}
static bool state_begin(NativeBlockState *s, MCObjectRootScope *scope) {
    return (NativeBlockState_isInstance((MCObject *)s) &&
            begin((MCObject *)s, s->runtime, scope)) ||
           fail(s ? s->object.heap : NULL);
}
static bool block_begin(NativeBlock *b, MCObjectRootScope *scope) {
    return (NativeBlock_isInstance((MCObject *)b) && begin((MCObject *)b, b->runtime, scope)) ||
           fail(b ? b->object.heap : NULL);
}
static bool callback_context(NativeBlockStateRuntime *r, MCObjectRootScope *scope) {
    return same(r->object.heap, r->context) && MCObjectRootScope_pin(scope, r->context);
}
NativeBlock *NativeBlockState_getBlock(NativeBlockState *s) {
    MCObjectRootScope scope = {0};

    if (!state_begin(s, &scope))
        return NULL;
    NativeBlockStateRuntime *r = s->runtime;
    NativeBlock *b = s->block;

    if (r->dependencies && r->dependencies->getBlock) {
        if (callback_context(r, &scope))
            b = r->dependencies->getBlock(r->context, s);
        else
            b = NULL;
    }
    if (!NativeBlock_isInstance((MCObject *)b) || b->object.heap != s->object.heap ||
        MCObjectHeap_failed(s->object.heap)) {
        fail(s->object.heap);
        b = NULL;
    }
    MCObjectRootScope_end(&scope);
    return b;
}
NativeBlockState *NativeBlock_getDefaultState(NativeBlock *b) {
    MCObjectRootScope scope = {0};

    if (!block_begin(b, &scope))
        return NULL;
    NativeBlockStateRuntime *r = b->runtime;
    NativeBlockState *s = b->defaultState;

    if (r->dependencies && r->dependencies->getDefaultState) {
        if (callback_context(r, &scope))
            s = r->dependencies->getDefaultState(r->context, b);
        else
            s = NULL;
    }
    if (!NativeBlockState_isInstance((MCObject *)s) || s->object.heap != b->object.heap ||
        MCObjectHeap_failed(b->object.heap)) {
        fail(b->object.heap);
        s = NULL;
    }
    MCObjectRootScope_end(&scope);
    return s;
}
bool NativeBlock_getTickRandomly(NativeBlock *b, bool *out) {
    MCObjectRootScope scope = {0};
    if (!block_begin(b, &scope))
        return false;

    NativeBlockStateRuntime *r = b->runtime;
    bool value = b->tickRandomly;
    bool ok = out != NULL;

    if (ok && r->dependencies && r->dependencies->getTickRandomly)
        ok = callback_context(r, &scope) && r->dependencies->getTickRandomly(r->context, b, &value);

    if (!ok || MCObjectHeap_failed(b->object.heap)) {
        fail(b->object.heap);
        ok = false;
    }
    if (ok)
        *out = value;
    MCObjectRootScope_end(&scope);
    return ok;
}
bool NativeBlock_getLightOpacity(NativeBlock *b, int32_t *out) {
    MCObjectRootScope scope = {0};
    if (!block_begin(b, &scope))
        return false;

    NativeBlockStateRuntime *r = b->runtime;
    int32_t value = b->lightOpacity;
    bool ok = out != NULL;

    if (ok && r->dependencies && r->dependencies->getLightOpacity)
        ok = callback_context(r, &scope) && r->dependencies->getLightOpacity(r->context, b, &value);

    if (!ok || MCObjectHeap_failed(b->object.heap)) {
        fail(b->object.heap);
        ok = false;
    }
    if (ok)
        *out = value;
    MCObjectRootScope_end(&scope);
    return ok;
}

bool NativeBlock_getMetaFromState(NativeBlock *b, NativeBlockState *s, int32_t *out) {
    MCObjectRootScope scope = {0};

    if (!block_begin(b, &scope))
        return false;
    NativeBlockStateRuntime *r = b->runtime;
    int32_t value = 0;
    bool ok = out && NativeBlockState_isInstance((MCObject *)s) &&
              s->object.heap == b->object.heap && MCObjectRootScope_pin(&scope, (MCObject *)s);

    if (ok) {
        value = s->metadata;

        if (r->dependencies && r->dependencies->getMetaFromState)
            ok = callback_context(r, &scope) &&
                 r->dependencies->getMetaFromState(r->context, b, s, &value);
        else
            ok = s->block == b;
    }
    if (!ok || MCObjectHeap_failed(b->object.heap)) {
        fail(b->object.heap);
        ok = false;
    }
    if (ok)
        *out = value;
    MCObjectRootScope_end(&scope);
    return ok;
}
static bool current_material_edges(NativeBlock *b, MCObjectRootScope *scope) {
    MCObjectHeap *heap = b->object.heap;
    bool runtimeOK = same(heap, (MCObject *)b->runtime);
    if (runtimeOK && b->runtime)
        runtimeOK = NativeBlockStateRuntime_isInstance((MCObject *)b->runtime) &&
                    MCObjectRootScope_pin(scope, (MCObject *)b->runtime);
    bool materialOK = same(heap, (MCObject *)b->material);
    if (materialOK && b->material)
        materialOK = Material_isInstance((MCObject *)b->material) &&
                     MCObjectRootScope_pin(scope, (MCObject *)b->material);
    return (runtimeOK && materialOK) || fail(heap);
}
Material *NativeBlock_getMaterial(NativeBlock *b) {
    MCObjectRootScope scope = {0};

    if (!block_begin(b, &scope))
        return NULL;
    NativeBlockStateRuntime *r = b->runtime;
    Material *m = b->material;
    bool invoked = false;

    if (r->dependencies && r->dependencies->getMaterial) {
        if (callback_context(r, &scope)) {
            invoked = true;
            m = r->dependencies->getMaterial(r->context, b);
        } else
            m = NULL;
    }
    bool contextOK = !invoked || callback_context(r, &scope);
    bool edgesOK = current_material_edges(b, &scope);
    if (!contextOK || !edgesOK || !m || !same(b->object.heap, (MCObject *)m) ||
        !Material_isInstance((MCObject *)m) || !MCObjectRootScope_pin(&scope, (MCObject *)m) ||
        MCObjectHeap_failed(b->object.heap)) {
        fail(b->object.heap);
        m = NULL;
    }
    MCObjectRootScope_end(&scope);
    return m;
}

static bool material_begin(NativeBlockStateRuntime *r, Material *m,
                           MCObjectRootScope *scope) {
    if (!runtime_valid(r) || !m || !same(r->object.heap, (MCObject *)m) ||
        !Material_isInstance((MCObject *)m))
        return fail(r ? r->object.heap : NULL);
    return begin((MCObject *)m, r, scope);
}
static bool material_property(NativeBlockStateRuntime *r, Material *m, bool *out,
                              bool liquid) {
    MCObjectRootScope scope = {0};
    if (!material_begin(r, m, &scope))
        return false;
    bool value = false, ok = out != NULL, invoked = false;
    bool (*hook)(MCObject *, Material *, bool *) = NULL;
    if (r->dependencies)
        hook = liquid ? r->dependencies->isLiquid : r->dependencies->blocksMovement;
    if (ok && hook) {
        ok = callback_context(r, &scope);
        if (ok) {
            invoked = true;
            MCObject *context = r->context;
            ok = hook(context, m, &value);
        }
    } else if (ok) {
        NativeArrayResult result = liquid ? Material_isLiquid(m, &value)
                                         : Material_blocksMovement(m, &value);
        ok = result == NATIVE_ARRAY_OK;
    }
    /* Revalidate the current traced context regardless of callback result. */
    bool contextOK = !invoked || callback_context(r, &scope);
    if (!ok || !contextOK || MCObjectHeap_failed(r->object.heap)) {
        fail(r->object.heap);
        ok = false;
    }
    if (ok)
        *out = value;
    MCObjectRootScope_end(&scope);
    return ok;
}
bool NativeBlockStateRuntime_materialBlocksMovement(NativeBlockStateRuntime *r,
                                                   Material *m, bool *out) {
    return material_property(r, m, out, false);
}
bool NativeBlockStateRuntime_materialIsLiquid(NativeBlockStateRuntime *r,
                                             Material *m, bool *out) {
    return material_property(r, m, out, true);
}
