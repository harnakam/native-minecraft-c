#ifndef C919_C13_PLAYER_ABILITIES_H
#define C919_C13_PLAYER_ABILITIES_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayServer.h"
#include "entity/player/PlayerCapabilities.h"
struct C13PacketPlayerAbilities {
    MCObject object;
    bool invulnerable, flying, allowFlying, creativeMode;
    float flySpeed, walkSpeed;
};
C13PacketPlayerAbilities *C13PacketPlayerAbilities_new_empty(MCObjectHeap *);
C13PacketPlayerAbilities *C13PacketPlayerAbilities_new(MCObjectHeap *, PlayerCapabilities *);
bool C13PacketPlayerAbilities_isInstance(const MCObject *);
bool C13PacketPlayerAbilities_readPacketData(C13PacketPlayerAbilities *, PacketBuffer *);
bool C13PacketPlayerAbilities_writePacketData(C13PacketPlayerAbilities *, PacketBuffer *);
bool C13PacketPlayerAbilities_processPacket(C13PacketPlayerAbilities *, INetHandlerPlayServer);
bool C13PacketPlayerAbilities_isInvulnerable(const C13PacketPlayerAbilities *);
bool C13PacketPlayerAbilities_isFlying(const C13PacketPlayerAbilities *);
bool C13PacketPlayerAbilities_isAllowFlying(const C13PacketPlayerAbilities *);
bool C13PacketPlayerAbilities_isCreativeMode(const C13PacketPlayerAbilities *);
bool C13PacketPlayerAbilities_setInvulnerable(C13PacketPlayerAbilities *,bool);
bool C13PacketPlayerAbilities_setFlying(C13PacketPlayerAbilities *,bool);
bool C13PacketPlayerAbilities_setAllowFlying(C13PacketPlayerAbilities *,bool);
bool C13PacketPlayerAbilities_setCreativeMode(C13PacketPlayerAbilities *,bool);
bool C13PacketPlayerAbilities_setFlySpeed(C13PacketPlayerAbilities *,float);
bool C13PacketPlayerAbilities_setWalkSpeed(C13PacketPlayerAbilities *,float);
#endif
