#include "block/material/MapColor.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr, "Source MapColor check %u line %d: %s\n", checks, __LINE__, #x); \
    exit(1); } } while (0)

static MapColor *named(MapColorStatics *s, int i) {
    MapColor *values[] = {
        s->airColor, s->grassColor, s->sandColor, s->clothColor, s->tntColor,
        s->iceColor, s->ironColor, s->foliageColor, s->snowColor, s->clayColor,
        s->dirtColor, s->stoneColor, s->waterColor, s->woodColor, s->quartzColor,
        s->adobeColor, s->magentaColor, s->lightBlueColor, s->yellowColor,
        s->limeColor, s->pinkColor, s->grayColor, s->silverColor, s->cyanColor,
        s->purpleColor, s->blueColor, s->brownColor, s->greenColor, s->redColor,
        s->blackColor, s->goldColor, s->diamondColor, s->lapisColor,
        s->emeraldColor, s->obsidianColor, s->netherrackColor};
    CHECK(i >= 0 && i < 36);
    return values[i];
}
static bool any(const MCObject *o, void *ctx) { (void)o; (void)ctx; return true; }
static uint32_t argb(MapColor *color, int32_t shade) {
    int32_t out = 0;
    CHECK(MapColor_getMapColor(color, shade, &out) == NATIVE_ARRAY_OK);
    return (uint32_t)out;
}
static void raw_allocation_runtime_class_before_statics(void) {
    MCObjectHeap *h = MCObjectHeap_new(1000000); CHECK(h);
    MapColor *raw = MapColor_nativeAllocate(h); CHECK(raw);
    CHECK(MCObjectHeap_liveObjects(h) == 1 && !raw->colorIndex && !raw->colorValue);
    NativeJavaClass *type = NativeJavaClass_getClass(h, (MCObject *)raw);
    CHECK(type && type->descriptor == &MapColor_Class && !MCObjectHeap_failed(h));
    CHECK(MCObjectHeap_liveObjects(h) == 2);
    CHECK(NativeJavaClass_getClass(h, (MCObject *)raw) == type);
    /* Native allocation/class lookup is explicitly not implicit Source NEW.
       The source constructor resolves its static prerequisite afterward. */
    CHECK(MapColor_construct(raw, 20, 0x112233) == NATIVE_ARRAY_OK);
    MapColorStatics *s = MapColor_getStatics(h); CHECK(s);
    CHECK(s->mapColorArray->componentType == type && s->mapColorArray->values[20] == (MCObject *)raw);
    CHECK(s->pinkColor != raw && raw->colorIndex == 20 && raw->colorValue == 0x112233);
    MCObjectHeap_free(h);
}
/* Missing a named root or reconstructing it from the array loses original
   identity after legal replacement and after collection/snapshot adoption. */
static void statics_registration_and_shades(void) {
    MCObjectHeap *h = MCObjectHeap_new(1000000);
    CHECK(h);
    MapColorStatics *s = MapColor_getStatics(h);
    CHECK(s && s->nativeInitializationState == MAP_COLOR_STATIC_READY && MapColor_getStatics(h) == s);
    CHECK(s->mapColorArray && s->mapColorArray->length == 64);
    CHECK(s->mapColorArray->componentType == MapColor_nativeClass(h));
    CHECK(s->mapColorArray->componentType->descriptor == &MapColor_Class);
    for (int i = 0; i < 36; ++i) {
        MapColor *c = named(s, i);
        CHECK(c && MapColor_isInstance((MCObject *)c) && c->colorIndex == i);
        CHECK(s->mapColorArray->values[i] == (MCObject *)c);
    }
    for (int i = 36; i < 64; ++i) CHECK(!s->mapColorArray->values[i]);
    CHECK(argb(s->grassColor, 0) == UINT32_C(0xff597d27));
    CHECK(argb(s->grassColor, 1) == UINT32_C(0xff6d9930));
    CHECK(argb(s->grassColor, 2) == UINT32_C(0xff7fb238));
    CHECK(argb(s->grassColor, 3) == UINT32_C(0xff435e1d));
    CHECK(argb(s->grassColor, 4) == UINT32_C(0xff6d9930));
    CHECK(argb(s->grassColor, -1) == UINT32_C(0xff6d9930));
    CHECK(argb(s->grassColor, INT_MIN) == UINT32_C(0xff6d9930));
    CHECK(argb(s->grassColor, INT_MAX) == UINT32_C(0xff6d9930));
    CHECK(argb(s->airColor, 2) == UINT32_C(0xff000000));
    CHECK(argb(s->snowColor, 0) == UINT32_C(0xffb4b4b4));
    CHECK(argb(s->snowColor, 3) == UINT32_C(0xff878787));
    CHECK(argb(s->waterColor, 2) == UINT32_C(0xff4040ff));
    MapColor *original = s->grassColor, *replacement = NULL;
    CHECK(MapColor_new(h, 1, -1, &replacement) == NATIVE_ARRAY_OK && replacement);
    CHECK(replacement != original && s->grassColor == original);
    CHECK(s->mapColorArray->values[1] == (MCObject *)replacement);
    CHECK(argb(replacement, 2) == UINT32_C(0xffffffff));
    CHECK(NativeTypedObjectArray_set(s->mapColorArray, 1, NULL) == NATIVE_ARRAY_OK);
    CHECK(MCObjectHeap_collect(h));
    s = MapColor_getStatics(h);
    CHECK(s && s->grassColor == original && argb(original, 2) == UINT32_C(0xff7fb238));
    CHECK(!s->mapColorArray->values[1]);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void constructor_source_exception_prefixes(void) {
    MCObjectHeap *h = MCObjectHeap_new(1000000);
    CHECK(h);
    MapColorStatics *s = MapColor_getStatics(h);
    MapColor *c = MapColor_nativeAllocate(h);
    CHECK(s && c);
    c->colorValue = 17; c->colorIndex = 18;
    const int32_t bad[] = {-1, 64, INT_MIN, INT_MAX};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(*bad); ++i) {
        CHECK(MapColor_construct(c, bad[i], 99) == NATIVE_ARRAY_EXCEPTION);
        CHECK(c->colorValue == 17 && c->colorIndex == 18 && !MCObjectHeap_failed(h));
    }
    MapColor *out = s->airColor;
    CHECK(MapColor_new(h, -1, 11, &out) == NATIVE_ARRAY_EXCEPTION && out == s->airColor);
    CHECK(MapColor_construct(c, 63, INT_MIN) == NATIVE_ARRAY_OK);
    CHECK(c->colorIndex == 63 && c->colorValue == INT_MIN && s->mapColorArray->values[63] == (MCObject *)c);
    CHECK(argb(c, 2) == UINT32_C(0xff000000));
    NativeTypedObjectArray *shortArray = NULL;
    CHECK(NativeTypedObjectArray_new(h, MapColor_nativeClass(h), 1, &shortArray) == NATIVE_ARRAY_OK);
    /* The static final array-ref replacement here is an explicit reflective
       corruption fixture, distinct from ordinary legal element mutation. */
    s->mapColorArray = shortArray;
    CHECK(MapColor_construct(c, 1, 0x010203) == NATIVE_ARRAY_EXCEPTION);
    CHECK(c->colorIndex == 1 && c->colorValue == 0x010203 && !shortArray->values[0]);
    CHECK(!MCObjectHeap_failed(h));
    s->mapColorArray = NULL;
    CHECK(MapColor_construct(c, 2, 0x102030) == NATIVE_ARRAY_EXCEPTION);
    CHECK(c->colorIndex == 2 && c->colorValue == 0x102030 && !MCObjectHeap_failed(h));
    CHECK(MapColor_construct(c, 64, 0) == NATIVE_ARRAY_EXCEPTION && c->colorIndex == 2);
    CHECK(MapColor_construct(NULL, 0, 0) == NATIVE_ARRAY_EXCEPTION);
    int32_t rgb = 71;
    CHECK(MapColor_getMapColor(NULL, 0, &rgb) == NATIVE_ARRAY_EXCEPTION && rgb == 71);
    CHECK(argb(c, 2) == UINT32_C(0xff102030));
    CHECK(MapColor_getStatics(h) == s && s->grassColor->colorIndex == 1);
    MCObjectHeap_free(h);
}
static void clone_adopt_preserves_independent_named_refs(void) {
    MCObjectHeap *h = MCObjectHeap_new(1000000), *other = MCObjectHeap_new(1000000);
    CHECK(h && other);
    MapColorStatics *s = MapColor_getStatics(h), *different = MapColor_getStatics(other);
    CHECK(s && different && s != different && s->grassColor != different->grassColor);
    NativeTypedObjectArray *array = NULL;
    CHECK(NativeTypedObjectArray_new(h, MapColor_nativeClass(h), 64, &array) == NATIVE_ARRAY_OK);
    s->mapColorArray = array;
    MapColor *replacement = NULL;
    CHECK(MapColor_new(h, 1, 0x010203, &replacement) == NATIVE_ARRAY_OK);
    CHECK(NativeTypedObjectArray_set(array, 2, (MCObject *)replacement) == NATIVE_ARRAY_OK);
    MCObjectRoot root = {0}; CHECK(MCObjectRoot_init(&root, h, (MCObject *)replacement));
    CHECK(MCObjectHeap_collect(h));
    CHECK(s->grassColor->colorValue == 8368696 && s->sandColor && !array->values[0]);
    MCObjectHeap *branch = MCObjectHeap_clone(h); CHECK(branch);
    MCObjectRoot br = {0}; CHECK(MCObjectRoot_rebind(&br, branch, &root));
    MapColorStatics *copy = MapColor_getStatics(branch);
    CHECK(copy && copy != s && copy->grassColor != s->grassColor);
    CHECK(copy->mapColorArray != array && copy->mapColorArray->values[1] == MCObjectRoot_get(&br));
    CHECK(copy->mapColorArray->values[2] == copy->mapColorArray->values[1]);
    CHECK((MCObject *)copy->grassColor != copy->mapColorArray->values[1]);
    CHECK(MapColor_construct((MapColor *)MCObjectRoot_get(&br), 1, 0xabcdef) == NATIVE_ARRAY_OK);
    CHECK(replacement->colorValue == 0x010203);
    CHECK(MCObjectHeap_canAdopt(h, branch) && MCObjectHeap_adopt(h, branch));
    MCObjectHeap_free(branch);
    s = MapColor_getStatics(h);
    CHECK(s && s->mapColorArray->values[1] == MCObjectRoot_get(&root));
    CHECK(((MapColor *)MCObjectRoot_get(&root))->colorValue == 0xabcdef);
    CHECK(s->grassColor->colorValue == 8368696 && argb(s->grassColor, 2) == UINT32_C(0xff7fb238));
    MCObjectRoot_drop(&root);
    CHECK(MCObjectHeap_collect(h) && MapColor_getStatics(h) == s);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(other); MCObjectHeap_free(h);
}
static void malformed_native_shapes(void) {
    for (int which = 0; which < 7; ++which) {
        MCObjectHeap *h = MCObjectHeap_new(1000000), *f = MCObjectHeap_new(1000000);
        CHECK(h && f);
        MapColorStatics *s = MapColor_getStatics(h);
        MapColor *c = MapColor_nativeAllocate(h);
        CHECK(s && c);
        c->colorIndex = 59; c->colorValue = 23;
        MCObject fake = {h, c->object.klass};
        if (which == 0) {
            CHECK(!MapColor_isInstance(&fake));
            CHECK(MapColor_construct((MapColor *)&fake, 1, 7) == NATIVE_ARRAY_FAILURE);
        } else if (which == 1) {
            MCObject *tiny = MCObjectHeap_alloc(h, sizeof(MCObject), c->object.klass); CHECK(tiny);
            CHECK(!MapColor_isInstance(tiny));
            int32_t out = 91;
            CHECK(MapColor_getMapColor((MapColor *)tiny, 1, &out) == NATIVE_ARRAY_FAILURE && out == 91);
        } else {
            if (which == 2) {
                s->mapColorArray = MapColor_getStatics(f)->mapColorArray;
            } else if (which == 3) {
                s->mapColorArray->length = INT_MAX;
            } else if (which == 4) {
                s->mapColorArray->componentType = MapColor_nativeClass(f);
            } else if (which == 5) {
                MCObject *tiny = MCObjectHeap_alloc(h, sizeof(MCObject), s->mapColorArray->componentType->object.klass);
                CHECK(tiny); s->mapColorArray->componentType = (NativeJavaClass *)tiny;
            } else {
                s->mapColorArray->componentType = NULL;
            }
            CHECK(MapColor_construct(c, 1, 7) == NATIVE_ARRAY_FAILURE);
            CHECK(c->colorIndex == 1 && c->colorValue == 7);
        }
        CHECK(MCObjectHeap_failed(h) && !MCObjectHeap_failed(f));
        MCObjectHeap_free(f); MCObjectHeap_free(h);
    }
}
static void allocation_failure_keeps_declaration_prefix(void) {
    MCObjectHeap *h = MCObjectHeap_new(1000000); CHECK(h);
    MapColorStatics *s = MapColor_getStatics(h); CHECK(s);
    const MCObjectClass *holderClass = s->object.klass;
    const size_t prefixBytes = sizeof(MapColorStatics) + sizeof(NativeJavaClass) +
                               sizeof(NativeTypedObjectArray) + 64 * sizeof(MCObject *);
    CHECK(MCObjectHeap_liveBytes(h) == prefixBytes + 36 * sizeof(MapColor));
    MCObjectHeap_free(h);
    for (int completed = 0; completed < 36; ++completed) {
        h = MCObjectHeap_new(prefixBytes + (size_t)completed * sizeof(MapColor)); CHECK(h);
        CHECK(!MapColor_getStatics(h) && MCObjectHeap_failed(h));
        s = (MapColorStatics *)MCObjectHeap_findObject(h, holderClass, any, NULL);
        CHECK(s && s->mapColorArray && s->nativeInitializationState == MAP_COLOR_STATIC_FAILED);
        for (int i = 0; i < 36; ++i) {
            MapColor *c = named(s, i);
            if (i < completed) {
                CHECK(c && c->colorIndex == i && s->mapColorArray->values[i] == (MCObject *)c);
            } else {
                CHECK(!c && !s->mapColorArray->values[i]);
            }
        }
        CHECK(!MapColor_getStatics(h));
        MCObjectHeap_free(h);
    }
    h = MCObjectHeap_new(sizeof(MapColorStatics) - 1); CHECK(h);
    CHECK(!MapColor_getStatics(h) && MCObjectHeap_failed(h));
    CHECK(!MCObjectHeap_findObject(h, holderClass, any, NULL)); MCObjectHeap_free(h);
    h = MCObjectHeap_new(prefixBytes + 36 * sizeof(MapColor)); CHECK(h);
    s = MapColor_getStatics(h); CHECK(s);
    MapColor *out = s->grassColor;
    CHECK(MapColor_new(h, 62, 77, &out) == NATIVE_ARRAY_FAILURE && out == s->grassColor);
    CHECK(MCObjectHeap_failed(h) && !s->mapColorArray->values[62]);
    MCObjectHeap_free(h);
}
int main(void) {
    raw_allocation_runtime_class_before_statics();
    statics_registration_and_shades();
    constructor_source_exception_prefixes();
    clone_adopt_preserves_independent_named_refs();
    malformed_native_shapes();
    allocation_failure_keeps_declaration_prefix();
    printf("Source MapColor: %u checks\n", checks);
    return 0;
}
