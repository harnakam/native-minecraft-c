#ifndef C919_SOURCE_MATERIAL_H
#define C919_SOURCE_MATERIAL_H
#include "block/material/MapColor.h"

/* The seven original instance fields, with one most-derived managed owner.
   Known virtual dispatch covers the base, four named subclasses and Material$1.
   Arbitrary subclasses and full JVM class initialization/Throwable are pending. */
typedef struct Material {
    MCObject object;
    bool canBurn;
    bool replaceable;
    bool isTranslucent;
    MapColor *materialMapColor;
    bool requiresNoTool;
    int32_t mobilityFlag;
    bool isAdventureModeExempt;
} Material;

extern const NativeJavaClassDescriptor Material_Class;
extern const NativeJavaClassDescriptor MaterialAnonymous1_Class;
/* Allocation descriptor, not a managed Class literal. Obtain a literal through
   NativeJavaClass_literal(heap, &Material_Class) only when actually reached. */
const MCObjectClass *Material_nativeClass(void);
bool Material_isInstance(const MCObject *);
bool Material_isRuntimeClass(const MCObject *);
Material *Material_nativeAllocate(MCObjectHeap *);
NativeArrayResult Material_construct(Material *, MapColor *nullableColor);
NativeArrayResult Material_new(MCObjectHeap *, MapColor *nullableColor, Material **out);
NativeArrayResult Material_isLiquid(Material *, bool *out);
NativeArrayResult Material_isSolid(Material *, bool *out);
NativeArrayResult Material_blocksLight(Material *, bool *out);
NativeArrayResult Material_blocksMovement(Material *, bool *out);
NativeArrayResult Material_isOpaque(Material *, bool *out);
NativeArrayResult Material_getCanBurn(Material *, bool *out);
NativeArrayResult Material_isReplaceable(Material *, bool *out);
NativeArrayResult Material_isToolNotRequired(Material *, bool *out);
NativeArrayResult Material_getMaterialMobility(Material *, int32_t *out);
NativeArrayResult Material_getMaterialMapColor(Material *, MapColor **out);
/* Original protected methods are exposed to translated constructors/statics;
   setReplaceable is public. Source has no isAdventureModeExempt getter. */
NativeArrayResult Material_setRequiresTool(Material *, Material **out);
NativeArrayResult Material_setBurning(Material *, Material **out);
NativeArrayResult Material_setReplaceable(Material *, Material **out);
NativeArrayResult Material_setNoPushMobility(Material *, Material **out);
NativeArrayResult Material_setImmovableMobility(Material *, Material **out);
NativeArrayResult Material_setAdventureModeExempt(Material *, Material **out);
/* Exact original base bodies used by closed inherited dispatch. */
NativeArrayResult Material_isLiquid_base(Material *, bool *out);
NativeArrayResult Material_isSolid_base(Material *, bool *out);
NativeArrayResult Material_blocksLight_base(Material *, bool *out);
NativeArrayResult Material_blocksMovement_base(Material *, bool *out);

typedef enum {
    MATERIAL_STATIC_INITIALIZING,
    MATERIAL_STATIC_READY,
    MATERIAL_STATIC_FAILED
} MaterialStaticInitializationState;
typedef struct MaterialStatics {
    MCObject object;
    Material *air, *grass, *ground, *wood, *rock, *iron, *anvil, *water, *lava;
    Material *leaves, *plants, *vine, *sponge, *cloth, *fire, *sand, *circuits;
    Material *carpet, *glass, *redstoneLight, *tnt, *coral, *ice, *packedIce;
    Material *snow, *craftedSnow, *cactus, *clay, *gourd, *dragonEgg, *portal;
    Material *cake, *web, *piston, *barrier;
    /* Native rooted class-static lifetime guard, not a Source declared field. */
    MaterialStaticInitializationState nativeInitializationState;
} MaterialStatics;
MaterialStatics *Material_getStatics(MCObjectHeap *);

/* Native shared lifetime/shape guards for the actual subclass return bodies;
   these are not invented Material Source methods. traceFields visits the one
   nullable color edge exactly once and guards partial/undersized instances. */
void Material_traceFields(Material *, MCObjectVisitor, void *);
NativeArrayResult Material_nativeReturnBoolean(Material *, bool value, bool *out);
/* NULL Source receiver: healthy EXCEPTION. Invalid shape/owner/out, OOM or a
   failed heap: sticky FAILURE. NULL color is legal. Out changes only on OK.
   Raw allocation performs neither statics nor Class-literal initialization.
   Retain/root returned borrowed refs before collection or graph adoption. */
#endif
