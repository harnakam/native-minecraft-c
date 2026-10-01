#ifndef C919_NATIVE_PACKET_QUEUE_H
#define C919_NATIVE_PACKET_QUEUE_H
#include "util/MCGameplayPlayer.h"
#include "network/PacketBuffer.h"

/* Native deferred transport storage shared by the source packet adapters.
   The immutable profile identifies packet direction and its commit boundary.
   No packet bytes or independent inventory values are retained in the graph. */
typedef struct {
    int32_t id;
    bool (*isInstance)(const MCObject *);
    bool (*write)(MCObject *, PacketBuffer *);
} MCPacketCodec;
typedef struct {
    const MCPacketCodec *codecs;
    size_t count;
    bool durable;
} MCPacketQueueProfile;
MCObject *MCPacketQueue_new(MCObjectHeap *, MCGameplayObjects *, const MCPacketQueueProfile *);
bool MCPacketQueue_isInstance(const MCObject *, const MCPacketQueueProfile *);
bool MCPacketQueue_append(MCObject *, MCObject *packet, int32_t id);
int32_t MCPacketQueue_count(const MCObject *);
MCObject *MCPacketQueue_packetAt(MCObject *, int32_t index, int32_t *id);
bool MCPacketQueue_encodeAt(MCObject *, int32_t index, mc_buf *out);
bool MCPacketQueue_validate(MCObject *);
bool MCPacketQueue_validateForOwners(MCObject *, const MCGameplayObjects *);
typedef bool (*MCPacketQueueSink)(const mc_buf *, void *context);
typedef enum {
    MC_PACKET_QUEUE_SENT,
    MC_PACKET_QUEUE_UNCOMMITTED,
    MC_PACKET_QUEUE_FAILED
} MCPacketQueueResult;
/* Same whole-packet sink contract as MCGameplayPackets. Accepted prefixes are
   removed exactly once; false accepts no bytes and preserves the suffix. */
MCPacketQueueResult MCPacketQueue_flush(MCGameplay *, size_t playerIndex,
                                        const MCPacketQueueProfile *, MCPacketQueueSink,
                                        void *context);
#endif
