#ifndef C919_SOURCE_MAP_DATA_VISIBILITY_H
#define C919_SOURCE_MAP_DATA_VISIBILITY_H
#include "world/storage/MapData.h"
#include "entity/item/EntityItemFrame.h"
#include "entity/player/InventoryPlayer.h"

/* Native virtual leaves reached by the translated method. NULL entries inherit
   the actual concrete Source methods. Private updateDecorations is always called
   directly and has no virtual substitute. This table is immutable; context belongs
   to a traced caller owner and is pinned for this call, not a second MapData
   authority. Callback outputs are borrowed and assigned only on OK. Unknown
   subtype behavior must be supplied explicitly, never inferred from a name. */
typedef struct MapDataVisibilityDependencies {
    WorldSavedDataResult (*inventoryHasItemStack)(MCObject *, InventoryPlayer *, ItemStack *, bool *);
    WorldSavedDataResult (*playerGetName)(MCObject *, MCGameplayPlayer *, NBTString **);
    WorldSavedDataResult (*frameGetHangingPosition)(MCObject *, EntityItemFrame *, BlockPos **);
    WorldSavedDataResult (*frameGetEntityId)(MCObject *, EntityItemFrame *, int32_t *);
    WorldSavedDataResult (*positionGetX)(MCObject *, BlockPos *, int32_t *);
    WorldSavedDataResult (*positionGetZ)(MCObject *, BlockPos *, int32_t *);
    WorldSavedDataResult (*facingGetHorizontalIndex)(MCObject *, const NativeHangingFacing *, int32_t *);
    WorldSavedDataResult (*tagHasKeyType)(MCObject *, NBTTagCompound *, const char *, int32_t, bool *);
    WorldSavedDataResult (*tagGetTagList)(MCObject *, NBTTagCompound *, const char *, int32_t, NBTTagList **);
    WorldSavedDataResult (*tagCount)(MCObject *, NBTTagList *, int32_t *);
    WorldSavedDataResult (*tagGetCompoundAt)(MCObject *, NBTTagList *, int32_t, NBTTagCompound **);
    WorldSavedDataResult (*tagGetString)(MCObject *, NBTTagCompound *, const char *, NBTString **);
    WorldSavedDataResult (*tagGetByte)(MCObject *, NBTTagCompound *, const char *, int32_t *);
    WorldSavedDataResult (*tagGetDouble)(MCObject *, NBTTagCompound *, const char *, double *);
    WorldSavedDataResult (*listSize)(MCObject *, NativeReferenceList *, int32_t *);
    WorldSavedDataResult (*listGet)(MCObject *, NativeReferenceList *, int32_t, MCObject **);
} MapDataVisibilityDependencies;
WorldSavedDataResult MapData_updateVisiblePlayers(MapData *, MCGameplayPlayer *, ItemStack *);
WorldSavedDataResult MapData_updateVisiblePlayersWithDependencies(MapData *, MCGameplayPlayer *,
    ItemStack *, const MapDataVisibilityDependencies *, MCObject *nullableContext);
/* Reached Source NULL/index/checkcast errors preserve prior mutations and a
   healthy heap. Native malformed/foreign/untracked references, unsupported
   dispatch and allocation failure are sticky FAILURE. This is the original
   method over the real managed collections/actors; full Java collection,
   arbitrary subtype/Throwable and World/Frame gameplay remain separate ports. */
/* Both the supplied Java declaration and target JAR make ItemStack final.
   This slice calls its actual concrete translated methods, with no impossible
   ItemStack subclass substituted through this dependency table. */
#endif
