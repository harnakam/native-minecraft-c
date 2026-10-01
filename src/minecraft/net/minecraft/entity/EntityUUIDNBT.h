#ifndef C919_SOURCE_ENTITY_UUID_NBT_H
#define C919_SOURCE_ENTITY_UUID_NBT_H
#include "nbt/NBTTagCompound.h"
#include "util/NativeJavaUUID.h"
typedef struct MCGameplayPlayer MCGameplayPlayer;

/* Required virtual/native field access for this named Entity persistence
   segment. Tables are immutable and outlive the call. The receiver and UUID
   references belong to one heap; a false setter is a native effect failure. */
typedef struct {
    NativeJavaUUID *(*getUniqueID)(MCObject *entity);
    bool (*setUniqueID)(MCObject *entity, NativeJavaUUID *uuid);
} EntityUUIDNBTDispatch;

/* Original getUniqueID field body through an explicit native owner adapter
   for MCGameplayPlayer and EntityItem. NULL is the actual nullable field value,
   not a request to synthesize an identity. Full Entity construction is separate. */
NativeJavaUUID *EntityUUIDNBT_nativeGetUniqueID(MCObject *entity);
const EntityUUIDNBTDispatch *EntityUUIDNBT_nativeOwnerDispatch(void);

/* Only the original UUID statements inside Entity.writeToNBT/readFromNBT.
   write reads the virtual getter separately for UUIDMost and UUIDLeast.
   read prefers two exact Long tags, otherwise uses the legacy String tag,
   and leaves the current reference unchanged if neither source branch applies.
   Failure preserves earlier source effects; native transaction rollback is a
   caller boundary. These methods do not implement the complete Entity NBT
   bodies, subclass reads, player profile restoration or Random persistence. */
bool Entity_writeUUIDToNBTSegment(MCObject *, NBTTagCompound *, const EntityUUIDNBTDispatch *);
bool Entity_readUUIDFromNBTSegment(MCObject *, NBTTagCompound *, const EntityUUIDNBTDispatch *);
/* The separate original EntityPlayer.readEntityFromNBT assignment, after the
   inherited UUID read and before Inventory. gameProfileUUID is the required
   native GameProfile.getId/offline fallback result; it is never synthesized by
   persistence. The source assignment adopts the same UUID reference. */
bool EntityPlayer_restoreProfileUUIDSegment(MCGameplayPlayer *);
#endif
