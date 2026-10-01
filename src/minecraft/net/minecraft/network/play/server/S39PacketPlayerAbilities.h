#ifndef C919_S39_PLAYER_ABILITIES_H
#define C919_S39_PLAYER_ABILITIES_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayClient.h"
#include "entity/player/PlayerCapabilities.h"
struct S39PacketPlayerAbilities {
    MCObject object;
    bool invulnerable, flying, allowFlying, creativeMode;
    float flySpeed, walkSpeed;
};
S39PacketPlayerAbilities *S39PacketPlayerAbilities_new_empty(MCObjectHeap *);
S39PacketPlayerAbilities *S39PacketPlayerAbilities_new(MCObjectHeap *, PlayerCapabilities *);
bool S39PacketPlayerAbilities_isInstance(const MCObject *);
bool S39PacketPlayerAbilities_readPacketData(S39PacketPlayerAbilities *, PacketBuffer *);
bool S39PacketPlayerAbilities_writePacketData(S39PacketPlayerAbilities *, PacketBuffer *);
bool S39PacketPlayerAbilities_processPacket(S39PacketPlayerAbilities *, INetHandlerPlayClient);
bool S39PacketPlayerAbilities_isInvulnerable(const S39PacketPlayerAbilities *);
bool S39PacketPlayerAbilities_isFlying(const S39PacketPlayerAbilities *);
bool S39PacketPlayerAbilities_isAllowFlying(const S39PacketPlayerAbilities *);
bool S39PacketPlayerAbilities_isCreativeMode(const S39PacketPlayerAbilities *);
bool S39PacketPlayerAbilities_setInvulnerable(S39PacketPlayerAbilities *,bool);
bool S39PacketPlayerAbilities_setFlying(S39PacketPlayerAbilities *,bool);
bool S39PacketPlayerAbilities_setAllowFlying(S39PacketPlayerAbilities *,bool);
bool S39PacketPlayerAbilities_setCreativeMode(S39PacketPlayerAbilities *,bool);
bool S39PacketPlayerAbilities_setFlySpeed(S39PacketPlayerAbilities *,float);
bool S39PacketPlayerAbilities_setWalkSpeed(S39PacketPlayerAbilities *,float);
float S39PacketPlayerAbilities_getFlySpeed(const S39PacketPlayerAbilities *);
float S39PacketPlayerAbilities_getWalkSpeed(const S39PacketPlayerAbilities *);
#endif
