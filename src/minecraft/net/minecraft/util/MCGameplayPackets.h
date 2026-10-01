#ifndef C919_NATIVE_GAMEPLAY_PACKETS_H
#define C919_NATIVE_GAMEPLAY_PACKETS_H
#include "entity/player/EntityPlayerMPWindows.h"
#include "network/play/server/S2EPacketCloseWindow.h"
#include "network/play/server/S2FPacketSetSlot.h"
#include "network/play/server/S30PacketWindowItems.h"
#include "network/play/server/S32PacketConfirmTransaction.h"
#include "network/play/server/S1CPacketEntityMetadata.h"
#include "network/play/server/S39PacketPlayerAbilities.h"

typedef enum {
    MC_GAMEPLAY_PACKET_EQUIPMENT = 0x04,
    MC_GAMEPLAY_PACKET_COLLECT = 0x0d,
    MC_GAMEPLAY_PACKET_SPAWN_OBJECT = 0x0e,
    MC_GAMEPLAY_PACKET_VELOCITY = 0x12,
    MC_GAMEPLAY_PACKET_DESTROY = 0x13,
    MC_GAMEPLAY_PACKET_TELEPORT = 0x18,
    MC_GAMEPLAY_PACKET_METADATA = 0x1c,
    MC_GAMEPLAY_PACKET_SOUND = 0x29,
    MC_GAMEPLAY_PACKET_OPEN_WINDOW = 0x2d,
    MC_GAMEPLAY_PACKET_CLOSE = 0x2e,
    MC_GAMEPLAY_PACKET_SLOT = 0x2f,
    MC_GAMEPLAY_PACKET_ITEMS = 0x30,
    MC_GAMEPLAY_PACKET_CONFIRM = 0x32,
    MC_GAMEPLAY_PACKET_MAP = 0x34,
    MC_GAMEPLAY_PACKET_STATISTICS = 0x37,
    MC_GAMEPLAY_PACKET_ABILITIES = 0x39
} MCGameplayPacketKind;
/* Native managed deferred transport queue. Entries retain the original packet
   objects; their constructors copy stack occurrences exactly as the source.
   Every packet is tagged with the graph's native commit fence at construction.
   No authoritative inventory or NBT value mirror is kept here. */
bool MCGameplayPackets_bind(MCGameplayPlayer *);
bool MCGameplayPackets_sendWindowItems(MCGameplayPlayer *, int32_t window, ContainerList *);
bool MCGameplayPackets_sendSetSlot(MCGameplayPlayer *, int32_t window, int32_t slot, ItemStack *);
bool MCGameplayPackets_sendCloseWindow(MCGameplayPlayer *, int32_t window);
bool MCGameplayPackets_sendConfirmTransaction(MCGameplayPlayer *, int32_t window, int16_t action,
                                              bool accepted);
/* Source metadata keeps the original WatchableObject/ItemStack references.
   Native packet classes retain immutable encoded transport messages only. */
bool MCGameplayPackets_sendAbilities(MCGameplayPlayer *,S39PacketPlayerAbilities *);
bool MCGameplayPackets_sendMetadata(MCGameplayPlayer *,S1CPacketEntityMetadata *);
bool MCGameplayPackets_sendNative(MCGameplayPlayer *,const mc_buf *completeMessage);
int32_t MCGameplayPackets_count(const MCGameplayPlayer *);
MCObject *MCGameplayPackets_packetAt(MCGameplayPlayer *, int32_t index, MCGameplayPacketKind *);
/* out is an initialized native buffer. Failure preserves its previous value.
   Call validate during player encoding, before MCGameplay_commit's journal.
   All parameters are borrowed only during the function's own RootScope. */
bool MCGameplayPackets_encodeAt(MCGameplayPlayer *, int32_t index, mc_buf *out);
bool MCGameplayPackets_validate(MCGameplayPlayer *);
/* Sink must not mutate the managed graph or its queues. It must accept the
   entire immutable packet, or return false without
   accepting any bytes. A successful prefix is removed; a failed suffix remains.
   Snapshots are refused. A newly queued entry remains blocked until the graph
   has passed a later durable commit. Bind/source calls never invoke the sink. */
typedef bool (*MCGameplayPacketSink)(const mc_buf *, void *context);
typedef enum {
    MC_GAMEPLAY_PACKETS_SENT,
    MC_GAMEPLAY_PACKETS_UNCOMMITTED,
    MC_GAMEPLAY_PACKETS_FAILED
} MCGameplayPacketsResult;
MCGameplayPacketsResult MCGameplayPackets_flush(MCGameplay *, size_t playerIndex,
                                                MCGameplayPacketSink, void *context);
#endif
