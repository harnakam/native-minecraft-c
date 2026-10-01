#ifndef C919_NATIVE_CLIENT_GAMEPLAY_PACKETS_H
#define C919_NATIVE_CLIENT_GAMEPLAY_PACKETS_H
#include "util/MCPacketQueue.h"
#include "network/play/client/C03PacketPlayer.h"
#include "network/play/client/C0BPacketEntityAction.h"
#include "network/play/client/C07PacketPlayerDigging.h"
#include "network/play/client/C09PacketHeldItemChange.h"
#include "network/play/client/C08PacketPlayerBlockPlacement.h"
#include "network/play/client/C0DPacketCloseWindow.h"
#include "network/play/client/C0EPacketClickWindow.h"
#include "network/play/client/C0FPacketConfirmTransaction.h"
#include "network/play/client/C10PacketCreativeInventoryAction.h"
#include "network/play/client/C13PacketPlayerAbilities.h"

/* Native NetworkManager queue for the currently translated client packets.
   Retains the exact source packet; any occurrence copies happen in its source
   constructor. A different/unsupported packet class fails, never casts by ID.
   bind requires a native remote World; flush requires an adopted client frame. */
bool MCGameplayClientPackets_bind(MCGameplayPlayer *);
bool MCGameplayClientPackets_addToSendQueue(MCGameplayPlayer *, MCObject *packet);
int32_t MCGameplayClientPackets_count(const MCGameplayPlayer *);
MCObject *MCGameplayClientPackets_packetAt(MCGameplayPlayer *, int32_t index, int32_t *id);
bool MCGameplayClientPackets_encodeAt(MCGameplayPlayer *, int32_t index, mc_buf *out);
bool MCGameplayClientPackets_validate(MCGameplayPlayer *);
/* Callback for MCGameplay_acceptClientFrame: validates all native actor queues,
   not just the actor changed in this frame. context is unused and may be NULL. */
bool MCGameplayClientPackets_validateFrame(MCGameplayObjects *, void *context);
MCPacketQueueResult MCGameplayClientPackets_flush(MCGameplay *, size_t playerIndex,
                                                  MCPacketQueueSink, void *context);
#endif
