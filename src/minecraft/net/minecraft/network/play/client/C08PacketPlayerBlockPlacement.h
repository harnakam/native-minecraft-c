#ifndef C919_SOURCE_C08_PACKET_PLAYER_BLOCK_PLACEMENT_H
#define C919_SOURCE_C08_PACKET_PLAYER_BLOCK_PLACEMENT_H
#include "entity/DataWatcher.h"
#include "network/play/INetHandlerPlayServer.h"

/* Original packet fields and method bodies over managed source ItemStack refs.
   DataWatcherBlockPos is the shared immutable native coordinate view; complete
   BlockPos/Vec3i and delegated PacketBuffer position methods remain dependencies.
   The use-item constructor retains a per-heap managed source class static.
   Getters borrow actual fields, including nullable position/count-zero stacks. */
struct C08PacketPlayerBlockPlacement {
    MCObject object;
    DataWatcherBlockPos *position;
    int32_t placedBlockDirection;
    ItemStack *stack;
    float facingX, facingY, facingZ;
};
C08PacketPlayerBlockPlacement *C08PacketPlayerBlockPlacement_new_empty(MCObjectHeap *);
C08PacketPlayerBlockPlacement *C08PacketPlayerBlockPlacement_new_useItem(MCObjectHeap *,ItemStack *);
C08PacketPlayerBlockPlacement *C08PacketPlayerBlockPlacement_new(MCObjectHeap *,DataWatcherBlockPos *,
    int32_t placedBlockDirectionIn,ItemStack *stackIn,float facingXIn,float facingYIn,float facingZIn);
bool C08PacketPlayerBlockPlacement_isInstance(const MCObject *);
/* Payload only. Source read assignments and partial writer bytes remain on a
   failed IO operation; callers discard the failed packet/frame. */
bool C08PacketPlayerBlockPlacement_readPacketData(C08PacketPlayerBlockPlacement *,PacketBuffer *);
bool C08PacketPlayerBlockPlacement_writePacketData(C08PacketPlayerBlockPlacement *,PacketBuffer *);
bool C08PacketPlayerBlockPlacement_processPacket(C08PacketPlayerBlockPlacement *,INetHandlerPlayServer);
DataWatcherBlockPos *C08PacketPlayerBlockPlacement_getPosition(const C08PacketPlayerBlockPlacement *);
int32_t C08PacketPlayerBlockPlacement_getPlacedBlockDirection(const C08PacketPlayerBlockPlacement *);
ItemStack *C08PacketPlayerBlockPlacement_getStack(const C08PacketPlayerBlockPlacement *);
float C08PacketPlayerBlockPlacement_getPlacedBlockOffsetX(const C08PacketPlayerBlockPlacement *);
float C08PacketPlayerBlockPlacement_getPlacedBlockOffsetY(const C08PacketPlayerBlockPlacement *);
float C08PacketPlayerBlockPlacement_getPlacedBlockOffsetZ(const C08PacketPlayerBlockPlacement *);
#endif
