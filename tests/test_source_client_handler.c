#include "client/network/NetHandlerPlayClient.h"
#include "network/GameplayPacketRouter.h"
#include "inventory/ContainerWorkbench.h"
#include "item/crafting/RecipeBookCloning.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "source client handler check %u at %d: %s\n", checks, __LINE__, #x);   \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
enum { THREAD = 1, PLAYER, SCREEN, SELECTED, TAB, CLOSE, SEND, ENTITY, WATCHER, UPDATE };
typedef struct {
    MCObject object;
    MCGameplayPlayer *player;
    MCObject *lastPacket;
    C0FPacketConfirmTransaction *sent[32];
    unsigned sentCount, eventCount;
    int events[128];
    MCPacketThreadResult thread;
    bool creativeScreen, failClose, failSend, poison;
    int32_t selectedTab, inventoryTab;
    DataWatcher *watcher;
    MCGameplayWorld *metadataWorld;
    int32_t trackedEntityId, notifiedId;
    bool failEntityLookup, failWatcher;
} Controller;
static void controller_trace(MCObject *o, MCObjectVisitor v, void *ctx) {
    Controller *c = (Controller *)o;
    c->player = (MCGameplayPlayer *)v((MCObject *)c->player, ctx);
    c->lastPacket = v(c->lastPacket, ctx);
    c->watcher = (DataWatcher *)v((MCObject *)c->watcher, ctx);
    c->metadataWorld = (MCGameplayWorld *)v((MCObject *)c->metadataWorld, ctx);
    for (unsigned i = 0; i < c->sentCount; i++)
        c->sent[i] = (C0FPacketConfirmTransaction *)v((MCObject *)c->sent[i], ctx);
}
static const MCObjectClass controller_class = {"fixture.client.required-controller",
                                               MCObjectHeap_plainClone, controller_trace, NULL};
/* These callbacks are explicit controller/thread/GUI/queue fixtures. The queue
   retains real C0F objects; they do not implement a production GUI or socket. */
static void event(Controller *c, int e) {
    CHECK(c->eventCount < 128);
    c->events[c->eventCount++] = e;
    CHECK(!MCObjectHeap_collect(c->object.heap));
    MCObjectHeap_touch(c->object.heap);
}
static MCPacketThreadResult thread_check(MCObject *ctx, NetHandlerPlayClient *h, MCObject *packet) {
    Controller *c = (Controller *)ctx;
    CHECK(h->gameController == (MCObject *)c);
    event(c, THREAD);
    c->lastPacket = packet;
    if (c->poison)
        MCObjectHeap_fail(c->object.heap);
    return c->thread;
}
static MCGameplayPlayer *get_player(MCObject *ctx, MCObject *controller) {
    Controller *c = (Controller *)ctx;
    CHECK(controller == ctx);
    event(c, PLAYER);
    return c->player;
}
static bool creative_screen(MCObject *ctx, MCObject *controller) {
    CHECK(ctx == controller);
    Controller *c = (Controller *)ctx;
    event(c, SCREEN);
    return c->creativeScreen;
}
static int32_t selected_tab(MCObject *ctx, MCObject *controller) {
    CHECK(ctx == controller);
    Controller *c = (Controller *)ctx;
    event(c, SELECTED);
    return c->selectedTab;
}
static int32_t inventory_tab(MCObject *ctx) {
    Controller *c = (Controller *)ctx;
    event(c, TAB);
    return c->inventoryTab;
}
static bool close_screen(MCObject *ctx, MCGameplayPlayer *p) {
    Controller *c = (Controller *)ctx;
    CHECK(p == c->player);
    event(c, CLOSE);
    if (c->failClose)
        return false;
    CHECK(InventoryPlayer_setItemStack(p->inventory, NULL));
    p->openContainer = p->inventoryContainer;
    MCObjectHeap_touch(p->living.entity.object.heap);
    return true;
}
static bool send_queue(MCObject *ctx, NetHandlerPlayClient *h,
                       C0FPacketConfirmTransaction *packet) {
    Controller *c = (Controller *)ctx;
    CHECK(h->gameController == ctx && packet && packet->object.heap == c->object.heap);
    event(c, SEND);
    CHECK(c->sentCount < 32);
    c->sent[c->sentCount++] = packet;
    return !c->failSend;
}
/* Explicit WorldClient/entity fixtures. The translated handler must read its
   own world field, not derive the entity world from Minecraft.thePlayer. */
static MCObject *get_entity(MCObject *ctx, MCGameplayWorld *world, int32_t id) {
    Controller *c = (Controller *)ctx;
    CHECK(world == c->metadataWorld);
    event(c, ENTITY);
    if (c->failEntityLookup)
        MCObjectHeap_fail(ctx->heap);
    return id == c->trackedEntityId ? world->owners->items[0] : NULL;
}
static DataWatcher *get_watcher(MCObject *ctx, MCObject *entity) {
    Controller *c = (Controller *)ctx;
    CHECK(entity == ctx);
    event(c, WATCHER);
    return c->failWatcher ? NULL : c->watcher;
}
static bool watcher_update(MCObject *ctx, MCObject *entity, int32_t id) {
    CHECK(ctx == entity);
    Controller *c = (Controller *)ctx;
    event(c, UPDATE);
    c->notifiedId = id;
    return true;
}
static const DataWatcherDependencies watcher_dependencies = {.onDataWatcherUpdate = watcher_update};
static const NetHandlerPlayClientDependencies dependencies = {
    .checkThreadAndEnqueue = thread_check,
    .getPlayer = get_player,
    .isCreativeScreen = creative_screen,
    .selectedCreativeTabIndex = selected_tab,
    .inventoryCreativeTabIndex = inventory_tab,
    .closeScreenAndDropStack = close_screen,
    .addToSendQueue = send_queue,
    .getEntityByID = get_entity,
    .getDataWatcher = get_watcher};
static NBTString *display(MCObject *world, const ItemStack *s) {
    (void)world;
    return NBTString_fromASCII(s->object.heap, "Book");
}
static ItemStack *recipe(InventoryCrafting *grid, MCObject *world) {
    return CraftingManager_findMatchingRecipe(((MCGameplayWorld *)world)->manager, grid, world,
                                              display, world);
}
static ItemStackArray *remaining(InventoryCrafting *grid, MCObject *world) {
    return CraftingManager_func_180303_b(((MCGameplayWorld *)world)->manager, grid, world);
}
static bool crafted(ItemStack *s, MCObject *w, MCObject *p, int32_t n) {
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
static bool drop(MCObject *p, ItemStack *s, bool scatter) {
    (void)p;
    (void)s;
    (void)scatter;
    CHECK(false);
    return false;
}
static bool false_item(const Item *i) {
    (void)i;
    return false;
}
static int32_t armor(const Item *i) {
    int n = ItemStack_registryId(i);
    return n >= 298 && n <= 317 ? (n - 298) % 4 : -1;
}
static const mc_crafting_dispatch crafting = {MCGameplayPlayer_inventory,
                                              MCGameplayPlayer_world,
                                              recipe,
                                              remaining,
                                              crafted,
                                              achievement,
                                              drop,
                                              false_item,
                                              false_item,
                                              false_item,
                                              false_item,
                                              armor,
                                              MCGameplayWorld_isRemote,
                                              MCGameplayWorld_isCraftingTable,
                                              MCGameplayPlayer_getDistanceSq};
static MCGameplayPlayer *setup(MCGameplay *game, MCObjectRootScope *scope, Controller **out,
                               NetHandlerPlayClient **handler) {
    CHECK(MCGameplay_init(game, 32 * 1024 * 1024));
    CHECK(MCObjectRootScope_begin(scope, game->heap));
    CraftingManager *m = CraftingManager_newEmpty(game->heap);
    CHECK(m);
    RecipeBookCloning *b = RecipeBookCloning_new(game->heap);
    CHECK(b);
    CHECK(CraftingManager_addRecipe(m, RecipeBookCloning_asRecipe(b)));
    MCGameplayWorld *w = MCGameplayWorld_new(game->heap, MCGameplay_get(game), NULL, m);
    CHECK(w);
    w->remote = true;
    CHECK(MCGameplay_setWorld(game, (MCObject *)w));
    MCGameplayPlayer *p =
        MCGameplayPlayer_new(w, NBTString_fromASCII(game->heap, "Player"), NULL, &crafting);
    CHECK(p);
    CHECK(MCGameplay_setPlayer(game, 0, "11111111-1111-1111-1111-111111111111", (MCObject *)p));
    Controller *c = (Controller *)MCObjectHeap_alloc(game->heap, sizeof(*c), &controller_class);
    CHECK(c);
    c->player = p;
    c->thread = MC_PACKET_THREAD_EXECUTE;
    c->inventoryTab = 11;
    *handler = NetHandlerPlayClient_nativeNew(p, (MCObject *)c, (MCObject *)c, &dependencies);
    CHECK(*handler && p->handler == (MCObject *)*handler);
    *out = c;
    return p;
}
static void finish(MCGameplay *g, MCObjectRootScope *s) {
    CHECK(!MCObjectHeap_failed(g->heap));
    MCObjectRootScope_end(s);
    CHECK(MCGameplay_free(g));
}
static void failed_finish(MCGameplay *g, MCObjectRootScope *s) {
    CHECK(MCObjectHeap_failed(g->heap));
    MCObjectRootScope_end(s);
    CHECK(!MCObjectHeap_hasBorrowers(g->heap));
    CHECK(MCGameplay_free(g));
}
static ItemStack *stack(MCGameplayPlayer *p, int32_t count) {
    ItemStack *s = ItemStack_new(p->living.entity.object.heap, ItemStack_registryItem(1), count, 0);
    CHECK(s);
    return s;
}
static S2FPacketSetSlot *slot_packet(MCGameplayPlayer *p, int32_t window, int32_t index,
                                     ItemStack *s) {
    S2FPacketSetSlot *packet = S2FPacketSetSlot_new_empty(p->living.entity.object.heap);
    CHECK(packet);
    packet->windowId = window;
    packet->slot = index;
    packet->item = s;
    return packet;
}
static S30PacketWindowItems *items_packet(MCGameplayPlayer *p, int32_t window, int32_t count) {
    S30PacketWindowItems *packet = S30PacketWindowItems_new_empty(p->living.entity.object.heap);
    CHECK(packet);
    packet->windowId = window;
    packet->itemStacks = ItemStackArray_new(p->living.entity.object.heap, count);
    CHECK(packet->itemStacks);
    return packet;
}
static ContainerWorkbench *bench(MCGameplayPlayer *p, int32_t window) {
    mc_crafting_position pos = {0, 0, 0};
    ContainerWorkbench *b =
        ContainerWorkbench_new(p->inventory, (MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj)), &pos, &crafting);
    CHECK(b);
    b->container.windowId = window;
    p->openContainer = &b->container;
    return b;
}
static ItemStack *at(Container *c, int index) {
    Slot *s = Container_getSlot(c, index);
    CHECK(s);
    return Slot_getStack(s);
}
static void reset_events(Controller *c) {
    c->eventCount = 0;
}
static void cursor_and_hotbar(void) {
    const int32_t counts[] = {INT32_MIN, -128, -1, 0, 1, 127, 128, INT32_MAX};
    for (unsigned n = 0; n < 8; n++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Controller *c;
        NetHandlerPlayClient *h;
        MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
        ContainerWorkbench *b = bench(p, 7);
        ItemStack *s = stack(p, counts[n]);
        s->animationsToGo = 17;
        S2FPacketSetSlot *packet = slot_packet(p, -1, INT32_MIN, s);
        CHECK(S2FPacketSetSlot_processPacket(packet, NetHandlerPlayClient_asHandler(h)));
        CHECK(InventoryPlayer_getItemStack(p->inventory) == s && s->animationsToGo == 17);
        CHECK(c->eventCount == 2 && c->events[0] == THREAD && c->events[1] == PLAYER);
        c->creativeScreen = true;
        c->selectedTab = 3;
        reset_events(c);
        packet = slot_packet(p, 0, 36, s);
        CHECK(NetHandlerPlayClient_handleSetSlot(h, packet));
        CHECK(at(p->inventoryContainer, 36) == s && at(&b->container, 37) == s &&
              s->animationsToGo == 5);
        CHECK(c->eventCount == 5 && c->events[2] == SCREEN && c->events[3] == SELECTED &&
              c->events[4] == TAB);
        ItemStack *equal = stack(p, counts[n]);
        equal->animationsToGo = 23;
        CHECK(NetHandlerPlayClient_handleSetSlot(h, slot_packet(p, 0, 36, equal)));
        CHECK(equal->animationsToGo == 23);
        ItemStack *larger = stack(p, counts[n] == INT32_MAX ? INT32_MIN : counts[n] + 1);
        larger->animationsToGo = 29;
        CHECK(NetHandlerPlayClient_handleSetSlot(h, slot_packet(p, 0, 36, larger)));
        CHECK(larger->animationsToGo == (counts[n] == INT32_MAX ? 29 : 5));
        CHECK(NetHandlerPlayClient_handleSetSlot(h, slot_packet(p, 0, 36, NULL)));
        CHECK(!at(p->inventoryContainer, 36));
        CHECK(NetHandlerPlayClient_handleSetSlot(h, slot_packet(p, 0, 44, s)));
        CHECK(at(p->inventoryContainer, 44) == s);
        finish(&g, &scope);
    }
}
static void routing_and_creative_predicates(void) {
    for (unsigned creative = 0; creative < 3; creative++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Controller *c;
        NetHandlerPlayClient *h;
        MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
        ItemStack *s = stack(p, 0);
        c->creativeScreen = creative != 0;
        c->selectedTab = creative == 2 ? 11 : 2;
        CHECK(NetHandlerPlayClient_handleSetSlot(h, slot_packet(p, 0, 9, s)));
        CHECK(at(p->openContainer, 9) == (creative == 1 ? NULL : s));
        CHECK(c->eventCount == (creative ? 5u : 3u));
        ContainerWorkbench *b = bench(p, 7);
        CHECK(NetHandlerPlayClient_handleSetSlot(h, slot_packet(p, 0, 10, s)));
        CHECK(!at(p->inventoryContainer, 10));
        CHECK(NetHandlerPlayClient_handleSetSlot(h, slot_packet(p, 7, 1, s)));
        CHECK(at(&b->container, 1) == s && s->animationsToGo == 0);
        CHECK(NetHandlerPlayClient_handleSetSlot(h, slot_packet(p, 6, INT32_MAX, s)));
        CHECK(NetHandlerPlayClient_handleSetSlot(h, slot_packet(p, -2, INT32_MAX, s)));
        CHECK(InventoryPlayer_getStackInSlot(p->inventory, 0) == NULL);
        b->container.windowId = -2;
        CHECK(NetHandlerPlayClient_handleSetSlot(h, slot_packet(p, -2, 37, s)));
        CHECK(InventoryPlayer_getStackInSlot(p->inventory, 0) == s);
        finish(&g, &scope);
    }
    MCGameplay g = {0};
    MCObjectRootScope scope = {0};
    Controller *c;
    NetHandlerPlayClient *h;
    MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
    c->creativeScreen = true;
    c->selectedTab = 0;
    CHECK(NetHandlerPlayClient_handleSetSlot(h, slot_packet(p, 0, INT32_MAX, NULL)));
    c->selectedTab = 11;
    CHECK(!NetHandlerPlayClient_handleSetSlot(h, slot_packet(p, 0, INT32_MAX, NULL)));
    failed_finish(&g, &scope);
}
static void window_lists_and_source_partial_failure(void) {
    MCGameplay g = {0};
    MCObjectRootScope scope = {0};
    Controller *c;
    NetHandlerPlayClient *h;
    MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
    ContainerWorkbench *b = bench(p, 257);
    c->creativeScreen = true;
    ItemStack *shared = stack(p, -1);
    S30PacketWindowItems *packet = items_packet(p, 257, 46);
    packet->itemStacks->items[1] = shared;
    packet->itemStacks->items[37] = shared;
    CHECK(S30PacketWindowItems_processPacket(packet, NetHandlerPlayClient_asHandler(h)));
    CHECK(at(&b->container, 1) == shared &&
          InventoryPlayer_getStackInSlot(p->inventory, 0) == shared);
    CHECK(c->eventCount == 2);
    packet = items_packet(p, 0, 45);
    packet->itemStacks->items[9] = shared;
    packet->itemStacks->items[36] = shared;
    reset_events(c);
    CHECK(NetHandlerPlayClient_handleWindowItems(h, packet));
    CHECK(InventoryPlayer_getStackInSlot(p->inventory, 9) == shared &&
          InventoryPlayer_getStackInSlot(p->inventory, 0) == shared &&
          at(&b->container, 1) == shared);
    CHECK(c->eventCount == 2);
    packet = items_packet(p, 256, 47);
    CHECK(NetHandlerPlayClient_handleWindowItems(h, packet));
    CHECK(InventoryPlayer_getStackInSlot(p->inventory, 0) == shared);
    packet = items_packet(p, 257, 0);
    CHECK(NetHandlerPlayClient_handleWindowItems(h, packet));
    CHECK(at(&b->container, 1) == shared);
    packet = items_packet(p, 257, 47);
    packet->itemStacks->items[1] = shared;
    CHECK(!NetHandlerPlayClient_handleWindowItems(h, packet));
    CHECK(at(&b->container, 1) == shared && !InventoryPlayer_getStackInSlot(p->inventory, 0));
    failed_finish(&g, &scope);
    g = (MCGameplay){0};
    scope = (MCObjectRootScope){0};
    p = setup(&g, &scope, &c, &h);
    packet = S30PacketWindowItems_new_empty(g.heap);
    CHECK(packet);
    CHECK(!NetHandlerPlayClient_handleWindowItems(h, packet));
    failed_finish(&g, &scope);
}
static void confirmations_close_and_thread_order(void) {
    const int32_t windows[] = {0, 7, -2, 256, INT32_MIN, INT32_MAX};
    for (unsigned n = 0; n < 6; n++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Controller *c;
        NetHandlerPlayClient *h;
        MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
        ContainerWorkbench *b = bench(p, windows[n]);
        S32PacketConfirmTransaction *a =
            S32PacketConfirmTransaction_new(g.heap, windows[n], INT16_MIN, false);
        CHECK(a);
        CHECK(S32PacketConfirmTransaction_processPacket(a, NetHandlerPlayClient_asHandler(h)));
        CHECK(c->sentCount == 1 && c->sent[0]->windowId == windows[n] &&
              c->sent[0]->uid == INT16_MIN && c->sent[0]->accepted);
        CHECK(c->eventCount == 3 && c->events[0] == THREAD && c->events[1] == PLAYER &&
              c->events[2] == SEND);
        CHECK(Container_getCanCraft(&b->container, (MCObject *)p));
        CHECK(NetHandlerPlayClient_handleConfirmTransaction(
            h, S32PacketConfirmTransaction_new(g.heap, windows[n], INT16_MAX, true)));
        CHECK(c->sentCount == 1);
        CHECK(NetHandlerPlayClient_handleConfirmTransaction(
            h, S32PacketConfirmTransaction_new(g.heap, 23, 0, false)));
        CHECK(c->sentCount == 1);
        CHECK(NetHandlerPlayClient_handleConfirmTransaction(
            h, S32PacketConfirmTransaction_new(g.heap, 0, INT16_MAX, false)));
        CHECK(c->sentCount == 2 && c->sent[1]->windowId == 0);
        ItemStack *s = stack(p, -1);
        CHECK(InventoryPlayer_setItemStack(p->inventory, s));
        CHECK(InventoryCrafting_setInventorySlotContents(b->craftMatrix, 0, s));
        reset_events(c);
        S2EPacketCloseWindow *close = S2EPacketCloseWindow_new(g.heap, 99);
        CHECK(close);
        CHECK(S2EPacketCloseWindow_processPacket(close, NetHandlerPlayClient_asHandler(h)));
        CHECK(c->eventCount == 3 && c->events[2] == CLOSE &&
              !InventoryPlayer_getItemStack(p->inventory) &&
              p->openContainer == p->inventoryContainer);
        CHECK(InventoryCrafting_getStackInSlot(b->craftMatrix, 0) == s && c->sentCount == 2);
        finish(&g, &scope);
    }
    for (unsigned op = 0; op < 4; op++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Controller *c;
        NetHandlerPlayClient *h;
        MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
        c->thread = MC_PACKET_THREAD_QUEUED;
        ItemStack *s = stack(p, 0);
        CHECK(InventoryPlayer_setItemStack(p->inventory, s));
        bool ok =
            op == 0 ? NetHandlerPlayClient_handleCloseWindow(h, S2EPacketCloseWindow_new(g.heap, 0))
            : op == 1 ? NetHandlerPlayClient_handleSetSlot(h, slot_packet(p, -1, 0, NULL))
            : op == 2 ? NetHandlerPlayClient_handleWindowItems(h, items_packet(p, 0, 45))
                      : NetHandlerPlayClient_handleConfirmTransaction(
                            h, S32PacketConfirmTransaction_new(g.heap, 0, 1, false));
        CHECK(ok && c->eventCount == 1 && c->events[0] == THREAD && c->sentCount == 0 &&
              InventoryPlayer_getItemStack(p->inventory) == s);
        finish(&g, &scope);
    }
}
static void replacement_and_snapshot_aliases(void) {
    MCGameplay g = {0};
    MCObjectRootScope scope = {0};
    Controller *c;
    NetHandlerPlayClient *h;
    MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
    MCGameplayPlayer *replacement = MCGameplayPlayer_new(
        ((MCGameplayWorld *)(p->living.entity.worldObj)), NBTString_fromASCII(g.heap, "Replacement"), NULL, &crafting);
    CHECK(replacement);
    CHECK(MCGameplay_setPlayer(&g, 1, "22222222-2222-2222-2222-222222222222",
                               (MCObject *)replacement));
    c->player = replacement;
    ItemStack *s = stack(p, 0);
    S2FPacketSetSlot *packet = slot_packet(p, -1, 0, s);
    CHECK(NetHandlerPlayClient_handleSetSlot(h, packet));
    CHECK(!InventoryPlayer_getItemStack(p->inventory) &&
          InventoryPlayer_getItemStack(replacement->inventory) == s);
    CHECK(p->handler == (MCObject *)h);
    MCObjectRootScope_end(&scope);
    MCGameplayTransaction tx = {0};
    CHECK(MCGameplay_begin(&g, &tx));
    CHECK(MCObjectRootScope_begin(&scope, tx.working.heap));
    MCGameplayPlayer *copy = (MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
    NetHandlerPlayClient *ch = (NetHandlerPlayClient *)copy->handler;
    Controller *cc = (Controller *)ch->gameController;
    MCGameplayPlayer *cr = (MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[1];
    CHECK(cc != c && cc->player == cr && ch->dependencyContext == (MCObject *)cc &&
          cc->lastPacket != (MCObject *)packet);
    CHECK(((S2FPacketSetSlot *)cc->lastPacket)->item ==
          InventoryPlayer_getItemStack(cr->inventory));
    CHECK(NetHandlerPlayClient_handleConfirmTransaction(
        ch, S32PacketConfirmTransaction_new(tx.working.heap, 0, -1, false)));
    CHECK(cc->sentCount == 1 && c->sentCount == 0);
    CHECK(cc->sent[0]->object.heap == tx.working.heap);
    CHECK(NetHandlerPlayClient_handleSetSlot(
        ch, slot_packet(cr, 0, 36, InventoryPlayer_getItemStack(cr->inventory))));
    CHECK(InventoryPlayer_getStackInSlot(cr->inventory, 0) ==
              InventoryPlayer_getItemStack(cr->inventory) &&
          InventoryPlayer_getStackInSlot(replacement->inventory, 0) == NULL);
    MCObjectRootScope_end(&scope);
    CHECK(MCGameplay_abort(&tx));
    CHECK(MCObjectRootScope_begin(&scope, g.heap));
    CHECK(c->sentCount == 0 && InventoryPlayer_getStackInSlot(replacement->inventory, 0) == NULL);
    finish(&g, &scope);
}
static void dependency_failures(void) {
    for (unsigned failure = 0; failure < 9; failure++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Controller *c;
        NetHandlerPlayClient *h;
        MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
        if (failure == 0) {
            c->thread = MC_PACKET_THREAD_FAILED;
            CHECK(!NetHandlerPlayClient_handleCloseWindow(h, S2EPacketCloseWindow_new(g.heap, 0)));
            CHECK(c->eventCount == 1);
        } else if (failure == 1) {
            c->thread = (MCPacketThreadResult)99;
            CHECK(!NetHandlerPlayClient_handleCloseWindow(h, S2EPacketCloseWindow_new(g.heap, 0)));
            CHECK(c->eventCount == 1);
        } else if (failure == 2) {
            c->poison = true;
            CHECK(!NetHandlerPlayClient_handleCloseWindow(h, S2EPacketCloseWindow_new(g.heap, 0)));
            CHECK(c->eventCount == 1);
        } else if (failure == 3) {
            c->player = NULL;
            CHECK(!NetHandlerPlayClient_handleCloseWindow(h, S2EPacketCloseWindow_new(g.heap, 0)));
            CHECK(c->eventCount == 2);
        } else if (failure == 4) {
            c->failClose = true;
            CHECK(!NetHandlerPlayClient_handleCloseWindow(h, S2EPacketCloseWindow_new(g.heap, 0)));
            CHECK(c->eventCount == 3);
        } else if (failure == 5) {
            c->failSend = true;
            CHECK(!NetHandlerPlayClient_handleConfirmTransaction(
                h, S32PacketConfirmTransaction_new(g.heap, 0, 0, false)));
            CHECK(c->sentCount == 1);
        } else if (failure == 6) {
            NetHandlerPlayClientDependencies d = dependencies;
            d.closeScreenAndDropStack = NULL;
            CHECK(!NetHandlerPlayClient_nativeNew(p, (MCObject *)c, (MCObject *)c, &d));
        } else if (failure == 7) {
            ((MCGameplayWorld *)(p->living.entity.worldObj))->remote = false;
            CHECK(!NetHandlerPlayClient_nativeNew(p, (MCObject *)c, (MCObject *)c, &dependencies));
        } else {
            CHECK(!NetHandlerPlayClient_handleSetSlot(h, NULL));
            CHECK(c->eventCount == 0);
        }
        failed_finish(&g, &scope);
    }
    for (unsigned op = 0; op < 3; op++) {
        MCGameplay g = {0}, other = {0};
        MCObjectRootScope scope = {0}, otherScope = {0};
        Controller *c, *oc;
        NetHandlerPlayClient *h, *oh;
        MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
        MCGameplayPlayer *foreign = setup(&other, &otherScope, &oc, &oh);
        if (op == 0)
            CHECK(!NetHandlerPlayClient_nativeNew(p, (MCObject *)oc, (MCObject *)c, &dependencies));
        else if (op == 1) {
            c->player = foreign;
            CHECK(!NetHandlerPlayClient_handleCloseWindow(h, S2EPacketCloseWindow_new(g.heap, 0)));
            c->player = p;
        } else
            CHECK(!NetHandlerPlayClient_handleSetSlot(h, slot_packet(foreign, -1, 0, NULL)));
        CHECK(!MCObjectHeap_failed(other.heap));
        failed_finish(&g, &scope);
        finish(&other, &otherScope);
    }
    MCGameplay g = {0};
    MCObjectRootScope scope = {0};
    Controller *c;
    NetHandlerPlayClient *h;
    setup(&g, &scope, &c, &h);
    INetHandlerPlayClient wrong = NetHandlerPlayClient_asHandler(h);
    wrong.instance = (MCObject *)c;
    CHECK(!S2EPacketCloseWindow_processPacket(S2EPacketCloseWindow_new(g.heap, 0), wrong));
    CHECK(c->eventCount == 0);
    failed_finish(&g, &scope);
}
static void packet_codec_to_source_inventory(void) {
    const int32_t counts[] = {INT32_MIN, -128, -1, 0, 1, 127, 128, INT32_MAX};
    const int32_t narrowed[] = {0, -128, -1, 0, 1, 127, -128, -1};
    MCGameplay g = {0};
    MCObjectRootScope scope = {0};
    Controller *c;
    NetHandlerPlayClient *h;
    MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
    mc_buf buffer = {0};
    PacketBuffer io;
    CHECK(PacketBuffer_init(&io, g.heap, &buffer));
    for (unsigned n = 0; n < 8; n++) {
        ItemStack *source = ItemStack_new(g.heap, ItemStack_registryItem(387), counts[n], 7);
        CHECK(source);
        NBTTagCompound *tag = NBTTagCompound_new(g.heap);
        CHECK(tag && NBTTagCompound_setInteger_ascii(tag, "generation", 0));
        CHECK(NBTTagCompound_setString_ascii(tag, "title", NBTString_fromUTF8(g.heap, "原本の本")));
        CHECK(ItemStack_setTagCompound(source, tag));
        S2FPacketSetSlot *sent = S2FPacketSetSlot_new(g.heap, -1, -1, source);
        CHECK(sent && sent->item != source && sent->item->stackTagCompound != tag);
        CHECK(S2FPacketSetSlot_writePacketData(sent, &io));
        S2FPacketSetSlot *received = S2FPacketSetSlot_new_empty(g.heap);
        CHECK(received && S2FPacketSetSlot_readPacketData(received, &io));
        CHECK(received->item && received->item->stackSize == narrowed[n] &&
              received->item != sent->item);
        CHECK(S2FPacketSetSlot_processPacket(received, NetHandlerPlayClient_asHandler(h)));
        CHECK(InventoryPlayer_getItemStack(p->inventory) == received->item &&
              received->item->stackTagCompound != sent->item->stackTagCompound);
        mc_buf_clear(&buffer);
        reset_events(c);
    }
    CHECK(NetHandlerPlayClient_handleConfirmTransaction(
        h, S32PacketConfirmTransaction_new(g.heap, 0, INT16_MIN, false)));
    CHECK(c->sentCount == 1 && C0FPacketConfirmTransaction_writePacketData(c->sent[0], &io));
    CHECK(buffer.len == 4 && buffer.data[0] == 0 && buffer.data[1] == 0x80 && buffer.data[2] == 0 &&
          buffer.data[3] == 1);
    C0FPacketConfirmTransaction *reply = C0FPacketConfirmTransaction_new_empty(g.heap);
    CHECK(reply && C0FPacketConfirmTransaction_readPacketData(reply, &io));
    CHECK(reply->windowId == 0 && reply->uid == INT16_MIN && reply->accepted);
    mc_buf_free(&buffer);
    finish(&g, &scope);
}
static void routed_wire_snapshots(void) {
    const int32_t ids[] = {0x2e, 0x2f, 0x30, 0x32};
    for (unsigned op = 0; op < 4; op++)
        for (unsigned malformed = 0; malformed < 3; malformed++) {
            MCGameplay g = {0};
            MCObjectRootScope scope = {0};
            Controller *c;
            NetHandlerPlayClient *h;
            MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
            ItemStack *original = stack(p, -1);
            CHECK(InventoryPlayer_setItemStack(p->inventory, original));
            mc_buf payload = {0};
            PacketBuffer io;
            CHECK(PacketBuffer_init(&io, g.heap, &payload));
            if (op == 0)
                CHECK(S2EPacketCloseWindow_writePacketData(S2EPacketCloseWindow_new(g.heap, 123),
                                                           &io));
            else if (op == 1)
                CHECK(S2FPacketSetSlot_writePacketData(
                    S2FPacketSetSlot_new(g.heap, -1, -1, original), &io));
            else if (op == 2) {
                S30PacketWindowItems *packet = items_packet(p, 0, 45);
                packet->itemStacks->items[9] = original;
                packet->itemStacks->items[36] = original;
                CHECK(S30PacketWindowItems_writePacketData(packet, &io));
            } else
                CHECK(S32PacketConfirmTransaction_writePacketData(
                    S32PacketConfirmTransaction_new(g.heap, 0, 17, false), &io));
            if (malformed == 1)
                --payload.len;
            if (malformed == 2)
                mc_put_u8(&payload, 42);
            CHECK(GameplayPacketRouter_client(&g, 0, ids[op], &payload) ==
                      MC_GAMEPLAY_PACKET_FAILED &&
                  payload.pos == 0);
            CHECK(GameplayPacketRouter_client(&g, 0, -1, &payload) ==
                      MC_GAMEPLAY_PACKET_NOT_HANDLED &&
                  payload.pos == 0);
            MCObjectRootScope_end(&scope);
            MCGameplayTransaction tx = {0};
            CHECK(MCGameplay_begin(&g, &tx));
            CHECK(GameplayPacketRouter_client(&tx.working, 0, ids[op], &payload) ==
                  (malformed ? MC_GAMEPLAY_PACKET_REJECTED : MC_GAMEPLAY_PACKET_APPLIED));
            CHECK(!MCObjectHeap_hasBorrowers(tx.working.heap));
            MCGameplayPlayer *wp = (MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
            Controller *wc = (Controller *)((NetHandlerPlayClient *)wp->handler)->gameController;
            CHECK(c->eventCount == 0 && c->sentCount == 0 &&
                  InventoryPlayer_getItemStack(p->inventory) == original);
            if (malformed)
                CHECK(wc->eventCount == 0 && wc->sentCount == 0 &&
                      InventoryPlayer_getItemStack(wp->inventory)->stackSize == -1);
            else if (op == 0)
                CHECK(wc->eventCount == 3 && !InventoryPlayer_getItemStack(wp->inventory));
            else if (op == 1)
                CHECK(wc->eventCount == 2 &&
                      InventoryPlayer_getItemStack(wp->inventory)->stackSize == -1 &&
                      InventoryPlayer_getItemStack(wp->inventory) != original);
            else if (op == 2) {
                ItemStack *a = InventoryPlayer_getStackInSlot(wp->inventory, 9),
                          *b = InventoryPlayer_getStackInSlot(wp->inventory, 0);
                CHECK(a && b && a != b && a->stackSize == -1 && b->stackSize == -1 &&
                      wc->eventCount == 2);
            } else
                CHECK(wc->sentCount == 1 && wc->sent[0]->accepted && wc->sent[0]->uid == 17);
            CHECK(MCGameplay_abort(&tx));
            CHECK(!MCObjectHeap_failed(g.heap));
            CHECK(MCObjectRootScope_begin(&scope, g.heap));
            mc_buf_free(&payload);
            finish(&g, &scope);
        }
}
static void metadata_owner(MCGameplay *game, MCGameplayPlayer *p, Controller *c,
                           NetHandlerPlayClient *h, ItemStack *initial) {
    c->metadataWorld = h->clientWorldController;
    c->trackedEntityId = INT32_MIN;
    CHECK(MCGameplay_addItem(game, (MCObject *)c));
    c->watcher =
        DataWatcher_new(p->living.entity.object.heap, (MCObject *)c, &watcher_dependencies, (MCObject *)c);
    CHECK(c->watcher && DataWatcher_addObjectByDataType(c->watcher, 10, 5));
    WatchableObject *watched = DataWatcher_nativeGetWatchedObject(c->watcher, 10);
    CHECK(watched && WatchableObject_setObject(watched, (MCObject *)initial));
    WatchableObject_setWatched(watched, false);
    DataWatcher_func_111144_e(c->watcher);
    c->eventCount = 0;
}
static S1CPacketEntityMetadata *metadata_packet(MCObjectHeap *heap, int32_t id, ItemStack *value) {
    S1CPacketEntityMetadata *packet = S1CPacketEntityMetadata_new_empty(heap);
    CHECK(packet);
    packet->entityId = id;
    packet->field_149378_b = WatchableObjectList_new(heap);
    CHECK(packet->field_149378_b);
    WatchableObject *entry = WatchableObject_new(heap, 5, 10, (MCObject *)value);
    CHECK(entry && WatchableObjectList_add(packet->field_149378_b, entry));
    return packet;
}
static void metadata_source_references_and_order(void) {
    const int32_t counts[] = {-1, 0, 1, 127};
    for (size_t n = 0; n < sizeof counts / sizeof counts[0]; n++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Controller *c;
        NetHandlerPlayClient *h;
        MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
        ItemStack *old = stack(p, 17), *incoming = stack(p, counts[n]);
        CHECK(InventoryPlayer_setInventorySlotContents(p->inventory, 0, old));
        CHECK(InventoryPlayer_setInventorySlotContents(p->inventory, 9, incoming));
        metadata_owner(&g, p, c, h, old);
        /* This packet handler reads WorldClient even when the controller has
           no current player. The original does not read Minecraft.thePlayer. */
        c->player = NULL;
        S1CPacketEntityMetadata *packet = metadata_packet(g.heap, INT32_MIN, incoming);
        WatchableObject *unknown = WatchableObject_new(g.heap, 5, 11, (MCObject *)old);
        CHECK(unknown && WatchableObjectList_add(packet->field_149378_b, unknown));
        CHECK(S1CPacketEntityMetadata_processPacket(packet, NetHandlerPlayClient_asHandler(h)));
        CHECK(c->eventCount == 4 && c->events[0] == THREAD && c->events[1] == ENTITY &&
              c->events[2] == WATCHER && c->events[3] == UPDATE && c->notifiedId == 10);
        CHECK(DataWatcher_getWatchableObjectItemStack(c->watcher, 10) == incoming &&
              InventoryPlayer_getStackInSlot(p->inventory, 0) == old &&
              InventoryPlayer_getStackInSlot(p->inventory, 9) == incoming);
        CHECK(DataWatcher_hasObjectChanged(c->watcher));
        CHECK(!WatchableObject_isWatched(DataWatcher_nativeGetWatchedObject(c->watcher, 10)));
        incoming->stackSize = -7;
        CHECK(DataWatcher_getWatchableObjectItemStack(c->watcher, 10)->stackSize == -7 &&
              InventoryPlayer_getStackInSlot(p->inventory, 9)->stackSize == -7);
        reset_events(c);
        CHECK(NetHandlerPlayClient_handleEntityMetadata(h, packet));
        CHECK(c->eventCount == 4); /* Identical value still notifies the entity. */
        CHECK(!DataWatcher_getChanged(c->watcher) && !DataWatcher_hasObjectChanged(c->watcher));
        reset_events(c);
        packet->entityId = 42;
        CHECK(NetHandlerPlayClient_handleEntityMetadata(h, packet));
        CHECK(c->eventCount == 2 && c->events[1] == ENTITY);
        reset_events(c);
        packet->entityId = INT32_MIN;
        packet->field_149378_b = NULL;
        CHECK(NetHandlerPlayClient_handleEntityMetadata(h, packet));
        CHECK(c->eventCount == 2 && c->events[1] == ENTITY);
        reset_events(c);
        packet->field_149378_b = WatchableObjectList_new(g.heap);
        CHECK(packet->field_149378_b && NetHandlerPlayClient_handleEntityMetadata(h, packet));
        CHECK(c->eventCount == 3 && c->events[2] == WATCHER &&
              DataWatcher_hasObjectChanged(c->watcher));
        finish(&g, &scope);
    }
}
static void metadata_thread_and_failures(void) {
    for (int failure = 0; failure < 6; failure++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Controller *c;
        NetHandlerPlayClient *h;
        MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
        ItemStack *old = stack(p, 1), *incoming = stack(p, 2);
        metadata_owner(&g, p, c, h, old);
        S1CPacketEntityMetadata *packet = metadata_packet(g.heap, INT32_MIN, incoming);
        NetHandlerPlayClientDependencies d = dependencies;
        h->dependencies = &d;
        if (failure == 0)
            c->thread = MC_PACKET_THREAD_QUEUED;
        if (failure == 1)
            h->clientWorldController = NULL;
        if (failure == 2)
            d.getEntityByID = NULL;
        if (failure == 3)
            c->failEntityLookup = true;
        if (failure == 4)
            c->failWatcher = true;
        if (failure == 5)
            d.getDataWatcher = NULL;
        CHECK(NetHandlerPlayClient_handleEntityMetadata(h, packet) == (failure == 0));
        CHECK(DataWatcher_getWatchableObjectItemStack(c->watcher, 10) == old);
        CHECK(!DataWatcher_hasObjectChanged(c->watcher));
        CHECK(c->eventCount == (failure < 3 ? 1 : failure == 3 || failure == 5 ? 2 : 3));
        if (!failure)
            finish(&g, &scope);
        else
            failed_finish(&g, &scope);
    }
}
static void metadata_wire_graph_rollback(void) {
    for (int malformed = 0; malformed < 3; malformed++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Controller *c;
        NetHandlerPlayClient *h;
        MCGameplayPlayer *p = setup(&g, &scope, &c, &h);
        ItemStack *old = stack(p, 1), *incoming = stack(p, -1);
        CHECK(InventoryPlayer_setInventorySlotContents(p->inventory, 0, old));
        metadata_owner(&g, p, c, h, old);
        S1CPacketEntityMetadata *packet = metadata_packet(g.heap, INT32_MIN, incoming);
        mc_buf payload;
        mc_buf_init(&payload);
        PacketBuffer view;
        CHECK(PacketBuffer_init(&view, g.heap, &payload));
        CHECK(S1CPacketEntityMetadata_writePacketData(packet, &view));
        if (malformed == 1)
            --payload.len;
        if (malformed == 2)
            mc_put_u8(&payload, 42);
        MCObjectRootScope_end(&scope);
        MCGameplayTransaction tx = {0};
        CHECK(MCGameplay_begin(&g, &tx));
        GameplayPacketResult routed = GameplayPacketRouter_client(&tx.working, 0, 0x1c, &payload);
        GameplayPacketResult expected =
            malformed ? MC_GAMEPLAY_PACKET_REJECTED : MC_GAMEPLAY_PACKET_APPLIED;
        if (routed != expected)
            fprintf(stderr,
                    "metadata route case=%d result=%d expected=%d pos=%zu/%zu bufferFail=%d "
                    "heapFail=%d\n",
                    malformed, routed, expected, payload.pos, payload.len, payload.failed,
                    MCObjectHeap_failed(tx.working.heap));
        CHECK(routed == expected);
        CHECK(!MCObjectHeap_hasBorrowers(tx.working.heap));
        MCGameplayPlayer *wp = (MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
        NetHandlerPlayClient *wh = (NetHandlerPlayClient *)wp->handler;
        Controller *wc = (Controller *)wh->gameController;
        CHECK(wh->clientWorldController == ((MCGameplayWorld *)(wp->living.entity.worldObj)) && wc->metadataWorld == ((MCGameplayWorld *)(wp->living.entity.worldObj)));
        CHECK(c->eventCount == 0 &&
              DataWatcher_getWatchableObjectItemStack(c->watcher, 10) == old &&
              InventoryPlayer_getStackInSlot(p->inventory, 0) == old);
        if (malformed) {
            CHECK(wc->eventCount == 0 && !DataWatcher_hasObjectChanged(wc->watcher));
            CHECK(DataWatcher_getWatchableObjectItemStack(wc->watcher, 10) ==
                  InventoryPlayer_getStackInSlot(wp->inventory, 0));
        } else {
            S1CPacketEntityMetadata *decoded = (S1CPacketEntityMetadata *)wc->lastPacket;
            MCObject *value =
                WatchableObject_getObject(WatchableObjectList_get(decoded->field_149378_b, 0));
            CHECK(wc->eventCount == 4 && value != (MCObject *)incoming &&
                  value->heap == tx.working.heap);
            CHECK(DataWatcher_getWatchableObjectItemStack(wc->watcher, 10) == (ItemStack *)value &&
                  ((ItemStack *)value)->stackSize == -1);
        }
        CHECK(MCGameplay_abort(&tx));
        CHECK(!MCObjectHeap_failed(g.heap));
        CHECK(MCObjectRootScope_begin(&scope, g.heap));
        mc_buf_free(&payload);
        finish(&g, &scope);
    }
}
int main(void) {
    cursor_and_hotbar();
    routing_and_creative_predicates();
    window_lists_and_source_partial_failure();
    confirmations_close_and_thread_order();
    replacement_and_snapshot_aliases();
    dependency_failures();
    packet_codec_to_source_inventory();
    routed_wire_snapshots();
    metadata_source_references_and_order();
    metadata_thread_and_failures();
    metadata_wire_graph_rollback();
    printf("source client handler: %u checks passed\n", checks);
    return 0;
}
