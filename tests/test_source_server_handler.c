#include "network/NetHandlerPlayServer.h"
#include "network/GameplayPacketRouter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "server handler check %u at %d: %s\n", checks, __LINE__, #x);          \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
/* Test effects record actual source dependency calls. They are not production
   packets, scheduling, TileEntities or a substitute for the runtime adapters. */
typedef struct {
    MCObject object;
    char calls[4096];
    size_t used;
    MCPacketThreadResult thread;
    int32_t window;
    int16_t action;
    bool accepted, failHeld;
    ContainerList *sent;
    ItemStack *drop;
    EntityItem *entity;
    NBTTagCompound *tile;
    int32_t x, y, z;
    unsigned dropped;
} Effects;
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
    Effects *f = (Effects *)o;
    f->sent = (ContainerList *)v((MCObject *)f->sent, c);
    f->drop = (ItemStack *)v((MCObject *)f->drop, c);
    f->entity = (EntityItem *)v((MCObject *)f->entity, c);
    f->tile = (NBTTagCompound *)v((MCObject *)f->tile, c);
}
static const MCObjectClass effectsClass = {"fixture.source-handler.effects",
                                           MCObjectHeap_plainClone, trace, NULL};
static void record(Effects *f, char event) {
    CHECK(f->used + 1 < sizeof(f->calls));
    f->calls[f->used++] = event;
    f->calls[f->used] = 0;
    CHECK(!MCObjectHeap_collect(f->object.heap));
}
static MCPacketThreadResult thread(MCObject *o, NetHandlerPlayServer *h, MCObject *p) {
    CHECK(h && p && p->heap == o->heap);
    record((Effects *)o, 'T');
    return ((Effects *)o)->thread;
}
static bool active(MCObject *o, MCGameplayPlayer *p) {
    CHECK(p);
    record((Effects *)o, 'A');
    return true;
}
static bool close_container(MCObject *o, MCGameplayPlayer *p) {
    record((Effects *)o, 'C');
    if (!Container_onContainerClosed(p->openContainer, p->inventory))
        return false;
    p->openContainer = &p->inventoryContainer->container;
    return true;
}
static bool confirm(MCObject *o, MCGameplayPlayer *p, int32_t w, int16_t a, bool b) {
    CHECK(p);
    Effects *f = (Effects *)o;
    record(f, 'F');
    f->window = w;
    f->action = a;
    f->accepted = b;
    return true;
}
static bool update(MCObject *o, MCGameplayPlayer *p, Container *c, ContainerList *l) {
    CHECK(p && c == p->openContainer);
    Effects *f = (Effects *)o;
    record(f, 'U');
    f->sent = l;
    return true;
}
static bool held(MCObject *o, MCGameplayPlayer *p) {
    CHECK(p->isChangingQuantityOnly);
    Effects *f = (Effects *)o;
    record(f, 'H');
    return !f->failHeld;
}
static MCObject *tile(MCObject *o, MCGameplayWorld *w, int32_t x, int32_t y, int32_t z) {
    CHECK(w);
    Effects *f = (Effects *)o;
    record(f, 'L');
    f->x = x;
    f->y = y;
    f->z = z;
    return (MCObject *)f->tile;
}
static bool tile_nbt(MCObject *o, MCObject *t, NBTTagCompound *out) {
    record((Effects *)o, 'W');
    return NBTTagCompound_merge(out, (NBTTagCompound *)t);
}
static EntityItem *drop_packet(MCObject *o, MCGameplayPlayer *p, ItemStack *s, bool unused) {
    CHECK(p && unused);
    Effects *f = (Effects *)o;
    record(f, 'D');
    f->drop = s;
    return f->entity;
}
static const NetHandlerPlayServerDependencies deps = {
    thread, active, close_container, confirm, update, held, tile, tile_nbt, drop_packet};
static bool unused_log(MCObject *o, int32_t n) {
    (void)o;
    (void)n;
    CHECK(false);
    return false;
}
static bool unused_remote(MCObject *o, MCObject *w) {
    (void)o;
    (void)w;
    CHECK(false);
    return false;
}
static InventoryPlayer *unused_inventory(MCObject *o, MCObject *p) {
    (void)o;
    (void)p;
    CHECK(false);
    return NULL;
}
static const NBTString *unused_name(MCObject *o, MCObject *p) {
    (void)o;
    (void)p;
    CHECK(false);
    return NULL;
}
static MCObject *unused_player(MCObject *o, MCObject *w, const NBTString *n) {
    (void)o;
    (void)w;
    (void)n;
    CHECK(false);
    return NULL;
}
static bool unused_achievement(MCObject *o, MCObject *p, EntityItemAchievement a) {
    (void)o;
    (void)p;
    (void)a;
    CHECK(false);
    return false;
}
static bool unused_silent(MCObject *o, const EntityItem *e) {
    (void)o;
    (void)e;
    CHECK(false);
    return false;
}
static float unused_random(MCObject *o, EntityItem *e) {
    (void)o;
    (void)e;
    CHECK(false);
    return 0;
}
static bool unused_sound(MCObject *o, MCObject *w, MCObject *p, const char *s, float v,
                         float pitch) {
    (void)o;
    (void)w;
    (void)p;
    (void)s;
    (void)v;
    (void)pitch;
    CHECK(false);
    return false;
}
static bool unused_pickup(MCObject *o, MCObject *p, EntityItem *e, int32_t n) {
    (void)o;
    (void)p;
    (void)e;
    (void)n;
    CHECK(false);
    return false;
}
static bool unused_dead(MCObject *o, EntityItem *e) {
    (void)o;
    (void)e;
    CHECK(false);
    return false;
}
static const EntityItemDependencies unusedEntityDeps = {
    unused_log,    unused_remote,      unused_inventory,
    unused_name,   unused_player, unused_achievement, unused_silent,
    unused_random, unused_sound,  unused_pickup,      unused_dead};
static ItemStack *recipe(InventoryCrafting *g, MCObject *w) {
    return CraftingManager_findMatchingRecipe(((MCGameplayWorld *)w)->manager, g, w, NULL, NULL);
}
static ItemStackArray *remaining(InventoryCrafting *g, MCObject *w) {
    return CraftingManager_func_180303_b(((MCGameplayWorld *)w)->manager, g, w);
}
static bool crafted(ItemStack *s, MCObject *w, MCObject *p, int32_t n) {
    (void)s;
    (void)w;
    (void)p;
    (void)n;
    CHECK(false);
    return false;
}
static bool achieve(MCObject *p, mc_crafting_achievement a) {
    (void)p;
    (void)a;
    CHECK(false);
    return false;
}
static bool dropped(MCObject *p, ItemStack *s, bool scatter) {
    (void)scatter;
    Effects *f = (Effects *)((MCGameplayPlayer *)p)->effects;
    CHECK(s);
    ++f->dropped;
    f->drop = s;
    return true;
}
static bool type_false(const Item *i) {
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
                                              crafted,
                                              achieve,
                                              dropped,
                                              type_false,
                                              type_false,
                                              type_false,
                                              type_false,
                                              armor,
                                              MCGameplayWorld_isRemote,
                                              MCGameplayWorld_isCraftingTable,
                                              MCGameplayPlayer_getDistanceSq};
typedef struct {
    MCGameplay game;
    MCGameplayPlayer *player;
    NetHandlerPlayServer *handler;
    Effects *effects;
    MCObjectRootScope scope;
} Fixture;
static void init(Fixture *f) {
    memset(f, 0, sizeof(*f));
    CHECK(MCGameplay_init(&f->game, 32 * 1024 * 1024));
    CHECK(MCObjectRootScope_begin(&f->scope, f->game.heap));
    CraftingManager *m = CraftingManager_newEmpty(f->game.heap);
    CHECK(m);
    MCGameplayWorld *w = MCGameplayWorld_new(f->game.heap, MCGameplay_get(&f->game), NULL, m);
    CHECK(w);
    CHECK(MCGameplay_setWorld(&f->game, (MCObject *)w));
    f->player =
        MCGameplayPlayer_new(w, NBTString_fromASCII(f->game.heap, "Player"), NULL, &crafting);
    CHECK(f->player);
    CHECK(MCGameplay_setPlayer(&f->game, 0, "11111111-1111-1111-1111-111111111111",
                               (MCObject *)f->player));
    f->effects = (Effects *)MCObjectHeap_alloc(f->game.heap, sizeof(*f->effects), &effectsClass);
    CHECK(f->effects);
    f->player->effects = (MCObject *)f->effects;
    f->handler = NetHandlerPlayServer_nativeNew(f->player, (MCObject *)f->effects, &deps);
    CHECK(f->handler && f->player->handler == (MCObject *)f->handler);
}
static void finish(Fixture *f) {
    MCObjectRootScope_end(&f->scope);
    CHECK(MCGameplay_free(&f->game));
}
static ItemStack *stack(Fixture *f, int32_t id, int32_t count) {
    ItemStack *s = ItemStack_new(f->game.heap, ItemStack_registryItem(id), count, 0);
    CHECK(s);
    return s;
}
static void reset(Effects *f) {
    f->used = 0;
    f->calls[0] = 0;
    f->sent = NULL;
}
static C0EPacketClickWindow *click(Fixture *f, int32_t w, int32_t slot, ItemStack *claimed,
                                   int16_t action) {
    C0EPacketClickWindow *p =
        C0EPacketClickWindow_new(f->game.heap, w, slot, 0, 0, claimed, action);
    CHECK(p);
    return p;
}
static void click_confirm_and_spectator(void) {
    Fixture f;
    init(&f);
    MCGameplayPlayer *p = f.player;
    Container *c = p->openContainer;
    Effects *e = f.effects;
    ItemStack *s = stack(&f, 1, 5);
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory, 0, s));
    C0EPacketClickWindow *packet = click(&f, 0, 36, s, 17);
    CHECK(C0EPacketClickWindow_processPacket(packet, NetHandlerPlayServer_asHandler(f.handler)));
    CHECK(strcmp(e->calls, "TAFH") == 0 && e->accepted && e->window == 0 && e->action == 17);
    CHECK(!p->isChangingQuantityOnly);
    CHECK(InventoryPlayer_getItemStack(p->inventory) == s &&
          InventoryPlayer_getStackInSlot(p->inventory, 0) == NULL);
    reset(e);
    CHECK(InventoryPlayer_setItemStack(p->inventory, NULL));
    ItemStack *diamond = stack(&f, 264, 3);
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory, 1, diamond));
    CHECK(NetHandlerPlayServer_processClickWindow(f.handler, click(&f, 0, 37, NULL, 99)));
    CHECK(strcmp(e->calls, "TAFU") == 0 && !e->accepted);
    CHECK(!Container_getCanCraft(c, (MCObject *)p));
    CHECK(InventoryPlayer_getItemStack(p->inventory) == diamond);
    CHECK(ContainerList_size(e->sent) == 45 && ContainerList_get(e->sent, 37) == NULL);
    int16_t rejected = 0;
    CHECK(NetHandlerPlayServer_rejectedAction(f.handler, 0, &rejected) && rejected == 99);
    reset(e);
    CHECK(NetHandlerPlayServer_processClickWindow(f.handler, click(&f, 0, 36, NULL, 100)));
    CHECK(strcmp(e->calls, "TA") == 0);
    C0FPacketConfirmTransaction *ack = C0FPacketConfirmTransaction_new(f.game.heap, 0, 98, true);
    CHECK(ack);
    reset(e);
    CHECK(NetHandlerPlayServer_processConfirmTransaction(f.handler, ack));
    CHECK(strcmp(e->calls, "T") == 0 && !Container_getCanCraft(c, (MCObject *)p));
    ack->uid = 99;
    ack->accepted = false;
    CHECK(NetHandlerPlayServer_processConfirmTransaction(f.handler, ack));
    CHECK(Container_getCanCraft(c, (MCObject *)p));
    CHECK(NetHandlerPlayServer_rejectedAction(f.handler, 0, &rejected) && rejected == 99);
    p->spectator = true;
    ItemStack *zero = stack(&f, 1, 0);
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory, 0, zero));
    reset(e);
    CHECK(NetHandlerPlayServer_processClickWindow(f.handler, click(&f, 0, 36, NULL, 101)));
    CHECK(strcmp(e->calls, "TAU") == 0 && ContainerList_get(e->sent, 36) == (MCObject *)zero);
    CHECK(InventoryPlayer_getItemStack(p->inventory) == diamond && zero->stackSize == 0);
    CHECK(Container_setCanCraft(c, (MCObject *)p, false));
    CHECK(NetHandlerPlayServer_processConfirmTransaction(f.handler, ack));
    CHECK(!Container_getCanCraft(c, (MCObject *)p));
    p->spectator = false;
    CHECK(Container_setCanCraft(c, (MCObject *)p, true));
    reset(e);
    CHECK(NetHandlerPlayServer_processClickWindow(f.handler, click(&f, 1, 36, NULL, 1)));
    CHECK(strcmp(e->calls, "TA") == 0);
    CHECK(!MCObjectHeap_failed(f.game.heap));
    finish(&f);
}
static void creative_and_tile(void) {
    Fixture f;
    init(&f);
    MCGameplayPlayer *p = f.player;
    Effects *e = f.effects;
    C10PacketCreativeInventoryAction *packet =
        C10PacketCreativeInventoryAction_new(f.game.heap, 36, stack(&f, 368, 64));
    CHECK(packet);
    CHECK(NetHandlerPlayServer_processCreativeInventoryAction(f.handler, packet));
    CHECK(strcmp(e->calls, "T") == 0 && InventoryPlayer_getStackInSlot(p->inventory, 0) == NULL);
    p->creative = true;
    reset(e);
    CHECK(NetHandlerPlayServer_processCreativeInventoryAction(f.handler, packet));
    CHECK(strcmp(e->calls, "T") == 0 &&
          InventoryPlayer_getStackInSlot(p->inventory, 0) == packet->stack &&
          packet->stack->stackSize == 64);
    NBTTagCompound *tag = NBTTagCompound_new(f.game.heap), *block = NBTTagCompound_new(f.game.heap);
    CHECK(tag && block);
    CHECK(NBTTagCompound_setTag_ascii(tag, "BlockEntityTag", (NBTBase *)block));
    CHECK(NBTTagCompound_setString_ascii(block, "x", NBTString_fromASCII(f.game.heap, "bad")));
    CHECK(NBTTagCompound_setInteger_ascii(block, "y", 2));
    CHECK(NBTTagCompound_setInteger_ascii(block, "z", 3));
    CHECK(ItemStack_setTagCompound(packet->stack, tag));
    e->tile = NBTTagCompound_new(f.game.heap);
    CHECK(e->tile);
    CHECK(NBTTagCompound_setInteger_ascii(e->tile, "x", 9) &&
          NBTTagCompound_setInteger_ascii(e->tile, "y", 9) &&
          NBTTagCompound_setInteger_ascii(e->tile, "z", 9) &&
          NBTTagCompound_setInteger_ascii(e->tile, "value", 42));
    reset(e);
    CHECK(NetHandlerPlayServer_processCreativeInventoryAction(f.handler, packet));
    CHECK(strcmp(e->calls, "TLW") == 0 && e->x == 0 && e->y == 2 && e->z == 3);
    NBTTagCompound *copy =
        NBTTagCompound_getCompoundTag_ascii(packet->stack->stackTagCompound, "BlockEntityTag");
    CHECK(copy != e->tile && copy != block && !NBTTagCompound_hasKey_ascii(copy, "x") &&
          !NBTTagCompound_hasKey_ascii(copy, "y") && !NBTTagCompound_hasKey_ascii(copy, "z") &&
          NBTTagCompound_getInteger_ascii(copy, "value") == 42);
    CHECK(NBTTagCompound_getInteger_ascii(e->tile, "x") == 9);
    C10PacketCreativeInventoryAction *nullDrop =
        C10PacketCreativeInventoryAction_new(f.game.heap, -1, NULL);
    CHECK(nullDrop);
    reset(e);
    CHECK(NetHandlerPlayServer_processCreativeInventoryAction(f.handler, nullDrop));
    CHECK(strcmp(e->calls, "TD") == 0 && f.handler->itemDropThreshold == 20 && e->drop == NULL);
    packet->slotId = -1;
    packet->stack->stackTagCompound = NULL;
    e->entity = EntityItem_nativeNew(f.game.heap, (MCObject *)p->worldObj, NULL, &unusedEntityDeps);
    CHECK(e->entity && EntityItem_nativeInitializeDataWatcher(e->entity, NULL, NULL));
    CHECK(e->entity);
    reset(e);
    CHECK(NetHandlerPlayServer_processCreativeInventoryAction(f.handler, packet));
    CHECK(strcmp(e->calls, "TD") == 0 && e->drop == packet->stack && e->entity->age == 4800 &&
          f.handler->itemDropThreshold == 40);
    f.handler->itemDropThreshold = 200;
    reset(e);
    CHECK(NetHandlerPlayServer_processCreativeInventoryAction(f.handler, packet));
    CHECK(strcmp(e->calls, "T") == 0 && f.handler->itemDropThreshold == 200);
    f.handler->itemDropThreshold = 0;
    packet->stack->stackSize = 0;
    reset(e);
    CHECK(NetHandlerPlayServer_processCreativeInventoryAction(f.handler, packet));
    CHECK(strcmp(e->calls, "T") == 0 && f.handler->itemDropThreshold == 0);
    for (int32_t slot = 0; slot <= 45; slot += 45) {
        packet->slotId = slot;
        packet->stack->stackSize = 1;
        reset(e);
        CHECK(NetHandlerPlayServer_processCreativeInventoryAction(f.handler, packet));
        CHECK(strcmp(e->calls, "T") == 0);
    }
    CHECK(!MCObjectHeap_failed(f.game.heap));
    finish(&f);
}
static void queued_close_clone_and_failure(void) {
    Fixture f;
    init(&f);
    Effects *e = f.effects;
    MCGameplayPlayer *p = f.player;
    ItemStack *s = stack(&f, 1, 5);
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory, 0, s));
    C0EPacketClickWindow *packet = click(&f, 0, 36, s, 1);
    e->thread = MC_PACKET_THREAD_QUEUED;
    CHECK(NetHandlerPlayServer_processClickWindow(f.handler, packet));
    CHECK(strcmp(e->calls, "T") == 0 && InventoryPlayer_getStackInSlot(p->inventory, 0) == s);
    e->thread = MC_PACKET_THREAD_EXECUTE;
    reset(e);
    C0DPacketCloseWindow *close = C0DPacketCloseWindow_new(f.game.heap, 123);
    CHECK(close);
    CHECK(InventoryPlayer_setItemStack(p->inventory, s));
    CHECK(NetHandlerPlayServer_processCloseWindow(f.handler, close));
    CHECK(strcmp(e->calls, "TC") == 0 && e->dropped == 1 && e->drop == s &&
          InventoryPlayer_getItemStack(p->inventory) == NULL);
    MCObjectRootScope_end(&f.scope);
    MCGameplayTransaction tx = {0};
    CHECK(MCGameplay_begin(&f.game, &tx));
    MCGameplayPlayer *wp = (MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
    NetHandlerPlayServer *wh = (NetHandlerPlayServer *)wp->handler;
    CHECK(wh != f.handler && wh->playerEntity == wp && wh->dependencyContext == wp->effects &&
          wp->worldObj != (MCGameplayWorld *)p->worldObj);
    CHECK(MCGameplay_abort(&tx));
    CHECK(MCObjectRootScope_begin(&f.scope, f.game.heap));
    reset(e);
    e->failHeld = true;
    CHECK(!NetHandlerPlayServer_processClickWindow(f.handler, packet));
    CHECK(MCObjectHeap_failed(f.game.heap) && p->isChangingQuantityOnly &&
          strcmp(e->calls, "TAFH") == 0);
    finish(&f);
}
static void routed_wire_snapshots(void) {
    for (unsigned op = 0; op < 4; op++)
        for (unsigned malformed = 0; malformed < 3; malformed++) {
            Fixture f;
            init(&f);
            ItemStack *original = stack(&f, 1, 5);
            CHECK(InventoryPlayer_setInventorySlotContents(f.player->inventory, 0, original));
            f.player->creative = true;
            if (!op)
                CHECK(InventoryPlayer_setItemStack(f.player->inventory, original));
            mc_buf payload = {0};
            PacketBuffer io;
            CHECK(PacketBuffer_init(&io, f.game.heap, &payload));
            int32_t id = (int32_t)(0x0d + op);
            if (op == 0)
                CHECK(C0DPacketCloseWindow_writePacketData(
                    C0DPacketCloseWindow_new(f.game.heap, 123), &io));
            else if (op == 1)
                CHECK(C0EPacketClickWindow_writePacketData(click(&f, 0, 36, original, 17), &io));
            else if (op == 2)
                CHECK(C0FPacketConfirmTransaction_writePacketData(
                    C0FPacketConfirmTransaction_new(f.game.heap, 0, 17, false), &io));
            else
                CHECK(C10PacketCreativeInventoryAction_writePacketData(
                    C10PacketCreativeInventoryAction_new(f.game.heap, 36, original), &io));
            if (malformed == 1)
                --payload.len;
            if (malformed == 2)
                mc_put_u8(&payload, 42);
            CHECK(GameplayPacketRouter_server(&f.game, 0, id, &payload) ==
                      MC_GAMEPLAY_PACKET_FAILED &&
                  payload.pos == 0);
            CHECK(GameplayPacketRouter_server(&f.game, 0, 0x7fffffff, &payload) ==
                      MC_GAMEPLAY_PACKET_NOT_HANDLED &&
                  payload.pos == 0);
            MCObjectRootScope_end(&f.scope);
            MCGameplayTransaction tx = {0};
            CHECK(MCGameplay_begin(&f.game, &tx));
            GameplayPacketResult result = GameplayPacketRouter_server(&tx.working, 0, id, &payload);
            CHECK(result == (malformed ? MC_GAMEPLAY_PACKET_REJECTED : MC_GAMEPLAY_PACKET_APPLIED));
            CHECK(!MCObjectHeap_hasBorrowers(tx.working.heap));
            MCGameplayPlayer *p = (MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
            Effects *e = (Effects *)p->effects;
            CHECK(f.effects->used == 0 &&
                  InventoryPlayer_getStackInSlot(f.player->inventory, 0) == original);
            CHECK(InventoryPlayer_getItemStack(f.player->inventory) == (!op ? original : NULL));
            if (malformed)
                CHECK(e->used == 0 &&
                      InventoryPlayer_getStackInSlot(p->inventory, 0)->stackSize == 5);
            else {
                CHECK(strcmp(e->calls, op == 0 ? "TC" : op == 1 ? "TAFH" : "T") == 0);
                if (op == 0)
                    CHECK(!InventoryPlayer_getItemStack(p->inventory) &&
                          e->drop == InventoryPlayer_getStackInSlot(p->inventory, 0));
                else if (op == 1)
                    CHECK(!InventoryPlayer_getStackInSlot(p->inventory, 0) &&
                          InventoryPlayer_getItemStack(p->inventory)->stackSize == 5 &&
                          e->accepted);
                else if (op == 3)
                    CHECK(InventoryPlayer_getStackInSlot(p->inventory, 0) != original &&
                          InventoryPlayer_getStackInSlot(p->inventory, 0)->stackSize == 5);
            }
            CHECK(MCGameplay_abort(&tx));
            CHECK(!MCObjectHeap_failed(f.game.heap));
            CHECK(MCObjectRootScope_begin(&f.scope, f.game.heap));
            mc_buf_free(&payload);
            finish(&f);
        }
}
int main(void) {
    click_confirm_and_spectator();
    creative_and_tile();
    queued_close_clone_and_failure();
    routed_wire_snapshots();
    printf("source server handler: %u checks\n", checks);
    return 0;
}
