#ifndef C919_SOURCE_SAVE_HANDLER_MP_H
#define C919_SOURCE_SAVE_HANDLER_MP_H
#include "world/storage/ISaveHandler.h"
/* The original class declares no fields and has only the implicit Object
   constructor. MCObject is the native lifetime header, not Source state. */
typedef struct SaveHandlerMP {
    MCObject object;
} SaveHandlerMP;
SaveHandlerMP *SaveHandlerMP_nativeAllocate(MCObjectHeap *);
bool SaveHandlerMP_construct(SaveHandlerMP *);
SaveHandlerMP *SaveHandlerMP_new(MCObjectHeap *);
bool SaveHandlerMP_isInstance(const MCObject *);
ISaveHandler SaveHandlerMP_asSaveHandler(SaveHandlerMP *);
ISaveHandlerResult SaveHandlerMP_loadWorldInfo(SaveHandlerMP *, WorldInfo **);
ISaveHandlerResult SaveHandlerMP_checkSessionLock(SaveHandlerMP *);
ISaveHandlerResult SaveHandlerMP_getChunkLoader(SaveHandlerMP *, WorldProvider *, MCObject **);
ISaveHandlerResult SaveHandlerMP_saveWorldInfoWithPlayer(SaveHandlerMP *, WorldInfo *,
                                                         NBTTagCompound *);
ISaveHandlerResult SaveHandlerMP_saveWorldInfo(SaveHandlerMP *, WorldInfo *);
ISaveHandlerResult SaveHandlerMP_getPlayerNBTManager(SaveHandlerMP *, MCObject **);
ISaveHandlerResult SaveHandlerMP_flush(SaveHandlerMP *);
ISaveHandlerResult SaveHandlerMP_getWorldDirectory(SaveHandlerMP *, MCObject **);
ISaveHandlerResult SaveHandlerMP_getMapFileFromName(SaveHandlerMP *, NBTString *, MCObject **);
ISaveHandlerResult SaveHandlerMP_getWorldDirectoryName(SaveHandlerMP *, NBTString **);
#endif
