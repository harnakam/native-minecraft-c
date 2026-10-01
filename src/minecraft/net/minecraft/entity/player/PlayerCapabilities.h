#ifndef C919_SOURCE_PLAYER_CAPABILITIES_H
#define C919_SOURCE_PLAYER_CAPABILITIES_H
#include "nbt/NBTTagCompound.h"

/* Complete supplied PlayerCapabilities state, constructor field initializers,
   two NBT methods and four speed accessors. Native failure replaces Java
   exceptions; graph ownership and RootScope are C lifetime dependencies. */
typedef struct PlayerCapabilities {
    MCObject object;
    bool disableDamage, isFlying, allowFlying, isCreativeMode, allowEdit;
    float flySpeed, walkSpeed;
} PlayerCapabilities;
PlayerCapabilities *PlayerCapabilities_new(MCObjectHeap *);
bool PlayerCapabilities_isInstance(const MCObject *);
bool PlayerCapabilities_writeCapabilitiesToNBT(PlayerCapabilities *, NBTTagCompound *);
bool PlayerCapabilities_readCapabilitiesFromNBT(PlayerCapabilities *, NBTTagCompound *);
float PlayerCapabilities_getFlySpeed(const PlayerCapabilities *);
bool PlayerCapabilities_setFlySpeed(PlayerCapabilities *, float);
float PlayerCapabilities_getWalkSpeed(const PlayerCapabilities *);
bool PlayerCapabilities_setPlayerWalkSpeed(PlayerCapabilities *, float);
#endif
