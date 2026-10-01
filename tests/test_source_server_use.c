#include "world/WorldDataStorage.h"
#include "server/management/ItemInWorldManagerUse.h"
#include "util/MCGameplayPackets.h"
#include "server/native_gameplay.h"
#include "client/native_runtime.h"
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
static int32_t map_next(const MCGameplayWorld *world) {
    int32_t value=-1;CHECK(World_nativeMapNextProjection(world,&value));
    CHECK(world->maps.next_id==0);return value;
}

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
    CHECK(p == f->player && container == p->inventoryContainer);
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
    CHECK(s == f->input && i == s->item && world == (MCObject *)((MCGameplayWorld *)(f->player->living.entity.worldObj)) &&
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
                        CHECK(ItemInWorldManager_tryUseItem(f->player, ((MCGameplayWorld *)(f->player->living.entity.worldObj)),
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
    CHECK(ItemInWorldManager_tryUseItem(f->player, ((MCGameplayWorld *)(f->player->living.entity.worldObj)), NULL, &deps, (MCObject *)f,
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
        CHECK(!ItemInWorldManager_tryUseItem(f->player, ((MCGameplayWorld *)(f->player->living.entity.worldObj)), f->input, &deps,
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
    CHECK(ItemInWorldManager_tryUseItem(f->player, ((MCGameplayWorld *)(f->player->living.entity.worldObj)), f->input, &deps,
                                        (MCObject *)f, &changed));
    CHECK(changed && f->input->stackSize == 3 && f->input->itemDamage == 9);
    finish(&g, &scope, false);
    g = (MCGameplay){0};
    scope = (MCObjectRootScope){0};
    f = setup(&g, &scope, 2, 1, 5);
    f->failSend = true;
    changed = false;
    CHECK(!ItemInWorldManager_tryUseItem(f->player, ((MCGameplayWorld *)(f->player->living.entity.worldObj)), f->input, &deps,
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
        CHECK(!ItemInWorldManager_tryUseItem(f->player, ((MCGameplayWorld *)(f->player->living.entity.worldObj)), f->input, &deps,
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
            CHECK(((MCGameplayWorld *)(one->living.entity.worldObj))->maps.count == 1 && map_next((MCGameplayWorld *)one->living.entity.worldObj) == 1);
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
                CHECK(e->delayBeforeCanPickup == 40 && e->thrower == one->gameProfile->name);
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
        CHECK(Entity_setSneaking(&p->living.entity,(state & 1) != 0));
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
              e->thrower == p->gameProfile->name && !InventoryPlayer_getCurrentItem(p->inventory));
        CHECK(MCGameplayPackets_validate(p));
        CHECK(!MCObjectHeap_failed(tx.working.heap) && MCGameplay_get(&parent)->itemCount == 0);
        MCObjectRootScope_end(&scope);
        CHECK(MCGameplay_abort(&tx) && MCGameplay_free(&parent));
    }
    mc_world_free(&terrain);
}
static bool any_entity(const MCObject *object,void *context) {
    (void)object;(void)context;return true;
}
static void native_client_virtual_eye_and_drop_position(void) {
    /* SP.isSneaking reads MovementInput rather than Entity's watcher, and
       suppresses sneak while sleeping. These binary64 literals come from the
       original float eye-height calculation promoted in dropItem's position. */
    static const uint64_t dropYBits[] = {UINT64_C(0x4050747ae1400000),
                                         UINT64_C(0x40506f5c28c00000),
                                         UINT64_C(0x4050199999900000),
                                         UINT64_C(0x4050199999900000)};
    mc_world terrain;
    mc_world_init(&terrain,919);
    for(unsigned state=0;state<4;state++) {
        MCGameplay parent={0};CHECK(mc_client_graph_init(&parent,&terrain,"Eye"));
        MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&parent,&tx));
        MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
        MCClientBindings *b=mc_client_graph_bindings(&tx.working);
        MCGameplayPlayer *p=mc_client_graph_player(&tx.working);
        CHECK(b&&p&&b->player==p&&b->sp->nativeActor==p&&p->effects==(MCObject *)b);
        CHECK(b!=mc_client_graph_bindings(&parent));
        CHECK(Entity_setPosition(&p->living.entity,1.25,64.5,-9.75));
        p->sleeping=(state&2)!=0;
        CHECK(mc_client_graph_set_input(&tx.working,0,0,false,(state&1)!=0,false));
        CHECK(EntityPlayerSP_isSneaking(b->sp)==(state==1));
        CHECK(!Entity_isSneaking(&p->living.entity));
        /* Source SP.joinEntityItemWithWorld is empty: retain a real spawned
           class witness, then observe the transient constructed drop while
           this borrow scope prevents collection. It is not a world mirror. */
        CHECK(mc_client_graph_spawn_item(&tx.working,99,8,64,8,0,0,0));
        EntityItem *reference=mc_client_graph_item(&tx.working,99);CHECK(reference);
        ItemStack *input=ItemStack_new(tx.working.heap,ItemStack_registryItem(1),1,0);
        CHECK(input&&mc_client_graph_crafting()->drop((MCObject *)p,input,false));
        EntityItem *created=(EntityItem *)MCObjectHeap_findObject(tx.working.heap,
            reference->entity.object.klass,any_entity,NULL);
        CHECK(created&&created!=reference);
        CHECK(double_bits(created->entity.posY)==dropYBits[state]);
        CHECK(created->entity.posX==1.25&&created->entity.posZ==-9.75);
        CHECK(EntityItem_getEntityItem(created)==input&&created->delayBeforeCanPickup==40);
        CHECK(!Entity_isSneaking(&p->living.entity));
        CHECK(MCGameplay_get(&tx.working)->itemCount==1&&MCGameplay_get(&parent)->itemCount==0);
        CHECK(!MCObjectHeap_failed(tx.working.heap));
        MCObjectRootScope_end(&scope);
        CHECK(MCGameplay_abort(&tx)&&MCGameplay_free(&parent));
    }
    mc_world_free(&terrain);
}
typedef struct {
    MCObject object;
    MCGameplayPlayer *captured,*owner,*replacement;
    char events[4],failEvent;
    unsigned eventCount;
    bool sneak,mutateSleeping,failViaHeap;
} EyeFixture;
static void trace_eye(MCObject *object,MCObjectVisitor visitor,void *context) {
    EyeFixture *f=(EyeFixture *)object;
    f->captured=(MCGameplayPlayer *)visitor((MCObject *)f->captured,context);
    f->owner=(MCGameplayPlayer *)visitor((MCObject *)f->owner,context);
    f->replacement=(MCGameplayPlayer *)visitor((MCObject *)f->replacement,context);
}
static const MCObjectClass eyeFixtureClass={"fixture.source.virtual.eye",
    MCObjectHeap_plainClone,trace_eye,NULL};
static bool enter_eye(EyeFixture *f,MCGameplayPlayer *p,char event) {
    CHECK(p==f->captured&&p->living.entity.object.heap==f->object.heap);
    CHECK(f->eventCount+1<sizeof f->events);
    f->events[f->eventCount++]=event;f->events[f->eventCount]=0;
    CHECK(MCObjectHeap_hasBorrowers(f->object.heap)&&!MCObjectHeap_collect(f->object.heap));
    if(f->failEvent==event&&f->failViaHeap)MCObjectHeap_fail(f->object.heap);
    return f->failEvent!=event||f->failViaHeap;
}
static bool virtual_sleeping(MCObject *context,MCGameplayPlayer *p,bool *out) {
    EyeFixture *f=(EyeFixture *)context;
    bool ok=enter_eye(f,p,'S');*out=p->sleeping;
    if(f->mutateSleeping) {
        p->sleeping=!p->sleeping;f->owner=f->replacement;
        MCObjectHeap_touch(f->object.heap);
    }
    return ok;
}
static bool virtual_sneaking(MCObject *context,MCGameplayPlayer *p,bool *out) {
    EyeFixture *f=(EyeFixture *)context;
    bool ok=enter_eye(f,p,'N');*out=f->sneak;return ok;
}
static const EntityPlayerEyeHeightDependencies virtual_eye={virtual_sleeping,virtual_sneaking};
static void virtual_eye_order_mutation_and_failure(void) {
    /* Virtual predicates are observable dependencies: reversing their order,
       rereading the first result or owner edge, skipping the sleeping branch's
       second call, or undoing a thrown getter mutation must fail these cases. */
    static const uint32_t expected[]={UINT32_C(0x3fcf5c29),UINT32_C(0x3fc51eb8),
        UINT32_C(0x3e4ccccd),UINT32_C(0x3df5c290)};
    for(unsigned mode=0;mode<13;mode++) {
        MCObjectHeap *h=MCObjectHeap_new(1024u*1024u);CHECK(h);
        EyeFixture *f=(EyeFixture *)MCObjectHeap_alloc(h,sizeof(*f),&eyeFixtureClass);CHECK(f);
        f->captured=MCGameplayPlayer_nativeAllocate(h);
        f->replacement=MCGameplayPlayer_nativeAllocate(h);
        CHECK(f->captured&&f->replacement);f->owner=f->captured;
        MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)f));
        f->captured->sleeping=mode<4?(mode&2)!=0:mode==5;
        f->sneak=mode<4?(mode&1)!=0:true;
        f->mutateSleeping=mode>=4;
        if(mode>=6&&mode<=9) {
            f->failEvent=(mode&1)?'N':'S';f->failViaHeap=mode>=8;
        }
        EntityPlayerEyeHeightDependencies deps=virtual_eye;
        if(mode==10)deps.isSneaking=NULL;
        if(mode==11)deps.isPlayerSleeping=NULL;
        float value=EntityPlayer_getEyeHeightWithDispatch(f->captured,(MCObject *)f,
            mode==12?NULL:&deps);
        const char *events=mode>=11?"":mode==6||mode==8||mode==10?"S":"SN";
        CHECK(!strcmp(f->events,events));
        CHECK(MCObjectHeap_failed(h)==(mode>=6));
        CHECK(!MCObjectHeap_hasBorrowers(h));
        if(mode<6)CHECK(float_bits(value)==expected[mode<4?mode:mode==4?1:3]);
        else CHECK(float_bits(value)==0);
        if(mode>=4&&mode<11) {
            CHECK(f->owner==f->replacement&&f->owner!=f->captured);
            CHECK(f->captured->sleeping==(mode!=5));
        }
        if(mode<6) {
            /* The dependency graph retains exact actors across clone/collect;
               callbacks receive cloned references, not original-heap pointers. */
            MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);
            MCObjectRoot copiedRoot={0};CHECK(MCObjectRoot_rebind(&copiedRoot,copy,&root));
            EyeFixture *cloned=(EyeFixture *)MCObjectRoot_get(&copiedRoot);
            CHECK(cloned&&cloned!=f&&cloned->captured!=f->captured);
            CHECK(cloned->captured->living.entity.object.heap==copy);
            CHECK(MCObjectHeap_collect(copy));
            cloned->eventCount=0;cloned->events[0]=0;cloned->mutateSleeping=false;
            CHECK(float_bits(EntityPlayer_getEyeHeightWithDispatch(cloned->captured,
                (MCObject *)cloned,&virtual_eye))==expected[mode<4?mode:mode==4?3:1]);
            CHECK(!strcmp(cloned->events,"SN")&&!MCObjectHeap_failed(copy));
            MCObjectRoot_drop(&copiedRoot);MCObjectHeap_free(copy);
        }
        MCObjectRoot_drop(&root);MCObjectHeap_free(h);
    }
    /* A foreign managed callback context cannot run or mutate either graph. */
    MCObjectHeap *h=MCObjectHeap_new(1024u*1024u),*foreign=MCObjectHeap_new(1024u*1024u);
    CHECK(h&&foreign);
    MCGameplayPlayer *p=MCGameplayPlayer_nativeAllocate(h);CHECK(p);
    EyeFixture *f=(EyeFixture *)MCObjectHeap_alloc(foreign,sizeof(*f),&eyeFixtureClass);CHECK(f);
    CHECK(EntityPlayer_getEyeHeightWithDispatch(p,(MCObject *)f,&virtual_eye)==0);
    CHECK(MCObjectHeap_failed(h)&&!MCObjectHeap_failed(foreign)&&!f->eventCount);
    CHECK(!MCObjectHeap_hasBorrowers(h));
    MCObjectHeap_free(h);MCObjectHeap_free(foreign);
}
int main(void) {
    vectors();
    short_circuit_and_failures();
    callback_failure_stops_source_flow();
    native_map_and_drop_bindings();
    native_eye_height_and_drop_position();
    native_client_virtual_eye_and_drop_position();
    virtual_eye_order_mutation_and_failure();
    printf("source server use: %u checks passed\n", checks);
    return 0;
}
