#ifndef C919_C08_PACKET_PLAYER_BLOCK_PLACEMENT_H
#define C919_C08_PACKET_PLAYER_BLOCK_PLACEMENT_H
#include "inventory/inventory.h"

/* Method/field port of the supplied C08PacketPlayerBlockPlacement.java.
   mc_slot and this immutable position value adapt ItemStack/BlockPos to the
   existing runtime; those Java classes are not claimed to be fully ported.
   C allocation/stream failures are explicit and owning outputs are atomic. */
typedef struct { int x, y, z; } C08BlockPos;
typedef struct {
    C08BlockPos position;
    bool hasPosition; /* Java's empty constructor leaves position null. */
    int placedBlockDirection;
    mc_slot stack;
    float facingX, facingY, facingZ;
} C08PacketPlayerBlockPlacement;

void C08PacketPlayerBlockPlacement_init(C08PacketPlayerBlockPlacement *packet);
void C08PacketPlayerBlockPlacement_free(C08PacketPlayerBlockPlacement *packet);
bool C08PacketPlayerBlockPlacement_construct(C08PacketPlayerBlockPlacement *packet,
    const C08BlockPos *positionIn, int placedBlockDirectionIn, const mc_slot *stackIn,
    float facingXIn, float facingYIn, float facingZIn);
bool C08PacketPlayerBlockPlacement_constructUseItem(C08PacketPlayerBlockPlacement *packet, const mc_slot *stackIn);
/* Payload only: PacketBuffer's routing layer owns the packet ID/framing. */
bool C08PacketPlayerBlockPlacement_readPacketData(C08PacketPlayerBlockPlacement *packet, mc_buf *buffer);
bool C08PacketPlayerBlockPlacement_writePacketData(const C08PacketPlayerBlockPlacement *packet, mc_buf *buffer);
typedef void (*C08ProcessPlayerBlockPlacement)(void *handler, const C08PacketPlayerBlockPlacement *packet);
void C08PacketPlayerBlockPlacement_processPacket(const C08PacketPlayerBlockPlacement *packet,
    void *handler, C08ProcessPlayerBlockPlacement processPlayerBlockPlacement);
const C08BlockPos *C08PacketPlayerBlockPlacement_getPosition(const C08PacketPlayerBlockPlacement *packet);
int C08PacketPlayerBlockPlacement_getPlacedBlockDirection(const C08PacketPlayerBlockPlacement *packet);
const mc_slot *C08PacketPlayerBlockPlacement_getStack(const C08PacketPlayerBlockPlacement *packet);
float C08PacketPlayerBlockPlacement_getPlacedBlockOffsetX(const C08PacketPlayerBlockPlacement *packet);
float C08PacketPlayerBlockPlacement_getPlacedBlockOffsetY(const C08PacketPlayerBlockPlacement *packet);
float C08PacketPlayerBlockPlacement_getPlacedBlockOffsetZ(const C08PacketPlayerBlockPlacement *packet);
#endif
