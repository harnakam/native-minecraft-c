#include "network/GameplayPacketRouter.h"
#include "network/NetHandlerPlayServer.h"
#include "client/network/NetHandlerPlayClient.h"

static MCGameplayPlayer *actor(MCGameplay *game, size_t index, mc_buf *payload) {
    if (!game || !game->snapshot || game->fatal || !payload || payload->failed ||
        payload->pos > payload->len || payload->len > payload->cap ||
        (payload->len && !payload->data) || index >= MC_TRANSFER_MAX_PLAYERS ||
        MCObjectHeap_failed(game->heap))
        return NULL;
    MCGameplayObjects *owners = MCGameplay_get(game);
    MCObject *object = owners ? owners->players[index] : NULL;
    if (!MCGameplayPlayer_isInstance(object))
        return NULL;
    return (MCGameplayPlayer *)object;
}
static GameplayPacketResult finish(MCGameplay *game, MCObjectRootScope *scope, bool parsed,
                                   bool applied) {
    GameplayPacketResult result = !parsed ? MC_GAMEPLAY_PACKET_REJECTED
                                  : applied && !MCObjectHeap_failed(game->heap)
                                      ? MC_GAMEPLAY_PACKET_APPLIED
                                      : MC_GAMEPLAY_PACKET_FAILED;
    MCObjectRootScope_end(scope);
    return result;
}
GameplayPacketResult GameplayPacketRouter_server(MCGameplay *game, size_t index, int32_t id,
                                                 mc_buf *payload) {
    if (id != 0x0b && id != 0x0d && id != 0x0e && id != 0x0f && id != 0x10 && id != 0x13)
        return MC_GAMEPLAY_PACKET_NOT_HANDLED;
    MCGameplayPlayer *player = actor(game, index, payload);
    MCObject *handlerObject=player?MCGameplayPlayer_handler(player):NULL;
    if (!NetHandlerPlayServer_isInstance(handlerObject) || handlerObject->heap != game->heap)
        return MC_GAMEPLAY_PACKET_FAILED;
    NetHandlerPlayServer *handler = (NetHandlerPlayServer *)handlerObject;
    if (handler->playerEntity != player)
        return MC_GAMEPLAY_PACKET_FAILED;
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, game->heap))
        return MC_GAMEPLAY_PACKET_FAILED;
    PacketBuffer buffer;
    if (!PacketBuffer_init(&buffer, game->heap, payload)) {
        MCObjectRootScope_end(&scope);
        return MC_GAMEPLAY_PACKET_FAILED;
    }
    INetHandlerPlayServer target = NetHandlerPlayServer_asHandler(handler);
    bool parsed = false, applied = false;
    switch (id) {
    case 0x0b: {
        C0BPacketEntityAction *packet=C0BPacketEntityAction_new_empty(game->heap);
        parsed=packet && C0BPacketEntityAction_readPacketData(packet,&buffer) && payload->pos==payload->len;
        if(parsed)applied=C0BPacketEntityAction_processPacket(packet,target);
        break;
    }
    case 0x0d: {
        C0DPacketCloseWindow *packet = C0DPacketCloseWindow_new_empty(game->heap);
        parsed = packet && C0DPacketCloseWindow_readPacketData(packet, &buffer) &&
                 payload->pos == payload->len;
        if (parsed)
            applied = C0DPacketCloseWindow_processPacket(packet, target);
        break;
    }
    case 0x0e: {
        C0EPacketClickWindow *packet = C0EPacketClickWindow_new_empty(game->heap);
        parsed = packet && C0EPacketClickWindow_readPacketData(packet, &buffer) &&
                 payload->pos == payload->len;
        if (parsed)
            applied = C0EPacketClickWindow_processPacket(packet, target);
        break;
    }
    case 0x0f: {
        C0FPacketConfirmTransaction *packet = C0FPacketConfirmTransaction_new_empty(game->heap);
        parsed = packet && C0FPacketConfirmTransaction_readPacketData(packet, &buffer) &&
                 payload->pos == payload->len;
        if (parsed)
            applied = C0FPacketConfirmTransaction_processPacket(packet, target);
        break;
    }
    case 0x13: {
        C13PacketPlayerAbilities *packet=C13PacketPlayerAbilities_new_empty(game->heap);
        parsed=packet && C13PacketPlayerAbilities_readPacketData(packet,&buffer) && payload->pos==payload->len;
        if (parsed) applied=C13PacketPlayerAbilities_processPacket(packet,target);
        break;
    }
    default: {
        C10PacketCreativeInventoryAction *packet =
            C10PacketCreativeInventoryAction_new_empty(game->heap);
        parsed = packet && C10PacketCreativeInventoryAction_readPacketData(packet, &buffer) &&
                 payload->pos == payload->len;
        if (parsed)
            applied = C10PacketCreativeInventoryAction_processPacket(packet, target);
        break;
    }
    }
    return finish(game, &scope, parsed, applied);
}
GameplayPacketResult GameplayPacketRouter_client(MCGameplay *game, size_t index, int32_t id,
                                                 mc_buf *payload) {
    if (id != 0x1c && id != 0x2e && id != 0x2f && id != 0x30 && id != 0x32 && id != 0x39)
        return MC_GAMEPLAY_PACKET_NOT_HANDLED;
    MCGameplayPlayer *player = actor(game, index, payload);
    if (!player || !NetHandlerPlayClient_isInstance(player->handler) ||
        player->handler->heap != game->heap)
        return MC_GAMEPLAY_PACKET_FAILED;
    NetHandlerPlayClient *handler = (NetHandlerPlayClient *)player->handler;
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, game->heap))
        return MC_GAMEPLAY_PACKET_FAILED;
    PacketBuffer buffer;
    if (!PacketBuffer_init(&buffer, game->heap, payload)) {
        MCObjectRootScope_end(&scope);
        return MC_GAMEPLAY_PACKET_FAILED;
    }
    INetHandlerPlayClient target = NetHandlerPlayClient_asHandler(handler);
    bool parsed = false, applied = false;
    switch (id) {
    case 0x1c: {
        S1CPacketEntityMetadata *packet = S1CPacketEntityMetadata_new_empty(game->heap);
        parsed = packet && S1CPacketEntityMetadata_readPacketData(packet, &buffer) &&
                 payload->pos == payload->len;
        if (parsed)
            applied = S1CPacketEntityMetadata_processPacket(packet, target);
        break;
    }
    case 0x2e: {
        S2EPacketCloseWindow *packet = S2EPacketCloseWindow_new_empty(game->heap);
        parsed = packet && S2EPacketCloseWindow_readPacketData(packet, &buffer) &&
                 payload->pos == payload->len;
        if (parsed)
            applied = S2EPacketCloseWindow_processPacket(packet, target);
        break;
    }
    case 0x2f: {
        S2FPacketSetSlot *packet = S2FPacketSetSlot_new_empty(game->heap);
        parsed = packet && S2FPacketSetSlot_readPacketData(packet, &buffer) &&
                 payload->pos == payload->len;
        if (parsed)
            applied = S2FPacketSetSlot_processPacket(packet, target);
        break;
    }
    case 0x30: {
        S30PacketWindowItems *packet = S30PacketWindowItems_new_empty(game->heap);
        parsed = packet && S30PacketWindowItems_readPacketData(packet, &buffer) &&
                 payload->pos == payload->len;
        if (parsed)
            applied = S30PacketWindowItems_processPacket(packet, target);
        break;
    }
    case 0x39: {
        S39PacketPlayerAbilities *packet=S39PacketPlayerAbilities_new_empty(game->heap);
        parsed=packet && S39PacketPlayerAbilities_readPacketData(packet,&buffer) && payload->pos==payload->len;
        if (parsed) applied=S39PacketPlayerAbilities_processPacket(packet,target);
        break;
    }
    default: {
        S32PacketConfirmTransaction *packet = S32PacketConfirmTransaction_new_empty(game->heap);
        parsed = packet && S32PacketConfirmTransaction_readPacketData(packet, &buffer) &&
                 payload->pos == payload->len;
        if (parsed)
            applied = S32PacketConfirmTransaction_processPacket(packet, target);
        break;
    }
    }
    return finish(game, &scope, parsed, applied);
}
