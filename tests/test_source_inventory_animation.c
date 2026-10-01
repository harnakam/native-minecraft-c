#include "world/WorldDataStorage.h"
#include "entity/player/InventoryPlayerAnimations.h"
#include "item/ItemAnimation.h"
#include "item/ItemMap.h"
#include "nbt/NBTTagCompound.h"
#include "server/native_gameplay.h"
#include "util/MCGameplayPackets.h"
#include "util/MCGameplayWorld.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static int32_t map_next(const MCGameplayWorld *world) {
    int32_t value=-1;CHECK(World_nativeMapNextProjection(world,&value));
    CHECK(world->maps.next_id==0);return value;
}

typedef struct { MCObject object; MCObject *world; } Actor;
typedef struct {
    MCObject object;
    InventoryPlayer *inventory;
    Actor *first, *second;
    ItemStack *replacement, *captured;
    ItemStackArray *replacementArray;
    int32_t slots[80], observedAnimation[80];
    bool selected[80];
    MCObject *worlds[80], *actors[80];
    unsigned updates, worldReads;
    int mode, failSlot;
} Fixture;
static void actor_trace(MCObject *o, MCObjectVisitor v, void *c) {
    Actor *a = (Actor *)o;
    a->world = v(a->world, c);
}
static void fixture_trace(MCObject *o, MCObjectVisitor v, void *c) {
    Fixture *f = (Fixture *)o;
    f->inventory = (InventoryPlayer *)v((MCObject *)f->inventory, c);
    f->first = (Actor *)v((MCObject *)f->first, c);
    f->second = (Actor *)v((MCObject *)f->second, c);
    f->replacement = (ItemStack *)v((MCObject *)f->replacement, c);
    f->captured = (ItemStack *)v((MCObject *)f->captured, c);
    f->replacementArray = (ItemStackArray *)v((MCObject *)f->replacementArray, c);
    for (unsigned i = 0; i < f->updates; ++i) {
        f->worlds[i] = v(f->worlds[i], c);
        f->actors[i] = v(f->actors[i], c);
    }
}
static const MCObjectClass actorClass = {"fixture.animation.actor", MCObjectHeap_plainClone,
                                         actor_trace, NULL};
static const MCObjectClass fixtureClass = {"fixture.animation.context", MCObjectHeap_plainClone,
                                           fixture_trace, NULL};
static const MCObjectClass worldClass = {"fixture.animation.world", MCObjectHeap_plainClone,
                                         NULL, NULL};
static Fixture *getterFixture;
static bool creative(const MCObject *p) { (void)p; return false; }
static ItemStack *make(MCObjectHeap *h, int32_t id, int32_t count, int32_t animation) {
    ItemStack *s = ItemStack_new(h, ItemStack_registryItem(id), count, 7);
    CHECK(s);
    s->animationsToGo = animation;
    return s;
}
static Fixture *fixture(MCObjectHeap *h) {
    Fixture *f = (Fixture *)MCObjectHeap_alloc(h, sizeof(*f), &fixtureClass);
    CHECK(f);
    f->first = (Actor *)MCObjectHeap_alloc(h, sizeof(*f->first), &actorClass);
    f->second = (Actor *)MCObjectHeap_alloc(h, sizeof(*f->second), &actorClass);
    CHECK(f->first && f->second);
    f->first->world = MCObjectHeap_alloc(h, sizeof(MCObject), &worldClass);
    f->second->world = MCObjectHeap_alloc(h, sizeof(MCObject), &worldClass);
    CHECK(f->first->world && f->second->world);
    f->inventory = InventoryPlayer_new(h, (MCObject *)f->first, creative);
    CHECK(f->inventory);
    f->failSlot = INT32_MIN;
    getterFixture = f;
    return f;
}
/* Explicit source-world and virtual Item fixtures expose call order; they are
   not production replacements for Item subclasses or EntityPlayer/worlds. */
static MCObject *get_world(MCObject *player) {
    Fixture *f = getterFixture;
    CHECK(player && player->klass == &actorClass);
    CHECK(MCObjectHeap_hasBorrowers(player->heap));
    ++f->worldReads;
    MCObject *world = ((Actor *)player)->world;
    if (f->mode == 2 && f->worldReads == 1) {
        f->inventory->player = (MCObject *)f->second;
        f->inventory->currentItem = 0;
        f->inventory->mainInventory->items[0] = f->replacement;
    }
    if (f->mode == 4) MCObjectHeap_fail(player->heap);
    return world;
}
static bool update(MCObject *context, const Item *item, ItemStack *s, MCObject *world,
                   MCObject *player, int32_t slot, bool selected) {
    Fixture *f = (Fixture *)context;
    CHECK(item == ItemStack_getItem(s));
    CHECK(MCObjectHeap_hasBorrowers(s->object.heap));
    CHECK(!MCObjectHeap_collect(s->object.heap));
    CHECK(f->updates < 80);
    unsigned i = f->updates++;
    f->slots[i] = slot;
    f->selected[i] = selected;
    f->observedAnimation[i] = s->animationsToGo;
    f->worlds[i] = world;
    f->actors[i] = player;
    if (f->mode == 1 && slot == 0) {
        f->inventory->mainInventory = f->replacementArray;
        f->inventory->currentItem = 2;
        f->inventory->player = (MCObject *)f->second;
    }
    if (f->mode == 3 && slot == 0) f->inventory->mainInventory = NULL;
    if (f->mode == 5) MCObjectHeap_fail(s->object.heap);
    return slot != f->failSlot;
}
static const ItemStackAnimationDependencies itemDependencies = {update};
static const InventoryPlayerAnimationDependencies dependencies = {get_world, &itemDependencies};

static void signed_counts_and_animation(void) {
    const int32_t animation[] = {INT32_MIN, -1, 0, 1, 5, INT32_MAX};
    const int32_t after[] = {INT32_MIN, -1, 0, 0, 4, INT32_MAX - 1};
    const int32_t count[] = {INT32_MIN, -1, 0, 1, 127, INT32_MAX};
    MCObjectHeap *h = MCObjectHeap_new(4 * 1024 * 1024);
    CHECK(h);
    Fixture *f = fixture(h);
    for (size_t a = 0; a < sizeof(animation) / sizeof(*animation); ++a) {
        for (size_t c = 0; c < sizeof(count) / sizeof(*count); ++c) {
            ItemStack *s = make(h, 1, count[c], animation[a]);
            unsigned before = f->updates;
            CHECK(ItemStack_updateAnimation(s, NULL, NULL, INT32_MAX, true,
                                             &itemDependencies, (MCObject *)f));
            CHECK(s->animationsToGo == after[a] && s->stackSize == count[c] && s->itemDamage == 7);
            CHECK(f->updates == before + 1 && f->observedAnimation[before] == after[a]);
            CHECK(f->slots[before] == INT32_MAX && f->selected[before]);
            CHECK(!f->worlds[before] && !f->actors[before]);
            Item_onUpdate(s->item, s, NULL, NULL, -9, false);
            CHECK(s->animationsToGo == after[a] && s->stackSize == count[c]);
        }
    }
    CHECK(!MCObjectHeap_failed(h) && !MCObjectHeap_hasBorrowers(h));
    MCObjectHeap_free(h);
}
static void main_only_and_aliases(void) {
    MCObjectHeap *h = MCObjectHeap_new(4 * 1024 * 1024);
    CHECK(h);
    Fixture *f = fixture(h);
    InventoryPlayer *p = f->inventory;
    ItemStack *shared = make(h, 1, 0, 5);
    p->mainInventory->items[0] = shared;
    p->mainInventory->items[3] = shared;
    p->mainInventory->items[35] = make(h, 1, -17, 1);
    p->armorInventory->items[0] = make(h, 310, 1, 8);
    p->itemStack = make(h, 1, 1, 9);
    p->currentItem = 3;
    CHECK(InventoryPlayer_decrementAnimations(p, &dependencies, (MCObject *)f));
    CHECK(f->updates == 3 && f->worldReads == 3);
    CHECK(f->slots[0] == 0 && f->slots[1] == 3 && f->slots[2] == 35);
    CHECK(!f->selected[0] && f->selected[1] && !f->selected[2]);
    CHECK(f->observedAnimation[0] == 4 && f->observedAnimation[1] == 3);
    CHECK(shared->animationsToGo == 3 && shared->stackSize == 0);
    CHECK(p->mainInventory->items[35]->animationsToGo == 0);
    CHECK(p->armorInventory->items[0]->animationsToGo == 8 && p->itemStack->animationsToGo == 9);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void callback_replaces_live_fields(void) {
    MCObjectHeap *h = MCObjectHeap_new(4 * 1024 * 1024);
    CHECK(h);
    Fixture *f = fixture(h);
    InventoryPlayer *p = f->inventory;
    ItemStack *first = make(h, 1, 1, 5), *oldLater = make(h, 1, 1, 6);
    p->mainInventory->items[0] = first;
    p->mainInventory->items[1] = oldLater;
    p->currentItem = 1;
    f->replacementArray = ItemStackArray_new(h, 3);
    CHECK(f->replacementArray);
    f->replacementArray->items[1] = make(h, 1, -1, 4);
    f->replacementArray->items[2] = make(h, 1, 0, 3);
    f->mode = 1;
    CHECK(InventoryPlayer_decrementAnimations(p, &dependencies, (MCObject *)f));
    CHECK(f->updates == 3 && f->worldReads == 3);
    CHECK(f->slots[0] == 0 && f->slots[1] == 1 && f->slots[2] == 2);
    CHECK(!f->selected[0] && !f->selected[1] && f->selected[2]);
    CHECK(f->worlds[0] == f->first->world && f->worlds[1] == f->second->world);
    CHECK(f->actors[0] == (MCObject *)f->first && f->actors[1] == (MCObject *)f->second);
    CHECK(first->animationsToGo == 4 && oldLater->animationsToGo == 6);
    CHECK(f->replacementArray->items[1]->animationsToGo == 3 &&
          f->replacementArray->items[2]->animationsToGo == 2);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void argument_evaluation_order(void) {
    MCObjectHeap *h = MCObjectHeap_new(4 * 1024 * 1024);
    CHECK(h);
    Fixture *f = fixture(h);
    InventoryPlayer *p = f->inventory;
    f->captured = make(h, 1, 1, 5);
    f->replacement = make(h, 1, 1, 8);
    p->mainInventory->items[0] = f->captured;
    p->mainInventory->items[1] = make(h, 1, 1, 4);
    p->currentItem = 1;
    f->mode = 2;
    CHECK(InventoryPlayer_decrementAnimations(p, &dependencies, (MCObject *)f));
    CHECK(f->updates == 2 && f->worldReads == 2);
    CHECK(f->captured->animationsToGo == 4 && f->replacement->animationsToGo == 8);
    CHECK(f->worlds[0] == f->first->world && f->actors[0] == (MCObject *)f->second);
    CHECK(f->selected[0] && !f->selected[1] && f->worlds[1] == f->second->world);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void failure_preserves_source_partial_state(void) {
    for (int which = 0; which < 8; ++which) {
        MCObjectHeap *h = MCObjectHeap_new(4 * 1024 * 1024);
        CHECK(h);
        Fixture *f = fixture(h);
        InventoryPlayer *p = f->inventory;
        ItemStack *s = make(h, 1, -1, 5), *later = make(h, 1, 0, 5);
        p->mainInventory->items[0] = s;
        p->mainInventory->items[1] = later;
        InventoryPlayerAnimationDependencies d = dependencies;
        ItemStackAnimationDependencies item = itemDependencies;
        if (which == 0) s->item = NULL;
        if (which == 1) item.onUpdate = NULL;
        if (which == 2) d.item = NULL;
        if (which == 3) f->failSlot = 0;
        if (which == 4) f->mode = 5;
        if (which == 5) f->mode = 4;
        if (which == 6) d.getWorld = NULL;
        if (which == 7) p->player = NULL;
        if (which != 2) d.item = &item;
        CHECK(!InventoryPlayer_decrementAnimations(p, &d, (MCObject *)f));
        CHECK(MCObjectHeap_failed(h) && !MCObjectHeap_hasBorrowers(h));
        CHECK(s->animationsToGo == (which < 5 ? 4 : 5));
        CHECK(later->animationsToGo == 5 && s->stackSize == -1 && later->stackSize == 0);
        CHECK(f->updates == (which == 3 || which == 4 ? 1u : 0u));
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h = MCObjectHeap_new(4 * 1024 * 1024);
    CHECK(h);
    Fixture *f = fixture(h);
    ItemStack *s = make(h, 1, 1, 5);
    f->inventory->mainInventory->items[0] = s;
    f->mode = 3;
    CHECK(!InventoryPlayer_decrementAnimations(f->inventory, &dependencies, (MCObject *)f));
    CHECK(s->animationsToGo == 4 && f->updates == 1 && MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void empty_and_null_world(void) {
    MCObjectHeap *h = MCObjectHeap_new(4 * 1024 * 1024);
    CHECK(h);
    Fixture *f = fixture(h);
    f->inventory->player = NULL;
    CHECK(InventoryPlayer_decrementAnimations(f->inventory, NULL, NULL));
    CHECK(!f->updates && !f->worldReads);
    f->inventory->player = (MCObject *)f->first;
    f->first->world = NULL;
    f->inventory->mainInventory->items[3] = make(h, 1, 0, 1);
    CHECK(InventoryPlayer_decrementAnimations(f->inventory, &dependencies, (MCObject *)f));
    CHECK(f->updates == 1 && f->worldReads == 1 && !f->worlds[0]);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void snapshot_preserves_aliases_and_animation(void) {
    MCObjectHeap *h = MCObjectHeap_new(4 * 1024 * 1024);
    CHECK(h);
    Fixture *f = fixture(h);
    ItemStack *s = make(h, 1, 0, 5);
    f->inventory->mainInventory->items[0] = s;
    f->inventory->mainInventory->items[3] = s;
    MCObjectRoot root = {0};
    CHECK(MCObjectRoot_init(&root, h, (MCObject *)f));
    MCObjectHeap *clone = MCObjectHeap_clone(h);
    CHECK(clone);
    MCObjectRoot branch = {0};
    CHECK(MCObjectRoot_rebind(&branch, clone, &root));
    Fixture *copy = (Fixture *)MCObjectRoot_get(&branch);
    CHECK(copy && copy != f);
    InventoryPlayer *p = copy->inventory;
    CHECK(p->mainInventory->items[0] == p->mainInventory->items[3]);
    CHECK(p->mainInventory->items[0] != s && p->mainInventory->items[0]->animationsToGo == 5);
    getterFixture = copy;
    CHECK(InventoryPlayer_decrementAnimations(p, &dependencies, (MCObject *)copy));
    CHECK(p->mainInventory->items[0]->animationsToGo == 3 && s->animationsToGo == 5);
    CHECK(copy->worlds[0] == copy->first->world && copy->first->world != f->first->world);
    ItemStack *originalCopy = ItemStack_copy(clone, p->mainInventory->items[0]);
    CHECK(originalCopy && originalCopy->animationsToGo == 0);
    CHECK(!MCObjectHeap_failed(h) && !MCObjectHeap_failed(clone));
    MCObjectHeap_free(clone);
    MCObjectHeap_free(h);
}
static bool real_base_map_dispatch(MCObject *context, const Item *self, ItemStack *s,
                                  MCObject *world, MCObject *entity, int32_t slot, bool selected) {
    (void)context;
    if (self == ItemStack_registryItem(358)) {
        bool changed = true;
        bool ok = ItemMap_onUpdate(s, (MCGameplayWorld *)world, entity, slot, selected, &changed);
        CHECK(!changed);
        return ok;
    }
    CHECK(self == ItemStack_registryItem(1));
    Item_onUpdate(self, s, world, entity, slot, selected);
    return true;
}
static void actual_remote_map_does_not_create(void) {
    MCGameplay game = {0};
    CHECK(MCGameplay_init(&game, 8 * 1024 * 1024));
    MCObjectHeap *h = game.heap;
    MCObjectRootScope scope = {0};
    CHECK(MCObjectRootScope_begin(&scope, h));
    MCGameplayWorld *world = MCGameplayWorld_new(h, MCGameplay_get(&game), NULL, NULL);
    CHECK(world && MCGameplay_setWorld(&game, (MCObject *)world));
    world->remote = true;
    world->worldTime = 4321;
    CHECK(World_nativeImportMapNextProjection(world,42));
    Fixture *f = fixture(h);
    f->first->world = (MCObject *)world;
    ItemStack *map = make(h, 358, -1, 5);
    map->itemDamage = 999;
    map->stackTagCompound = NBTTagCompound_new(h);
    CHECK(map->stackTagCompound && NBTTagCompound_setInteger_ascii(map->stackTagCompound, "foreign", 17));
    f->inventory->mainInventory->items[0] = map;
    f->inventory->mainInventory->items[8] = map;
    f->inventory->currentItem = 8;
    f->inventory->mainInventory->items[35] = make(h, 1, 0, 2);
    const ItemStackAnimationDependencies virtual = {real_base_map_dispatch};
    const InventoryPlayerAnimationDependencies d = {get_world, &virtual};
    size_t objects = MCObjectHeap_liveObjects(h), bytes = MCObjectHeap_liveBytes(h);
    CHECK(InventoryPlayer_decrementAnimations(f->inventory, &d, (MCObject *)f));
    CHECK(map->animationsToGo == 3 && map->stackSize == -1 && map->itemDamage == 999);
    CHECK(NBTTagCompound_getInteger_ascii(map->stackTagCompound, "foreign") == 17);
    CHECK(map_next(world) == 42 && !world->maps.count && world->worldTime == 4321);
    CHECK(MCObjectHeap_liveObjects(h) == objects && MCObjectHeap_liveBytes(h) == bytes);
    CHECK(f->inventory->mainInventory->items[35]->animationsToGo == 1);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectRootScope_end(&scope);
    CHECK(MCGameplay_free(&game));
}
static void native_argument_boundaries(void) {
    for (int which = 0; which < 3; ++which) {
        MCObjectHeap *h = MCObjectHeap_new(4 * 1024 * 1024);
        MCObjectHeap *foreign = MCObjectHeap_new(1024 * 1024);
        CHECK(h && foreign);
        Fixture *f = fixture(h);
        ItemStack *s = make(h, 1, 1, 5);
        MCObject *other = MCObjectHeap_alloc(foreign, sizeof(MCObject), &worldClass);
        CHECK(other);
        CHECK(!ItemStack_updateAnimation(s, which == 0 ? other : NULL,
                                         which == 1 ? other : NULL, 0, false,
                                         &itemDependencies, which == 2 ? other : (MCObject *)f));
        CHECK(s->animationsToGo == 5 && MCObjectHeap_failed(h) && !MCObjectHeap_failed(foreign));
        CHECK(!MCObjectHeap_hasBorrowers(h) && !MCObjectHeap_hasBorrowers(foreign));
        MCObjectHeap_free(foreign);
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h = MCObjectHeap_new(4 * 1024 * 1024);
    CHECK(h);
    Fixture *f = fixture(h);
    f->inventory->mainInventory = (ItemStackArray *)f->first;
    CHECK(!InventoryPlayer_decrementAnimations(f->inventory, &dependencies, (MCObject *)f));
    CHECK(MCObjectHeap_failed(h) && !f->updates);
    MCObjectHeap_free(h);
    for (int32_t length = -1; length <= 37; length += 38) {
        h = MCObjectHeap_new(4 * 1024 * 1024);
        CHECK(h);
        f = fixture(h);
        f->inventory->mainInventory->length = length;
        CHECK(!InventoryPlayer_decrementAnimations(f->inventory, &dependencies, (MCObject *)f));
        CHECK(MCObjectHeap_failed(h) && !f->updates && !f->worldReads);
        CHECK(!MCObjectHeap_hasBorrowers(h));
        MCObjectHeap_free(h);
    }
}
static const unsigned char unregisteredIdentity = 0;
static void native_registry_identity_is_not_pointer_arithmetic(void) {
    const Item *unregistered = (const Item *)&unregisteredIdentity;
    CHECK(ItemStack_registryId(NULL) == 0 && !ItemStack_registryIsKnownItem(NULL));
    CHECK(ItemStack_registryId(unregistered) == -1);
    CHECK(!ItemStack_registryIsKnownItem(unregistered));
    for (int32_t id = 0; id < 2268; ++id) {
        const Item *known = ItemStack_registryItem(id);
        if (known) {
            CHECK(ItemStack_registryId(known) == id);
            CHECK(ItemStack_registryIsKnownItem(known));
        }
    }
}
static void actual_server_binding_updates_shared_source_refs(void) {
    mc_world terrain;
    mc_world_init(&terrain, 919);
    MCGameplay parent = {0};
    CHECK(mc_server_graph_init(&parent, &terrain, 0, 0, 919));
    MCGameplayTransaction transaction = {0};
    CHECK(MCGameplay_begin(&parent, &transaction));
    MCGameplay *game = &transaction.working;
    MCObjectRootScope scope = {0};
    CHECK(MCObjectRootScope_begin(&scope, game->heap));
    CHECK(mc_server_graph_add_player(game, 0, "11111111-1111-1111-1111-111111111111",
                                     "animation-one", 1, 0.5, 64, 0.5, false));
    CHECK(mc_server_graph_add_player(game, 1, "22222222-2222-2222-2222-222222222222",
                                     "animation-two", 2, 0.5, 64, 0.5, false));
    MCGameplayPlayer *first = mc_server_graph_player(game, 0);
    MCGameplayPlayer *second = mc_server_graph_player(game, 1);
    MCGameplayWorld *world = mc_server_graph_world(game);
    CHECK(first && second && world && !world->remote);
    CHECK(((MCGameplayWorld *)(first->living.entity.worldObj)) == world && ((MCGameplayWorld *)(second->living.entity.worldObj)) == world);
    ItemStack *shared = make(game->heap, 1, 0, 5);
    ItemStack *armor = make(game->heap, 310, -1, 8);
    ItemStack *cursor = make(game->heap, 1, 0, 9);
    CHECK(InventoryPlayer_setInventorySlotContents(first->inventory, 0, shared));
    CHECK(InventoryPlayer_setInventorySlotContents(first->inventory, 3, shared));
    CHECK(InventoryPlayer_setInventorySlotContents(first->inventory, 36, armor));
    CHECK(InventoryPlayer_setItemStack(first->inventory, cursor));
    CHECK(InventoryPlayer_setInventorySlotContents(second->inventory, 7, shared));
    ItemStack *tool = make(game->heap, 276, -1, 2);
    CHECK(InventoryPlayer_setInventorySlotContents(first->inventory, 35, tool));
    size_t objects = MCObjectHeap_liveObjects(game->heap), bytes = MCObjectHeap_liveBytes(game->heap);
    int32_t packets = MCGameplayPackets_count(first) + MCGameplayPackets_count(second);
    int32_t mapId = map_next(world);
    int64_t time = world->worldTime;
    CHECK(mc_server_graph_tick_inventory(first));
    CHECK(shared->animationsToGo == 3 && tool->animationsToGo == 1);
    CHECK(mc_server_graph_tick_inventory(second));
    CHECK(shared->animationsToGo == 2 && shared->stackSize == 0 && tool->stackSize == -1);
    CHECK(armor->animationsToGo == 8 && cursor->animationsToGo == 9);
    CHECK(first->inventory->mainInventory->items[0] == second->inventory->mainInventory->items[7]);
    CHECK(map_next(world) == mapId && world->worldTime == time && !world->maps.count);
    CHECK(MCGameplayPackets_count(first) + MCGameplayPackets_count(second) == packets);
    CHECK(MCObjectHeap_liveObjects(game->heap) == objects && MCObjectHeap_liveBytes(game->heap) == bytes);
    CHECK(!MCObjectHeap_failed(game->heap));
    ItemStack *unknown = ItemStack_new(game->heap, (const Item *)&unregisteredIdentity, 1, 0);
    CHECK(unknown);
    unknown->animationsToGo = 5;
    ItemStack *later = make(game->heap, 1, 1, 5);
    CHECK(InventoryPlayer_setInventorySlotContents(first->inventory, 0, unknown));
    CHECK(InventoryPlayer_setInventorySlotContents(first->inventory, 1, later));
    CHECK(!mc_server_graph_tick_inventory(first));
    CHECK(unknown->animationsToGo == 4 && later->animationsToGo == 5);
    CHECK(MCObjectHeap_failed(game->heap));
    MCObjectRootScope_end(&scope);
    CHECK(MCGameplay_abort(&transaction));
    CHECK(MCGameplay_free(&parent));
    mc_world_free(&terrain);
}
int main(void) {
    signed_counts_and_animation();
    main_only_and_aliases();
    callback_replaces_live_fields();
    argument_evaluation_order();
    failure_preserves_source_partial_state();
    empty_and_null_world();
    snapshot_preserves_aliases_and_animation();
    actual_remote_map_does_not_create();
    native_argument_boundaries();
    native_registry_identity_is_not_pointer_arithmetic();
    actual_server_binding_updates_shared_source_refs();
    printf("Source inventory animation: %u checks passed\n", checks);
    return 0;
}
