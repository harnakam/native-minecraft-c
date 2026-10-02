#ifndef C919_SOURCE_I_SAVE_HANDLER_H
#define C919_SOURCE_I_SAVE_HANDLER_H
#include "nbt/NBTString.h"
#include "util/MCObjectHeap.h"

typedef struct WorldInfo WorldInfo;
typedef struct WorldProvider WorldProvider;
typedef struct NBTTagCompound NBTTagCompound;
typedef enum {
    I_SAVE_HANDLER_OK,
    I_SAVE_HANDLER_EXCEPTION,
    I_SAVE_HANDLER_FAILURE
} ISaveHandlerResult;
/* Actual object identity plus immutable C interface dispatch. Unported File,
   IChunkLoader and IPlayerFileData returns are traced managed native views,
   not fabricated Source classes. A real IO implementor owns/traces its context;
   this interface value has no separate authoritative object or context. */
typedef struct ISaveHandlerMethods {
    bool (*isInstance)(const MCObject *);
    ISaveHandlerResult (*loadWorldInfo)(MCObject *, WorldInfo **);
    ISaveHandlerResult (*checkSessionLock)(MCObject *);
    ISaveHandlerResult (*getChunkLoader)(MCObject *, WorldProvider *, MCObject **);
    ISaveHandlerResult (*saveWorldInfoWithPlayer)(MCObject *, WorldInfo *, NBTTagCompound *);
    ISaveHandlerResult (*saveWorldInfo)(MCObject *, WorldInfo *);
    ISaveHandlerResult (*getPlayerNBTManager)(MCObject *, MCObject **);
    ISaveHandlerResult (*flush)(MCObject *);
    ISaveHandlerResult (*getWorldDirectory)(MCObject *, MCObject **);
    ISaveHandlerResult (*getMapFileFromName)(MCObject *, NBTString *, MCObject **);
    ISaveHandlerResult (*getWorldDirectoryName)(MCObject *, NBTString **);
} ISaveHandlerMethods;
typedef struct ISaveHandler {
    MCObject *instance;
    const ISaveHandlerMethods *methods;
} ISaveHandler;
bool ISaveHandler_nativeIsValid(ISaveHandler);
/* Captured interface receiver; outputs change only on successful validated
   return. EXCEPTION is healthy Source exception state for caller catch logic. */
ISaveHandlerResult ISaveHandler_loadWorldInfo(ISaveHandler, WorldInfo **);
ISaveHandlerResult ISaveHandler_checkSessionLock(ISaveHandler);
ISaveHandlerResult ISaveHandler_getChunkLoader(ISaveHandler, WorldProvider *, MCObject **);
ISaveHandlerResult ISaveHandler_saveWorldInfoWithPlayer(ISaveHandler, WorldInfo *,
                                                        NBTTagCompound *);
ISaveHandlerResult ISaveHandler_saveWorldInfo(ISaveHandler, WorldInfo *);
ISaveHandlerResult ISaveHandler_getPlayerNBTManager(ISaveHandler, MCObject **);
ISaveHandlerResult ISaveHandler_flush(ISaveHandler);
ISaveHandlerResult ISaveHandler_getWorldDirectory(ISaveHandler, MCObject **);
ISaveHandlerResult ISaveHandler_getMapFileFromName(ISaveHandler, NBTString *, MCObject **);
ISaveHandlerResult ISaveHandler_getWorldDirectoryName(ISaveHandler, NBTString **);
#endif
