#include "server/management/ItemInWorldManagerUse.h"
#include "util/MCGameplayPackets.h"
#include "server/native_gameplay.h"
#include "entity/player/EntityPlayerDrops.h"
#include "nbt/NBTTagByte.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "server use %u at %d: %s\n", checks, __LINE__, #x);                    \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
typedef struct {
    MCObject object;
    MCGameplayPlayer *player;
    ItemStack *input, *returned;
    char events[32], failEvent;
    unsigned eventCount;
    int mode;
    bool creative, spectator, usingItem, failSend;
} Fixture;
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
    Fixture *f = (Fixture *)o;
    f->player = (MCGameplayPlayer *)v((MCObject *)f->player, c);
    f->input = (ItemStack *)v((MCObject *)f->input, c);
    f->returned = (ItemStack *)v((MCObject *)f->returned, c);
}
static const MCObjectClass fixtureClass = {"fixture.server.use.dependencies",
                                           MCObjectHeap_plainClone, trace, NULL};
/* These immutable callbacks are explicit manager/Item/player fixtures. The
   send callback calls real source window code and queues actual S30/S2F objects. */
static void enter(Fixture *f, char id) {
    CHECK(f->eventCount + 1 < sizeof(f->events));
    f->events[f->eventCount++] = id;
    f->events[f->eventCount] = 0;
    CHECK(MCObjectHeap_hasBorrowers(f->object.heap) && !MCObjectHeap_collect(f->object.heap));
    if (f->failEvent == id)
        MCObjectHeap_fail(f->object.heap);
}
static bool spectator(MCObject *o) {
    Fixture *f = (Fixture *)o;
    enter(f, 'S');
    return f->spectator;
}
static bool creative(MCObject *o) {
    Fixture *f = (Fixture *)o;
    enter(f, 'C');
    return f->creative;
}
static int32_t duration(MCObject *o, ItemStack *s) {
    Fixture *f = (Fixture *)o;
    CHECK(s == f->returned);
    enter(f, 'D');
    return f->mode == 3 ? 1 : 0;
}
static bool using_item(MCObject *o, MCGameplayPlayer *p) {
    Fixture *f = (Fixture *)o;
    CHECK(p == f->player);
    enter(f, 'I');
    return f->usingItem;
}
static bool send_container(MCObject *o, MCGameplayPlayer *p, Container *container) {
    Fixture *f = (Fixture *)o;
    CHECK(p == f->player && container == &p->inventoryContainer->container);
    enter(f, 'P');
    if (f->failSend)
        return false;
    return EntityPlayerMPWindows_sendContainerToPlayer(p, container);
}
static int32_t decrement(int32_t value) {
    uint32_t bits = (uint32_t)value - 1;
    int32_t out;
    memcpy(&out, &bits, sizeof out);
    return out;
}
static bool use(MCObject *o, const Item *i, ItemStack *s, MCObject *world, MCObject *actor,
                ItemStack **out) {
    Fixture *f = (Fixture *)o;
    CHECK(s == f->input && i == s->item && world == (MCObject *)f->player->worldObj &&
          actor == (MCObject *)f->player);
    enter(f, 'U');
    if (f->failEvent == 'U')
        return false;
    if (f->mode == 1)
        s->stackSize = decrement(s->stackSize);
    if (f->mode == 2)
        ItemStack_setItemDamage(s, 9);
    *out = f->returned;
    return true;
}
static const ItemStackUseDependencies itemUse = {use};
static const ItemInWorldManagerUseDependencies deps = {spectator, creative,   &itemUse,
                                                       duration,  using_item, send_container};
static ItemStack *recipe(InventoryCrafting *g, MCObject *world) {
    (void)g;
    (void)world;
    return NULL;
}
static ItemStackArray *remaining(InventoryCrafting *g, MCObject *world) {
    (void)world;
    return ItemStackArray_new(g->object.heap, InventoryCrafting_getSizeInventory(g));
}
static bool created(ItemStack *s, MCObject *world, MCObject *p, int32_t count) {
    (void)s;
    (void)world;
    (void)p;
    (void)count;
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
static bool no_item(const Item *i) {
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
                                              created,
                                              achievement,
                                              drop,
                                              no_item,
                                              no_item,
                                              no_item,
                                              no_item,
                                              armor,
                                              MCGameplayWorld_isRemote,
                                              MCGameplayWorld_isCraftingTable,
                                              MCGameplayPlayer_getDistanceSq};
static Fixture *setup(MCGameplay *game, MCObjectRootScope *scope, int32_t count, int item,
                      int mode) {
    CHECK(MCGameplay_init(game, 4 * 1024 * 1024));
    CHECK(MCObjectRootScope_begin(scope, game->heap));
    CraftingManager *m = CraftingManager_newEmpty(game->heap);
    CHECK(m);
    MCGameplayWorld *w = MCGameplayWorld_new(game->heap, MCGameplay_get(game), NULL, m);
    CHECK(w && MCGameplay_setWorld(game, (MCObject *)w));
    MCGameplayPlayer *p =
        MCGameplayPlayer_new(w, NBTString_fromASCII(game->heap, "Player"), NULL, &crafting);
    CHECK(p);
    CHECK(MCGameplay_setPlayer(game, 0, "11111111-1111-1111-1111-111111111111", (MCObject *)p));
    Fixture *f = (Fixture *)MCObjectHeap_alloc(game->heap, sizeof(*f), &fixtureClass);
    CHECK(f);
    f->player = p;
    p->effects = (MCObject *)f;
    f->mode = mode;
    f->input = ItemStack_new(game->heap, ItemStack_registryItem(item), count, 7);
    CHECK(f->input);
    f->returned = mode < 4    ? f->input
                  : mode == 6 ? NULL
                              : ItemStack_new(game->heap, ItemStack_registryItem(item),
                                              mode == 4 ? 0 : 1, 11);
    CHECK(mode == 6 || f->returned);
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory, 0, f->input));
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory, 36, f->input));
    CHECK(MCGameplayPackets_bind(p));
    return f;
}
static void finish(MCGameplay *g, MCObjectRootScope *scope, bool failed) {
    CHECK(MCObjectHeap_failed(g->heap) == failed);
    MCObjectRootScope_end(scope);
    CHECK(MCGameplay_free(g));
}
static void vectors(void) {
    const int32_t counts[] = {INT32_MIN, -1, 0, 1, 2, 127, INT32_MAX};
    for (unsigned ci = 0; ci < 7; ci++)
        for (int item = 0; item < 2; item++)
            for (int mode = 0; mode < 6; mode++)
                for (int c = 0; c < 2; c++)
                    for (int busy = 0; busy < 2; busy++) {
                        MCGameplay g = {0};
                        MCObjectRootScope scope = {0};
                        Fixture *f = setup(&g, &scope, counts[ci], item ? 276 : 1, mode);
                        f->creative = c;
                        f->usingItem = busy;
                        /* Manager game type is independent from player capabilities. */
                        f->player->capabilities->isCreativeMode = !c;
                        bool changed = true;
                        CHECK(ItemInWorldManager_tryUseItem(f->player, f->player->worldObj,
                                                            f->input, &deps, (MCObject *)f,
                                                            &changed));
                        CHECK(changed == (mode != 0));
                        ItemStack *expected = mode == 0 ? f->input : f->returned;
                        int32_t expectedCount = mode == 1   ? decrement(counts[ci])
                                                : mode < 4  ? counts[ci]
                                                : mode == 4 ? 0
                                                            : 1;
                        if (mode != 0 && c)
                            expectedCount = counts[ci];
                        CHECK(f->returned->stackSize == expectedCount);
                        if (mode != 0 && expectedCount == 0)
                            expected = NULL;
                        CHECK(InventoryPlayer_getCurrentItem(f->player->inventory) == expected);
                        CHECK(InventoryPlayer_getStackInSlot(f->player->inventory, 36) == f->input);
                        CHECK(f->input->stackSize ==
                              (mode == 1 ? (c ? counts[ci] : decrement(counts[ci])) : counts[ci]));
                        int32_t expectedDamage = mode == 2 ? 9 : mode >= 4 ? 11 : 7;
                        if (mode != 0 && c && item)
                            expectedDamage = 7;
                        CHECK(f->returned->itemDamage == expectedDamage);
                        const char *prefix = mode == 0                ? "SUD"
                                             : mode == 2 || mode == 3 ? "SUDC"
                                                                      : "SUC";
                        char calls[16];
                        snprintf(calls, sizeof calls, "%s%s", prefix,
                                 mode == 0 ? ""
                                 : busy    ? "I"
                                           : "IP");
                        CHECK(!strcmp(f->events, calls));
                        CHECK(MCGameplayPackets_count(f->player) == (mode != 0 && !busy ? 2 : 0));
                        if (mode != 0 && !busy) {
                            MCGameplayPacketKind kind;
                            S30PacketWindowItems *packet =
                                (S30PacketWindowItems *)MCGameplayPackets_packetAt(f->player, 0,
                                                                                   &kind);
                            CHECK(kind == MC_GAMEPLAY_PACKET_ITEMS && packet->windowId == 0 &&
                                  packet->itemStacks->length == 45);
                            ItemStack *encoded = packet->itemStacks->items[36];
                            CHECK(expected ? (encoded && encoded != expected &&
                                              encoded->stackSize == expected->stackSize)
                                           : encoded == NULL);
                            CHECK(MCGameplayPackets_packetAt(f->player, 1, &kind) &&
                                  kind == MC_GAMEPLAY_PACKET_SLOT);
                        }
                        finish(&g, &scope, false);
                    }
}
static void short_circuit_and_failures(void) {
    MCGameplay g = {0};
    MCObjectRootScope scope = {0};
    Fixture *f = setup(&g, &scope, 0, 1, 0);
    f->spectator = true;
    bool changed = true;
    CHECK(ItemInWorldManager_tryUseItem(f->player, f->player->worldObj, NULL, &deps, (MCObject *)f,
                                        &changed) &&
          !changed);
    CHECK(!strcmp(f->events, "S") && MCGameplayPackets_count(f->player) == 0 &&
          InventoryPlayer_getCurrentItem(f->player->inventory) == f->input);
    finish(&g, &scope, false);
    for (int c = 0; c < 2; c++) {
        g = (MCGameplay){0};
        scope = (MCObjectRootScope){0};
        f = setup(&g, &scope, 2, 1, 6);
        f->creative = c;
        changed = false;
        CHECK(!ItemInWorldManager_tryUseItem(f->player, f->player->worldObj, f->input, &deps,
                                             (MCObject *)f, &changed));
        CHECK(!changed && !InventoryPlayer_getCurrentItem(f->player->inventory) &&
              !strcmp(f->events, "SUC"));
        finish(&g, &scope, true);
    }
    g = (MCGameplay){0};
    scope = (MCObjectRootScope){0};
    f = setup(&g, &scope, 3, 276, 2);
    f->creative = true;
    NBTTagCompound *tag = NBTTagCompound_new(g.heap);
    CHECK(tag &&
          NBTTagCompound_setTag_ascii(tag, "Unbreakable", (NBTBase *)NBTTagByte_new(g.heap, 1)) &&
          ItemStack_setTagCompound(f->input, tag));
    changed = false;
    CHECK(ItemInWorldManager_tryUseItem(f->player, f->player->worldObj, f->input, &deps,
                                        (MCObject *)f, &changed));
    CHECK(changed && f->input->stackSize == 3 && f->input->itemDamage == 9);
    finish(&g, &scope, false);
    g = (MCGameplay){0};
    scope = (MCObjectRootScope){0};
    f = setup(&g, &scope, 2, 1, 5);
    f->failSend = true;
    changed = false;
    CHECK(!ItemInWorldManager_tryUseItem(f->player, f->player->worldObj, f->input, &deps,
                                         (MCObject *)f, &changed));
    CHECK(!changed && InventoryPlayer_getCurrentItem(f->player->inventory) == f->returned &&
          !strcmp(f->events, "SUCIP"));
    finish(&g, &scope, true);
}
static void callback_failure_stops_source_flow(void) {
    const char events[] = {'S', 'U', 'D', 'C', 'I'};
    const char *prefix[] = {"S", "SU", "SUD", "SUC", "SUCI"};
    for (unsigned i = 0; i < sizeof events; i++) {
        MCGameplay g = {0};
        MCObjectRootScope scope = {0};
        Fixture *f = setup(&g, &scope, 2, 1, i == 2 ? 3 : 5);
        f->failEvent = events[i];
        f->usingItem = false;
        bool changed = false;
        CHECK(!ItemInWorldManager_tryUseItem(f->player, f->player->worldObj, f->input, &deps,
                                             (MCObject *)f, &changed));
        CHECK(!changed && !strcmp(f->events, prefix[i]));
        CHECK(MCGameplayPackets_count(f->player) == 0);
        finish(&g, &scope, true);
    }
}
static void native_map_and_drop_bindings(void) {
    mc_world terrain;
    mc_world_init(&terrain, 919);
    for (int count = 0; count < 3; count++)
        for (int creativeMode = 0; creativeMode < 2; creativeMode++) {
            MCGameplay parent = {0};
            CHECK(mc_server_graph_init(&parent, &terrain, 0, 0, 919));
            MCGameplayTransaction tx = {0};
            CHECK(MCGameplay_begin(&parent, &tx));
            MCObjectRootScope scope = {0};
            CHECK(MCObjectRootScope_begin(&scope, tx.working.heap));
            CHECK(mc_server_graph_add_player(&tx.working, 0, "11111111-1111-1111-1111-111111111111",
                                             "One", 1, 0, 64, 0, creativeMode));
            CHECK(mc_server_graph_add_player(&tx.working, 1, "22222222-2222-2222-2222-222222222222",
                                             "Two", 2, 0, 64, 0, false));
            MCGameplayPlayer *one = mc_server_graph_player(&tx.working, 0),
                             *two = mc_server_graph_player(&tx.working, 1);
            CHECK(one && two);
            ItemStack *input =
                ItemStack_new(tx.working.heap, ItemStack_registryItem(395), count, 0);
            CHECK(input);
            CHECK(InventoryPlayer_setInventorySlotContents(one->inventory, 0, input));
            CHECK(InventoryPlayer_setInventorySlotContents(two->inventory, 0, input));
            CHECK(mc_server_graph_use_item(one));
            CHECK(one->worldObj->maps.count == 1 && one->worldObj->maps.next_id == 1);
            ItemStack *held = InventoryPlayer_getCurrentItem(one->inventory);
            CHECK(count == 0 && creativeMode ? held == NULL : held != NULL);
            if (held)
                CHECK(held->item == ItemStack_registryItem(count == 2 ? 395 : 358) &&
                      held->stackSize == (creativeMode ? count : 1));
            CHECK(InventoryPlayer_getCurrentItem(two->inventory) == input);
            CHECK(input->stackSize == (creativeMode && count == 2 ? 2 : count - 1));
            CHECK(MCGameplayPackets_count(one) == 2 && MCGameplayPackets_validate(one));
            CHECK(mc_server_graph_world(&parent)->maps.count == 0 &&
                  mc_server_graph_player(&parent, 0) == NULL);
            MCObjectRootScope_end(&scope);
            CHECK(MCGameplay_abort(&tx) && MCGameplay_free(&parent));
        }
    const int32_t counts[] = {0, 1, 2, -1};
    for (unsigned ci = 0; ci < 4; ci++)
        for (int all = 0; all < 2; all++) {
            MCGameplay parent = {0};
            CHECK(mc_server_graph_init(&parent, &terrain, 0, 0, 919));
            MCGameplayTransaction tx = {0};
            CHECK(MCGameplay_begin(&parent, &tx));
            MCObjectRootScope scope = {0};
            CHECK(MCObjectRootScope_begin(&scope, tx.working.heap));
            CHECK(mc_server_graph_add_player(&tx.working, 0, "11111111-1111-1111-1111-111111111111",
                                             "One", 1, 0, 64, 0, false));
            CHECK(mc_server_graph_add_player(&tx.working, 1, "22222222-2222-2222-2222-222222222222",
                                             "Two", 2, 0, 64, 0, false));
            MCGameplayPlayer *one = mc_server_graph_player(&tx.working, 0),
                             *two = mc_server_graph_player(&tx.working, 1);
            CHECK(one && two);
            ItemStack *input =
                ItemStack_new(tx.working.heap, ItemStack_registryItem(1), counts[ci], 0);
            CHECK(input);
            CHECK(InventoryPlayer_setInventorySlotContents(one->inventory, 0, input));
            CHECK(InventoryPlayer_setInventorySlotContents(two->inventory, 0, input));
            CHECK(mc_server_graph_drop(one, all));
            MCGameplayObjects *owners = MCGameplay_get(&tx.working);
            CHECK(owners);
            CHECK(owners->itemCount == (counts[ci] == 0 ? 0u : 1u));
            CHECK(InventoryPlayer_getCurrentItem(two->inventory) == input);
            if (owners->itemCount) {
                EntityItem *e = (EntityItem *)owners->items[0];
                CHECK(EntityItem_isInstance((MCObject *)e));
                ItemStack *watched = EntityItem_getEntityItem(e);
                CHECK(e->delayBeforeCanPickup == 40 && e->thrower == one->name);
                bool whole = all || counts[ci] <= 1;
                CHECK(whole ? watched == input : watched != input);
                CHECK(watched->stackSize == (whole ? counts[ci] : 1));
                CHECK(InventoryPlayer_getCurrentItem(one->inventory) == (whole ? NULL : input));
                CHECK(input->stackSize == (whole ? counts[ci] : counts[ci] - 1));
                int32_t beforeOne = MCGameplayPackets_count(one),
                        beforeTwo = MCGameplayPackets_count(two);
                CHECK(mc_server_graph_kill_item(e) && e->entity.isDead);
                CHECK(MCGameplayPackets_count(one) == beforeOne + 1 &&
                      MCGameplayPackets_count(two) == beforeTwo + 1);
                CHECK(mc_server_graph_kill_item(e) &&
                      MCGameplayPackets_count(one) == beforeOne + 1 &&
                      MCGameplayPackets_count(two) == beforeTwo + 1);
                CHECK(mc_server_graph_remove_dead(owners) && owners->itemCount == 0);
            } else
                CHECK(!InventoryPlayer_getCurrentItem(one->inventory) && input->stackSize == 0);
            CHECK(MCGameplayPackets_validate(one) && MCGameplayPackets_validate(two));
            CHECK(MCGameplay_get(&parent)->itemCount == 0);
            MCObjectRootScope_end(&scope);
            CHECK(MCGameplay_abort(&tx) && MCGameplay_free(&parent));
        }
    mc_world_free(&terrain);
}
static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}
static uint64_t double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}
static void native_eye_height_and_drop_position(void) {
    /* Literal binary32 results cover independent sleep/sneak predicates. Drop
       position must promote that exact float after the double subtraction. */
    static const uint32_t eyeBits[] = {UINT32_C(0x3fcf5c29), UINT32_C(0x3fc51eb8),
                                       UINT32_C(0x3e4ccccd), UINT32_C(0x3df5c290)};
    static const uint64_t dropYBits[] = {UINT64_C(0x4050747ae1400000), UINT64_C(0x40506f5c28c00000),
                                         UINT64_C(0x4050199999900000),
                                         UINT64_C(0x4050147ae1400000)};
    mc_world terrain;
    mc_world_init(&terrain, 919);
    for (unsigned state = 0; state < 4; ++state) {
        MCGameplay parent = {0};
        CHECK(mc_server_graph_init(&parent, &terrain, 0, 0, 919));
        MCGameplayTransaction tx = {0};
        CHECK(MCGameplay_begin(&parent, &tx));
        MCObjectRootScope scope = {0};
        CHECK(MCObjectRootScope_begin(&scope, tx.working.heap));
        CHECK(mc_server_graph_add_player(&tx.working, 0, "11111111-1111-1111-1111-111111111111",
                                         "Eye", 1, 1.25, 64.5, -9.75, false));
        MCGameplayPlayer *p = mc_server_graph_player(&tx.working, 0);
        CHECK(p);
        p->sleeping = (state & 2) != 0;
        p->sneaking = (state & 1) != 0;
        CHECK(float_bits(EntityPlayer_getEyeHeight(p)) == eyeBits[state]);
        ItemStack *input = ItemStack_new(tx.working.heap, ItemStack_registryItem(1), 1, 0);
        CHECK(input && InventoryPlayer_setInventorySlotContents(p->inventory, 0, input));
        CHECK(mc_server_graph_drop(p, false));
        MCGameplayObjects *owners = MCGameplay_get(&tx.working);
        CHECK(owners && owners->itemCount == 1);
        EntityItem *e = (EntityItem *)owners->items[0];
        CHECK(double_bits(e->entity.posY) == dropYBits[state]);
        CHECK(e->entity.posX == 1.25 && e->entity.posZ == -9.75);
        CHECK(EntityItem_getEntityItem(e) == input && e->delayBeforeCanPickup == 40 &&
              e->thrower == p->name && !InventoryPlayer_getCurrentItem(p->inventory));
        CHECK(MCGameplayPackets_validate(p));
        CHECK(!MCObjectHeap_failed(tx.working.heap) && MCGameplay_get(&parent)->itemCount == 0);
        MCObjectRootScope_end(&scope);
        CHECK(MCGameplay_abort(&tx) && MCGameplay_free(&parent));
    }
    mc_world_free(&terrain);
}
int main(void) {
    vectors();
    short_circuit_and_failures();
    callback_failure_stops_source_flow();
    native_map_and_drop_bindings();
    native_eye_height_and_drop_position();
    printf("source server use: %u checks passed\n", checks);
    return 0;
}
