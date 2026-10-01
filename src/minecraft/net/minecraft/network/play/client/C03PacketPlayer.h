#ifndef C919_SOURCE_C03_PACKET_PLAYER_H
#define C919_SOURCE_C03_PACKET_PLAYER_H
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayServer.h"
/* The nested subclasses declare no additional fields. Distinct managed class
   descriptors preserve their original inherited storage and virtual IO. */
typedef struct C03PacketPlayer {
    MCObject object;
    double x,y,z;
    float yaw,pitch;
    bool onGround,moving,rotating;
} C03PacketPlayer;
typedef C03PacketPlayer C04PacketPlayerPosition;
typedef C03PacketPlayer C05PacketPlayerLook;
typedef C03PacketPlayer C06PacketPlayerPosLook;
C03PacketPlayer *C03PacketPlayer_new_empty(MCObjectHeap *);
C03PacketPlayer *C03PacketPlayer_new(MCObjectHeap *,bool);
C04PacketPlayerPosition *C04PacketPlayerPosition_new_empty(MCObjectHeap *);
C04PacketPlayerPosition *C04PacketPlayerPosition_new(MCObjectHeap *,double,double,double,bool);
C05PacketPlayerLook *C05PacketPlayerLook_new_empty(MCObjectHeap *);
C05PacketPlayerLook *C05PacketPlayerLook_new(MCObjectHeap *,float,float,bool);
C06PacketPlayerPosLook *C06PacketPlayerPosLook_new_empty(MCObjectHeap *);
C06PacketPlayerPosLook *C06PacketPlayerPosLook_new(MCObjectHeap *,double,double,double,float,float,bool);
bool C03PacketPlayer_isInstance(const MCObject *);
/* Exact runtime classes for native protocol registration, independent of the
   source instanceof predicate that also accepts the three nested subclasses. */
bool C03PacketPlayer_nativeBaseIsInstance(const MCObject *);
bool C04PacketPlayerPosition_isInstance(const MCObject *);
bool C05PacketPlayerLook_isInstance(const MCObject *);
bool C06PacketPlayerPosLook_isInstance(const MCObject *);
bool C03PacketPlayer_readPacketData(C03PacketPlayer *,PacketBuffer *);
bool C03PacketPlayer_writePacketData(C03PacketPlayer *,PacketBuffer *);
bool C04PacketPlayerPosition_readPacketData(C04PacketPlayerPosition *,PacketBuffer *);
bool C04PacketPlayerPosition_writePacketData(C04PacketPlayerPosition *,PacketBuffer *);
bool C05PacketPlayerLook_readPacketData(C05PacketPlayerLook *,PacketBuffer *);
bool C05PacketPlayerLook_writePacketData(C05PacketPlayerLook *,PacketBuffer *);
bool C06PacketPlayerPosLook_readPacketData(C06PacketPlayerPosLook *,PacketBuffer *);
bool C06PacketPlayerPosLook_writePacketData(C06PacketPlayerPosLook *,PacketBuffer *);
bool C03PacketPlayer_processPacket(C03PacketPlayer *,INetHandlerPlayServer);
double C03PacketPlayer_getPositionX(const C03PacketPlayer *);
double C03PacketPlayer_getPositionY(const C03PacketPlayer *);
double C03PacketPlayer_getPositionZ(const C03PacketPlayer *);
float C03PacketPlayer_getYaw(const C03PacketPlayer *);
float C03PacketPlayer_getPitch(const C03PacketPlayer *);
bool C03PacketPlayer_isOnGround(const C03PacketPlayer *);
bool C03PacketPlayer_isMoving(const C03PacketPlayer *);
bool C03PacketPlayer_getRotating(const C03PacketPlayer *);
bool C03PacketPlayer_setMoving(C03PacketPlayer *,bool);
#endif
