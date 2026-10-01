#ifndef C919_SOURCE_DATA_WATCHER_H
#define C919_SOURCE_DATA_WATCHER_H
#include "network/PacketBuffer.h"
#include "nbt/NBTString.h"

typedef struct DataWatcher DataWatcher;
typedef struct WatchableObject WatchableObject;
typedef struct WatchableObjectList WatchableObjectList;
/* Immutable native views for the Java boxed primitive and coordinate classes.
   They retain actual values and direct references; boxed valueOf caches and
   complete BlockPos/Rotations class bodies are separate dependencies. */
typedef struct {
    MCObject object;
    int32_t x, y, z;
} DataWatcherBlockPos;
typedef struct {
    MCObject object;
    float x, y, z;
} DataWatcherRotations;
MCObject *DataWatcher_boxByte(MCObjectHeap *, int32_t);
MCObject *DataWatcher_boxShort(MCObjectHeap *, int32_t);
MCObject *DataWatcher_boxInt(MCObjectHeap *, int32_t);
MCObject *DataWatcher_boxFloat(MCObjectHeap *, float);
DataWatcherBlockPos *DataWatcher_blockPos(MCObjectHeap *, int32_t, int32_t, int32_t);
DataWatcherRotations *DataWatcher_rotations(MCObjectHeap *, float, float, float);
/* Native ObjectUtils/equals dispatch for these registered value classes.
   Unknown unequal class references require the explicit subclass dependency. */
typedef struct {
    bool (*onDataWatcherUpdate)(MCObject *context, MCObject *owner, int32_t id);
    bool (*objectEquals)(MCObject *context, const MCObject *, const MCObject *, bool *equal);
} DataWatcherDependencies;
DataWatcher *DataWatcher_new(MCObjectHeap *, MCObject *owner, const DataWatcherDependencies *,
                             MCObject *context);
bool DataWatcher_isInstance(const MCObject *);
bool DataWatcher_addObject(DataWatcher *, int32_t id, MCObject *);
bool DataWatcher_addObjectByDataType(DataWatcher *, int32_t id, int32_t type);
int8_t DataWatcher_getWatchableObjectByte(DataWatcher *, int32_t);
int16_t DataWatcher_getWatchableObjectShort(DataWatcher *, int32_t);
int32_t DataWatcher_getWatchableObjectInt(DataWatcher *, int32_t);
float DataWatcher_getWatchableObjectFloat(DataWatcher *, int32_t);
NBTString *DataWatcher_getWatchableObjectString(DataWatcher *, int32_t);
ItemStack *DataWatcher_getWatchableObjectItemStack(DataWatcher *, int32_t);
DataWatcherRotations *DataWatcher_getWatchableObjectRotations(DataWatcher *, int32_t);
bool DataWatcher_updateObject(DataWatcher *, int32_t, MCObject *);
bool DataWatcher_setObjectWatched(DataWatcher *, int32_t);
bool DataWatcher_hasObjectChanged(const DataWatcher *);
WatchableObjectList *DataWatcher_getChanged(DataWatcher *);
WatchableObjectList *DataWatcher_getAllWatched(DataWatcher *);
bool DataWatcher_writeTo(DataWatcher *, PacketBuffer *);
bool DataWatcher_writeWatchedListToPacketBuffer(WatchableObjectList *, PacketBuffer *);
/* Decode is an atomic output boundary; read position and already allocated
   objects retain source order on failure. No ItemStack occurrence is copied. */
bool DataWatcher_readWatchedListFromPacketBuffer(PacketBuffer *, WatchableObjectList **);
bool DataWatcher_updateWatchedObjectsFromList(DataWatcher *, WatchableObjectList *);
bool DataWatcher_getIsBlank(const DataWatcher *);
void DataWatcher_func_111144_e(DataWatcher *);

WatchableObject *WatchableObject_new(MCObjectHeap *, int32_t type, int32_t id, MCObject *value);
int32_t WatchableObject_getDataValueId(const WatchableObject *);
bool WatchableObject_setObject(WatchableObject *, MCObject *);
MCObject *WatchableObject_getObject(const WatchableObject *);
int32_t WatchableObject_getObjectType(const WatchableObject *);
bool WatchableObject_isWatched(const WatchableObject *);
void WatchableObject_setWatched(WatchableObject *, bool);
/* Managed native ArrayList storage. Entries are nullable direct Watchable
   references, including source foreach/modCount behavior. */
WatchableObjectList *WatchableObjectList_new(MCObjectHeap *);
int32_t WatchableObjectList_size(const WatchableObjectList *);
WatchableObject *WatchableObjectList_get(WatchableObjectList *, int32_t);
bool WatchableObjectList_add(WatchableObjectList *, WatchableObject *);
bool WatchableObjectList_set(WatchableObjectList *, int32_t, WatchableObject *);
WatchableObject *WatchableObjectList_remove(WatchableObjectList *, int32_t);
void WatchableObjectList_clear(WatchableObjectList *);
/* Native inspection of the original private lookup, without a value mirror. */
WatchableObject *DataWatcher_nativeGetWatchedObject(DataWatcher *, int32_t);
/* Entity, crash-report text, JDK lock/collection classes and custom Object
   subclasses are explicit dependencies. This single-writer map adapter owns
   Integer-key bucket ordering; it does not claim a complete java.util.Map. */
#endif
