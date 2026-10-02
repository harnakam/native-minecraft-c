#include "block/material/Material.h"
#include "block/material/MaterialLiquid.h"
#include "block/material/MaterialLogic.h"
#include "block/material/MaterialPortal.h"
#include "block/material/MaterialTransparent.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Source Material check %u line %d: %s\n", checks, __LINE__, #x);       \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static void raw_allocation_is_a_single_zeroed_owner(void) {
    MCObjectHeap *h = MCObjectHeap_new(1000000);
    CHECK(h);
    Material *m = Material_nativeAllocate(h);
    CHECK(m);
    CHECK(MCObjectHeap_liveObjects(h) == 1 && !m->canBurn && !m->replaceable && !m->isTranslucent &&
          !m->materialMapColor && !m->requiresNoTool && !m->mobilityFlag &&
          !m->isAdventureModeExempt);
    MCObjectHeap_free(h);
}
static Material *raw(MCObjectHeap *h, int kind) {
    switch (kind) {
    case 0:
        return Material_nativeAllocate(h);
    case 1:
        return (Material *)MaterialTransparent_nativeAllocate(h);
    case 2:
        return (Material *)MaterialLiquid_nativeAllocate(h);
    case 3:
        return (Material *)MaterialLogic_nativeAllocate(h);
    case 4:
        return (Material *)MaterialPortal_nativeAllocate(h);
    default:
        CHECK(false);
        return NULL;
    }
}
static NativeArrayResult construct(Material *m, int kind, MapColor *c) {
    switch (kind) {
    case 0:
        return Material_construct(m, c);
    case 1:
        return MaterialTransparent_construct((MaterialTransparent *)m, c);
    case 2:
        return MaterialLiquid_construct((MaterialLiquid *)m, c);
    case 3:
        return MaterialLogic_construct((MaterialLogic *)m, c);
    case 4:
        return MaterialPortal_construct((MaterialPortal *)m, c);
    default:
        CHECK(false);
        return NATIVE_ARRAY_FAILURE;
    }
}
static const NativeJavaClassDescriptor *fact(int kind) {
    const NativeJavaClassDescriptor *facts[] = {&Material_Class,       &MaterialTransparent_Class,
                                                &MaterialLiquid_Class, &MaterialLogic_Class,
                                                &MaterialPortal_Class, &MaterialAnonymous1_Class};
    CHECK(kind >= 0 && kind < 6);
    return facts[kind];
}
static Material *named(MaterialStatics *s, int i) {
    Material *values[] = {s->air,         s->grass,    s->ground, s->wood,      s->rock,
                          s->iron,        s->anvil,    s->water,  s->lava,      s->leaves,
                          s->plants,      s->vine,     s->sponge, s->cloth,     s->fire,
                          s->sand,        s->circuits, s->carpet, s->glass,     s->redstoneLight,
                          s->tnt,         s->coral,    s->ice,    s->packedIce, s->snow,
                          s->craftedSnow, s->cactus,   s->clay,   s->gourd,     s->dragonEgg,
                          s->portal,      s->cake,     s->web,    s->piston,    s->barrier};
    CHECK(i >= 0 && i < 35);
    return values[i];
}
static bool any(const MCObject *o, void *c) {
    (void)o;
    (void)c;
    return true;
}
typedef struct {
    int kind, color, mobility;
    bool burning, replaceable, translucent, noTool, exempt;
} StaticWant;
/* Independently transcribed supplied Source declaration expressions, including
   subclass constructor effects. Not a palette or native material-group order. */
static const StaticWant wants[] = {
    {1, 0, 0, 0, 1, 0, 1, 0},  {0, 1, 0, 0, 0, 0, 1, 0},  {0, 10, 0, 0, 0, 0, 1, 0},
    {0, 13, 0, 1, 0, 0, 1, 0}, {0, 11, 0, 0, 0, 0, 0, 0}, {0, 6, 0, 0, 0, 0, 0, 0},
    {0, 6, 2, 0, 0, 0, 0, 0},  {2, 12, 1, 0, 1, 0, 1, 0}, {2, 4, 1, 0, 1, 0, 1, 0},
    {0, 7, 1, 1, 0, 1, 1, 0},  {3, 7, 1, 0, 0, 0, 1, 1},  {3, 7, 1, 1, 1, 0, 1, 1},
    {0, 18, 0, 0, 0, 0, 1, 0}, {0, 3, 0, 1, 0, 0, 1, 0},  {1, 0, 1, 0, 1, 0, 1, 0},
    {0, 2, 0, 0, 0, 0, 1, 0},  {3, 0, 1, 0, 0, 0, 1, 1},  {3, 3, 0, 1, 0, 0, 1, 1},
    {0, 0, 0, 0, 0, 1, 1, 1},  {0, 0, 0, 0, 0, 0, 1, 1},  {0, 4, 0, 1, 0, 1, 1, 0},
    {0, 7, 1, 0, 0, 0, 1, 0},  {0, 5, 0, 0, 0, 1, 1, 1},  {0, 5, 0, 0, 0, 0, 1, 1},
    {3, 8, 1, 0, 1, 1, 0, 1},  {0, 8, 0, 0, 0, 0, 0, 0},  {0, 7, 1, 0, 0, 1, 1, 0},
    {0, 9, 0, 0, 0, 0, 1, 0},  {0, 7, 1, 0, 0, 0, 1, 0},  {0, 7, 1, 0, 0, 0, 1, 0},
    {4, 0, 2, 0, 0, 0, 1, 0},  {0, 0, 1, 0, 0, 0, 1, 0},  {5, 3, 1, 0, 0, 0, 0, 0},
    {0, 11, 2, 0, 0, 0, 1, 0}, {0, 0, 2, 0, 0, 0, 0, 0}};
static void methods(Material *m, int kind) {
    static const bool expected[6][4] = {{0, 1, 1, 1}, {0, 0, 0, 0}, {1, 0, 1, 0},
                                        {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 1, 1, 0}};
    bool value = !expected[kind][0];
    CHECK(Material_isLiquid(m, &value) == NATIVE_ARRAY_OK && value == expected[kind][0]);
    CHECK(Material_isSolid(m, &value) == NATIVE_ARRAY_OK && value == expected[kind][1]);
    CHECK(Material_blocksLight(m, &value) == NATIVE_ARRAY_OK && value == expected[kind][2]);
    CHECK(Material_blocksMovement(m, &value) == NATIVE_ARRAY_OK && value == expected[kind][3]);
    CHECK(Material_isOpaque(m, &value) == NATIVE_ARRAY_OK &&
          value == (!m->isTranslucent && expected[kind][3]));
    CHECK(Material_getCanBurn(m, &value) == NATIVE_ARRAY_OK && value == m->canBurn);
    CHECK(Material_isReplaceable(m, &value) == NATIVE_ARRAY_OK && value == m->replaceable);
    CHECK(Material_isToolNotRequired(m, &value) == NATIVE_ARRAY_OK && value == m->requiresNoTool);
    int32_t mobility = -99;
    CHECK(Material_getMaterialMobility(m, &mobility) == NATIVE_ARRAY_OK &&
          mobility == m->mobilityFlag);
    MapColor *color = (MapColor *)m;
    CHECK(Material_getMaterialMapColor(m, &color) == NATIVE_ARRAY_OK &&
          color == m->materialMapColor);
}
/* Resetting implicit allocation defaults in construct erases early subtype
   writes. A manufactured base object breaks direct subtype/parent aliases. */
static void initializer_vs_allocation_defaults_and_class_identity(void) {
    for (int kind = 0; kind < 5; ++kind) {
        MCObjectHeap *h = MCObjectHeap_new(1000000);
        CHECK(h);
        Material *m = raw(h, kind);
        CHECK(m && MCObjectHeap_liveObjects(h) == 1);
        NativeJavaClass *klass = NativeJavaClass_getClass(h, (MCObject *)m);
        CHECK(klass && klass->descriptor == fact(kind));
        bool compatible = false;
        CHECK(NativeJavaClass_isInstanceOf(klass, (MCObject *)m, &compatible) && compatible);
        CHECK(NativeJavaClass_isAssignableFrom(NativeJavaClass_literal(h, &Material_Class), klass,
                                               &compatible) &&
              compatible);
        CHECK(NativeJavaClass_isAssignableFrom(NativeJavaClass_Object(h), klass, &compatible) &&
              compatible);
        CHECK(Material_isInstance((MCObject *)m) &&
              Material_isRuntimeClass((MCObject *)m) == (kind == 0));
        m->canBurn = true;
        m->replaceable = true;
        m->isTranslucent = true;
        m->mobilityFlag = 19;
        m->isAdventureModeExempt = true;
        CHECK(construct(m, kind, NULL) == NATIVE_ARRAY_OK);
        CHECK(m->canBurn && m->replaceable && m->isTranslucent && m->requiresNoTool &&
              m->isAdventureModeExempt && !m->materialMapColor);
        CHECK(m->mobilityFlag == (kind == 2 ? 1 : 19));
        methods(m, kind);
        MapColor *c = MapColor_nativeAllocate(h);
        CHECK(c);
        CHECK(construct(m, kind, c) == NATIVE_ARRAY_OK && m->materialMapColor == c);
        if (kind == 1) {
            bool out = true;
            CHECK(MaterialTransparent_isSolid((MaterialTransparent *)m, &out) == NATIVE_ARRAY_OK &&
                  !out);
            CHECK(MaterialTransparent_blocksLight((MaterialTransparent *)m, &out) ==
                      NATIVE_ARRAY_OK &&
                  !out);
            CHECK(MaterialTransparent_blocksMovement((MaterialTransparent *)m, &out) ==
                      NATIVE_ARRAY_OK &&
                  !out);
        } else if (kind == 2) {
            bool out = false;
            CHECK(MaterialLiquid_isLiquid((MaterialLiquid *)m, &out) == NATIVE_ARRAY_OK && out);
            CHECK(MaterialLiquid_blocksMovement((MaterialLiquid *)m, &out) == NATIVE_ARRAY_OK &&
                  !out);
            CHECK(MaterialLiquid_isSolid((MaterialLiquid *)m, &out) == NATIVE_ARRAY_OK && !out);
        } else if (kind == 3) {
            bool out = true;
            CHECK(MaterialLogic_isSolid((MaterialLogic *)m, &out) == NATIVE_ARRAY_OK && !out);
            CHECK(MaterialLogic_blocksLight((MaterialLogic *)m, &out) == NATIVE_ARRAY_OK && !out);
            CHECK(MaterialLogic_blocksMovement((MaterialLogic *)m, &out) == NATIVE_ARRAY_OK &&
                  !out);
        } else if (kind == 4) {
            bool out = true;
            CHECK(MaterialPortal_isSolid((MaterialPortal *)m, &out) == NATIVE_ARRAY_OK && !out);
            CHECK(MaterialPortal_blocksLight((MaterialPortal *)m, &out) == NATIVE_ARRAY_OK && !out);
            CHECK(MaterialPortal_blocksMovement((MaterialPortal *)m, &out) == NATIVE_ARRAY_OK &&
                  !out);
        }
        MCObjectHeap_free(h);
    }
}
static void statics_and_aliases_are_source_refs(void) {
    MCObjectHeap *h = MCObjectHeap_new(1000000);
    CHECK(h);
    MaterialStatics *s = Material_getStatics(h);
    CHECK(s && s->nativeInitializationState == MATERIAL_STATIC_READY);
    MapColorStatics *colors = MapColor_getStatics(h);
    CHECK(colors);
    CHECK(sizeof(MaterialTransparent) == sizeof(Material) &&
          sizeof(MaterialLiquid) == sizeof(Material) && sizeof(MaterialLogic) == sizeof(Material) &&
          sizeof(MaterialPortal) == sizeof(Material));
    size_t objects = MCObjectHeap_liveObjects(h), bytes = MCObjectHeap_liveBytes(h);
    CHECK(Material_getStatics(h) == s && MCObjectHeap_liveObjects(h) == objects &&
          MCObjectHeap_liveBytes(h) == bytes);
    for (int i = 0; i < 35; ++i) {
        Material *m = named(s, i);
        const StaticWant *w = &wants[i];
        CHECK(m);
        CHECK(m->canBurn == w->burning && m->replaceable == w->replaceable &&
              m->isTranslucent == w->translucent && m->requiresNoTool == w->noTool &&
              m->isAdventureModeExempt == w->exempt && m->mobilityFlag == w->mobility);
        CHECK(m->materialMapColor == (MapColor *)colors->mapColorArray->values[w->color]);
        NativeJavaClass *klass = NativeJavaClass_getClass(h, (MCObject *)m);
        CHECK(klass && klass->descriptor == fact(w->kind));
        for (int j = 0; j < i; ++j)
            CHECK(m != named(s, j));
        methods(m, w->kind);
    }
    CHECK(s->air != s->fire && s->air->materialMapColor == s->fire->materialMapColor);
    CHECK(s->iron != s->anvil && s->iron->materialMapColor == s->anvil->materialMapColor);
    CHECK(s->ice != s->packedIce && s->ice->materialMapColor == s->packedIce->materialMapColor);
    CHECK(s->web->materialMapColor == colors->clothColor && !s->web->requiresNoTool);
    CHECK(s->coral != s->plants && s->coral->materialMapColor == s->plants->materialMapColor);
    MapColor *original = s->grass->materialMapColor, *replacement = NULL;
    CHECK(MapColor_new(h, 1, 0x010203, &replacement) == NATIVE_ARRAY_OK);
    CHECK(replacement != original && colors->mapColorArray->values[1] == (MCObject *)replacement);
    CHECK(s->grass->materialMapColor == original && colors->grassColor == original);
    CHECK(NativeTypedObjectArray_set(colors->mapColorArray, 1, NULL) == NATIVE_ARRAY_OK);
    CHECK(MCObjectHeap_collect(h));
    CHECK(Material_getStatics(h) == s && s->grass->materialMapColor == original);
    MCObjectHeap_free(h);
}
static void setters_preserve_exact_receiver_and_snapshot_revision(void) {
    MCObjectHeap *h = MCObjectHeap_new(1000000);
    CHECK(h);
    MaterialLiquid *liquid = MaterialLiquid_nativeAllocate(h);
    CHECK(liquid);
    CHECK(MaterialLiquid_construct(liquid, NULL) == NATIVE_ARRAY_OK);
    Material *m = &liquid->material, *out = NULL;
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, h, (MCObject *)m));
    MCObjectHeap *branch = MCObjectHeap_clone(h);
    CHECK(branch);
    CHECK(MCObjectHeap_canAdopt(h, branch));
    CHECK(Material_setBurning(m, &out) == NATIVE_ARRAY_OK && out == m && m->canBurn);
    CHECK(!MCObjectHeap_canAdopt(h, branch));
    MCObjectHeap_free(branch);
    CHECK(Material_setRequiresTool(m, &out) == NATIVE_ARRAY_OK && out == m && !m->requiresNoTool);
    CHECK(Material_setReplaceable(m, &out) == NATIVE_ARRAY_OK && out == m && m->replaceable);
    CHECK(Material_setImmovableMobility(m, &out) == NATIVE_ARRAY_OK && out == m &&
          m->mobilityFlag == 2);
    CHECK(Material_setNoPushMobility(m, &out) == NATIVE_ARRAY_OK && out == m &&
          m->mobilityFlag == 1);
    CHECK(Material_setAdventureModeExempt(m, &out) == NATIVE_ARRAY_OK && out == m &&
          m->isAdventureModeExempt);
    CHECK(Material_isInstance((MCObject *)out) && MaterialLiquid_isRuntimeClass((MCObject *)out));
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(h) && MCObjectHeap_liveObjects(h) == 0);
    MCObjectHeap_free(h);
}
/* A factory must keep both the exact requested subtype and its nullable color,
   and must not publish a partially constructed output on native failure. */
static void factories_and_direct_base_bodies(void) {
    MCObjectHeap *h = MCObjectHeap_new(1000000);
    CHECK(h);
    MaterialStatics *s = Material_getStatics(h);
    CHECK(s);
    Material *base = NULL;
    MaterialTransparent *transparent = NULL;
    MaterialLiquid *liquid = NULL;
    MaterialLogic *logic = NULL;
    MaterialPortal *portal = NULL;
    CHECK(Material_new(h, NULL, &base) == NATIVE_ARRAY_OK && base && !base->materialMapColor);
    CHECK(MaterialTransparent_new(h, s->wood->materialMapColor, &transparent) == NATIVE_ARRAY_OK);
    CHECK(MaterialLiquid_new(h, NULL, &liquid) == NATIVE_ARRAY_OK);
    CHECK(MaterialLogic_new(h, s->web->materialMapColor, &logic) == NATIVE_ARRAY_OK);
    CHECK(MaterialPortal_new(h, NULL, &portal) == NATIVE_ARRAY_OK);
    CHECK(Material_isRuntimeClass((MCObject *)base) &&
          MaterialTransparent_isRuntimeClass((MCObject *)transparent));
    CHECK(MaterialLiquid_isRuntimeClass((MCObject *)liquid) &&
          MaterialLogic_isRuntimeClass((MCObject *)logic));
    CHECK(MaterialPortal_isRuntimeClass((MCObject *)portal));
    CHECK(transparent->material.materialMapColor == s->wood->materialMapColor &&
          logic->material.materialMapColor == s->web->materialMapColor &&
          !liquid->material.materialMapColor && !portal->material.materialMapColor);
    bool out = false;
    CHECK(Material_isLiquid_base(&liquid->material, &out) == NATIVE_ARRAY_OK && !out);
    CHECK(Material_isSolid_base(&transparent->material, &out) == NATIVE_ARRAY_OK && out);
    CHECK(Material_blocksLight_base(&logic->material, &out) == NATIVE_ARRAY_OK && out);
    CHECK(Material_blocksMovement_base(&portal->material, &out) == NATIVE_ARRAY_OK && out);
    CHECK(Material_isLiquid(&liquid->material, &out) == NATIVE_ARRAY_OK && out);
    CHECK(Material_isSolid(&transparent->material, &out) == NATIVE_ARRAY_OK && !out);
    CHECK(Material_blocksLight(&logic->material, &out) == NATIVE_ARRAY_OK && !out);
    CHECK(Material_blocksMovement(&portal->material, &out) == NATIVE_ARRAY_OK && !out);
    MCObjectHeap_free(h);
    for (int kind = 0; kind < 5; ++kind) {
        h = MCObjectHeap_new(1000000);
        CHECK(h);
        s = Material_getStatics(h);
        CHECK(s);
        size_t count = MCObjectHeap_liveObjects(h);
        NativeArrayResult result = kind == 0   ? Material_new(h, NULL, NULL)
                                   : kind == 1 ? MaterialTransparent_new(h, NULL, NULL)
                                   : kind == 2 ? MaterialLiquid_new(h, NULL, NULL)
                                   : kind == 3 ? MaterialLogic_new(h, NULL, NULL)
                                               : MaterialPortal_new(h, NULL, NULL);
        CHECK(result == NATIVE_ARRAY_FAILURE && MCObjectHeap_failed(h) &&
              MCObjectHeap_liveObjects(h) == count);
        MCObjectHeap_free(h);
    }
}
static void clone_gc_adopt_keeps_named_and_custom_aliases(void) {
    MCObjectHeap *h = MCObjectHeap_new(1000000);
    CHECK(h);
    MaterialStatics *s = Material_getStatics(h);
    CHECK(s);
    Material *custom = Material_nativeAllocate(h);
    CHECK(custom);
    CHECK(Material_construct(custom, s->wood->materialMapColor) == NATIVE_ARRAY_OK);
    MCObjectRoot customRoot = {0};
    CHECK(MCObjectRoot_init(&customRoot, h, (MCObject *)custom));
    size_t baseline = MCObjectHeap_liveObjects(h);
    CHECK(Material_nativeAllocate(h));
    CHECK(MCObjectHeap_collect(h) && MCObjectHeap_liveObjects(h) == baseline);
    MCObjectHeap *working = MCObjectHeap_clone(h);
    CHECK(working);
    MCObjectRoot wr = {0};
    CHECK(MCObjectRoot_rebind(&wr, working, &customRoot));
    MaterialStatics *copy = Material_getStatics(working);
    CHECK(copy && copy != s);
    Material *wc = (Material *)MCObjectRoot_get(&wr);
    CHECK(wc && wc != custom);
    CHECK(wc->materialMapColor == copy->wood->materialMapColor &&
          wc->materialMapColor != custom->materialMapColor);
    CHECK(copy->wood->materialMapColor == MapColor_getStatics(working)->woodColor);
    Material *out = NULL;
    CHECK(Material_setBurning(wc, &out) == NATIVE_ARRAY_OK && out == wc);
    CHECK(!custom->canBurn && MCObjectHeap_canAdopt(h, working) && MCObjectHeap_adopt(h, working));
    MCObjectHeap_free(working);
    s = Material_getStatics(h);
    custom = (Material *)MCObjectRoot_get(&customRoot);
    CHECK(s && custom && custom->canBurn && custom->materialMapColor == s->wood->materialMapColor);
    CHECK(s->iron->materialMapColor == s->anvil->materialMapColor);
    CHECK(MCObjectHeap_collect(h) && Material_getStatics(h) == s);
    MCObjectRoot_drop(&customRoot);
    CHECK(MCObjectHeap_collect(h));
    size_t count = MCObjectHeap_liveObjects(h);
    CHECK(Material_getStatics(h) == s && MCObjectHeap_collect(h) &&
          MCObjectHeap_liveObjects(h) == count);
    MCObjectHeap_free(h);
}
static void nulls_and_output_preservation(void) {
    bool value = true;
    int32_t number = 77;
    MapColor *color = NULL;
    Material *out = NULL;
    CHECK(Material_construct(NULL, NULL) == NATIVE_ARRAY_EXCEPTION);
    CHECK(Material_isLiquid(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value);
    CHECK(Material_isSolid(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value);
    CHECK(Material_blocksLight(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value);
    CHECK(Material_blocksMovement(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value);
    CHECK(Material_isOpaque(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value);
    CHECK(Material_getCanBurn(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value);
    CHECK(Material_isReplaceable(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value);
    CHECK(Material_isToolNotRequired(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value);
    CHECK(Material_getMaterialMobility(NULL, &number) == NATIVE_ARRAY_EXCEPTION && number == 77);
    CHECK(Material_getMaterialMapColor(NULL, &color) == NATIVE_ARRAY_EXCEPTION && !color);
    CHECK(Material_setRequiresTool(NULL, &out) == NATIVE_ARRAY_EXCEPTION && !out);
    CHECK(Material_setBurning(NULL, &out) == NATIVE_ARRAY_EXCEPTION && !out);
    CHECK(Material_setReplaceable(NULL, &out) == NATIVE_ARRAY_EXCEPTION && !out);
    CHECK(Material_setNoPushMobility(NULL, &out) == NATIVE_ARRAY_EXCEPTION && !out);
    CHECK(Material_setImmovableMobility(NULL, &out) == NATIVE_ARRAY_EXCEPTION && !out);
    CHECK(Material_setAdventureModeExempt(NULL, &out) == NATIVE_ARRAY_EXCEPTION && !out);
    CHECK(MaterialTransparent_construct(NULL, NULL) == NATIVE_ARRAY_EXCEPTION);
    CHECK(MaterialLiquid_construct(NULL, NULL) == NATIVE_ARRAY_EXCEPTION);
    CHECK(MaterialLogic_construct(NULL, NULL) == NATIVE_ARRAY_EXCEPTION);
    CHECK(MaterialPortal_construct(NULL, NULL) == NATIVE_ARRAY_EXCEPTION);
    CHECK(MaterialTransparent_isSolid(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value);
    CHECK(MaterialLiquid_isLiquid(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value);
    CHECK(MaterialLogic_blocksLight(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value);
    CHECK(MaterialPortal_blocksMovement(NULL, &value) == NATIVE_ARRAY_EXCEPTION && value);
}
static void malformed_shapes_and_color_owners(void) {
    for (int kind = 0; kind < 5; ++kind)
        for (int bad = 0; bad < 7; ++bad) {
            MCObjectHeap *h = MCObjectHeap_new(1000000), *foreign = MCObjectHeap_new(1000000);
            CHECK(h && foreign);
            Material *m = raw(h, kind);
            CHECK(m);
            CHECK(construct(m, kind, NULL) == NATIVE_ARRAY_OK);
            bool value = true;
            Material *out = m;
            MCObject fake = {h, m->object.klass};
            if (bad == 0) {
                MCObject *tiny = MCObjectHeap_alloc(h, sizeof(MCObject), m->object.klass);
                CHECK(tiny);
                CHECK(!Material_isInstance(tiny));
                CHECK(Material_blocksMovement((Material *)tiny, &value) == NATIVE_ARRAY_FAILURE &&
                      value);
            } else if (bad == 1) {
                CHECK(!Material_isInstance(&fake));
                CHECK(Material_setBurning((Material *)&fake, &out) == NATIVE_ARRAY_FAILURE &&
                      out == m && !m->canBurn);
            } else if (bad == 2) {
                MapColor *other = MapColor_nativeAllocate(foreign);
                CHECK(other);
                m->requiresNoTool = false;
                CHECK(construct(m, kind, other) == NATIVE_ARRAY_FAILURE && !m->materialMapColor &&
                      !m->requiresNoTool);
                CHECK(!MCObjectHeap_failed(foreign));
            } else if (bad == 3) {
                MapColor *color = MapColor_nativeAllocate(h);
                CHECK(color);
                MCObject *tiny = MCObjectHeap_alloc(h, sizeof(MCObject), color->object.klass);
                CHECK(tiny);
                m->materialMapColor = (MapColor *)tiny;
                CHECK(Material_isLiquid(m, &value) == NATIVE_ARRAY_FAILURE && value);
            } else if (bad == 4) {
                MapColor *other = MapColor_nativeAllocate(foreign);
                CHECK(other);
                m->materialMapColor = other;
                MapColor *color = NULL;
                CHECK(Material_getMaterialMapColor(m, &color) == NATIVE_ARRAY_FAILURE && !color);
            } else if (bad == 5) {
                CHECK(Material_setRequiresTool(m, NULL) == NATIVE_ARRAY_FAILURE &&
                      m->requiresNoTool);
            } else {
                MCObjectHeap_fail(h);
                CHECK(Material_setImmovableMobility(m, &out) == NATIVE_ARRAY_FAILURE && out == m &&
                      m->mobilityFlag == (kind == 2 ? 1 : 0));
            }
            CHECK(MCObjectHeap_failed(h) && !MCObjectHeap_hasBorrowers(h));
            MCObjectHeap_free(foreign);
            MCObjectHeap_free(h);
        }
    for (int kind = 0; kind < 4; ++kind) {
        MCObjectHeap *h = MCObjectHeap_new(1000000);
        CHECK(h);
        Material *m = Material_nativeAllocate(h);
        CHECK(m);
        bool out = true;
        NativeArrayResult status =
            kind == 0   ? MaterialTransparent_isSolid((MaterialTransparent *)m, &out)
            : kind == 1 ? MaterialLiquid_isLiquid((MaterialLiquid *)m, &out)
            : kind == 2 ? MaterialLogic_blocksLight((MaterialLogic *)m, &out)
                        : MaterialPortal_blocksMovement((MaterialPortal *)m, &out);
        CHECK(status == NATIVE_ARRAY_FAILURE && out && MCObjectHeap_failed(h));
        MCObjectHeap_free(h);
    }
}
static void statics_minimum_shape_and_edge_guards(void) {
    MCObjectHeap *sample = MCObjectHeap_new(1000000);
    CHECK(sample);
    MaterialStatics *source = Material_getStatics(sample);
    CHECK(source);
    const MCObjectClass *holderClass = source->object.klass;
    for (int mode = 0; mode < 6; ++mode) {
        MCObjectHeap *h = MCObjectHeap_new(1000000), *foreign = MCObjectHeap_new(1000000);
        CHECK(h && foreign);
        if (mode <= 1) {
            MCObject *tiny = MCObjectHeap_alloc(h, sizeof(MCObject), holderClass);
            CHECK(tiny);
            if (mode == 0)
                CHECK(!Material_getStatics(h) && MCObjectHeap_failed(h));
            else {
                MCObjectRoot root = {0};
                CHECK(MCObjectRoot_init(&root, h, tiny));
                CHECK(!MCObjectHeap_collect(h) && MCObjectHeap_failed(h));
            }
        } else {
            MaterialStatics *s = Material_getStatics(h);
            CHECK(s);
            if (mode == 2)
                s->air = Material_nativeAllocate(foreign);
            else if (mode == 3) {
                MCObject *tiny = MCObjectHeap_alloc(h, sizeof(MCObject), s->grass->object.klass);
                CHECK(tiny);
                s->grass = (Material *)tiny;
            } else if (mode == 4)
                s->nativeInitializationState = MATERIAL_STATIC_INITIALIZING;
            else
                s->nativeInitializationState = MATERIAL_STATIC_FAILED;
            CHECK(!Material_getStatics(h) && MCObjectHeap_failed(h));
        }
        CHECK(!MCObjectHeap_failed(foreign));
        MCObjectHeap_free(foreign);
        MCObjectHeap_free(h);
    }
    MCObjectHeap_free(sample);
}
/* Managed aliases are traced only after validating the actual complete payload;
   descriptor identity alone must not let GC/clone read beyond a tiny instance. */
static void material_trace_rejects_undersized_payloads(void) {
    MCObjectHeap *sample = MCObjectHeap_new(1000000);
    CHECK(sample);
    MaterialStatics *s = Material_getStatics(sample);
    CHECK(s);
    const MCObjectClass *classes[] = {s->grass->object.klass,  s->air->object.klass,
                                      s->water->object.klass,  s->plants->object.klass,
                                      s->portal->object.klass, s->web->object.klass};
    for (int i = 0; i < 6; ++i)
        for (int mode = 0; mode < 2; ++mode) {
            MCObjectHeap *h = MCObjectHeap_new(1000000);
            CHECK(h);
            MCObject *tiny = MCObjectHeap_alloc(h, sizeof(MCObject), classes[i]);
            CHECK(tiny);
            MCObjectRoot root = {0};
            CHECK(MCObjectRoot_init(&root, h, tiny));
            if (mode == 0)
                CHECK(!MCObjectHeap_collect(h) && MCObjectHeap_failed(h));
            else
                CHECK(!MCObjectHeap_clone(h) && !MCObjectHeap_failed(h));
            MCObjectHeap_free(h);
        }
    MCObjectHeap_free(sample);
}
/* Native budget witness, not a JVM unreachable-object-count/OOM-byte claim.
   Air NEW must reach allocation before the first MapColor argument resolution. */
static void static_allocation_failure_preserves_reached_prefix(void) {
    MCObjectHeap *full = MCObjectHeap_new(1000000);
    CHECK(full);
    MapColorStatics *colors = MapColor_getStatics(full);
    CHECK(colors);
    size_t colorBytes = MCObjectHeap_liveBytes(full);
    MaterialStatics *s = Material_getStatics(full);
    CHECK(s);
    const MCObjectClass *holderClass = s->object.klass;
    size_t total = MCObjectHeap_liveBytes(full);
    CHECK(total == colorBytes + sizeof(MaterialStatics) + 35 * sizeof(Material));
    MCObjectHeap_free(full);
    MCObjectHeap *h = MCObjectHeap_new(sizeof(MaterialStatics) + sizeof(Material));
    CHECK(h);
    CHECK(!Material_getStatics(h) && MCObjectHeap_failed(h));
    s = (MaterialStatics *)MCObjectHeap_findObject(h, holderClass, any, NULL);
    CHECK(s && !s->air);
    MCObject *first = MCObjectHeap_findObject(h, MaterialTransparent_nativeClass(), any, NULL);
    CHECK(first && MCObjectHeap_liveObjects(h) == 2 && !((Material *)first)->requiresNoTool);
    CHECK(s->nativeInitializationState == MATERIAL_STATIC_FAILED);
    MCObjectHeap_free(h);
    for (int completed = 0; completed < 35; ++completed) {
        /* Prime only MapColor to isolate each subsequent Source declaration. */
        h = MCObjectHeap_new(colorBytes + sizeof(MaterialStatics) +
                             (size_t)completed * sizeof(Material));
        CHECK(h);
        CHECK(MapColor_getStatics(h));
        CHECK(!Material_getStatics(h) && MCObjectHeap_failed(h));
        s = (MaterialStatics *)MCObjectHeap_findObject(h, holderClass, any, NULL);
        CHECK(s);
        CHECK(s->nativeInitializationState == MATERIAL_STATIC_FAILED && !Material_getStatics(h));
        for (int i = 0; i < 35; ++i) {
            Material *m = named(s, i);
            if (i < completed) {
                CHECK(m && m->materialMapColor && m->requiresNoTool == wants[i].noTool &&
                      m->mobilityFlag == wants[i].mobility);
            } else
                CHECK(!m);
        }
        MCObjectHeap_free(h);
    }
    h = MCObjectHeap_new(total);
    CHECK(h);
    CHECK(Material_getStatics(h));
    Material *out = named(Material_getStatics(h), 0);
    Material *originalOut = out;
    CHECK(Material_new(h, NULL, &out) == NATIVE_ARRAY_FAILURE && out == originalOut);
    CHECK(MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
int main(void) {
    raw_allocation_is_a_single_zeroed_owner();
    initializer_vs_allocation_defaults_and_class_identity();
    statics_and_aliases_are_source_refs();
    setters_preserve_exact_receiver_and_snapshot_revision();
    factories_and_direct_base_bodies();
    clone_gc_adopt_keeps_named_and_custom_aliases();
    nulls_and_output_preservation();
    malformed_shapes_and_color_owners();
    statics_minimum_shape_and_edge_guards();
    material_trace_rejects_undersized_payloads();
    static_allocation_failure_preserves_reached_prefix();
    printf("Source Material: %u checks\n", checks);
    return 0;
}
