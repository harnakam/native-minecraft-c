#ifndef C919_NATIVE_GAMEPLAY_PACKET_ROUTER_H
#define C919_NATIVE_GAMEPLAY_PACKET_ROUTER_H
#include "util/MCGameplay.h"
#include "network/protocol.h"

/* Native protocol registration/lifetime boundary, not another play handler.
   Payload starts after the already decoded packet ID. Only working snapshots
   are accepted; callbacks/effects stay in that graph until the caller commits.
   The caller releases borrowed pointers before commit/abort and reacquires all
   owners after adoption. Unrecognized IDs are left entirely untouched.
   Required handler thread bindings execute inline on this native single writer;
   general Netty scheduling/queued-task ownership remains a separate dependency. */
typedef enum {
    MC_GAMEPLAY_PACKET_NOT_HANDLED,
    MC_GAMEPLAY_PACKET_APPLIED,
    MC_GAMEPLAY_PACKET_REJECTED,
    MC_GAMEPLAY_PACKET_FAILED
} GameplayPacketResult;

/* A rejected payload can have consumed a prefix or allocated source objects;
   no handler is called before its complete payload is validated. Both rejected
   and failed outcomes require discarding the whole working snapshot. Success
   does not imply journal commitment or authorize a socket response. */
GameplayPacketResult GameplayPacketRouter_server(MCGameplay *working, size_t playerIndex,
                                                 int32_t packetId, mc_buf *payload);
GameplayPacketResult GameplayPacketRouter_client(MCGameplay *working, size_t playerIndex,
                                                 int32_t packetId, mc_buf *payload);
#endif
