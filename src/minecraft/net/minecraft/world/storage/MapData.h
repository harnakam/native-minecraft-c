#ifndef C919_SOURCE_MAP_DATA_H
#define C919_SOURCE_MAP_DATA_H
#include "util/NativeHashMap.h"
#include "util/NativeJavaClass.h"
#include "util/NativeLinkedHashMap.h"
#include "world/WorldSavedData.h"

typedef struct MapData MapData;
typedef struct MapInfo MapInfo;
typedef struct World World;
typedef struct MCGameplayPlayer MCGameplayPlayer;
typedef struct ItemStack ItemStack;
typedef struct S34PacketMaps S34PacketMaps;

/* Immutable native virtual-method boundaries for the supplied concrete NBT
   and saved-data implementations. NULL entries inherit actual translated
   methods. Context is the single traced WorldSavedData.nativeContext edge;
   no second NBT/color authority is retained here. Numeric callbacks return
   the already narrowed byte/short/int result, according to kind 1/2/3. */
typedef struct MapDataDependencies {
    WorldSavedDataResult (*getNumber)(MCObject *, NBTTagCompound *, const char *, int, int32_t *);
    WorldSavedDataResult (*getByteArray)(MCObject *, NBTTagCompound *, const char *,
                                         NativeByteArray **);
    WorldSavedDataResult (*setNumber)(MCObject *, NBTTagCompound *, const char *, int, int32_t);
    WorldSavedDataResult (*setByteArray)(MCObject *, NBTTagCompound *, const char *,
                                         NativeByteArray *);
    WorldSavedDataResult (*setDirty)(MCObject *, MapData *, bool);
} MapDataDependencies;
typedef struct MapInfoDependencies {
    WorldSavedDataResult (*update)(MCObject *, MapInfo *, int32_t, int32_t);
} MapInfoDependencies;

/* Complete declared Source state, followed by explicit native dispatch only.
   updateVisiblePlayers/EntityItemFrame and the full ItemMap integration are
   pending and have no placeholder API or successful substitute. */
struct MapData {
    WorldSavedData base;
    int32_t xCenter, zCenter;
    int8_t dimension, scale;
    NativeByteArray *colors;
    NativeReferenceList *playersArrayList;
    NativeHashMap *playersHashMap;
    NativeLinkedHashMap *mapDecorations;
    const MapDataDependencies *nativeDependencies;
};
struct MapInfo {
    MCObject object;
    MapData *outer; /* Source synthetic this$0. */
    MCGameplayPlayer *entityplayerObj;
    bool field_176105_d;
    int32_t minX, minY, maxX, maxY, field_176109_i, field_82569_d;
    const MapInfoDependencies *nativeDependencies;
    MCObject *nativeContext;
};

bool MapData_isInstance(const MCObject *);
extern const NativeJavaClassDescriptor MapData_Class;
NativeJavaClass *MapData_nativeClass(MCObjectHeap *);
MapData *MapData_nativeAllocate(MCObjectHeap *, const MapDataDependencies *, MCObject *);
bool MapData_construct(MapData *, NBTString *nullableName);
MapData *MapData_new(MCObjectHeap *, NBTString *nullableName, const MapDataDependencies *,
                     MCObject *);
WorldSavedDataResult MapData_calculateMapCenter(MapData *, double, double, int32_t);
WorldSavedDataResult MapData_readFromNBT(MapData *, NBTTagCompound *);
WorldSavedDataResult MapData_writeToNBT(MapData *, NBTTagCompound *);
WorldSavedDataResult MapData_getMapInfo(MapData *, MCGameplayPlayer *nullablePlayer, MapInfo **out);
WorldSavedDataResult MapData_updateDecorations(MapData *, int32_t, World *, NBTString *nullableId,
                                               double, double, double);
WorldSavedDataResult MapData_updateMapData(MapData *, int32_t, int32_t);
WorldSavedDataResult MapData_getMapPacket(MapData *, ItemStack *, World *, MCGameplayPlayer *,
                                          S34PacketMaps **out);

bool MapInfo_isInstance(const MCObject *);
MapInfo *MapInfo_nativeAllocate(MCObjectHeap *, const MapInfoDependencies *, MCObject *);
bool MapInfo_construct(MapInfo *, MapData *nullableOuter, MCGameplayPlayer *nullablePlayer);
MapInfo *MapInfo_new(MCObjectHeap *, MapData *, MCGameplayPlayer *, const MapInfoDependencies *,
                     MCObject *);
WorldSavedDataResult MapInfo_update(MapInfo *, int32_t, int32_t);
WorldSavedDataResult MapInfo_update_base(MapInfo *, int32_t, int32_t);
/* Implemented in the separate translated MapInfo packet-method unit. */
WorldSavedDataResult MapInfo_getPacket(MapInfo *, ItemStack *, S34PacketMaps **out);

/* EXCEPTION models reached Source NULL/index/checkcast failures with healthy
   heap and preceding mutations retained; no Throwable hierarchy is claimed.
   Native malformed/foreign/unsupported dispatch/OOM is sticky FAILURE.
   The reached NativeIterator dependency currently reports concurrent changes
   as sticky FAILURE; that limitation is not hidden as Java catch semantics.
   Returned references are borrowed, and every method pins its live receiver
   and arguments until completion. Full Java subclass dispatch is not inferred
   from a matching name or struct layout. */
#endif
