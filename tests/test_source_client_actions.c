#include "client/entity/EntityPlayerSP.h"
#include "client/multiplayer/PlayerControllerMP.h"
#include "inventory/ContainerWorkbench.h"
#include "stats/StatFileWriter.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "client actions %u at %d: %s\n", checks, __LINE__, #x);                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
enum { GET_PLAYER = 1, QUEUE, GUI, DROP, USE };
typedef struct {
    MCObject object;
    MCGameplayPlayer *player;
    EntityPlayerSP *sp;
    PlayerControllerMP *controller;
    Container *screenContainer;
    DataWatcherBlockPos *origin;
    MCObject *packets[32];
    ItemStack *drops[8], *useReturn;
    int events[64];
    unsigned packetCount, eventCount, dropCount;
    bool failSend, failGUI, screenOpen, changeCount, failUse;
} Fixture;
static void fixture_trace(MCObject *o, MCObjectVisitor v, void *ctx) {
    Fixture *f = (Fixture *)o;
    f->player = (MCGameplayPlayer *)v((MCObject *)f->player, ctx);
    f->sp = (EntityPlayerSP *)v((MCObject *)f->sp, ctx);
    f->controller = (PlayerControllerMP *)v((MCObject *)f->controller, ctx);
    f->screenContainer = (Container *)v((MCObject *)f->screenContainer, ctx);
    f->origin = (DataWatcherBlockPos *)v((MCObject *)f->origin, ctx);
    f->useReturn = (ItemStack *)v((MCObject *)f->useReturn, ctx);
    for (unsigned i = 0; i < f->packetCount; i++)
        f->packets[i] = v(f->packets[i], ctx);
    for (unsigned i = 0; i < f->dropCount; i++)
        f->drops[i] = (ItemStack *)v((MCObject *)f->drops[i], ctx);
}
static const MCObjectClass fixture_class = {"fixture.client.actions", MCObjectHeap_plainClone,
                                            fixture_trace, NULL};
static void event(Fixture *f, int id) {
    CHECK(f->eventCount < 64);
    f->events[f->eventCount++] = id;
    CHECK(!MCObjectHeap_collect(f->object.heap));
}
/* Explicit external thread/GUI/send fixtures. Queues retain actual source
   packet objects and GUI close invokes the retained source Container. */
static MCPacketThreadResult thread(MCObject *ctx, NetHandlerPlayClient *h, MCObject *p) {
    (void)ctx;
    (void)h;
    (void)p;
    CHECK(false);
    return MC_PACKET_THREAD_FAILED;
}
static MCGameplayPlayer *get_player(MCObject *ctx, MCObject *mc) {
    CHECK(ctx == mc);
    Fixture *f = (Fixture *)ctx;
    event(f, GET_PLAYER);
    return f->player;
}
static bool false_screen(MCObject *ctx, MCObject *mc) {
    (void)ctx;
    (void)mc;
    CHECK(false);
    return false;
}
static int32_t no_tab(MCObject *ctx, MCObject *mc) {
    (void)ctx;
    (void)mc;
    CHECK(false);
    return 0;
}
static int32_t no_inventory_tab(MCObject *ctx) {
    (void)ctx;
    CHECK(false);
    return 0;
}
static bool no_close(MCObject *ctx, MCGameplayPlayer *p) {
    (void)ctx;
    (void)p;
    CHECK(false);
    return false;
}
static bool no_confirm(MCObject *ctx, NetHandlerPlayClient *h, C0FPacketConfirmTransaction *p) {
    (void)ctx;
    (void)h;
    (void)p;
    CHECK(false);
    return false;
}
static const NetHandlerPlayClientDependencies handler_dependencies = {
    .checkThreadAndEnqueue = thread,
    .getPlayer = get_player,
    .isCreativeScreen = false_screen,
    .selectedCreativeTabIndex = no_tab,
    .inventoryCreativeTabIndex = no_inventory_tab,
    .closeScreenAndDropStack = no_close,
    .addToSendQueue = no_confirm};
static bool queue_any(MCObject *ctx, NetHandlerPlayClient *h, MCObject *p) {
    Fixture *f = (Fixture *)ctx;
    CHECK(h->gameController == ctx && p && p->heap == ctx->heap);
    event(f, QUEUE);
    CHECK(f->packetCount < 32);
    f->packets[f->packetCount++] = p;
    return !f->failSend;
}
static bool queue_click(MCObject *ctx, NetHandlerPlayClient *h, C0EPacketClickWindow *p) {
    return queue_any(ctx, h, (MCObject *)p);
}
static bool queue_held(MCObject *ctx, NetHandlerPlayClient *h, C09PacketHeldItemChange *p) {
    return queue_any(ctx, h, (MCObject *)p);
}
static bool queue_creative(MCObject *ctx, NetHandlerPlayClient *h,
                           C10PacketCreativeInventoryAction *p) {
    return queue_any(ctx, h, (MCObject *)p);
}
static bool queue_placement(MCObject *ctx, NetHandlerPlayClient *h,
                            C08PacketPlayerBlockPlacement *p) {
    return queue_any(ctx, h, (MCObject *)p);
}
static DataWatcherBlockPos *origin(MCObject *ctx) { return ((Fixture *)ctx)->origin; }
static bool display_null(MCObject *ctx, MCObject *mc) {
    CHECK(ctx == mc);
    Fixture *f = (Fixture *)ctx;
    event(f, GUI);
    CHECK(!InventoryPlayer_getItemStack(f->player->inventory));
    CHECK(f->player->openContainer == f->player->inventoryContainer);
    if (f->failGUI)
        return false;
    if (f->screenOpen && f->screenContainer) {
        CHECK(Container_onContainerClosed(f->screenContainer, f->player->inventory));
        f->screenOpen = false;
    }
    return true;
}
static const EntityPlayerSPDependencies sp_dependencies = {queue_any, origin, display_null};
static NativeGameProfile *profile(MCObject *context,NetHandlerPlayClient *handler) {
    (void)context;return NetHandlerPlayClient_getGameProfile(handler);
}
static const PlayerControllerMPDependencies click_dependencies = {queue_click};
static bool use_item(MCObject *ctx, const Item *item, ItemStack *s, MCObject *world,
                     MCObject *player, ItemStack **out) {
    Fixture *f = (Fixture *)ctx;
    event(f, USE);
    CHECK(item == s->item && world == (MCObject *)((MCGameplayWorld *)(f->player->living.entity.worldObj)) &&
          player == (MCObject *)f->player);
    if (f->changeCount)
        --s->stackSize;
    *out = f->useReturn;
    return !f->failUse;
}
static const ItemStackUseDependencies item_use = {use_item};
static const PlayerControllerMPActionsDependencies actions = {
    get_player, queue_held, queue_creative, queue_placement, &item_use};
static ItemStack *recipe(InventoryCrafting *grid, MCObject *w) {
    (void)grid;
    (void)w;
    return NULL;
}
static ItemStackArray *remaining(InventoryCrafting *grid, MCObject *w) {
    (void)w;
    return ItemStackArray_new(grid->object.heap, InventoryCrafting_getSizeInventory(grid));
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
    MCGameplayPlayer *actor = (MCGameplayPlayer *)p;
    Fixture *f = (Fixture *)actor->effects;
    CHECK(!scatter && s && f->dropCount < 8);
    event(f, DROP);
    f->drops[f->dropCount++] = s;
    return EntityPlayerSP_joinEntityItemWithWorld(f->sp, NULL);
}
static bool false_item(const Item *i) {
    (void)i;
    return false;
}
static int32_t armor(const Item *i) {
    (void)i;
    return -1;
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
static Fixture *setup(MCGameplay *g, MCObjectRootScope *scope) {
    CHECK(MCGameplay_init(g, 32 * 1024 * 1024));
    CHECK(MCObjectRootScope_begin(scope, g->heap));
    CraftingManager *m = CraftingManager_newEmpty(g->heap);
    CHECK(m);
    MCGameplayWorld *w = MCGameplayWorld_new(g->heap, MCGameplay_get(g), NULL, m);
    CHECK(w);
    w->remote = true;
    CHECK(MCGameplay_setWorld(g, (MCObject *)w));
    StatFileWriter *stats = StatFileWriter_new(g->heap);
    CHECK(stats);
    Fixture *f = (Fixture *)MCObjectHeap_alloc(g->heap, sizeof(*f), &fixture_class);
    CHECK(f);
    NativeGameProfile *identity=NativeGameProfile_new(g->heap,NULL,NBTString_fromASCII(g->heap,"Player"));
    CHECK(identity);
    NetHandlerPlayClient *h=NetHandlerPlayClient_nativeBootstrap(g->heap,identity,(MCObject *)f,(MCObject *)f,&handler_dependencies);
    CHECK(h);
    f->sp=EntityPlayerSP_nativeAllocate(g->heap);CHECK(f->sp);
    MCGameplayPlayer *p=EntityPlayerSP_asPlayer(f->sp);
    f->player=p;p->effects=(MCObject *)f;
    const EntityPlayerSPConstructorDependencies constructors={MCGameplayPlayer_nativeConstructorDependencies(),profile};
    CHECK(EntityPlayerSP_construct(f->sp,(MCObject *)f,(MCObject *)w,h,stats,&constructors,
        &crafting,(MCObject *)p,w->randomRuntime,NativeEntityIDRuntime_process()));
    CHECK(MCGameplayPlayer_nativeAttachEnvironment(p,stats)&&NetHandlerPlayClient_nativeBindPlayer(h,p));
    CHECK(EntityPlayerSP_bindActions(f->sp,(MCObject *)f,&sp_dependencies));
    CHECK((MCObject *)f->sp==(MCObject *)p&&p->handler==(MCObject *)h&&f->sp->sendQueue==h);
    CHECK(MCGameplay_setPlayer(g, 0, "11111111-1111-1111-1111-111111111111", (MCObject *)p));
    f->origin = DataWatcher_blockPos(g->heap, 0, 0, 0);
    CHECK(f->origin);
    f->controller = PlayerControllerMP_nativeNew(g->heap, h, (MCObject *)f, &click_dependencies);
    CHECK(f->controller);
    CHECK(PlayerControllerMP_bindActions(f->controller, (MCObject *)f, (MCObject *)f, &actions));
    return f;
}
static void finish(MCGameplay *g, MCObjectRootScope *scope, bool failed) {
    CHECK(MCObjectHeap_failed(g->heap) == failed);
    MCObjectRootScope_end(scope);
    CHECK(MCGameplay_free(g));
}
static ItemStack *stack(Fixture *f, int32_t count) {
    ItemStack *s = ItemStack_new(f->object.heap, ItemStack_registryItem(1), count, 0);
    CHECK(s);
    return s;
}
static void clear(Fixture *f) { f->packetCount = f->eventCount = 0; }
static ContainerWorkbench *bench(Fixture *f) {
    mc_crafting_position pos = {0, 0, 0};
    ContainerWorkbench *b = ContainerWorkbench_new(
        f->player->inventory, (MCObject *)((MCGameplayWorld *)(f->player->living.entity.worldObj)), &pos, &crafting);
    CHECK(b);
    b->container.windowId = 77;
    f->player->openContainer = &b->container;
    return b;
}
static void test_drop_and_close(void) {
    MCGameplay g = {0};
    MCObjectRootScope scope = {0};
    Fixture *f = setup(&g, &scope);
    MCGameplayPlayer *p = f->player;
    ItemStack *s = stack(f, 7);
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory, 0, s));
    EntityItem *result = (EntityItem *)f;
    CHECK(EntityPlayerSP_dropOneItem(f->sp, false, &result) && !result);
    CHECK(f->packetCount == 1 && s->stackSize == 7);
    C07PacketPlayerDigging *dig = (C07PacketPlayerDigging *)f->packets[0];
    CHECK(C07PacketPlayerDigging_isInstance((MCObject *)dig));
    CHECK(dig->status == C07PacketPlayerDigging_action(C07_DROP_ITEM) &&
          dig->position == f->origin && dig->facing == C07PacketPlayerDigging_facing(0));
    CHECK(EntityPlayerSP_dropOneItem(f->sp, true, &result));
    CHECK(((C07PacketPlayerDigging *)f->packets[1])->status ==
          C07PacketPlayerDigging_action(C07_DROP_ALL_ITEMS));
    CHECK(InventoryPlayer_setItemStack(p->inventory, s));
    CHECK(InventoryCrafting_setInventorySlotContents(((ContainerPlayer *)(p->inventoryContainer))->craftMatrix, 0, s));
    CHECK(InventoryCraftResult_setInventorySlotContents(((ContainerPlayer *)(p->inventoryContainer))->craftResult, 0, s));
    clear(f);
    CHECK(EntityPlayerSP_closeScreenAndDropStack(f->sp));
    CHECK(f->packetCount == 0 && f->eventCount == 1 && f->events[0] == GUI);
    CHECK(InventoryCrafting_getStackInSlot(((ContainerPlayer *)(p->inventoryContainer))->craftMatrix, 0) == s);
    CHECK(InventoryCraftResult_getStackInSlot(((ContainerPlayer *)(p->inventoryContainer))->craftResult, 0) == s);
    CHECK(InventoryPlayer_setItemStack(p->inventory, s));
    f->screenContainer = p->inventoryContainer;
    f->screenOpen = true;
    clear(f);
    CHECK(EntityPlayerSP_closeScreen(f->sp));
    CHECK(f->packetCount == 1 && ((C0DPacketCloseWindow *)f->packets[0])->windowId == 0);
    CHECK(f->events[0] == QUEUE && f->events[1] == GUI && f->events[2] == DROP);
    CHECK(f->drops[0] == s &&
          !InventoryCrafting_getStackInSlot(((ContainerPlayer *)(p->inventoryContainer))->craftMatrix, 0));
    CHECK(!InventoryCraftResult_getStackInSlot(((ContainerPlayer *)(p->inventoryContainer))->craftResult, 0));
    ContainerWorkbench *b = bench(f);
    CHECK(InventoryCrafting_setInventorySlotContents(b->craftMatrix, 0, s));
    f->screenContainer = &b->container;
    f->screenOpen = true;
    clear(f);
    CHECK(EntityPlayerSP_closeScreen(f->sp));
    CHECK(((C0DPacketCloseWindow *)f->packets[0])->windowId == 77);
    CHECK(InventoryCrafting_getStackInSlot(b->craftMatrix, 0) == s && f->dropCount == 1);
    finish(&g, &scope, false);
}
static void test_close_failures_and_empty_stats(void) {
    for (int gui = 0; gui < 2; gui++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Fixture *f = setup(&g, &scope);
        ContainerWorkbench *b = bench(f);
        ItemStack *s = stack(f, 2);
        CHECK(InventoryPlayer_setItemStack(f->player->inventory, s));
        f->failSend = !gui;
        f->failGUI = gui;
        CHECK(!EntityPlayerSP_closeScreen(f->sp));
        CHECK(f->packetCount == 1);
        CHECK(InventoryPlayer_getItemStack(f->player->inventory) == (gui ? NULL : s));
        CHECK(f->player->openContainer ==
              (gui ? f->player->inventoryContainer : &b->container));
        finish(&g, &scope, true);
    }
    MCGameplay g = {0};
    MCObjectRootScope scope = {0};
    Fixture *f = setup(&g, &scope);
    StatBase *stat =
        StatBase_newIdentity(g.heap, NBTString_fromASCII(g.heap, "test"), STAT_BASE_KIND_BASIC);
    CHECK(stat);
    CHECK(StatFileWriter_unlockAchievement(f->player->stats, (MCObject *)f->player, stat, 9));
    CHECK(EntityPlayerSP_addStat(f->sp, NULL, 100));
    CHECK(EntityPlayerSP_addStat(f->sp, stat, 100));
    CHECK(StatBase_initIndependentStat(stat) == stat);
    CHECK(EntityPlayerSP_addStat(f->sp, stat, 100));
    CHECK(EntityPlayerSP_triggerAchievement(f->sp, stat));
    CHECK(StatFileWriter_readStat(f->player->stats, stat) == 9);
    CHECK(EntityPlayerSP_joinEntityItemWithWorld(f->sp, NULL));
    CHECK(MCGameplay_get(&g)->itemCount == 0);
    finish(&g, &scope, false);
}
static void test_sync_and_creative(void) {
    MCGameplay g = {0};
    MCObjectRootScope scope = {0};
    Fixture *f = setup(&g, &scope);
    PlayerControllerMP *c = f->controller;
    CHECK(c->currentGameType == &PlayerControllerMP_SURVIVAL && c->currentPlayerItem == 0);
    CHECK(PlayerControllerMP_syncCurrentPlayItem(c) && f->packetCount == 0);
    f->player->inventory->currentItem = 8;
    CHECK(PlayerControllerMP_syncCurrentPlayItem(c));
    CHECK(c->currentPlayerItem == 8 &&
          C09PacketHeldItemChange_getSlotId((C09PacketHeldItemChange *)f->packets[0]) == 8);
    CHECK(PlayerControllerMP_syncCurrentPlayItem(c) && f->packetCount == 1);
    ItemStack *s = stack(f, -7);
    f->player->capabilities->isCreativeMode = true;
    clear(f);
    CHECK(PlayerControllerMP_sendSlotPacket(c, s, 99) &&
          PlayerControllerMP_sendPacketDropItem(c, s) && f->packetCount == 0);
    c->currentGameType = &PlayerControllerMP_CREATIVE;
    f->player->capabilities->isCreativeMode = false;
    CHECK(PlayerControllerMP_sendSlotPacket(c, s, 99));
    C10PacketCreativeInventoryAction *p = (C10PacketCreativeInventoryAction *)f->packets[0];
    CHECK(p->slotId == 99 && p->stack != s && p->stack->stackSize == -7);
    s->stackSize = 6;
    CHECK(p->stack->stackSize == -7);
    CHECK(PlayerControllerMP_sendSlotPacket(c, NULL, 5));
    CHECK(!((C10PacketCreativeInventoryAction *)f->packets[1])->stack);
    CHECK(PlayerControllerMP_sendPacketDropItem(c, NULL) && f->packetCount == 2);
    CHECK(PlayerControllerMP_sendPacketDropItem(c, s));
    CHECK(((C10PacketCreativeInventoryAction *)f->packets[2])->slotId == -1);
    MCGameplayPlayer *replacement = MCGameplayPlayer_new(
        ((MCGameplayWorld *)(f->player->living.entity.worldObj)), NBTString_fromASCII(g.heap, "Second"), NULL, &crafting);
    CHECK(replacement);
    replacement->effects = (MCObject *)f;
    f->player = replacement;
    replacement->inventory->currentItem = 3;
    CHECK(PlayerControllerMP_syncCurrentPlayItem(c) && c->currentPlayerItem == 3);
    CHECK(C09PacketHeldItemChange_getSlotId((C09PacketHeldItemChange *)f->packets[3]) == 3);
    finish(&g, &scope, false);
    scope = (MCObjectRootScope){0};
    f = setup(&g, &scope);
    f->player->inventory->currentItem = 4;
    f->failSend = true;
    CHECK(!PlayerControllerMP_syncCurrentPlayItem(f->controller));
    CHECK(f->controller->currentPlayerItem == 4 && f->packetCount == 1);
    finish(&g, &scope, true);
}
static void test_use(void) {
    for (int mode = 0; mode < 5; mode++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Fixture *f = setup(&g, &scope);
        MCGameplayPlayer *p = f->player;
        ItemStack *s = stack(f, mode == 3 ? 1 : 7);
        CHECK(InventoryPlayer_setInventorySlotContents(p->inventory, 2, s));
        p->inventory->currentItem = 2;
        f->useReturn = mode == 2 ? stack(f, 3) : s;
        f->changeCount = mode == 1 || mode == 3;
        if (mode == 4)
            f->controller->currentGameType = &PlayerControllerMP_SPECTATOR;
        bool changed = true;
        CHECK(PlayerControllerMP_sendUseItem(f->controller, p, ((MCGameplayWorld *)(p->living.entity.worldObj)), s, &changed));
        if (mode == 4) {
            CHECK(!changed && f->eventCount == 0 && s->stackSize == 7);
        } else {
            CHECK(changed == (mode != 0));
            CHECK(f->eventCount == 4 && f->events[0] == GET_PLAYER && f->events[1] == QUEUE &&
                  f->events[2] == QUEUE && f->events[3] == USE);
            CHECK(C09PacketHeldItemChange_isInstance(f->packets[0]) &&
                  C08PacketPlayerBlockPlacement_isInstance(f->packets[1]));
            C08PacketPlayerBlockPlacement *packet = (C08PacketPlayerBlockPlacement *)f->packets[1];
            CHECK(C08PacketPlayerBlockPlacement_getStack(packet) != s &&
                  C08PacketPlayerBlockPlacement_getStack(packet)->stackSize == (mode == 3 ? 1 : 7));
            CHECK(InventoryPlayer_getCurrentItem(p->inventory) ==
                  (mode == 3 ? NULL : f->useReturn));
        }
        finish(&g, &scope, false);
    }
    const int32_t signed_counts[] = {INT32_MIN, -128, -1, 0, 127, INT32_MAX};
    for (unsigned i = 0; i < sizeof(signed_counts) / sizeof(signed_counts[0]); i++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Fixture *f = setup(&g, &scope);
        ItemStack *s = stack(f, signed_counts[i]);
        CHECK(InventoryPlayer_setInventorySlotContents(f->player->inventory, 0, s));
        f->useReturn = s;
        bool changed = true;
        CHECK(PlayerControllerMP_sendUseItem(f->controller, f->player, ((MCGameplayWorld *)(f->player->living.entity.worldObj)), s,
                                            &changed));
        CHECK(!changed && InventoryPlayer_getCurrentItem(f->player->inventory) == s);
        CHECK(s->stackSize == signed_counts[i] && f->packetCount == 1);
        CHECK(C08PacketPlayerBlockPlacement_getStack((C08PacketPlayerBlockPlacement *)f->packets[0])
                  ->stackSize == signed_counts[i]);
        finish(&g, &scope, false);
    }
    for (int missing = 0; missing < 2; missing++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Fixture *f = setup(&g, &scope);
        ItemStack *s = stack(f, 3);
        CHECK(InventoryPlayer_setInventorySlotContents(f->player->inventory, 0, s));
        bool out = true;
        CHECK(!PlayerControllerMP_sendUseItem(f->controller, f->player, ((MCGameplayWorld *)(f->player->living.entity.worldObj)),
                                              missing ? NULL : s, &out));
        CHECK(out && f->packetCount == 1 &&
              C08PacketPlayerBlockPlacement_isInstance(f->packets[0]));
        CHECK(InventoryPlayer_getCurrentItem(f->player->inventory) == (missing ? s : NULL));
        CHECK(f->eventCount == (missing ? 2u : 3u));
        finish(&g, &scope, true);
    }
    MCGameplay g = {0};
    MCObjectRootScope scope = {0};
    Fixture *f = setup(&g, &scope);
    ItemStack *s = stack(f, 4);
    f->useReturn = s;
    ItemStack *out = NULL;
    CHECK(ItemStack_useItemRightClick(s, (MCObject *)((MCGameplayWorld *)(f->player->living.entity.worldObj)), (MCObject *)f->player,
                                      &item_use, (MCObject *)f, &out) &&
          out == s);
    f->useReturn = NULL;
    out = s;
    CHECK(ItemStack_useItemRightClick(s, (MCObject *)((MCGameplayWorld *)(f->player->living.entity.worldObj)), (MCObject *)f->player,
                                      &item_use, (MCObject *)f, &out) &&
          !out);
    finish(&g, &scope, false);
    scope = (MCObjectRootScope){0};
    f = setup(&g, &scope);
    s = ItemStack_new(g.heap, NULL, 1, 0);
    CHECK(s);
    out = (ItemStack *)f;
    CHECK(!ItemStack_useItemRightClick(s, (MCObject *)((MCGameplayWorld *)(f->player->living.entity.worldObj)), (MCObject *)f->player,
                                       &item_use, (MCObject *)f, &out) &&
          out == (ItemStack *)f);
    finish(&g, &scope, true);
}
static void test_snapshot_identity(void) {
    MCGameplay g = {0};
    MCObjectRootScope scope = {0};
    Fixture *f = setup(&g, &scope);
    ItemStack *s = stack(f, 6);
    f->useReturn = s;
    CHECK(InventoryPlayer_setInventorySlotContents(f->player->inventory, 0, s));
    EntityItem *unused = NULL;
    CHECK(EntityPlayerSP_dropOneItem(f->sp, false, &unused));
    MCObjectRootScope_end(&scope);
    MCGameplayTransaction transaction = {0};
    CHECK(MCGameplay_begin(&g, &transaction));
    MCGameplayPlayer *p = (MCGameplayPlayer *)MCGameplay_get(&transaction.working)->players[0];
    Fixture *b = (Fixture *)p->effects;
    CHECK(b != f && EntityPlayerSP_asPlayer(b->sp) == p && (MCObject *)b->sp == (MCObject *)p && b->controller->mc == (MCObject *)b &&
          b->controller->actionsContext == (MCObject *)b);
    CHECK(b->sp->mc == (MCObject *)b && b->sp->dependencyContext == (MCObject *)b);
    CHECK(b->useReturn == InventoryPlayer_getCurrentItem(p->inventory) && b->useReturn != s);
    CHECK(((C07PacketPlayerDigging *)b->packets[0])->position == b->origin &&
          b->origin != f->origin);
    CHECK(MCGameplay_abort(&transaction) && MCGameplay_free(&g));
}
static void test_required_and_foreign_dependencies(void) {
    for (int which = 0; which < 5; which++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Fixture *f = setup(&g, &scope);
        ItemStack *s = stack(f, 5);
        PlayerControllerMPActionsDependencies d = actions;
        if (which == 0) {
            d.addCreativeItemToSendQueue = NULL;
            f->controller->actionsDependencies = &d;
            CHECK(PlayerControllerMP_sendSlotPacket(f->controller, s, 5));
            f->controller->currentGameType = &PlayerControllerMP_CREATIVE;
            CHECK(!PlayerControllerMP_sendSlotPacket(f->controller, s, 5));
        }
        if (which == 1) {
            d.addHeldItemToSendQueue = NULL;
            f->controller->actionsDependencies = &d;
            CHECK(PlayerControllerMP_syncCurrentPlayItem(f->controller));
            f->player->inventory->currentItem = 5;
            CHECK(!PlayerControllerMP_syncCurrentPlayItem(f->controller));
            CHECK(f->controller->currentPlayerItem == 5);
        }
        if (which == 2) {
            f->controller->currentGameType = NULL;
            CHECK(!PlayerControllerMP_sendPacketDropItem(f->controller, NULL));
        }
        if (which == 3) {
            f->useReturn = s;
            f->failUse = true;
            ItemStack *out = (ItemStack *)f;
            CHECK(!ItemStack_useItemRightClick(s, (MCObject *)((MCGameplayWorld *)(f->player->living.entity.worldObj)),
                                               (MCObject *)f->player, &item_use, (MCObject *)f,
                                               &out) &&
                  out == (ItemStack *)f);
        }
        if (which == 4) {
            d.itemUse = NULL;
            f->controller->actionsDependencies = &d;
            CHECK(InventoryPlayer_setInventorySlotContents(f->player->inventory, 0, s));
            bool out = true;
            CHECK(!PlayerControllerMP_sendUseItem(f->controller, f->player, ((MCGameplayWorld *)(f->player->living.entity.worldObj)), s,
                                                  &out) &&
                  out && f->packetCount == 1);
        }
        finish(&g, &scope, true);
    }
    for (int which = 0; which < 3; which++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Fixture *f = setup(&g, &scope);
        MCObjectHeap *foreign = MCObjectHeap_new(1024 * 1024);
        CHECK(foreign);
        if (which == 0) {
            f->origin = DataWatcher_blockPos(foreign, 0, 0, 0);
            CHECK(f->origin);
            EntityItem *out = (EntityItem *)f;
            CHECK(!EntityPlayerSP_dropOneItem(f->sp, false, &out) && out == (EntityItem *)f &&
                  f->packetCount == 0);
            f->origin = NULL;
        }
        if (which == 1) {
            ItemStack *s = stack(f, 1);
            f->useReturn = ItemStack_new(foreign, ItemStack_registryItem(1), 2, 0);
            CHECK(f->useReturn);
            ItemStack *out = s;
            CHECK(!ItemStack_useItemRightClick(s, (MCObject *)((MCGameplayWorld *)(f->player->living.entity.worldObj)),
                                               (MCObject *)f->player, &item_use, (MCObject *)f,
                                               &out) &&
                  out == s);
            f->useReturn = NULL;
        }
        if (which == 2) {
            MCObject *mc = (MCObject *)DataWatcher_blockPos(foreign, 0, 0, 0);
            CHECK(mc);
            CHECK(!PlayerControllerMP_bindActions(f->controller, mc, (MCObject *)f, &actions));
            CHECK(f->controller->mc == (MCObject *)f);
        }
        MCObjectHeap_free(foreign);
        finish(&g, &scope, true);
    }
}
int main(void) {
    test_drop_and_close();
    test_close_failures_and_empty_stats();
    test_sync_and_creative();
    test_use();
    test_snapshot_identity();
    test_required_and_foreign_dependencies();
    printf("source client actions: %u checks passed\n", checks);
    return 0;
}
