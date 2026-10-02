#include "block/material/Material.h"
#include "block/material/MaterialLiquid.h"
#include "block/material/MaterialLogic.h"
#include "block/material/MaterialPortal.h"
#include "block/material/MaterialTransparent.h"

static const MCObjectClass materialClass, anonymousClass, staticsClass;
static NativeArrayResult fail(MCObjectHeap *h) {
    MCObjectHeap_fail(h);
    return NATIVE_ARRAY_FAILURE;
}
static bool identity(const MCObject *o, void *ctx) { return o == ctx; }
static bool tracked(MCObjectHeap *h, const MCObject *o) {
    return o && o->heap == h && o->klass &&
           MCObjectHeap_findObject(h, o->klass, identity, (void *)o) == o;
}
static bool known_class(const MCObjectClass *k) {
    return k == &materialClass || k == &anonymousClass || k == MaterialTransparent_nativeClass() ||
           k == MaterialLiquid_nativeClass() || k == MaterialLogic_nativeClass() ||
           k == MaterialPortal_nativeClass();
}
bool Material_isInstance(const MCObject *o) {
    return o && known_class(o->klass) && tracked(o->heap, o) &&
           MCObjectHeap_objectSize(o) >= sizeof(Material);
}
bool Material_isRuntimeClass(const MCObject *o) {
    return o && o->klass == &materialClass && Material_isInstance(o);
}
static bool anonymous_is_runtime_class(const MCObject *o) {
    return o && o->klass == &anonymousClass && Material_isInstance(o);
}
static const NativeJavaClassDescriptor *const objectParents[] = {&NativeJavaClass_ObjectClass};
static const NativeJavaClassDescriptor *const materialParents[] = {&Material_Class};
const NativeJavaClassDescriptor Material_Class = {"net.minecraft.block.material.Material",
                                                  objectParents, 1, Material_isRuntimeClass};
const NativeJavaClassDescriptor MaterialAnonymous1_Class = {
    "net.minecraft.block.material.Material$1", materialParents, 1, anonymous_is_runtime_class};
const MCObjectClass *Material_nativeClass(void) { return &materialClass; }

static bool color_owned(MCObjectHeap *h, MapColor *c) {
    return !c || (tracked(h, (MCObject *)c) && MapColor_isInstance((MCObject *)c));
}
void Material_traceFields(Material *m, MCObjectVisitor v, void *ctx) {
    if (!Material_isInstance((MCObject *)m)) {
        fail(m ? m->object.heap : NULL);
        return;
    }
    /* Clone visitors remap the original color edge into the destination. Do not
       require that unvisited clone edges already belong to the new heap. */
    m->materialMapColor = (MapColor *)v((MCObject *)m->materialMapColor, ctx);
}
static void trace(MCObject *o, MCObjectVisitor v, void *ctx) {
    Material_traceFields((Material *)o, v, ctx);
}
static const MCObjectClass materialClass = {"net.minecraft.block.material.Material",
                                            MCObjectHeap_plainClone, trace, NULL};
static const MCObjectClass anonymousClass = {"net.minecraft.block.material.Material$1",
                                             MCObjectHeap_plainClone, trace, NULL};
Material *Material_nativeAllocate(MCObjectHeap *h) {
    return (Material *)MCObjectHeap_alloc(h, sizeof(Material), &materialClass);
}
static NativeArrayResult read_begin(Material *m, const void *out, MCObjectReadScope *scope) {
    if (!m)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectHeap *h = m->object.heap;
    if (!Material_isInstance((MCObject *)m) || !out || MCObjectHeap_failed(h) ||
        !color_owned(h, m->materialMapColor) || !MCObjectReadScope_begin(scope, h))
        return fail(h);
    return NATIVE_ARRAY_OK;
}
NativeArrayResult Material_nativeReturnBoolean(Material *m, bool value, bool *out) {
    MCObjectReadScope scope = {0};
    NativeArrayResult result = read_begin(m, out, &scope);
    if (result == NATIVE_ARRAY_OK)
        *out = value;
    MCObjectReadScope_end(&scope);
    return result;
}
NativeArrayResult Material_construct(Material *m, MapColor *color) {
    if (!m)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectHeap *h = m->object.heap;
    if (!Material_isInstance((MCObject *)m) || MCObjectHeap_failed(h) || !color_owned(h, color))
        return fail(h);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h) || !MCObjectRootScope_pin(&scope, (MCObject *)m) ||
        !MCObjectRootScope_pin(&scope, (MCObject *)color)) {
        MCObjectRootScope_end(&scope);
        return fail(h);
    }
    /* The only explicit initializer precedes the original constructor store.
       All other field defaults came from allocation, not this constructor. */
    m->requiresNoTool = true;
    MCObjectHeap_touch(h);
    m->materialMapColor = color;
    MCObjectHeap_touch(h);
    MCObjectRootScope_end(&scope);
    return NATIVE_ARRAY_OK;
}
NativeArrayResult Material_new(MCObjectHeap *h, MapColor *color, Material **out) {
    if (!h || !out || MCObjectHeap_failed(h))
        return fail(h);
    if (!Material_getStatics(h))
        return fail(h);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return fail(h);
    Material *m = Material_nativeAllocate(h);
    NativeArrayResult result = m ? Material_construct(m, color) : fail(h);
    if (result == NATIVE_ARRAY_OK)
        *out = m;
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult Material_isLiquid_base(Material *m, bool *out) {
    return Material_nativeReturnBoolean(m, false, out);
}
NativeArrayResult Material_isSolid_base(Material *m, bool *out) {
    return Material_nativeReturnBoolean(m, true, out);
}
NativeArrayResult Material_blocksLight_base(Material *m, bool *out) {
    return Material_nativeReturnBoolean(m, true, out);
}
NativeArrayResult Material_blocksMovement_base(Material *m, bool *out) {
    return Material_nativeReturnBoolean(m, true, out);
}
/* Exact anonymous body declared in Material.java, with no additional fields. */
static NativeArrayResult MaterialAnonymous1_blocksMovement(Material *m, bool *out) {
    if (!m)
        return NATIVE_ARRAY_EXCEPTION;
    if (!anonymous_is_runtime_class((MCObject *)m))
        return fail(m->object.heap);
    return Material_nativeReturnBoolean(m, false, out);
}
static NativeArrayResult dispatch_valid(Material *m) {
    if (!m)
        return NATIVE_ARRAY_EXCEPTION;
    return Material_isInstance((MCObject *)m) && !MCObjectHeap_failed(m->object.heap)
               ? NATIVE_ARRAY_OK
               : fail(m->object.heap);
}
NativeArrayResult Material_isLiquid(Material *m, bool *out) {
    NativeArrayResult result = dispatch_valid(m);
    if (result != NATIVE_ARRAY_OK)
        return result;
    if (MaterialLiquid_isRuntimeClass((MCObject *)m))
        return MaterialLiquid_isLiquid((MaterialLiquid *)m, out);
    return Material_isLiquid_base(m, out);
}
NativeArrayResult Material_isSolid(Material *m, bool *out) {
    NativeArrayResult result = dispatch_valid(m);
    if (result != NATIVE_ARRAY_OK)
        return result;
    if (MaterialTransparent_isRuntimeClass((MCObject *)m))
        return MaterialTransparent_isSolid((MaterialTransparent *)m, out);
    if (MaterialLiquid_isRuntimeClass((MCObject *)m))
        return MaterialLiquid_isSolid((MaterialLiquid *)m, out);
    if (MaterialLogic_isRuntimeClass((MCObject *)m))
        return MaterialLogic_isSolid((MaterialLogic *)m, out);
    if (MaterialPortal_isRuntimeClass((MCObject *)m))
        return MaterialPortal_isSolid((MaterialPortal *)m, out);
    return Material_isSolid_base(m, out);
}
NativeArrayResult Material_blocksLight(Material *m, bool *out) {
    NativeArrayResult result = dispatch_valid(m);
    if (result != NATIVE_ARRAY_OK)
        return result;
    if (MaterialTransparent_isRuntimeClass((MCObject *)m))
        return MaterialTransparent_blocksLight((MaterialTransparent *)m, out);
    if (MaterialLogic_isRuntimeClass((MCObject *)m))
        return MaterialLogic_blocksLight((MaterialLogic *)m, out);
    if (MaterialPortal_isRuntimeClass((MCObject *)m))
        return MaterialPortal_blocksLight((MaterialPortal *)m, out);
    return Material_blocksLight_base(m, out);
}
NativeArrayResult Material_blocksMovement(Material *m, bool *out) {
    NativeArrayResult result = dispatch_valid(m);
    if (result != NATIVE_ARRAY_OK)
        return result;
    if (MaterialTransparent_isRuntimeClass((MCObject *)m))
        return MaterialTransparent_blocksMovement((MaterialTransparent *)m, out);
    if (MaterialLiquid_isRuntimeClass((MCObject *)m))
        return MaterialLiquid_blocksMovement((MaterialLiquid *)m, out);
    if (MaterialLogic_isRuntimeClass((MCObject *)m))
        return MaterialLogic_blocksMovement((MaterialLogic *)m, out);
    if (MaterialPortal_isRuntimeClass((MCObject *)m))
        return MaterialPortal_blocksMovement((MaterialPortal *)m, out);
    if (anonymous_is_runtime_class((MCObject *)m))
        return MaterialAnonymous1_blocksMovement(m, out);
    return Material_blocksMovement_base(m, out);
}
NativeArrayResult Material_isOpaque(Material *m, bool *out) {
    MCObjectReadScope scope = {0};
    NativeArrayResult result = read_begin(m, out, &scope);
    if (result == NATIVE_ARRAY_OK) {
        if (m->isTranslucent)
            *out = false;
        else
            result = Material_blocksMovement(m, out);
    }
    MCObjectReadScope_end(&scope);
    return result;
}
#define BOOL_GETTER(method, field)                                                                 \
    NativeArrayResult Material_##method(Material *m, bool *out) {                                  \
        MCObjectReadScope scope = {0};                                                             \
        NativeArrayResult result = read_begin(m, out, &scope);                                     \
        if (result == NATIVE_ARRAY_OK)                                                             \
            *out = m->field;                                                                       \
        MCObjectReadScope_end(&scope);                                                             \
        return result;                                                                             \
    }
BOOL_GETTER(getCanBurn, canBurn)
BOOL_GETTER(isReplaceable, replaceable)
BOOL_GETTER(isToolNotRequired, requiresNoTool)
#undef BOOL_GETTER
NativeArrayResult Material_getMaterialMobility(Material *m, int32_t *out) {
    MCObjectReadScope scope = {0};
    NativeArrayResult result = read_begin(m, out, &scope);
    if (result == NATIVE_ARRAY_OK)
        *out = m->mobilityFlag;
    MCObjectReadScope_end(&scope);
    return result;
}
NativeArrayResult Material_getMaterialMapColor(Material *m, MapColor **out) {
    MCObjectReadScope scope = {0};
    NativeArrayResult result = read_begin(m, out, &scope);
    if (result == NATIVE_ARRAY_OK)
        *out = m->materialMapColor;
    MCObjectReadScope_end(&scope);
    return result;
}
static NativeArrayResult mutation_begin(Material *m, Material **out, MCObjectRootScope *scope) {
    if (!m)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectHeap *h = m->object.heap;
    if (!Material_isInstance((MCObject *)m) || !out || MCObjectHeap_failed(h) ||
        !color_owned(h, m->materialMapColor) || !MCObjectRootScope_begin(scope, h) ||
        !MCObjectRootScope_pin(scope, (MCObject *)m))
        return fail(h);
    return NATIVE_ARRAY_OK;
}
#define SETTER(method, field, value)                                                               \
    NativeArrayResult Material_##method(Material *m, Material **out) {                             \
        MCObjectRootScope scope = {0};                                                             \
        NativeArrayResult result = mutation_begin(m, out, &scope);                                 \
        if (result == NATIVE_ARRAY_OK) {                                                           \
            m->field = value;                                                                      \
            MCObjectHeap_touch(m->object.heap);                                                    \
            *out = m;                                                                              \
        }                                                                                          \
        MCObjectRootScope_end(&scope);                                                             \
        return result;                                                                             \
    }
SETTER(setRequiresTool, requiresNoTool, false)
SETTER(setBurning, canBurn, true)
SETTER(setReplaceable, replaceable, true)
SETTER(setNoPushMobility, mobilityFlag, 1)
SETTER(setImmovableMobility, mobilityFlag, 2)
SETTER(setAdventureModeExempt, isAdventureModeExempt, true)
#undef SETTER
static NativeArrayResult Material_setTranslucent(Material *m, Material **out) {
    MCObjectRootScope scope = {0};
    NativeArrayResult result = mutation_begin(m, out, &scope);
    if (result == NATIVE_ARRAY_OK) {
        m->isTranslucent = true;
        MCObjectHeap_touch(m->object.heap);
        *out = m;
    }
    MCObjectRootScope_end(&scope);
    return result;
}

#define NAMED_FIELDS(V)                                                                            \
    V(air)                                                                                         \
    V(grass) V(ground) V(wood) V(rock) V(iron) V(anvil) V(water) V(lava) V(leaves) V(plants)       \
        V(vine) V(sponge) V(cloth) V(fire) V(sand) V(circuits) V(carpet) V(glass) V(redstoneLight) \
            V(tnt) V(coral) V(ice) V(packedIce) V(snow) V(craftedSnow) V(cactus) V(clay) V(gourd)  \
                V(dragonEgg) V(portal) V(cake) V(web) V(piston) V(barrier)
static void trace_statics(MCObject *o, MCObjectVisitor v, void *ctx) {
    if (!tracked(o->heap, o) || MCObjectHeap_objectSize(o) < sizeof(MaterialStatics)) {
        fail(o->heap);
        return;
    }
    MaterialStatics *s = (MaterialStatics *)o;
#define VISIT(name) s->name = (Material *)v((MCObject *)s->name, ctx);
    NAMED_FIELDS(VISIT)
#undef VISIT
}
static const MCObjectClass staticsClass = {"native.Material.Statics", MCObjectHeap_plainClone,
                                           trace_statics, NULL};
static bool any(const MCObject *o, void *ctx) {
    (void)o;
    (void)ctx;
    return true;
}
static bool statics_ready(MaterialStatics *s) {
    if (MCObjectHeap_objectSize((MCObject *)s) < sizeof(*s) ||
        s->nativeInitializationState != MATERIAL_STATIC_READY)
        return false;
#define VALIDATE(name)                                                                             \
    if (!tracked(s->object.heap, (MCObject *)s->name) ||                                           \
        !Material_isInstance((MCObject *)s->name) ||                                               \
        !color_owned(s->object.heap, s->name->materialMapColor))                                   \
        return false;
    NAMED_FIELDS(VALIDATE)
#undef VALIDATE
    return true;
}
static NativeArrayResult transparent_construct(Material *m, MapColor *c) {
    return MaterialTransparent_construct((MaterialTransparent *)m, c);
}
static NativeArrayResult liquid_construct(Material *m, MapColor *c) {
    return MaterialLiquid_construct((MaterialLiquid *)m, c);
}
static NativeArrayResult logic_construct(Material *m, MapColor *c) {
    return MaterialLogic_construct((MaterialLogic *)m, c);
}
static NativeArrayResult portal_construct(Material *m, MapColor *c) {
    return MaterialPortal_construct((MaterialPortal *)m, c);
}
static Material *anonymous_allocate(MCObjectHeap *h) {
    return (Material *)MCObjectHeap_alloc(h, sizeof(Material), &anonymousClass);
}
MaterialStatics *Material_getStatics(MCObjectHeap *h) {
    if (!h || MCObjectHeap_failed(h)) {
        fail(h);
        return NULL;
    }
    MaterialStatics *s = (MaterialStatics *)MCObjectHeap_findObject(h, &staticsClass, any, NULL);
    if (s) {
        if (!statics_ready(s)) {
            fail(h);
            return NULL;
        }
        return s;
    }
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, h))
        return NULL;
    s = (MaterialStatics *)MCObjectHeap_alloc(h, sizeof(*s), &staticsClass);
    MCObjectRoot root = {0};
    bool ok = s && MCObjectRoot_init(&root, h, (MCObject *)s);
    /* The native holder/root is a lifetime prefix. Each original expression
       allocates its actual instance BEFORE reaching its MapColor argument. */
#define CALL(method)                                                                               \
    do {                                                                                           \
        if (ok)                                                                                    \
            ok = Material_##method(m, &m) == NATIVE_ARRAY_OK;                                      \
    } while (0)
#define INITIALIZE(name, allocate, ctor, colorField, chain)                                        \
    do {                                                                                           \
        if (ok) {                                                                                  \
            Material *m = (Material *)allocate(h);                                                 \
            MapColorStatics *colors = m ? MapColor_getStatics(h) : NULL;                           \
            ok = m && colors;                                                                      \
            if (ok)                                                                                \
                ok = ctor(m, colors->colorField) == NATIVE_ARRAY_OK;                               \
            if (ok) {                                                                              \
                chain;                                                                             \
            }                                                                                      \
            if (ok) {                                                                              \
                s->name = m;                                                                       \
                MCObjectHeap_touch(h);                                                             \
            }                                                                                      \
        }                                                                                          \
    } while (0)
    INITIALIZE(air, MaterialTransparent_nativeAllocate, transparent_construct, airColor, (void)0);
    INITIALIZE(grass, Material_nativeAllocate, Material_construct, grassColor, (void)0);
    INITIALIZE(ground, Material_nativeAllocate, Material_construct, dirtColor, (void)0);
    INITIALIZE(wood, Material_nativeAllocate, Material_construct, woodColor, CALL(setBurning));
    INITIALIZE(rock, Material_nativeAllocate, Material_construct, stoneColor,
               CALL(setRequiresTool));
    INITIALIZE(iron, Material_nativeAllocate, Material_construct, ironColor, CALL(setRequiresTool));
    INITIALIZE(anvil, Material_nativeAllocate, Material_construct, ironColor, CALL(setRequiresTool);
               CALL(setImmovableMobility));
    INITIALIZE(water, MaterialLiquid_nativeAllocate, liquid_construct, waterColor,
               CALL(setNoPushMobility));
    INITIALIZE(lava, MaterialLiquid_nativeAllocate, liquid_construct, tntColor,
               CALL(setNoPushMobility));
    INITIALIZE(leaves, Material_nativeAllocate, Material_construct, foliageColor, CALL(setBurning);
               CALL(setTranslucent); CALL(setNoPushMobility));
    INITIALIZE(plants, MaterialLogic_nativeAllocate, logic_construct, foliageColor,
               CALL(setNoPushMobility));
    INITIALIZE(vine, MaterialLogic_nativeAllocate, logic_construct, foliageColor, CALL(setBurning);
               CALL(setNoPushMobility); CALL(setReplaceable));
    INITIALIZE(sponge, Material_nativeAllocate, Material_construct, yellowColor, (void)0);
    INITIALIZE(cloth, Material_nativeAllocate, Material_construct, clothColor, CALL(setBurning));
    INITIALIZE(fire, MaterialTransparent_nativeAllocate, transparent_construct, airColor,
               CALL(setNoPushMobility));
    INITIALIZE(sand, Material_nativeAllocate, Material_construct, sandColor, (void)0);
    INITIALIZE(circuits, MaterialLogic_nativeAllocate, logic_construct, airColor,
               CALL(setNoPushMobility));
    INITIALIZE(carpet, MaterialLogic_nativeAllocate, logic_construct, clothColor, CALL(setBurning));
    INITIALIZE(glass, Material_nativeAllocate, Material_construct, airColor, CALL(setTranslucent);
               CALL(setAdventureModeExempt));
    INITIALIZE(redstoneLight, Material_nativeAllocate, Material_construct, airColor,
               CALL(setAdventureModeExempt));
    INITIALIZE(tnt, Material_nativeAllocate, Material_construct, tntColor, CALL(setBurning);
               CALL(setTranslucent));
    INITIALIZE(coral, Material_nativeAllocate, Material_construct, foliageColor,
               CALL(setNoPushMobility));
    INITIALIZE(ice, Material_nativeAllocate, Material_construct, iceColor, CALL(setTranslucent);
               CALL(setAdventureModeExempt));
    INITIALIZE(packedIce, Material_nativeAllocate, Material_construct, iceColor,
               CALL(setAdventureModeExempt));
    INITIALIZE(snow, MaterialLogic_nativeAllocate, logic_construct, snowColor, CALL(setReplaceable);
               CALL(setTranslucent); CALL(setRequiresTool); CALL(setNoPushMobility));
    INITIALIZE(craftedSnow, Material_nativeAllocate, Material_construct, snowColor,
               CALL(setRequiresTool));
    INITIALIZE(cactus, Material_nativeAllocate, Material_construct, foliageColor,
               CALL(setTranslucent);
               CALL(setNoPushMobility));
    INITIALIZE(clay, Material_nativeAllocate, Material_construct, clayColor, (void)0);
    INITIALIZE(gourd, Material_nativeAllocate, Material_construct, foliageColor,
               CALL(setNoPushMobility));
    INITIALIZE(dragonEgg, Material_nativeAllocate, Material_construct, foliageColor,
               CALL(setNoPushMobility));
    INITIALIZE(portal, MaterialPortal_nativeAllocate, portal_construct, airColor,
               CALL(setImmovableMobility));
    INITIALIZE(cake, Material_nativeAllocate, Material_construct, airColor,
               CALL(setNoPushMobility));
    INITIALIZE(web, anonymous_allocate, Material_construct, clothColor, CALL(setRequiresTool);
               CALL(setNoPushMobility));
    INITIALIZE(piston, Material_nativeAllocate, Material_construct, stoneColor,
               CALL(setImmovableMobility));
    INITIALIZE(barrier, Material_nativeAllocate, Material_construct, airColor,
               CALL(setRequiresTool);
               CALL(setImmovableMobility));
#undef INITIALIZE
#undef CALL
    if (s) {
        s->nativeInitializationState = ok ? MATERIAL_STATIC_READY : MATERIAL_STATIC_FAILED;
        MCObjectHeap_touch(h);
    }
    if (!ok)
        fail(h);
    MCObjectRootScope_end(&scope);
    return ok && !MCObjectHeap_failed(h) ? s : NULL;
}
#undef NAMED_FIELDS
