#ifndef C919_SOURCE_MAP_STORAGE_H
#define C919_SOURCE_MAP_STORAGE_H
#include "nbt/NBTTagCompound.h"

typedef struct MapStorageIdCounts MapStorageIdCounts;
typedef struct MapStorage MapStorage;
/* Source caught Exception is distinct from an unsupported/failed native
   dependency. The former calls printCaughtException and returns the local ID;
   the latter fails the graph. Callbacks retain any native exception diagnosis
   in their traced context until printCaughtException consumes it. */
typedef enum {
    MAP_STORAGE_IO_OK, MAP_STORAGE_IO_EXCEPTION, MAP_STORAGE_IO_FAILURE
} MapStorageIOResult;
typedef struct {
    MapStorageIOResult (*getMapFileFromName)(MCObject *context,MCObject *saveHandler,
                                            NBTString *name,MCObject **file);
    MapStorageIOResult (*newDataOutputStream)(MCObject *context,MCObject *file,MCObject **stream);
    MapStorageIOResult (*writeRawNBT)(MCObject *context,NBTTagCompound *,MCObject *stream);
    MapStorageIOResult (*closeOutputStream)(MCObject *context,MCObject *stream);
    bool (*printCaughtException)(MCObject *context);
} MapStorageDependencies;
struct MapStorage {
    MCObject object;
    MCObject *saveHandler;
    MapStorageIdCounts *idCounts;
    MCObject *nativeContext;
    const MapStorageDependencies *dependencies;
};
/* Native partial factory for the connected counter method. The full original
   constructor, loadedDataMap/list, loadIdCounts and saved-data bodies are NOT
   translated here. Counter entries are a managed nullable String -> signed
   Short native collection adapter; JDK boxing/collections are not class ports.
   Ordinary HashMap bucket/resize traversal is preserved; unsupported tree-bin
   construction explicitly fails instead of supplying a different order. */
MapStorage *MapStorage_nativeNewCounterProvider(MCObjectHeap *,MCObject *saveHandler,
                                               MCObject *context,const MapStorageDependencies *);
/* Shared partial-factory/tracer for the actual first-member subclass. */
bool MapStorage_nativeInitializeCounterProvider(MapStorage *,MCObject *saveHandler,
                                                MCObject *context,const MapStorageDependencies *);
void MapStorage_nativeTraceFields(MCObject *,MCObjectVisitor,void *context);
bool MapStorage_isInstance(const MCObject *);
/* Source virtual method dispatch, including SaveDataMemoryStorage's override.
   Successful output is the captured signed-short local even if IO callbacks
   subsequently change the map. NULL is a legitimate String key. */
bool MapStorage_getUniqueDataId(MapStorage *,NBTString *key,int32_t *output);

/* Explicit native storage/projection adapters, not additional Source methods.
   A snapshot allocates a fresh NBT compound/Short tags but retains immutable
   key refs; no counter state is shared with that exported snapshot. Imports
   have normal allocation/failure prefixes; caller transactions provide atomic
   native adoption. clearFirst accepts only exact Short tags after clearing. */
int32_t MapStorage_nativeIdCountSize(const MapStorage *);
/* Borrowed key in native HashMap keySet traversal order; NULL can be a key.
   False with an unchanged output signals an invalid index/native failure. */
bool MapStorage_nativeIdCountEntryAt(const MapStorage *,int32_t index,
                                     NBTString **key,int16_t *value);
bool MapStorage_nativeFindExactShort(const MapStorage *,const NBTString *nullableKey,
                                     bool *present,int16_t *value);
bool MapStorage_nativeImportExactShort(MapStorage *,NBTString *nullableKey,int16_t value);
bool MapStorage_nativeClearIdCounts(MapStorage *);
NBTTagCompound *MapStorage_nativeSnapshotIdCounts(MapStorage *);
bool MapStorage_nativeImportIdCounts(MapStorage *,NBTTagCompound *,bool clearFirst);
/* Legacy C919 NEXT-map bits, computed from the sole LAST-ID namespace state.
   Missing map namespace projects0; this API neither allocates an ID nor saves. */
bool MapStorage_nativeGetMapNextProjection(const MapStorage *,int32_t *output);
/* Read-only native failure-prefix diagnostic. Validates receiver/owner/storage
   layout but permits a preexisting heap failure; never clears failure, allocates,
   draws an ID or changes graph revisions. Not an original Source method. */
bool MapStorage_nativeGetMapNextProjectionDiagnostic(const MapStorage *,int32_t *output);
#endif
