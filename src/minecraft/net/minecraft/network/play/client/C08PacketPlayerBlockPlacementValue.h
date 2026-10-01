#ifndef C919_C08_PACKET_PLAYER_BLOCK_PLACEMENT_VALUE_H
#define C919_C08_PACKET_PLAYER_BLOCK_PLACEMENT_VALUE_H
#include "inventory/inventory.h"

/* Legacy owning Slot/coordinate adapter used by the native value runtime.
   The managed original packet is C08PacketPlayerBlockPlacement. This adapter
   keeps its existing atomic IO and positive Slot-count validation; it cannot
   preserve source ItemStack or BlockPos reference identity. */
typedef struct { int x, y, z; } C08ValueBlockPos;
typedef struct {
    C08ValueBlockPos position;
    bool hasPosition; /* Java's empty constructor leaves position null. */
    int placedBlockDirection;
    mc_slot stack;
    float facingX, facingY, facingZ;
} C08PacketPlayerBlockPlacementValue;

void C08PacketPlayerBlockPlacementValue_init(C08PacketPlayerBlockPlacementValue *packet);
void C08PacketPlayerBlockPlacementValue_free(C08PacketPlayerBlockPlacementValue *packet);
bool C08PacketPlayerBlockPlacementValue_construct(C08PacketPlayerBlockPlacementValue *packet,
    const C08ValueBlockPos *positionIn, int placedBlockDirectionIn, const mc_slot *stackIn,
    float facingXIn, float facingYIn, float facingZIn);
bool C08PacketPlayerBlockPlacementValue_constructUseItem(C08PacketPlayerBlockPlacementValue *packet, const mc_slot *stackIn);
/* Payload only: PacketBuffer's routing layer owns the packet ID/framing. */
bool C08PacketPlayerBlockPlacementValue_readPacketData(C08PacketPlayerBlockPlacementValue *packet, mc_buf *buffer);
bool C08PacketPlayerBlockPlacementValue_writePacketData(const C08PacketPlayerBlockPlacementValue *packet, mc_buf *buffer);
typedef void (*C08ValueProcessPlayerBlockPlacement)(void *handler, const C08PacketPlayerBlockPlacementValue *packet);
void C08PacketPlayerBlockPlacementValue_processPacket(const C08PacketPlayerBlockPlacementValue *packet,
    void *handler, C08ValueProcessPlayerBlockPlacement processPlayerBlockPlacement);
const C08ValueBlockPos *C08PacketPlayerBlockPlacementValue_getPosition(const C08PacketPlayerBlockPlacementValue *packet);
int C08PacketPlayerBlockPlacementValue_getPlacedBlockDirection(const C08PacketPlayerBlockPlacementValue *packet);
const mc_slot *C08PacketPlayerBlockPlacementValue_getStack(const C08PacketPlayerBlockPlacementValue *packet);
float C08PacketPlayerBlockPlacementValue_getPlacedBlockOffsetX(const C08PacketPlayerBlockPlacementValue *packet);
float C08PacketPlayerBlockPlacementValue_getPlacedBlockOffsetY(const C08PacketPlayerBlockPlacementValue *packet);
float C08PacketPlayerBlockPlacementValue_getPlacedBlockOffsetZ(const C08PacketPlayerBlockPlacementValue *packet);
#endif
