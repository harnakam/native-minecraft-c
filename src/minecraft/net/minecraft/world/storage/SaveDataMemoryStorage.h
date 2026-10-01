#ifndef C919_SOURCE_SAVE_DATA_MEMORY_STORAGE_H
#define C919_SOURCE_SAVE_DATA_MEMORY_STORAGE_H
#include "world/storage/MapStorage.h"
typedef struct SaveDataMemoryStorage { MapStorage base; } SaveDataMemoryStorage;
/* Native partial initialization, not the original whole subclass/base ctor.
   Its counter override is the actual constant-zero method. The other four
   original subclass methods remain outside this connected counter subset. */
SaveDataMemoryStorage *SaveDataMemoryStorage_nativeNewCounterProvider(MCObjectHeap *);
bool SaveDataMemoryStorage_isInstance(const MCObject *);
bool SaveDataMemoryStorage_getUniqueDataId(SaveDataMemoryStorage *,NBTString *key,int32_t *output);
#endif
