#ifndef C919_SOURCE_C0B_PACKET_ENTITY_ACTION_H
#define C919_SOURCE_C0B_PACKET_ENTITY_ACTION_H
#include "entity/Entity.h"
#include "network/PacketBuffer.h"
#include "network/play/INetHandlerPlayServer.h"
/* Immutable canonical ordinal identities for the original seven enum values.
   Java Enum/generated values/name infrastructure remains a native boundary. */
typedef struct C0BPacketEntityActionAction {
    int32_t ordinal;
} C0BPacketEntityActionAction;
enum {
    C0B_START_SNEAKING,
    C0B_STOP_SNEAKING,
    C0B_STOP_SLEEPING,
    C0B_START_SPRINTING,
    C0B_STOP_SPRINTING,
    C0B_RIDING_JUMP,
    C0B_OPEN_INVENTORY
};
const C0BPacketEntityActionAction *C0BPacketEntityAction_nativeAction(int32_t ordinal);
bool C0BPacketEntityAction_nativeOrdinal(const C0BPacketEntityActionAction *, int32_t *out);
struct C0BPacketEntityAction {
    MCObject object;
    int32_t entityID;
    const C0BPacketEntityActionAction *action;
    int32_t auxData;
};
C0BPacketEntityAction *C0BPacketEntityAction_new_empty(MCObjectHeap *);
C0BPacketEntityAction *C0BPacketEntityAction_new(MCObjectHeap *, Entity *,
                                                 const C0BPacketEntityActionAction *);
C0BPacketEntityAction *C0BPacketEntityAction_new_aux(MCObjectHeap *, Entity *,
                                                     const C0BPacketEntityActionAction *, int32_t);
bool C0BPacketEntityAction_isInstance(const MCObject *);
bool C0BPacketEntityAction_readPacketData(C0BPacketEntityAction *, PacketBuffer *);
bool C0BPacketEntityAction_writePacketData(C0BPacketEntityAction *, PacketBuffer *);
bool C0BPacketEntityAction_processPacket(C0BPacketEntityAction *, INetHandlerPlayServer);
const C0BPacketEntityActionAction *C0BPacketEntityAction_getAction(const C0BPacketEntityAction *);
int32_t C0BPacketEntityAction_getAuxData(const C0BPacketEntityAction *);
#endif
