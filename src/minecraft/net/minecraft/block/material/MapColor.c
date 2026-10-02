#include "block/material/MapColor.h"
#include <string.h>

static const MCObjectClass colorClass, staticsClass;
static NativeArrayResult fail(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return NATIVE_ARRAY_FAILURE;
}
static bool identity(const MCObject *object, void *context) { return object == context; }
static bool tracked(MCObjectHeap *heap, const MCObject *object) {
    return object && object->heap == heap && object->klass &&
           MCObjectHeap_findObject(heap, object->klass, identity, (void *)object) == object;
}
bool MapColor_isInstance(const MCObject *object) {
    return object && object->klass == &colorClass && tracked(object->heap, object) &&
           MCObjectHeap_objectSize(object) >= sizeof(MapColor);
}
static const NativeJavaClassDescriptor *const parents[] = {&NativeJavaClass_ObjectClass};
const NativeJavaClassDescriptor MapColor_Class = {
    "net.minecraft.block.material.MapColor", parents, 1, MapColor_isInstance};
NativeJavaClass *MapColor_nativeClass(MCObjectHeap *heap) {
    return NativeJavaClass_literal(heap, &MapColor_Class);
}
static void trace_statics(MCObject *object, MCObjectVisitor visitor, void *context) {
    if (MCObjectHeap_objectSize(object) < sizeof(MapColorStatics)) {
        fail(object->heap);
        return;
    }
    MapColorStatics *s = (MapColorStatics *)object;
    s->mapColorArray = (NativeTypedObjectArray *)visitor((MCObject *)s->mapColorArray, context);
#define VISIT(field) s->field = (MapColor *)visitor((MCObject *)s->field, context)
    VISIT(airColor); VISIT(grassColor); VISIT(sandColor); VISIT(clothColor); VISIT(tntColor);
    VISIT(iceColor); VISIT(ironColor); VISIT(foliageColor); VISIT(snowColor); VISIT(clayColor);
    VISIT(dirtColor); VISIT(stoneColor); VISIT(waterColor); VISIT(woodColor); VISIT(quartzColor);
    VISIT(adobeColor); VISIT(magentaColor); VISIT(lightBlueColor); VISIT(yellowColor);
    VISIT(limeColor); VISIT(pinkColor); VISIT(grayColor); VISIT(silverColor); VISIT(cyanColor);
    VISIT(purpleColor); VISIT(blueColor); VISIT(brownColor); VISIT(greenColor); VISIT(redColor);
    VISIT(blackColor); VISIT(goldColor); VISIT(diamondColor); VISIT(lapisColor);
    VISIT(emeraldColor); VISIT(obsidianColor); VISIT(netherrackColor);
#undef VISIT
}
static const MCObjectClass colorClass = {
    "net.minecraft.block.material.MapColor", MCObjectHeap_plainClone, NULL, NULL};
static const MCObjectClass staticsClass = {
    "native.MapColor.Statics", MCObjectHeap_plainClone, trace_statics, NULL};
MapColor *MapColor_nativeAllocate(MCObjectHeap *heap) {
    return (MapColor *)MCObjectHeap_alloc(heap, sizeof(MapColor), &colorClass);
}
/* Validate the native array/component shape before a dependency reads tracked
   allocation metadata. Source NULL and short arrays remain reached exceptions. */
static NativeArrayResult register_color(MapColorStatics *s, MapColor *value, int32_t index) {
    MCObjectHeap *heap = value->object.heap;
    NativeTypedObjectArray *array = s->mapColorArray;
    if (!array)
        return NATIVE_ARRAY_EXCEPTION;
    if (!tracked(heap, (MCObject *)array) ||
        !NativeObjectArray_isRuntimeClass((MCObject *)array) ||
        MCObjectHeap_objectSize((MCObject *)array) < sizeof(*array))
        return fail(heap);
    NativeJavaClass *component = array->componentType;
    if (!component || !tracked(heap, (MCObject *)component) ||
        MCObjectHeap_objectSize((MCObject *)component) < sizeof(*component) ||
        !NativeJavaClass_isInstance((MCObject *)component) ||
        !NativeTypedObjectArray_isInstance((MCObject *)array))
        return fail(heap);
    NativeJavaClass *expected = MapColor_nativeClass(heap);
    bool compatible = false;
    if (!expected || !NativeTypedObjectArray_isAssignableTo(array, expected, &compatible) ||
        !compatible)
        return fail(heap);
    return NativeTypedObjectArray_set(array, index, (MCObject *)value);
}
static NativeArrayResult construct(MapColor *value, MapColorStatics *s, int32_t index,
                                   int32_t color) {
    if (index < 0 || index > 63)
        return NATIVE_ARRAY_EXCEPTION;
    value->colorIndex = index;
    value->colorValue = color;
    MCObjectHeap_touch(value->object.heap);
    return register_color(s, value, index);
}
static bool any(const MCObject *object, void *context) {
    (void)object;
    (void)context;
    return true;
}
MapColorStatics *MapColor_getStatics(MCObjectHeap *heap) {
    if (!heap || MCObjectHeap_failed(heap))
        return NULL;
    MapColorStatics *s = (MapColorStatics *)MCObjectHeap_findObject(heap, &staticsClass, any, NULL);
    if (s) {
        if (MCObjectHeap_objectSize((MCObject *)s) < sizeof(*s) ||
            s->nativeInitializationState != MAP_COLOR_STATIC_READY) {
            fail(heap);
            return NULL;
        }
        return s;
    }
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap))
        return NULL;
    s = (MapColorStatics *)MCObjectHeap_alloc(heap, sizeof(*s), &staticsClass);
    MCObjectRoot root = {0};
    bool ok = s && MCObjectRoot_init(&root, heap, (MCObject *)s);
    NativeJavaClass *component = ok ? MapColor_nativeClass(heap) : NULL;
    if (!component || NativeTypedObjectArray_new(heap, component, 64, &s->mapColorArray) !=
                          NATIVE_ARRAY_OK)
        ok = false;
    MapColor *color = NULL;
    /* Declaration-order assignment is after each successful constructor. This
       helper uses the initializing holder directly, avoiding recursive lookup.
       The named references do not derive from the mutable array afterward. */
#define INITIALIZE(field, index, rgb) do { \
    if (ok) { \
        color = MapColor_nativeAllocate(heap); \
        ok = color && construct(color, s, index, rgb) == NATIVE_ARRAY_OK; \
        if (ok) { s->field = color; MCObjectHeap_touch(heap); } \
    } \
} while (0)
    INITIALIZE(airColor, 0, 0);
    INITIALIZE(grassColor, 1, 8368696);
    INITIALIZE(sandColor, 2, 16247203);
    INITIALIZE(clothColor, 3, 13092807);
    INITIALIZE(tntColor, 4, 16711680);
    INITIALIZE(iceColor, 5, 10526975);
    INITIALIZE(ironColor, 6, 10987431);
    INITIALIZE(foliageColor, 7, 31744);
    INITIALIZE(snowColor, 8, 16777215);
    INITIALIZE(clayColor, 9, 10791096);
    INITIALIZE(dirtColor, 10, 9923917);
    INITIALIZE(stoneColor, 11, 7368816);
    INITIALIZE(waterColor, 12, 4210943);
    INITIALIZE(woodColor, 13, 9402184);
    INITIALIZE(quartzColor, 14, 16776437);
    INITIALIZE(adobeColor, 15, 14188339);
    INITIALIZE(magentaColor, 16, 11685080);
    INITIALIZE(lightBlueColor, 17, 6724056);
    INITIALIZE(yellowColor, 18, 15066419);
    INITIALIZE(limeColor, 19, 8375321);
    INITIALIZE(pinkColor, 20, 15892389);
    INITIALIZE(grayColor, 21, 5000268);
    INITIALIZE(silverColor, 22, 10066329);
    INITIALIZE(cyanColor, 23, 5013401);
    INITIALIZE(purpleColor, 24, 8339378);
    INITIALIZE(blueColor, 25, 3361970);
    INITIALIZE(brownColor, 26, 6704179);
    INITIALIZE(greenColor, 27, 6717235);
    INITIALIZE(redColor, 28, 10040115);
    INITIALIZE(blackColor, 29, 1644825);
    INITIALIZE(goldColor, 30, 16445005);
    INITIALIZE(diamondColor, 31, 6085589);
    INITIALIZE(lapisColor, 32, 4882687);
    INITIALIZE(emeraldColor, 33, 55610);
    INITIALIZE(obsidianColor, 34, 8476209);
    INITIALIZE(netherrackColor, 35, 7340544);
#undef INITIALIZE
    if (ok) {
        s->nativeInitializationState = MAP_COLOR_STATIC_READY;
        MCObjectHeap_touch(heap);
    } else {
        if (s) {
            s->nativeInitializationState = MAP_COLOR_STATIC_FAILED;
            MCObjectHeap_touch(heap);
        }
        fail(heap);
    }
    MCObjectRootScope_end(&scope);
    return ok && !MCObjectHeap_failed(heap) ? s : NULL;
}
NativeArrayResult MapColor_construct(MapColor *value, int32_t index, int32_t color) {
    if (!value)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectHeap *heap = value->object.heap;
    if (!MapColor_isInstance((MCObject *)value) || MCObjectHeap_failed(heap))
        return fail(heap);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap) ||
        !MCObjectRootScope_pin(&scope, (MCObject *)value)) {
        MCObjectRootScope_end(&scope);
        return fail(heap);
    }
    MapColorStatics *s = MapColor_getStatics(heap);
    NativeArrayResult result = s ? construct(value, s, index, color) : fail(heap);
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult MapColor_new(MCObjectHeap *heap, int32_t index, int32_t color,
                              MapColor **out) {
    if (!heap || !out || MCObjectHeap_failed(heap))
        return fail(heap);
    MapColorStatics *s = MapColor_getStatics(heap);
    if (!s)
        return fail(heap);
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap))
        return fail(heap);
    MapColor *value = MapColor_nativeAllocate(heap);
    NativeArrayResult result = value ? construct(value, s, index, color) : fail(heap);
    if (result == NATIVE_ARRAY_OK)
        *out = value;
    MCObjectRootScope_end(&scope);
    return result;
}
NativeArrayResult MapColor_getMapColor(MapColor *value, int32_t shade, int32_t *out) {
    if (!value)
        return NATIVE_ARRAY_EXCEPTION;
    MCObjectHeap *heap = value->object.heap;
    if (!MapColor_isInstance((MCObject *)value) || !out || MCObjectHeap_failed(heap))
        return fail(heap);
    MCObjectReadScope scope = {0};
    if (!MCObjectReadScope_begin(&scope, heap))
        return fail(heap);
    int32_t factor = 220;
    if (shade == 3) factor = 135;
    if (shade == 2) factor = 255;
    if (shade == 1) factor = 220;
    if (shade == 0) factor = 180;
    uint32_t red = ((uint32_t)value->colorValue >> 16 & 255u) * (uint32_t)factor / 255u;
    uint32_t green = ((uint32_t)value->colorValue >> 8 & 255u) * (uint32_t)factor / 255u;
    uint32_t blue = ((uint32_t)value->colorValue & 255u) * (uint32_t)factor / 255u;
    uint32_t bits = UINT32_C(0xff000000) | red << 16 | green << 8 | blue;
    memcpy(out, &bits, sizeof(bits));
    MCObjectReadScope_end(&scope);
    return NATIVE_ARRAY_OK;
}
