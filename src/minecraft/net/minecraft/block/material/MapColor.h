#ifndef C919_SOURCE_MAP_COLOR_H
#define C919_SOURCE_MAP_COLOR_H
#include "util/NativeTypedObjectArray.h"

typedef struct MapColor {
    MCObject object;
    int32_t colorValue;
    int32_t colorIndex;
} MapColor;

typedef enum {
    MAP_COLOR_STATIC_INITIALIZING,
    MAP_COLOR_STATIC_READY,
    MAP_COLOR_STATIC_FAILED
} MapColorStaticInitializationState;

typedef struct MapColorStatics {
    MCObject object;
    NativeTypedObjectArray *mapColorArray;
    MapColor *airColor, *grassColor, *sandColor, *clothColor, *tntColor, *iceColor;
    MapColor *ironColor, *foliageColor, *snowColor, *clayColor, *dirtColor, *stoneColor;
    MapColor *waterColor, *woodColor, *quartzColor, *adobeColor, *magentaColor;
    MapColor *lightBlueColor, *yellowColor, *limeColor, *pinkColor, *grayColor;
    MapColor *silverColor, *cyanColor, *purpleColor, *blueColor, *brownColor;
    MapColor *greenColor, *redColor, *blackColor, *goldColor, *diamondColor;
    MapColor *lapisColor, *emeraldColor, *obsidianColor, *netherrackColor;
    /* Native per-heap initialization guard, not an original declared field. */
    MapColorStaticInitializationState nativeInitializationState;
} MapColorStatics;

extern const NativeJavaClassDescriptor MapColor_Class;
NativeJavaClass *MapColor_nativeClass(MCObjectHeap *);
bool MapColor_isInstance(const MCObject *);
/* Raw zeroed allocation only. This helper does not claim Java NEW/class-init;
   new/construct resolve the per-heap static prerequisite separately. */
MapColor *MapColor_nativeAllocate(MCObjectHeap *);
/* The original private constructor is exposed for translated initialization.
   Range/NULL/index exceptions retain prior stores; native failure is sticky. */
NativeArrayResult MapColor_construct(MapColor *, int32_t index, int32_t color);
NativeArrayResult MapColor_new(MCObjectHeap *, int32_t index, int32_t color,
                              MapColor **out);
NativeArrayResult MapColor_getMapColor(MapColor *, int32_t shade, int32_t *out);
/* One native class-static root per heap. The typed array and all 36 named
   references are independent graph edges. Java class-init locks/Throwable and
   arbitrary MapColor subclasses are not provided by this concrete subset.
   mapColorArray and named references are final in Source; legal array element
   mutation does not replace the named references. */
MapColorStatics *MapColor_getStatics(MCObjectHeap *);
#endif
