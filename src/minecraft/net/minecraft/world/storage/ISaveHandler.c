#include "world/storage/ISaveHandler.h"
#include "nbt/NBTTagCompound.h"
#include "world/WorldProvider.h"
#include "world/storage/WorldInfo.h"

static ISaveHandlerResult failure(ISaveHandler self) {
    MCObjectHeap_fail(self.instance ? self.instance->heap : NULL);
    return I_SAVE_HANDLER_FAILURE;
}
bool ISaveHandler_nativeIsValid(ISaveHandler self) {
    return self.instance && MCObjectHeap_objectSize(self.instance) >= sizeof(MCObject) &&
           self.methods && self.methods->isInstance && self.methods->isInstance(self.instance);
}
static bool begin(ISaveHandler self, MCObjectRootScope *scope) {
    if (!ISaveHandler_nativeIsValid(self) || MCObjectHeap_failed(self.instance->heap)) {
        failure(self);
        return false;
    }
    if (!MCObjectRootScope_begin(scope, self.instance->heap) ||
        !MCObjectRootScope_pin(scope, self.instance)) {
        MCObjectRootScope_end(scope);
        failure(self);
        return false;
    }
    return true;
}
static bool reference(ISaveHandler self, MCObject *object, MCObjectRootScope *scope) {
    if (object && (object->heap != self.instance->heap ||
                   MCObjectHeap_objectSize(object) < sizeof(MCObject))) {
        failure(self);
        return false;
    }
    if (!MCObjectRootScope_pin(scope, object)) {
        failure(self);
        return false;
    }
    return true;
}
static ISaveHandlerResult finish(ISaveHandler self, MCObjectRootScope *scope,
                                 ISaveHandlerResult result) {
    if (result != I_SAVE_HANDLER_OK && result != I_SAVE_HANDLER_EXCEPTION)
        failure(self);
    if (MCObjectHeap_failed(self.instance->heap))
        result = I_SAVE_HANDLER_FAILURE;
    MCObjectRootScope_end(scope);
    return result;
}
static bool info_ref(ISaveHandler self, WorldInfo *info, MCObjectRootScope *scope) {
    if (info && !WorldInfo_isInstance((MCObject *)info)) {
        failure(self);
        return false;
    }
    return reference(self, (MCObject *)info, scope);
}
static bool string_ref(ISaveHandler self, NBTString *text, MCObjectRootScope *scope) {
    if (text && !NBTString_isInstance((MCObject *)text)) {
        failure(self);
        return false;
    }
    return reference(self, (MCObject *)text, scope);
}
#define NO_ARGS(Name)                                                                              \
    ISaveHandlerResult ISaveHandler_##Name(ISaveHandler self) {                                    \
        MCObjectRootScope scope = {0};                                                             \
        if (!begin(self, &scope))                                                                  \
            return I_SAVE_HANDLER_FAILURE;                                                         \
        ISaveHandlerResult result =                                                                \
            self.methods->Name ? self.methods->Name(self.instance) : I_SAVE_HANDLER_FAILURE;       \
        return finish(self, &scope, result);                                                       \
    }
NO_ARGS(checkSessionLock)
NO_ARGS(flush)
#undef NO_ARGS
#define OBJECT_OUT(Name)                                                                           \
    ISaveHandlerResult ISaveHandler_##Name(ISaveHandler self, MCObject **output) {                 \
        MCObjectRootScope scope = {0};                                                             \
        if (!begin(self, &scope))                                                                  \
            return I_SAVE_HANDLER_FAILURE;                                                         \
        MCObject *value = NULL;                                                                    \
        ISaveHandlerResult result = I_SAVE_HANDLER_FAILURE;                                        \
        if (output && self.methods->Name)                                                          \
            result = self.methods->Name(self.instance, &value);                                    \
        if (result == I_SAVE_HANDLER_OK && !reference(self, value, &scope))                        \
            result = I_SAVE_HANDLER_FAILURE;                                                       \
        result = finish(self, &scope, result);                                                     \
        if (result == I_SAVE_HANDLER_OK)                                                           \
            *output = value;                                                                       \
        return result;                                                                             \
    }
OBJECT_OUT(getPlayerNBTManager)
OBJECT_OUT(getWorldDirectory)
#undef OBJECT_OUT
ISaveHandlerResult ISaveHandler_loadWorldInfo(ISaveHandler self, WorldInfo **output) {
    MCObjectRootScope scope = {0};
    if (!begin(self, &scope))
        return I_SAVE_HANDLER_FAILURE;
    WorldInfo *value = NULL;
    ISaveHandlerResult result = I_SAVE_HANDLER_FAILURE;
    if (output && self.methods->loadWorldInfo)
        result = self.methods->loadWorldInfo(self.instance, &value);
    if (result == I_SAVE_HANDLER_OK && !info_ref(self, value, &scope))
        result = I_SAVE_HANDLER_FAILURE;
    result = finish(self, &scope, result);
    if (result == I_SAVE_HANDLER_OK)
        *output = value;
    return result;
}
ISaveHandlerResult ISaveHandler_getChunkLoader(ISaveHandler self, WorldProvider *provider,
                                               MCObject **output) {
    MCObjectRootScope scope = {0};
    if (!begin(self, &scope))
        return I_SAVE_HANDLER_FAILURE;
    MCObject *value = NULL;
    ISaveHandlerResult result = I_SAVE_HANDLER_FAILURE;
    if (output && (!provider || WorldProvider_isInstance((MCObject *)provider)) &&
        reference(self, (MCObject *)provider, &scope) && self.methods->getChunkLoader)
        result = self.methods->getChunkLoader(self.instance, provider, &value);
    if (result == I_SAVE_HANDLER_OK && !reference(self, value, &scope))
        result = I_SAVE_HANDLER_FAILURE;
    result = finish(self, &scope, result);
    if (result == I_SAVE_HANDLER_OK)
        *output = value;
    return result;
}
ISaveHandlerResult ISaveHandler_saveWorldInfoWithPlayer(ISaveHandler self, WorldInfo *info,
                                                        NBTTagCompound *tag) {
    MCObjectRootScope scope = {0};
    if (!begin(self, &scope))
        return I_SAVE_HANDLER_FAILURE;
    ISaveHandlerResult result = I_SAVE_HANDLER_FAILURE;
    if (info_ref(self, info, &scope) && (!tag || NBTTagCompound_isInstance((MCObject *)tag)) &&
        reference(self, (MCObject *)tag, &scope) && self.methods->saveWorldInfoWithPlayer)
        result = self.methods->saveWorldInfoWithPlayer(self.instance, info, tag);
    return finish(self, &scope, result);
}
ISaveHandlerResult ISaveHandler_saveWorldInfo(ISaveHandler self, WorldInfo *info) {
    MCObjectRootScope scope = {0};
    if (!begin(self, &scope))
        return I_SAVE_HANDLER_FAILURE;
    ISaveHandlerResult result = I_SAVE_HANDLER_FAILURE;
    if (info_ref(self, info, &scope) && self.methods->saveWorldInfo)
        result = self.methods->saveWorldInfo(self.instance, info);
    return finish(self, &scope, result);
}
ISaveHandlerResult ISaveHandler_getMapFileFromName(ISaveHandler self, NBTString *name,
                                                   MCObject **output) {
    MCObjectRootScope scope = {0};
    if (!begin(self, &scope))
        return I_SAVE_HANDLER_FAILURE;
    MCObject *value = NULL;
    ISaveHandlerResult result = I_SAVE_HANDLER_FAILURE;
    if (output && string_ref(self, name, &scope) && self.methods->getMapFileFromName)
        result = self.methods->getMapFileFromName(self.instance, name, &value);
    if (result == I_SAVE_HANDLER_OK && !reference(self, value, &scope))
        result = I_SAVE_HANDLER_FAILURE;
    result = finish(self, &scope, result);
    if (result == I_SAVE_HANDLER_OK)
        *output = value;
    return result;
}
ISaveHandlerResult ISaveHandler_getWorldDirectoryName(ISaveHandler self, NBTString **output) {
    MCObjectRootScope scope = {0};
    if (!begin(self, &scope))
        return I_SAVE_HANDLER_FAILURE;
    NBTString *value = NULL;
    ISaveHandlerResult result = I_SAVE_HANDLER_FAILURE;
    if (output && self.methods->getWorldDirectoryName)
        result = self.methods->getWorldDirectoryName(self.instance, &value);
    if (result == I_SAVE_HANDLER_OK && !string_ref(self, value, &scope))
        result = I_SAVE_HANDLER_FAILURE;
    result = finish(self, &scope, result);
    if (result == I_SAVE_HANDLER_OK)
        *output = value;
    return result;
}
