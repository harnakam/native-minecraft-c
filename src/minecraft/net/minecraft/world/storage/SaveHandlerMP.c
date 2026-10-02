#include "world/storage/SaveHandlerMP.h"

static const MCObjectClass klass = {"net.minecraft.world.storage.SaveHandlerMP",
                                    MCObjectHeap_plainClone, NULL, NULL};
bool SaveHandlerMP_isInstance(const MCObject *object) {
    return object && object->klass == &klass &&
           MCObjectHeap_objectSize(object) >= sizeof(SaveHandlerMP);
}
SaveHandlerMP *SaveHandlerMP_nativeAllocate(MCObjectHeap *heap) {
    return (SaveHandlerMP *)MCObjectHeap_alloc(heap, sizeof(SaveHandlerMP), &klass);
}
bool SaveHandlerMP_construct(SaveHandlerMP *self) {
    if (!SaveHandlerMP_isInstance((MCObject *)self) || MCObjectHeap_failed(self->object.heap)) {
        MCObjectHeap_fail(self ? self->object.heap : NULL);
        return false;
    }
    /* Implicit Object constructor; the original declares no state to assign. */
    return true;
}
SaveHandlerMP *SaveHandlerMP_new(MCObjectHeap *heap) {
    SaveHandlerMP *self = SaveHandlerMP_nativeAllocate(heap);
    return self && SaveHandlerMP_construct(self) ? self : NULL;
}
/* Genuine original method bodies. These are not generic defaults for a disk
   save handler: only this exact class's interface table installs them. */
static ISaveHandlerResult load_info(MCObject *self, WorldInfo **output) {
    (void)self;
    *output = NULL;
    return I_SAVE_HANDLER_OK;
}
static ISaveHandlerResult check_lock(MCObject *self) {
    (void)self;
    return I_SAVE_HANDLER_OK;
}
static ISaveHandlerResult chunk_loader(MCObject *self, WorldProvider *provider, MCObject **output) {
    (void)self;
    (void)provider;
    *output = NULL;
    return I_SAVE_HANDLER_OK;
}
static ISaveHandlerResult save_info_player(MCObject *self, WorldInfo *info, NBTTagCompound *tag) {
    (void)self;
    (void)info;
    (void)tag;
    return I_SAVE_HANDLER_OK;
}
static ISaveHandlerResult save_info(MCObject *self, WorldInfo *info) {
    (void)self;
    (void)info;
    return I_SAVE_HANDLER_OK;
}
static ISaveHandlerResult player_data(MCObject *self, MCObject **output) {
    (void)self;
    *output = NULL;
    return I_SAVE_HANDLER_OK;
}
static ISaveHandlerResult flush(MCObject *self) {
    (void)self;
    return I_SAVE_HANDLER_OK;
}
static ISaveHandlerResult directory(MCObject *self, MCObject **output) {
    (void)self;
    *output = NULL;
    return I_SAVE_HANDLER_OK;
}
static ISaveHandlerResult map_file(MCObject *self, NBTString *name, MCObject **output) {
    (void)self;
    (void)name;
    *output = NULL;
    return I_SAVE_HANDLER_OK;
}
static ISaveHandlerResult directory_name(MCObject *self, NBTString **output) {
    NBTString *name = NBTString_literalASCII(self->heap, "none");
    if (!name)
        return I_SAVE_HANDLER_FAILURE;
    *output = name;
    return I_SAVE_HANDLER_OK;
}
static const ISaveHandlerMethods methods = {.isInstance = SaveHandlerMP_isInstance,
                                            .loadWorldInfo = load_info,
                                            .checkSessionLock = check_lock,
                                            .getChunkLoader = chunk_loader,
                                            .saveWorldInfoWithPlayer = save_info_player,
                                            .saveWorldInfo = save_info,
                                            .getPlayerNBTManager = player_data,
                                            .flush = flush,
                                            .getWorldDirectory = directory,
                                            .getMapFileFromName = map_file,
                                            .getWorldDirectoryName = directory_name};
ISaveHandler SaveHandlerMP_asSaveHandler(SaveHandlerMP *self) {
    return (ISaveHandler){(MCObject *)self, &methods};
}
#define NO_ARGS(Name)                                                                              \
    ISaveHandlerResult SaveHandlerMP_##Name(SaveHandlerMP *self) {                                 \
        return ISaveHandler_##Name(SaveHandlerMP_asSaveHandler(self));                             \
    }
NO_ARGS(checkSessionLock)
NO_ARGS(flush)
#undef NO_ARGS
#define OUTPUT(Name, Type)                                                                         \
    ISaveHandlerResult SaveHandlerMP_##Name(SaveHandlerMP *self, Type **output) {                  \
        return ISaveHandler_##Name(SaveHandlerMP_asSaveHandler(self), output);                     \
    }
OUTPUT(loadWorldInfo, WorldInfo)
OUTPUT(getPlayerNBTManager, MCObject)
OUTPUT(getWorldDirectory, MCObject)
OUTPUT(getWorldDirectoryName, NBTString)
#undef OUTPUT
ISaveHandlerResult SaveHandlerMP_getChunkLoader(SaveHandlerMP *self, WorldProvider *provider,
                                                MCObject **output) {
    return ISaveHandler_getChunkLoader(SaveHandlerMP_asSaveHandler(self), provider, output);
}
ISaveHandlerResult SaveHandlerMP_saveWorldInfoWithPlayer(SaveHandlerMP *self, WorldInfo *info,
                                                         NBTTagCompound *tag) {
    return ISaveHandler_saveWorldInfoWithPlayer(SaveHandlerMP_asSaveHandler(self), info, tag);
}
ISaveHandlerResult SaveHandlerMP_saveWorldInfo(SaveHandlerMP *self, WorldInfo *info) {
    return ISaveHandler_saveWorldInfo(SaveHandlerMP_asSaveHandler(self), info);
}
ISaveHandlerResult SaveHandlerMP_getMapFileFromName(SaveHandlerMP *self, NBTString *name,
                                                    MCObject **output) {
    return ISaveHandler_getMapFileFromName(SaveHandlerMP_asSaveHandler(self), name, output);
}
