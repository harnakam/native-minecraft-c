#include "item/ItemStackCrafting.h"
#include "item/ItemMapCreated.h"
#include "world/WorldDataStorage.h"
#include "world/WorldType.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
typedef MCGameplayWorld World;
static int32_t map_next(const World *w) {
    int32_t value=-1;CHECK(w->maps.next_id==0);
    CHECK(MCObjectHeap_failed(w->object.heap)?MapStorage_nativeGetMapNextProjectionDiagnostic(w->mapStorage,&value):World_nativeMapNextProjection(w,&value));
    return value;
}
typedef struct {
    MCObject object;
    ItemStack *stack;
    int32_t count, observed_amount;
    const Item *observed_item;
    unsigned calls, created;
    bool fail_stat, fail_created, fail_heap, replace_item;
} Player;
static void player_trace(MCObject *object, MCObjectVisitor visit, void *context) {
    Player *p = (Player *)object;
    p->stack = (ItemStack *)visit((MCObject *)p->stack, context);
}
static const MCObjectClass owners_class = {"TestCraftOwners", MCObjectHeap_plainClone, NULL, NULL};
static const MCObjectClass player_class = {"TestCraftPlayer", MCObjectHeap_plainClone, player_trace,
                                           NULL};
static bool any_object(const MCObject *object,void *context) {
    (void)object; (void)context; return true;
}
static World *make_world(MCObjectHeap *heap) {
    MCObjectRootScope scope = {0};
    CHECK(MCObjectRootScope_begin(&scope, heap));
    MCGameplayObjects *owners =
        (MCGameplayObjects *)MCObjectHeap_alloc(heap, sizeof(*owners), &owners_class);
    CHECK(owners);
    World *w = MCGameplayWorld_new(heap, owners, NULL, NULL);
    CHECK(w);
    MCObjectRootScope_end(&scope);
    return w;
}
static Player *make_player(MCObjectHeap *heap, int32_t count) {
    Player *p = (Player *)MCObjectHeap_alloc(heap, sizeof(*p), &player_class);
    CHECK(p);
    p->stack = ItemStack_new(heap, ItemStack_registryItem(358), count, 4);
    CHECK(p->stack);
    return p;
}
static bool add_stat(MCObject *object, const Item *item, int32_t amount) {
    Player *p = (Player *)object;
    CHECK(p->calls == 0);
    ++p->calls;
    p->observed_item = item;
    p->observed_amount = amount;
    CHECK(!MCObjectHeap_collect(object->heap));
    CHECK(!MCObjectHeap_clone(object->heap));
    if (p->fail_stat)
        return false;
    if (p->fail_heap) {
        MCObjectHeap_fail(object->heap);
        return true;
    }
    uint32_t bits = (uint32_t)p->count + (uint32_t)amount;
    memcpy(&p->count, &bits, sizeof(bits));
    if (p->replace_item)
        ItemStack_setItem(p->stack, ItemStack_registryItem(1));
    MCObjectHeap_touch(object->heap);
    return true;
}
static bool created(ItemStack *stack, MCObject *world, MCObject *object) {
    Player *p = (Player *)object;
    CHECK(p->calls == 1);
    ++p->calls;
    ++p->created;
    CHECK(stack == p->stack && world->heap == object->heap);
    CHECK(ItemStack_getItem(stack) ==
          (p->replace_item ? ItemStack_registryItem(1) : p->observed_item));
    if (p->fail_created)
        return false;
    return NativeItemMap_onCreated(stack, (World *)world, object);
}
static const ItemStackCraftingDispatch dispatch = {add_stat, created};
static void add_map(World *world, int32_t id, uint8_t scale, int32_t x, int32_t z,
                    int8_t dimension) {
    mc_map_info map = {0};
    map.id = id;
    map.scale = scale;
    map.center_x = x;
    map.center_z = z;
    map.dimension = dimension;
    map.metadata_known = true;
    map.colors[19] = 73;
    CHECK(NativeItemMapData_setItemData(world, &map));
}
static NBTTagCompound *mark(ItemStack *stack) {
    NBTTagCompound *tag = NBTTagCompound_new(stack->object.heap);
    CHECK(tag);
    CHECK(NBTTagCompound_setBoolean_ascii(tag, "map_is_scaling", true));
    CHECK(NBTTagCompound_setString_ascii(tag, "custom",
                                         NBTString_fromASCII(stack->object.heap, "shared")));
    CHECK(ItemStack_setTagCompound(stack, tag));
    return tag;
}
static void order_and_amount(void) {
    const int32_t amounts[] = {INT32_MIN, -1, 0, 1, 4, INT32_MAX};
    for (size_t i = 0; i < sizeof(amounts) / sizeof(*amounts); i++) {
        MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
        CHECK(h);
        World *w = make_world(h);
        Player *p = make_player(h, 0);
        p->count = 7;
        CHECK(ItemStack_onCrafting(p->stack, (MCObject *)w, (MCObject *)p, amounts[i], &dispatch));
        uint32_t bits = 7u + (uint32_t)amounts[i];
        int32_t expected;
        memcpy(&expected, &bits, sizeof(bits));
        CHECK(p->count == expected && p->observed_amount == amounts[i] && p->calls == 2 &&
              p->created == 1);
        CHECK(p->stack->stackSize == 0 && w->maps.count == 0 && !MCObjectHeap_failed(h));
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
    World *w = make_world(h);
    Player *p = make_player(h, 1);
    p->replace_item = true;
    CHECK(ItemStack_onCrafting(p->stack, (MCObject *)w, (MCObject *)p, 1, &dispatch));
    CHECK(p->observed_item == ItemStack_registryItem(358) &&
          p->stack->item == ItemStack_registryItem(1));
    /* The original empty base Item.onCreated can receive a null World when
       the map-specific body has no scaling marker to inspect. */
    CHECK(NativeItemMap_onCreated(p->stack, NULL, NULL));
    MCObjectHeap_free(h);
}
static void failures(void) {
    for (unsigned mode = 0; mode < 6; mode++) {
        MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
        World *w = make_world(h);
        Player *p = make_player(h, 1);
        ItemStackCraftingDispatch d = dispatch;
        p->fail_stat = mode == 0;
        p->fail_created = mode == 1;
        p->fail_heap = mode == 2;
        if (mode == 3)
            d.addCraftStat = NULL;
        if (mode == 4)
            d.onCreated = NULL;
        CHECK(!ItemStack_onCrafting(p->stack, (MCObject *)w, (MCObject *)p, 2,
                                    mode == 5 ? NULL : &d));
        CHECK(MCObjectHeap_failed(h));
        CHECK(p->calls == (mode == 1 ? 2u : mode < 3 ? 1u : 0u));
        CHECK(p->created == (mode == 1 ? 1u : 0u));
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h = MCObjectHeap_new(1024 * 1024), *other = MCObjectHeap_new(1024 * 1024);
    Player *p = make_player(h, 1);
    World *w = make_world(other);
    CHECK(!ItemStack_onCrafting(p->stack, (MCObject *)w, (MCObject *)p, 1, &dispatch));
    CHECK(MCObjectHeap_failed(h) && !MCObjectHeap_failed(other) && p->calls == 0);
    MCObjectHeap_free(other);
    MCObjectHeap_free(h);
}
static void scaling_aliases(void) {
    MCObjectHeap *h = MCObjectHeap_new(8 * 1024 * 1024);
    World *w = make_world(h);
    Player *p = make_player(h, 0);
    CHECK(World_nativeImportMapNextProjection(w, 10));
    add_map(w, 4, 1, 100, -50, -1);
    NBTTagCompound *tag = mark(p->stack);
    ItemStack *alias = p->stack;
    ItemStack *second = ItemStack_new(h, ItemStack_registryItem(358), -1, 4);
    CHECK(second);
    CHECK(ItemStack_setTagCompound(second, tag));
    CHECK(ItemStack_onCrafting(p->stack, (MCObject *)w, (MCObject *)p, 4, &dispatch));
    CHECK(alias == p->stack && alias->itemDamage == 10 && alias->stackSize == 0 &&
          alias->stackTagCompound == tag);
    CHECK(second->stackTagCompound == tag && second->itemDamage == 4 && second->stackSize == -1);
    CHECK(NBTTagCompound_getBoolean_ascii(tag, "map_is_scaling") &&
          NBTString_equalsASCII(NBTTagCompound_getString_ascii(tag, "custom"), "shared"));
    const mc_map_info *map = mc_maps_find_const(&w->maps, 10);
    CHECK(map && map->scale == 2 && map->center_x == 192 && map->center_z == 192 &&
          map->dimension == -1);
    CHECK(map_next(w) == 11 && w->maps.count == 2);
    for (size_t i = 0; i < MC_MAP_PIXELS; i++)
        CHECK(map->colors[i] == 0);
    CHECK(mc_maps_find_const(&w->maps, 4)->colors[19] == 73);
    CHECK(NativeItemMap_onCreated(alias, w, (MCObject *)p));
    CHECK(alias->itemDamage == 11 && map_next(w) == 12);
    CHECK(mc_maps_find_const(&w->maps, 11)->scale == 3 && alias->stackTagCompound == tag);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void missing_remote_and_limits(void) {
    MCObjectHeap *h = MCObjectHeap_new(8 * 1024 * 1024);
    World *w = make_world(h);
    Player *p = make_player(h, -1);
    w->worldInfo->spawnX = -65;
    w->worldInfo->spawnZ = 1024;
    w->provider->dimensionId = -1;
    CHECK(World_nativeImportMapNextProjection(w, 10));
    p->stack->itemDamage = 100000;
    NBTTagCompound *tag = mark(p->stack);
    CHECK(NativeItemMap_onCreated(p->stack, w, (MCObject *)p));
    CHECK(p->stack->itemDamage == 11 && p->stack->stackTagCompound == tag && w->maps.count == 2 &&
          map_next(w) == 12);
    const mc_map_info *old = mc_maps_find_const(&w->maps, 10),
                      *scaled = mc_maps_find_const(&w->maps, 11);
    CHECK(old && old->scale == 3 && old->center_x == -576 && old->center_z == 1472 &&
          old->dimension == -1);
    CHECK(scaled && scaled->scale == 4 && scaled->center_x == -1088 && scaled->center_z == 960 &&
          scaled->dimension == -1);
    CHECK(NativeItemMap_onCreated(p->stack, w, (MCObject *)p));
    CHECK(mc_maps_find_const(&w->maps, 12)->scale == 4);
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(1024 * 1024);
    w = make_world(h);
    p = make_player(h, 1);
    w->isRemote = true;
    tag = mark(p->stack);
    CHECK(!NativeItemMap_onCreated(p->stack, w, (MCObject *)p));
    CHECK(MCObjectHeap_failed(h));
    CHECK(p->stack->itemDamage == 0 && p->stack->stackTagCompound == tag && !w->maps.count &&
          map_next(w) == 1);
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(8 * 1024 * 1024);
    w = make_world(h);
    p = make_player(h, 1);
    w->isRemote = true;
    add_map(w, 4, 4, 192, 192, 0);
    CHECK(World_nativeImportMapNextProjection(w,5));
    tag = mark(p->stack);
    CHECK(NativeItemMap_onCreated(p->stack, w, (MCObject *)p));
    CHECK(p->stack->itemDamage == 5 && w->maps.count == 2);
    CHECK(mc_maps_find_const(&w->maps, 5)->scale == 4 && p->stack->stackTagCompound == tag);
    MCObjectHeap_free(h);
    h = MCObjectHeap_new(1024 * 1024);
    w = make_world(h);
    p = make_player(h, 1);
    add_map(w, 4, 1, 100, -50, 0);
    CHECK(World_nativeImportMapNextProjection(w, INT16_MAX + 1));
    tag = mark(p->stack);
    CHECK(NativeItemMap_onCreated(p->stack, w, (MCObject *)p));
    CHECK(!MCObjectHeap_failed(h));
    CHECK(p->stack->itemDamage == 0 && p->stack->stackTagCompound == tag && w->maps.count == 2);
    CHECK(map_next(w) == INT16_MAX + 2 && mc_maps_find_const(&w->maps, 0)->scale == 2);
    MCObjectHeap_free(h);
}
static void numeric_markers(void) {
    for (unsigned i = 0; i < 7; i++) {
        MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
        World *w = make_world(h);
        Player *p = make_player(h, 1);
        add_map(w, 4, 1, 100, -50, 0);
        CHECK(World_nativeImportMapNextProjection(w, 10));
        NBTTagCompound *tag = mark(p->stack);
        bool scaling = i == 0 || i == 2 || i == 3 || i == 6;
        if (i < 4)
            CHECK(NBTTagCompound_setFloat_ascii(tag, "map_is_scaling",
                                                i == 0   ? -.5f
                                                : i == 1 ? NAN
                                                : i == 2 ? INFINITY
                                                         : -INFINITY));
        else if (i < 6)
            CHECK(NBTTagCompound_setLong_ascii(tag, "map_is_scaling", i == 4 ? 256 : 0));
        else
            CHECK(NBTTagCompound_setDouble_ascii(tag, "map_is_scaling", -.5));
        CHECK(NativeItemMap_onCreated(p->stack, w, (MCObject *)p));
        CHECK(p->stack->itemDamage == (scaling ? 10 : 4));
        CHECK(w->maps.count == (scaling ? 2u : 1u));
        CHECK(p->stack->stackTagCompound == tag && !MCObjectHeap_failed(h));
        MCObjectHeap_free(h);
    }
}
static void counter_replacement(void) {
    const int32_t next[] = {0, 4, 32767, 32768, 65535};
    const int32_t damage[] = {0, 4, 32767, 0, 0};
    const int32_t after[] = {1, 5, 32768, 32769, 0};
    for (size_t n = 0; n < sizeof(next) / sizeof(*next); n++) {
        MCObjectHeap *h = MCObjectHeap_new(4 * 1024 * 1024);
        World *w = make_world(h);
        Player *p = make_player(h, 0);
        add_map(w, 0, 1, 100, -50, -1);
        add_map(w, 4, 1, 100, -50, -1);
        CHECK(World_nativeImportMapNextProjection(w, next[n]));
        NBTTagCompound *tag = mark(p->stack);
        const mc_map_info *old = mc_maps_find_const(&w->maps, 4);
        CHECK(old);
        /* Native S34 metadata presence does not add a source ItemMap guard. */
        mc_maps_find(&w->maps, 4)->metadata_known = false;
        CHECK(NativeItemMap_onCreated(p->stack, w, (MCObject *)p));
        CHECK(p->stack->itemDamage == damage[n] && map_next(w) == after[n]);
        CHECK(p->stack->stackSize == 0 && p->stack->stackTagCompound == tag);
        CHECK(w->maps.count == (damage[n] == 0 || damage[n] == 4 ? 2u : 3u));
        const mc_map_info *scaled = mc_maps_find_const(&w->maps, damage[n]);
        CHECK(scaled);
        CHECK(scaled->scale == 2 && scaled->dimension == -1 && scaled->center_x == 192 &&
              scaled->center_z == 192);
        CHECK(scaled->dirty && scaled->metadata_known && scaled->colors[19] == 0);
        CHECK(NBTTagCompound_getBoolean_ascii(tag, "map_is_scaling"));
        /* Store encoding retains unsigned counter bits; dirty is transient. */
        mc_nbt encoded = {0};
        mc_maps decoded = {0};
        if (!mc_maps_find_const(&w->maps, 4)->metadata_known) {
            CHECK(!mc_maps_encode(&w->maps, &encoded) && !encoded.data);
            /* The native saved-store codec requires complete metadata, unlike
               the original ItemMap body above. Supply it at that boundary. */
            mc_maps_find(&w->maps, 4)->metadata_known = true;
        }
        mc_maps exportView={0};CHECK(mc_maps_copy(&exportView,&w->maps));
        exportView.next_id=map_next(w); /* Ephemeral native export, not owning state. */
        CHECK(mc_maps_encode(&exportView, &encoded));mc_maps_free(&exportView);
        CHECK(mc_maps_decode(&encoded, &decoded));
        CHECK(decoded.next_id == after[n] && decoded.count == w->maps.count);
        CHECK(mc_maps_find_const(&decoded, damage[n])->scale == 2 &&
              !mc_maps_find_const(&decoded, damage[n])->dirty);
        mc_maps_free(&decoded);
        mc_nbt_free(&encoded);
        MCObjectHeap_free(h);
    }
}
static void transaction_graph(void) {
    MCObjectHeap *h = MCObjectHeap_new(8 * 1024 * 1024);
    /* Initialize the same reached class statics/literals, then reclaim the
       complete warm-up World/player graph before measuring retained roots. */
    World *warm_world=make_world(h);
    Player *warm_player=make_player(h,0);
    mark(warm_player->stack);
    CHECK(World_nativeImportMapNextProjection(warm_world,10));
    CHECK(ItemStack_onCrafting(warm_player->stack,(MCObject *)warm_world,(MCObject *)warm_player,2,&dispatch));
    const MCObjectClass *world_class=warm_world->object.klass;
    CHECK(MCObjectHeap_collect(h));
    CHECK(!MCObjectHeap_findObject(h,world_class,any_object,NULL));
    CHECK(!MCObjectHeap_findObject(h,&player_class,any_object,NULL));
    CHECK(!MCObjectHeap_findObject(h,&owners_class,any_object,NULL));
    WorldTypeStatics *world_types=WorldType_getStatics(h);CHECK(world_types);
    CHECK(world_types->DEFAULT==world_types->worldTypes->items[0] &&
          world_types->FLAT==world_types->worldTypes->items[1]);
    size_t static_objects=MCObjectHeap_liveObjects(h),static_bytes=MCObjectHeap_liveBytes(h);
    CHECK(static_objects>0);
    World *w = make_world(h);
    Player *p = make_player(h, 0);
    add_map(w, 4, 1, 100, -50, 0);
    CHECK(World_nativeImportMapNextProjection(w, 10));
    NBTTagCompound *tag = mark(p->stack);
    MCObjectRoot rw, rp;
    CHECK(MCObjectRoot_init(&rw, h, (MCObject *)w));
    CHECK(MCObjectRoot_init(&rp, h, (MCObject *)p));
    MCObjectHeap *working = MCObjectHeap_clone(h);
    CHECK(working);
    MCObjectRoot cw, cp;
    CHECK(MCObjectRoot_rebind(&cw, working, &rw));
    CHECK(MCObjectRoot_rebind(&cp, working, &rp));
    World *ww = (World *)MCObjectRoot_get(&cw);
    Player *pp = (Player *)MCObjectRoot_get(&cp);
    /* Missing-data creation can precede a later allocation failure. Retire
       the failed entire branch; neither its counter nor its first new map is
       allowed into the live graph. */
    for (int i = 5; i < 99; i++)
        add_map(ww, i, 1, 100, -50, 0);
    CHECK(World_nativeImportMapNextProjection(ww, 100));
    pp->stack->itemDamage = 100000;
    CHECK(!ItemStack_onCrafting(pp->stack, (MCObject *)ww, (MCObject *)pp, 2, &dispatch));
    CHECK(MCObjectHeap_failed(working) && pp->count == 2 && pp->stack->itemDamage == 101 &&
          ww->maps.count == MC_MAX_MAPS);
    CHECK(!MCObjectHeap_canAdopt(h, working));
    CHECK(!MCObjectHeap_adopt(h, working));
    CHECK(p->count == 0 && p->stack->itemDamage == 4 && w->maps.count == 1 &&
          map_next(w) == 10);
    MCObjectHeap_free(working);
    working = MCObjectHeap_clone(h);
    CHECK(working);
    CHECK(MCObjectRoot_rebind(&cw, working, &rw));
    CHECK(MCObjectRoot_rebind(&cp, working, &rp));
    ww = (World *)MCObjectRoot_get(&cw);
    pp = (Player *)MCObjectRoot_get(&cp);
    CHECK(ww->maps.entries != w->maps.entries && pp->stack != p->stack &&
          pp->stack->stackTagCompound != tag);
    CHECK(ItemStack_onCrafting(pp->stack, (MCObject *)ww, (MCObject *)pp, 2, &dispatch));
    CHECK(p->count == 0 && p->stack->itemDamage == 4 && w->maps.count == 1 &&
          map_next(w) == 10);
    CHECK(MCObjectHeap_adopt(h, working));
    MCObjectHeap_free(working);
    w = (World *)MCObjectRoot_get(&rw);
    p = (Player *)MCObjectRoot_get(&rp);
    CHECK(p->count == 2 && p->stack->itemDamage == 10 && w->maps.count == 2 &&
          map_next(w) == 11);
    CHECK(NBTTagCompound_getBoolean_ascii(p->stack->stackTagCompound, "map_is_scaling"));
    CHECK(MCObjectHeap_collect(h));
    CHECK(p->stack->itemDamage == 10);
    MCObjectRoot_drop(&rw);
    MCObjectRoot_drop(&rp);
    CHECK(MCObjectHeap_collect(h));
    CHECK(MCObjectHeap_liveObjects(h)==static_objects && MCObjectHeap_liveBytes(h)==static_bytes);
    world_types=WorldType_getStatics(h);CHECK(world_types);
    CHECK(world_types->DEFAULT==world_types->worldTypes->items[0] &&
          world_types->FLAT==world_types->worldTypes->items[1]);
    CHECK(!MCObjectHeap_findObject(h,world_class,any_object,NULL));
    CHECK(!MCObjectHeap_findObject(h,&player_class,any_object,NULL));
    CHECK(!MCObjectHeap_findObject(h,&owners_class,any_object,NULL));
    MCObjectHeap_free(h);
}
int main(void) {
    order_and_amount();
    failures();
    scaling_aliases();
    missing_remote_and_limits();
    numeric_markers();
    counter_replacement();
    transaction_graph();
    printf("Source item crafting: %u checks passed\n", checks);
    return 0;
}
