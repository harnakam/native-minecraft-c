#ifndef C919_NATIVE_GAME_PROFILE_H
#define C919_NATIVE_GAME_PROFILE_H
#include "util/NativeJavaUUID.h"
/* Managed native view of the constructor-reachable authlib GameProfile API.
   These are the exact nullable id/name references; authlib properties, profile
   authentication and subclass getter overrides are separate dependencies. */
typedef struct NativeGameProfile {
    MCObject object;
    NativeJavaUUID *id;
    NBTString *name;
} NativeGameProfile;
NativeGameProfile *NativeGameProfile_new(MCObjectHeap *,NativeJavaUUID *,NBTString *);
bool NativeGameProfile_isInstance(const MCObject *);
NativeJavaUUID *NativeGameProfile_getId(NativeGameProfile *);
NBTString *NativeGameProfile_getName(NativeGameProfile *);
#endif
