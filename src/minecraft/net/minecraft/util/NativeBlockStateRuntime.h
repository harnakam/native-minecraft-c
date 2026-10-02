#ifndef C919_NATIVE_BLOCK_STATE_RUNTIME_H
#define C919_NATIVE_BLOCK_STATE_RUNTIME_H
#include "util/ObjectIntIdentityMap.h"
#include "block/material/Material.h"
typedef struct NativeBlockStateRuntime NativeBlockStateRuntime;
typedef struct NativeBlock NativeBlock;
typedef struct NativeBlockState NativeBlockState;
/* Explicit native immutable target-registry identities/properties. These are
   not full Source Block or BlockState.StateImplementation classes. Material
   edges retain the actual named Source statics, not property snapshots.
   Registered valid-state order/default/meta aliases come from unchanged 1.8.9
   registry observations; full property maps/transitions and physics are absent.
   The actual ObjectIntIdentityMap remains mutable and authoritative for IDs. */
struct NativeBlockState {
    MCObject object;NativeBlockStateRuntime *runtime;NativeBlock *block;
    int32_t metadata,ordinal;
};
struct NativeBlock {
    MCObject object;NativeBlockStateRuntime *runtime;NativeBlockState *defaultState;
    Material *material;int32_t registeredId,lightOpacity,lightValue;
    bool tickRandomly;
};
typedef struct NativeBlockStateDependencies {
    NativeBlock *(*getBlock)(MCObject *,NativeBlockState *);
    NativeBlockState *(*getDefaultState)(MCObject *,NativeBlock *);
    bool (*getTickRandomly)(MCObject *,NativeBlock *,bool *);
    bool (*getMetaFromState)(MCObject *,NativeBlock *,NativeBlockState *,int32_t *);
    bool (*getLightOpacity)(MCObject *,NativeBlock *,int32_t *);
    Material *(*getMaterial)(MCObject *,NativeBlock *);
    bool (*blocksMovement)(MCObject *,Material *,bool *);
    bool (*isLiquid)(MCObject *,Material *,bool *);
} NativeBlockStateDependencies;
struct NativeBlockStateRuntime {
    MCObject object;ObjectIntIdentityMap *BLOCK_STATE_IDS;
    NativeObjectArray *blocks,*validStates,*materials;
    NativeBlock *air,*barrier;Material *airMaterial,*leavesMaterial;
    const NativeBlockStateDependencies *dependencies;MCObject *context;
};
NativeBlockStateRuntime *NativeBlockStateRuntime_get(MCObjectHeap *);
/* Native dense raw-ID bridge. Returns the proven named Source static without
   creating the Block/state registry. Unknown IDs return NULL on a healthy
   heap. It does not dispatch mutable NativeBlock fields/dependencies. */
Material *NativeBlockStateRuntime_materialForBlockId(MCObjectHeap *,int32_t registeredId);
bool NativeBlockStateRuntime_isInstance(const MCObject *);
bool NativeBlock_isInstance(const MCObject *);
bool NativeBlockState_isInstance(const MCObject *);
bool NativeBlockStateRuntime_bindDependencies(NativeBlockStateRuntime *,const NativeBlockStateDependencies *,MCObject *);
NativeBlock *NativeBlockStateRuntime_block(NativeBlockStateRuntime *,int32_t registeredId);
NativeBlockState *NativeBlockStateRuntime_validState(NativeBlockStateRuntime *,int32_t blockId,int32_t ordinal);
/* NULL on an unmapped/out-of-range ID is a successful registry lookup. */
NativeBlockState *NativeBlockStateRuntime_state(NativeBlockStateRuntime *,int32_t rawId);
NativeBlockState *NativeBlockState_nativeNew(NativeBlockStateRuntime *,NativeBlock *,int32_t metadata);
NativeBlock *NativeBlockState_getBlock(NativeBlockState *);
NativeBlockState *NativeBlock_getDefaultState(NativeBlock *);
bool NativeBlock_getTickRandomly(NativeBlock *,bool *out);
bool NativeBlock_getMetaFromState(NativeBlock *,NativeBlockState *,int32_t *out);
bool NativeBlock_getLightOpacity(NativeBlock *,int32_t *out);
Material *NativeBlock_getMaterial(NativeBlock *);
/* Explicit native interception boundaries. The runtime is supplied by the
   caller; Material has no runtime/context field or hidden registry lookup.
   Without a hook these call the actual Source Material method. */
bool NativeBlockStateRuntime_materialBlocksMovement(NativeBlockStateRuntime *,Material *,bool *out);
bool NativeBlockStateRuntime_materialIsLiquid(NativeBlockStateRuntime *,Material *,bool *out);
#endif
