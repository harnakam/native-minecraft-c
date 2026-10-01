#include "util/MCGameplayClientPackets.h"
#include "util/MCGameplayPackets.h"
#include "client/multiplayer/PlayerControllerMP.h"
#include "network/GameplayPacketRouter.h"
#include "nbt/NBTTagByte.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "client frame check %u at %d: %s\n", checks, __LINE__, #x);            \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static ItemStack *recipe(InventoryCrafting *g, MCObject *w) {
    return CraftingManager_findMatchingRecipe(((MCGameplayWorld *)w)->manager, g, w, NULL, NULL);
}
static ItemStackArray *remaining(InventoryCrafting *g, MCObject *w) {
    return CraftingManager_func_180303_b(((MCGameplayWorld *)w)->manager, g, w);
}
/* Required fixture dependencies outside this test's exercised source methods.
   Invoking one is a test failure, never an empty-success production callback. */
static bool craft(ItemStack *s, MCObject *w, MCObject *p, int32_t n) {
    (void)s;
    (void)w;
    (void)p;
    (void)n;
    CHECK(false);
    return false;
}
static bool achievement(MCObject *p, mc_crafting_achievement a) {
    (void)p;
    (void)a;
    CHECK(false);
    return false;
}
static bool drop(MCObject *p, ItemStack *s, bool b) {
    (void)p;
    (void)s;
    (void)b;
    CHECK(false);
    return false;
}
static bool type(const Item *i) {
    (void)i;
    return false;
}
static int32_t armor(const Item *i) {
    int32_t n = ItemStack_registryId(i);
    return n >= 298 && n <= 317 ? (n - 298) % 4 : -1;
}
static const mc_crafting_dispatch crafting = {MCGameplayPlayer_inventory,
                                              MCGameplayPlayer_world,
                                              recipe,
                                              remaining,
                                              craft,
                                              achievement,
                                              drop,
                                              type,
                                              type,
                                              type,
                                              type,
                                              armor,
                                              MCGameplayWorld_isRemote,
                                              MCGameplayWorld_isCraftingTable,
                                              MCGameplayPlayer_getDistanceSq};
typedef struct {
    MCObject object;
    MCGameplayPlayer *player;
    PlayerControllerMP *controller;
    unsigned sends, threads;
    bool failSend;
} Context;
static void context_trace(MCObject *o, MCObjectVisitor v, void *c) {
    Context *s = (Context *)o;
    s->player = (MCGameplayPlayer *)v((MCObject *)s->player, c);
    s->controller = (PlayerControllerMP *)v((MCObject *)s->controller, c);
}
static const MCObjectClass context_class = {"fixture.ClientFrameController",
                                            MCObjectHeap_plainClone, context_trace, NULL};
static MCPacketThreadResult thread(MCObject *o, NetHandlerPlayClient *h, MCObject *p) {
    Context *c = (Context *)o;
    CHECK(h->gameController == o && p->heap == o->heap);
    ++c->threads;
    MCObjectHeap_touch(o->heap);
    return MC_PACKET_THREAD_EXECUTE;
}
static MCGameplayPlayer *get_player(MCObject *o, MCObject *controller) {
    CHECK(o == controller);
    return ((Context *)o)->player;
}
static bool creative_screen(MCObject *o, MCObject *controller) {
    (void)o;
    (void)controller;
    CHECK(false);
    return false;
}
static int32_t tab(MCObject *o, MCObject *controller) {
    (void)o;
    (void)controller;
    CHECK(false);
    return 0;
}
static int32_t inventory_tab(MCObject *o) {
    (void)o;
    CHECK(false);
    return 0;
}
static bool close_screen(MCObject *o, MCGameplayPlayer *p) {
    (void)o;
    (void)p;
    CHECK(false);
    return false;
}
static bool send_confirm(MCObject *o, NetHandlerPlayClient *h, C0FPacketConfirmTransaction *p) {
    Context *c = (Context *)o;
    CHECK(h == (NetHandlerPlayClient *)c->player->handler);
    ++c->sends;
    MCObjectHeap_touch(o->heap);
    return !c->failSend && MCGameplayClientPackets_addToSendQueue(c->player, (MCObject *)p);
}
static bool send_click(MCObject *o, NetHandlerPlayClient *h, C0EPacketClickWindow *p) {
    Context *c = (Context *)o;
    CHECK(h == (NetHandlerPlayClient *)c->player->handler);
    /* Source transaction increment and prediction precede the queue call. */
    CHECK(c->player->openContainer->transactionID == p->actionNumber);
    if (p->slotId == 36)
        CHECK(!InventoryPlayer_getStackInSlot(c->player->inventory, 0) &&
              InventoryPlayer_getItemStack(c->player->inventory));
    ++c->sends;
    MCObjectHeap_touch(o->heap);
    return !c->failSend && MCGameplayClientPackets_addToSendQueue(c->player, (MCObject *)p);
}
static const NetHandlerPlayClientDependencies handler_deps = {
    .checkThreadAndEnqueue = thread,
    .getPlayer = get_player,
    .isCreativeScreen = creative_screen,
    .selectedCreativeTabIndex = tab,
    .inventoryCreativeTabIndex = inventory_tab,
    .closeScreenAndDropStack = close_screen,
    .addToSendQueue = send_confirm};
static const PlayerControllerMPDependencies controller_deps = {send_click};
static const char *uuids[] = {"11111111-1111-1111-1111-111111111111",
                              "22222222-2222-2222-2222-222222222222"};
static Context *setup(MCGameplay *game, bool remote) {
    CHECK(MCGameplay_init(game, 32u * 1024u * 1024u));
    MCObjectRootScope scope = {0};
    CHECK(MCObjectRootScope_begin(&scope, game->heap));
    CraftingManager *m = CraftingManager_newEmpty(game->heap);
    CHECK(m);
    MCGameplayWorld *w = MCGameplayWorld_new(game->heap, MCGameplay_get(game), NULL, m);
    CHECK(w);
    w->remote = remote;
    CHECK(MCGameplay_setWorld(game, (MCObject *)w));
    MCGameplayPlayer *p =
        MCGameplayPlayer_new(w, NBTString_fromASCII(game->heap, "Alice"), NULL, &crafting);
    MCGameplayPlayer *q =
        MCGameplayPlayer_new(w, NBTString_fromASCII(game->heap, "Bob"), NULL, &crafting);
    CHECK(p && q && MCGameplay_setPlayer(game, 0, uuids[0], (MCObject *)p) &&
          MCGameplay_setPlayer(game, 1, uuids[1], (MCObject *)q));
    Context *c = (Context *)MCObjectHeap_alloc(game->heap, sizeof(*c), &context_class);
    CHECK(c);
    c->player = p;
    p->effects = (MCObject *)c;
    if (remote) {
        CHECK(MCGameplayClientPackets_bind(p));
        NetHandlerPlayClient *h =
            NetHandlerPlayClient_nativeNew(p, (MCObject *)c, (MCObject *)c, &handler_deps);
        CHECK(h);
        c->controller =
            PlayerControllerMP_nativeNew(game->heap, h, (MCObject *)c, &controller_deps);
        CHECK(c->controller);
    }
    ItemStack *shared = ItemStack_new(game->heap, ItemStack_registryItem(1), 5, 0);
    CHECK(shared);
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory, 0, shared) &&
          InventoryPlayer_setInventorySlotContents(q->inventory, 1, shared));
    CHECK(MCGameplay_addItem(game, (MCObject *)shared));
    MCObjectRootScope_end(&scope);
    return c;
}
static MCGameplayPlayer *player(MCGameplay *game, size_t i) {
    return (MCGameplayPlayer *)MCGameplay_get(game)->players[i];
}
typedef struct {
    MCGameplay *game;
    unsigned calls, accepted;
    int failAt;
    bool reenter, append;
    int32_t ids[128];
} Sink;
static bool sink(const mc_buf *b, void *o) {
    Sink *s = (Sink *)o;
    ++s->calls;
    CHECK(MCObjectHeap_hasBorrowers(s->game->heap) && b->len && !b->failed);
    if (s->reenter) {
        s->reenter = false;
        CHECK(MCGameplayClientPackets_flush(s->game, 0, sink, s) == MC_PACKET_QUEUE_FAILED);
    }
    if ((int)s->calls == s->failAt)
        return false;
    mc_buf view = *b;
    int32_t id = mc_get_varint(&view);
    CHECK(!view.failed);
    CHECK(s->accepted < 128);
    s->ids[s->accepted++] = id;
    if (s->append)
        CHECK(!MCGameplayClientPackets_addToSendQueue(
            player(s->game, 0), (MCObject *)C0DPacketCloseWindow_new(s->game->heap, 9)));
    return true;
}
static bool validate(MCGameplayObjects *o, void *context) {
    CHECK(MCObjectHeap_hasBorrowers(o->object.heap));
    return MCGameplayClientPackets_validateFrame(o, context);
}
static bool mutate_parent(MCGameplayObjects *o, void *context) {
    MCGameplay *parent = (MCGameplay *)context;
    CHECK(MCGameplayClientPackets_validateFrame(o, NULL));
    MCObjectHeap_touch(parent->heap);
    return true;
}
static bool fail_validation(MCGameplayObjects *o, void *context) {
    (void)o;
    (void)context;
    return false;
}
static bool mixed_queue_validation(MCGameplayObjects *o, void *context) {
    (void)context;
    return MCGameplayClientPackets_validate((MCGameplayPlayer *)o->players[0]) &&
           MCGameplayPackets_validate((MCGameplayPlayer *)o->players[1]);
}
static bool try_parent_reentry(MCGameplayObjects *o, void *context) {
    MCGameplay *parent = (MCGameplay *)context;
    CHECK(MCObjectHeap_hasBorrowers(parent->heap) && !MCGameplay_free(parent));
    CHECK(!MCObjectHeap_collect(parent->heap));
    MCGameplayTransaction nested = {0};
    CHECK(!MCGameplay_begin(parent, &nested));
    return MCGameplayClientPackets_validateFrame(o, NULL);
}
static void accept_frame(MCGameplayTransaction *tx) {
    char error[256];
    CHECK(MCGameplay_acceptClientFrame(tx, validate, NULL, error, sizeof error));
    CHECK(!tx->active && !tx->working.heap);
}
int main(void) {
    MCGameplay game = {0};
    Context *c = setup(&game, true);
    MCGameplayPlayer *p = c->player;
    MCObjectRoot extra = {0};
    CHECK(MCObjectRoot_init(&extra, game.heap, MCGameplay_get(&game)->items[0]));
    MCGameplayTransaction tx = {0};
    CHECK(MCGameplay_begin(&game, &tx));
    MCGameplayPlayer *wp = player(&tx.working, 0), *wq = player(&tx.working, 1);
    Context *wc = (Context *)wp->effects;
    CHECK(wc != c && wc->player == wp);
    MCObjectRootScope scope = {0};
    CHECK(MCObjectRootScope_begin(&scope, tx.working.heap));
    wp->openContainer->transactionID = INT16_MAX;
    ItemStack *result = (ItemStack *)MCGameplay_get(&game)->items[0];
    CHECK(PlayerControllerMP_windowClick(wc->controller, 0, 36, 0, 0, wp, &result));
    CHECK(result && result->stackSize == 5 && wp->openContainer->transactionID == INT16_MIN);
    ItemStack *shared = InventoryPlayer_getItemStack(wp->inventory);
    CHECK(shared && shared == InventoryPlayer_getStackInSlot(wq->inventory, 1) &&
          (MCObject *)shared == MCGameplay_get(&tx.working)->items[0]);
    int32_t id = 0;
    C0EPacketClickWindow *first =
        (C0EPacketClickWindow *)MCGameplayClientPackets_packetAt(wp, 0, &id);
    CHECK(id == 0x0e && first->clickedItem && first->clickedItem != result &&
          first->clickedItem != shared && first->actionNumber == INT16_MIN);
    CHECK(PlayerControllerMP_windowClick(wc->controller, 0, 37, 0, 0, wp, &result) && !result);
    CHECK(wc->sends == 2 && MCGameplayClientPackets_count(wp) == 2 &&
          InventoryPlayer_getStackInSlot(wp->inventory, 1) != shared &&
          InventoryPlayer_getStackInSlot(wp->inventory, 1)->stackSize == 5 &&
          shared->stackSize == 0 && !InventoryPlayer_getItemStack(wp->inventory));
    /* Source splitStack creates the destination occurrence and decrements the
       original
     * cursor object, including Bob's still-non-null shared reference. */
    CHECK(first->clickedItem->stackSize == 5);
    CHECK(MCGameplayClientPackets_addToSendQueue(
        wp, (MCObject *)C08PacketPlayerBlockPlacement_new_useItem(tx.working.heap, shared)));
    CHECK(MCGameplayClientPackets_addToSendQueue(
        wp, (MCObject *)C0DPacketCloseWindow_new(tx.working.heap, 17)));
    CHECK(MCGameplayClientPackets_addToSendQueue(
        wp, (MCObject *)C10PacketCreativeInventoryAction_new(tx.working.heap, 5, shared)));
    CHECK(MCGameplayClientPackets_validate(wp));
    char error[256];
    CHECK(!MCGameplay_acceptClientFrame(&tx, validate, NULL, error, sizeof error) && tx.active);
    CHECK(MCGameplay_get(&game)->commitSerial == 0 && MCGameplayClientPackets_count(p) == 0 &&
          InventoryPlayer_getStackInSlot(p->inventory, 0)->stackSize == 5);
    MCObjectRootScope_end(&scope);
    Sink s = {.game = &game};
    CHECK(MCGameplayClientPackets_flush(&tx.working, 0, sink, &s) == MC_PACKET_QUEUE_FAILED &&
          !s.calls);
    accept_frame(&tx);
    p = player(&game, 0);
    c = (Context *)p->effects;
    shared = InventoryPlayer_getStackInSlot(player(&game, 1)->inventory, 1);
    CHECK(shared && !shared->stackSize &&
          shared != InventoryPlayer_getStackInSlot(p->inventory, 1) &&
          InventoryPlayer_getStackInSlot(p->inventory, 1)->stackSize == 5 &&
          (MCObject *)shared == MCGameplay_get(&game)->items[0] &&
          (MCObject *)shared == MCObjectRoot_get(&extra));
    CHECK(MCGameplay_get(&game)->commitSerial == 1 && !MCGameplay_get(&game)->durableSerial);
    CHECK(c->controller->netClientHandler == (NetHandlerPlayClient *)p->handler && c->player == p);
    /* Exercise the encoded source packet after whole-graph adoption. */
    mc_buf wire = {0};
    CHECK(MCGameplayClientPackets_encodeAt(p, 0, &wire));
    CHECK(mc_get_varint(&wire) == 0x0e);
    MCObjectHeap *decode = MCObjectHeap_new(1024u * 1024u);
    CHECK(decode);
    C0EPacketClickWindow *decoded = C0EPacketClickWindow_new_empty(decode);
    PacketBuffer pb;
    CHECK(decoded && PacketBuffer_init(&pb, decode, &wire) &&
          C0EPacketClickWindow_readPacketData(decoded, &pb));
    CHECK(wire.pos == wire.len && decoded->slotId == 36 && decoded->actionNumber == INT16_MIN &&
          decoded->clickedItem->stackSize == 5);
    mc_buf_free(&wire);
    MCObjectHeap_free(decode);
    s.failAt = 2;
    s.reenter = true;
    CHECK(MCGameplayClientPackets_flush(&game, 0, sink, &s) == MC_PACKET_QUEUE_FAILED &&
          s.accepted == 1 && MCGameplayClientPackets_count(p) == 4 && !game.fatal);
    s.failAt = 0;
    CHECK(MCGameplayClientPackets_flush(&game, 0, sink, &s) == MC_PACKET_QUEUE_SENT &&
          s.accepted == 5 && s.ids[0] == 0x0e && s.ids[1] == 0x0e && s.ids[2] == 0x08 &&
          s.ids[3] == 0x0d && s.ids[4] == 0x10);
    /* Real inbound source handler -> actual C0F -> adopted frame -> wire queue.
       Source rejection sends ACK; it leaves the predicted alias graph intact. */
    CHECK(MCGameplay_begin(&game, &tx));
    mc_put_u8(&wire, 0);
    mc_put_i16(&wire, INT16_MIN);
    mc_put_u8(&wire, 0);
    CHECK(GameplayPacketRouter_client(&tx.working, 0, 0x32, &wire) == MC_GAMEPLAY_PACKET_APPLIED);
    wp = player(&tx.working, 0);
    wc = (Context *)wp->effects;
    CHECK(wc->threads == 1 && wc->sends == 3 && !InventoryPlayer_getStackInSlot(wp->inventory, 0) &&
          InventoryPlayer_getStackInSlot(wp->inventory, 1)->stackSize == 5 &&
          InventoryPlayer_getStackInSlot(player(&tx.working, 1)->inventory, 1)->stackSize == 0);
    C0FPacketConfirmTransaction *ack =
        (C0FPacketConfirmTransaction *)MCGameplayClientPackets_packetAt(wp, 0, &id);
    CHECK(id == 0x0f && ack->uid == INT16_MIN && ack->accepted);
    mc_buf_free(&wire);
    accept_frame(&tx);
    p = player(&game, 0);
    CHECK(MCGameplayClientPackets_encodeAt(p, 0, &wire) && wire.len == 5 && wire.data[0] == 0x0f &&
          wire.data[1] == 0 && wire.data[2] == 0x80 && wire.data[3] == 0 && wire.data[4] == 1);
    mc_buf_free(&wire);
    CHECK(MCGameplayClientPackets_flush(&game, 0, sink, &s) == MC_PACKET_QUEUE_SENT &&
          s.accepted == 6);
    /* A malformed packet fails preflight before adoption. */
    CHECK(MCGameplay_begin(&game, &tx));
    wp = player(&tx.working, 0);
    ItemStack *bad = ItemStack_new(tx.working.heap, NULL, 1, 0);
    CHECK(bad);
    CHECK(MCGameplayClientPackets_addToSendQueue(
        wp, (MCObject *)C10PacketCreativeInventoryAction_new(tx.working.heap, 0, bad)));
    CHECK(!MCGameplay_acceptClientFrame(&tx, validate, NULL, error, sizeof error) && !tx.active &&
          MCGameplay_get(&game)->commitSerial == 2 && !game.fatal &&
          !MCObjectHeap_failed(game.heap));
    CHECK(MCGameplay_begin(&game, &tx));
    CHECK(!MCGameplay_acceptClientFrame(&tx, fail_validation, NULL, error, sizeof error) &&
          !tx.active);
    CHECK(MCGameplay_begin(&game, &tx));
    CHECK(!MCGameplay_acceptClientFrame(&tx, mutate_parent, &game, error, sizeof error) &&
          !tx.active && MCGameplay_get(&game)->commitSerial == 2);
    CHECK(MCGameplay_begin(&game, &tx));
    CHECK(MCGameplay_acceptClientFrame(&tx, try_parent_reentry, &game, error, sizeof error));
    p = player(&game, 0);
    CHECK(MCGameplay_begin(&game, &tx));
    wp = player(&tx.working, 0);
    wc = (Context *)wp->effects;
    wc->failSend = true;
    result = (ItemStack *)MCObjectRoot_get(&extra);
    CHECK(!PlayerControllerMP_windowClick(wc->controller, 0, 38, 0, 0, wp, &result) &&
          result == (ItemStack *)MCObjectRoot_get(&extra));
    CHECK(!MCGameplay_acceptClientFrame(&tx, validate, NULL, error, sizeof error) && !tx.active);
    /* Newly queued entries remain fenced until a subsequent adopted frame. */
    CHECK(MCGameplayClientPackets_addToSendQueue(
        p, (MCObject *)C0DPacketCloseWindow_new(game.heap, 18)));
    unsigned calls = s.calls;
    CHECK(MCGameplayClientPackets_flush(&game, 0, sink, &s) == MC_PACKET_QUEUE_UNCOMMITTED &&
          s.calls == calls);
    CHECK(MCGameplay_begin(&game, &tx));
    accept_frame(&tx);
    p = player(&game, 0);
    CHECK(MCGameplayClientPackets_flush(&game, 0, sink, &s) == MC_PACKET_QUEUE_SENT &&
          s.calls == calls + 1);
    CHECK(MCGameplayClientPackets_addToSendQueue(
        p, (MCObject *)C0DPacketCloseWindow_new(game.heap, 19)));
    CHECK(MCGameplayClientPackets_addToSendQueue(
        p, (MCObject *)C0DPacketCloseWindow_new(game.heap, 20)));
    CHECK(MCGameplay_begin(&game, &tx));
    accept_frame(&tx);
    p = player(&game, 0);
    s.append = true;
    CHECK(MCGameplayClientPackets_flush(&game, 0, sink, &s) == MC_PACKET_QUEUE_FAILED &&
          game.fatal && MCGameplayClientPackets_count(p) == 1);
    MCObjectRoot_drop(&extra);
    CHECK(MCGameplay_free(&game));
    /* Client adoption must not bypass server durability, including when the
       working graph tries to turn a server World into a remote World. */
    setup(&game, false);
    CHECK(MCGameplay_begin(&game, &tx));
    ((MCGameplayWorld *)MCGameplay_get(&tx.working)->world)->remote = true;
    CHECK(!MCGameplay_acceptClientFrame(&tx, validate, NULL, error, sizeof error) && !tx.active &&
          !((MCGameplayWorld *)MCGameplay_get(&game)->world)->remote);
    CHECK(MCGameplay_free(&game));
    setup(&game, true);
    MCGameplay_get(&game)->commitSerial = UINT64_MAX;
    MCObjectHeap_touch(game.heap);
    CHECK(MCGameplay_begin(&game, &tx));
    CHECK(!MCGameplay_acceptClientFrame(&tx, validate, NULL, error, sizeof error) && !tx.active &&
          MCGameplay_get(&game)->commitSerial == UINT64_MAX);
    CHECK(MCGameplay_free(&game));
    /* Exact packet classes prevent malformed casts and cross-direction queues. */
    setup(&game, true);
    CHECK(MCGameplay_begin(&game, &tx));
    wp = player(&tx.working, 0);
    NBTTagByte *small = NBTTagByte_new(tx.working.heap, 1);
    CHECK(small);
    CHECK(!MCGameplayClientPackets_addToSendQueue(wp, (MCObject *)small));
    CHECK(MCGameplay_abort(&tx));
    CHECK(MCGameplay_begin(&game, &tx));
    wp = player(&tx.working, 0);
    CHECK(!MCGameplayPackets_bind(wp));
    CHECK(MCGameplay_abort(&tx));
    CHECK(MCGameplay_free(&game));
    /* Even a native validator accepting a server-direction queue in a remote
       graph cannot
     * mistake client adoption for a durable server commitment. */
    setup(&game, true);
    MCGameplayPlayer *q = player(&game, 1);
    CHECK(MCGameplayPackets_bind(q) && MCGameplayPackets_sendConfirmTransaction(q, 0, 1, true));
    CHECK(MCGameplay_begin(&game, &tx));
    CHECK(MCGameplay_acceptClientFrame(&tx, mixed_queue_validation, NULL, error, sizeof error));
    s = (Sink){.game = &game};
    CHECK(MCGameplay_get(&game)->commitSerial == 1 && !MCGameplay_get(&game)->durableSerial &&
          MCGameplayPackets_flush(&game, 1, sink, &s) == MC_GAMEPLAY_PACKETS_UNCOMMITTED &&
          !s.calls);
    CHECK(MCGameplay_free(&game));
    /* A same-heap queue bound to a different native owner must fail preflight,
       rather than
     * adopt a graph whose packets can never pass the flush guard. */
    setup(&game, true);
    p = player(&game, 0);
    MCGameplayObjects *owners = MCGameplay_get(&game);
    MCGameplayObjects *other =
        (MCGameplayObjects *)MCObjectHeap_plainClone(game.heap, (MCObject *)owners);
    CHECK(other);
    p->pendingPackets = NULL;
    p->worldObj->owners = other;
    MCObjectHeap_touch(game.heap);
    CHECK(MCGameplayClientPackets_bind(p) &&
          MCGameplayClientPackets_addToSendQueue(
              p, (MCObject *)C0DPacketCloseWindow_new(game.heap, 2)));
    p->worldObj->owners = owners;
    MCObjectHeap_touch(game.heap);
    CHECK(MCGameplay_begin(&game, &tx));
    CHECK(!MCGameplay_acceptClientFrame(&tx, validate, NULL, error, sizeof error) && !tx.active &&
          !MCGameplay_get(&game)->commitSerial && !MCObjectHeap_failed(game.heap));
    CHECK(MCGameplay_free(&game));
    printf("gameplay client frame: %u checks\n", checks);
    return 0;
}
