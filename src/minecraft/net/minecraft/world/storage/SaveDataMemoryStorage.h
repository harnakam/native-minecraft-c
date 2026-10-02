#ifndef C919_SOURCE_SAVE_DATA_MEMORY_STORAGE_H
#define C919_SOURCE_SAVE_DATA_MEMORY_STORAGE_H
#include "world/storage/MapStorage.h"
typedef struct SaveDataMemoryStorage {
    MapStorage base;
} SaveDataMemoryStorage;
SaveDataMemoryStorage *SaveDataMemoryStorage_nativeAllocate(MCObjectHeap *);
bool SaveDataMemoryStorage_construct(SaveDataMemoryStorage *);
SaveDataMemoryStorage *SaveDataMemoryStorage_new(MCObjectHeap *);
/* Compatibility entry now uses the actual complete no-argument constructor. */
SaveDataMemoryStorage *SaveDataMemoryStorage_nativeNewCounterProvider(MCObjectHeap *);
bool SaveDataMemoryStorage_isInstance(const MCObject *);
bool SaveDataMemoryStorage_loadData(SaveDataMemoryStorage *, NativeJavaClass *, NBTString *,
                                    WorldSavedData **);
bool SaveDataMemoryStorage_setData(SaveDataMemoryStorage *, NBTString *, WorldSavedData *);
bool SaveDataMemoryStorage_saveAllData(SaveDataMemoryStorage *);
bool SaveDataMemoryStorage_getUniqueDataId(SaveDataMemoryStorage *, NBTString *key,
                                           int32_t *output);
#endif
