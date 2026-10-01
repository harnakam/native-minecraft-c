#ifndef C919_SOURCE_C07_PACKET_PLAYER_DIGGING_H
#define C919_SOURCE_C07_PACKET_PLAYER_DIGGING_H
#include "entity/DataWatcher.h"
#include "network/play/INetHandlerPlayServer.h"

/* Immutable native views of the original nested Action and EnumFacing enum
   identities. Full JDK Enum/EnumFacing methods are separate dependencies. */
typedef struct {
    int32_t ordinal;
} C07PacketPlayerDiggingAction;
typedef struct {
    int32_t index;
} MCNativeEnumFacing;
extern const C07PacketPlayerDiggingAction C07PacketPlayerDigging_ACTIONS[6];
extern const MCNativeEnumFacing C07PacketPlayerDigging_FACINGS[6];
enum {
    C07_START_DESTROY_BLOCK,
    C07_ABORT_DESTROY_BLOCK,
    C07_STOP_DESTROY_BLOCK,
    C07_DROP_ALL_ITEMS,
    C07_DROP_ITEM,
    C07_RELEASE_USE_ITEM
};
/* action accepts only a declared ordinal; facing implements the original
   EnumFacing.getFront's absolute remainder, rather than rejecting u8 >5. */
const C07PacketPlayerDiggingAction *C07PacketPlayerDigging_action(int32_t ordinal);
const MCNativeEnumFacing *C07PacketPlayerDigging_facing(int32_t index);
struct C07PacketPlayerDigging {
    MCObject object;
    DataWatcherBlockPos *position;
    const MCNativeEnumFacing *facing;
    const C07PacketPlayerDiggingAction *status;
};
C07PacketPlayerDigging *C07PacketPlayerDigging_new_empty(MCObjectHeap *);
C07PacketPlayerDigging *C07PacketPlayerDigging_new(MCObjectHeap *,
                                                   const C07PacketPlayerDiggingAction *,
                                                   DataWatcherBlockPos *,
                                                   const MCNativeEnumFacing *);
bool C07PacketPlayerDigging_isInstance(const MCObject *);
bool C07PacketPlayerDigging_readPacketData(C07PacketPlayerDigging *, PacketBuffer *);
bool C07PacketPlayerDigging_writePacketData(C07PacketPlayerDigging *, PacketBuffer *);
bool C07PacketPlayerDigging_processPacket(C07PacketPlayerDigging *, INetHandlerPlayServer);
DataWatcherBlockPos *C07PacketPlayerDigging_getPosition(const C07PacketPlayerDigging *);
const MCNativeEnumFacing *C07PacketPlayerDigging_getFacing(const C07PacketPlayerDigging *);
const C07PacketPlayerDiggingAction *
C07PacketPlayerDigging_getStatus(const C07PacketPlayerDigging *);
#endif
