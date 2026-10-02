#ifndef C919_SOURCE_MAP_STORAGE_H
#define C919_SOURCE_MAP_STORAGE_H
#include "nbt/NBTTagCompound.h"
#include "util/NativeHashMap.h"
#include "util/NativeJavaClass.h"
#include "world/WorldSavedData.h"
#include "world/storage/ISaveHandler.h"

typedef struct MapStorageIdCounts MapStorageIdCounts;
typedef struct MapStorage MapStorage;
/* Source caught Exception is distinct from an unsupported/failed native
   dependency. The former calls printCaughtException and returns the local ID;
   the latter fails the graph. Callbacks retain any native exception diagnosis
   in their traced context until printCaughtException consumes it. */
typedef enum {
    MAP_STORAGE_IO_OK,
    MAP_STORAGE_IO_EXCEPTION,
    MAP_STORAGE_IO_FAILURE
} MapStorageIOResult;
typedef struct {
    MapStorageIOResult (*getMapFileFromName)(MCObject *context, MCObject *saveHandler,
                                             NBTString *name, MCObject **file);
    MapStorageIOResult (*newDataOutputStream)(MCObject *context, MCObject *file, MCObject **stream);
    MapStorageIOResult (*writeRawNBT)(MCObject *context, NBTTagCompound *, MCObject *stream);
    MapStorageIOResult (*closeOutputStream)(MCObject *context, MCObject *stream);
    bool (*printCaughtException)(MCObject *context);
} MapStorageDependencies;
/* Reached IO/reflection/JDK equals dependencies for the complete Source bodies.
   These immutable native facts do not implement java.io or reflection classes.
   Each returned reference belongs to the captured receiver's heap. EXCEPTION
   retains Source partial effects and is printed by the enclosing catch; an
   uncaught Error/unsupported native dependency uses FAILURE. The constructor
   adapter includes reflection's InvocationTargetException wrapping, so even
   an Error thrown inside that constructor is reported as EXCEPTION here. */
typedef struct {
    MapStorageIOResult (*fileExists)(MCObject *, MCObject *, bool *);
    MapStorageIOResult (*constructSavedData)(MCObject *, NativeJavaClass *, NBTString *,
                                             WorldSavedData **);
    MapStorageIOResult (*newFileInputStream)(MCObject *, MCObject *, MCObject **);
    MapStorageIOResult (*newDataInputStream)(MCObject *, MCObject *, MCObject **);
    MapStorageIOResult (*readRawNBT)(MCObject *, MCObject *, NBTTagCompound **);
    MapStorageIOResult (*readCompressedNBT)(MCObject *, MCObject *, NBTTagCompound **);
    MapStorageIOResult (*closeInputStream)(MCObject *, MCObject *);
    MapStorageIOResult (*newFileOutputStream)(MCObject *, MCObject *, MCObject **);
    MapStorageIOResult (*newDataOutputStream)(MCObject *, MCObject *, MCObject **);
    MapStorageIOResult (*writeRawNBT)(MCObject *, NBTTagCompound *, MCObject *);
    MapStorageIOResult (*writeCompressedNBT)(MCObject *, NBTTagCompound *, MCObject *);
    MapStorageIOResult (*closeOutputStream)(MCObject *, MCObject *);
    MapStorageIOResult (*savedDataEquals)(MCObject *, MCObject *query, MCObject *stored, bool *);
    bool (*printCaughtException)(MCObject *);
} MapStorageSourceDependencies;
struct MapStorage {
    MCObject object;
    MCObject *saveHandler;
    NativeHashMap *loadedDataMap;
    NativeReferenceList *loadedDataList;
    MapStorageIdCounts *idCounts;
    MCObject *nativeContext;
    const MapStorageDependencies *dependencies;
    const ISaveHandlerMethods *nativeSaveHandlerMethods;
    const MapStorageSourceDependencies *sourceDependencies;
};
MapStorage *MapStorage_nativeAllocate(MCObjectHeap *, MCObject *context,
                                      const MapStorageSourceDependencies *);
bool MapStorage_construct(MapStorage *, ISaveHandler nullableSaveHandler);
MapStorage *MapStorage_new(MCObjectHeap *, ISaveHandler nullableSaveHandler, MCObject *context,
                           const MapStorageSourceDependencies *);
/* Virtual entry points and explicit base bodies for super dispatch. NULL Class
   is ignored on a cache hit or absent-file branch; it is consumed only at the
   actual reflection point. Outputs are assigned only on successful return. */
bool MapStorage_loadData(MapStorage *, NativeJavaClass *, NBTString *, WorldSavedData **);
bool MapStorage_loadData_base(MapStorage *, NativeJavaClass *, NBTString *, WorldSavedData **);
bool MapStorage_setData(MapStorage *, NBTString *, WorldSavedData *nullableData);
bool MapStorage_setData_base(MapStorage *, NBTString *, WorldSavedData *nullableData);
bool MapStorage_saveAllData(MapStorage *);
bool MapStorage_saveAllData_base(MapStorage *);
/* Compatibility factory for the existing native counter-only server adapter.
   It deliberately does not run the Source disk constructor or allocate cache
   collections; use MapStorage_new for the translated constructor. Counter
   entries are a managed nullable String -> signed
   Short native collection adapter; JDK boxing/collections are not class ports.
   Ordinary HashMap bucket/resize traversal is preserved; unsupported tree-bin
   construction explicitly fails instead of supplying a different order. */
MapStorage *MapStorage_nativeNewCounterProvider(MCObjectHeap *, MCObject *saveHandler,
                                                MCObject *context, const MapStorageDependencies *);
/* Shared partial-factory/tracer for the actual first-member subclass. */
bool MapStorage_nativeInitializeCounterProvider(MapStorage *, MCObject *saveHandler,
                                                MCObject *context, const MapStorageDependencies *);
void MapStorage_nativeTraceFields(MCObject *, MCObjectVisitor, void *context);
bool MapStorage_isInstance(const MCObject *);
/* Source virtual method dispatch, including SaveDataMemoryStorage's override.
   Successful output is the captured signed-short local even if IO callbacks
   subsequently change the map. NULL is a legitimate String key. */
bool MapStorage_getUniqueDataId(MapStorage *, NBTString *key, int32_t *output);

/* Explicit native storage/projection adapters, not additional Source methods.
   A snapshot allocates a fresh NBT compound/Short tags but retains immutable
   key refs; no counter state is shared with that exported snapshot. Imports
   have normal allocation/failure prefixes; caller transactions provide atomic
   native adoption. clearFirst accepts only exact Short tags after clearing. */
int32_t MapStorage_nativeIdCountSize(const MapStorage *);
/* Borrowed key in native HashMap keySet traversal order; NULL can be a key.
   False with an unchanged output signals an invalid index/native failure. */
bool MapStorage_nativeIdCountEntryAt(const MapStorage *, int32_t index, NBTString **key,
                                     int16_t *value);
bool MapStorage_nativeFindExactShort(const MapStorage *, const NBTString *nullableKey,
                                     bool *present, int16_t *value);
bool MapStorage_nativeImportExactShort(MapStorage *, NBTString *nullableKey, int16_t value);
bool MapStorage_nativeClearIdCounts(MapStorage *);
NBTTagCompound *MapStorage_nativeSnapshotIdCounts(MapStorage *);
bool MapStorage_nativeImportIdCounts(MapStorage *, NBTTagCompound *, bool clearFirst);
/* Legacy C919 NEXT-map bits, computed from the sole LAST-ID namespace state.
   Missing map namespace projects0; this API neither allocates an ID nor saves. */
bool MapStorage_nativeGetMapNextProjection(const MapStorage *, int32_t *output);
/* Read-only native failure-prefix diagnostic. Validates receiver/owner/storage
   layout but permits a preexisting heap failure; never clears failure, allocates,
   draws an ID or changes graph revisions. Not an original Source method. */
bool MapStorage_nativeGetMapNextProjectionDiagnostic(const MapStorage *, int32_t *output);
#endif
