#include "util/MCGameplayCrafting.h"
#include "util/MCGameplayPlayer.h"
#include "item/ItemMapData.h"
#include "world/WorldDataStorage.h"
#include "item/ItemMapCreated.h"
#include "item/ItemStackCrafting.h"
#include "item/ItemArmor.h"
#include "inventory/ContainerWorkbench.h"
#include "stats/StatFileWriter.h"
#include "stats/StatList.h"
#include "crafting/crafting.h"
#include "tileentity/TileEntityBanner.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "gameplay crafting check %u line %d: %s\n", checks, __LINE__, #x);     \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static int32_t map_next(const MCGameplayWorld *world) {
    int32_t value=-1;CHECK(world->maps.next_id==0);
    CHECK(MCObjectHeap_failed(world->object.heap)?MapStorage_nativeGetMapNextProjectionDiagnostic(world->mapStorage,&value):World_nativeMapNextProjection(world,&value));
    return value;
}
/* These effects are real managed test records and source StatFileWriter
   counters. They stand at explicit actor/localization dependencies, rather
   than production no-op handlers or a replacement authoritative inventory. */
typedef struct {
    MCObject object;
    NBTString *display;
    ItemStackList *drops;
    unsigned crafted, achievements;
    ItemStack *last;
} Effects;
static void trace(MCObject *o, MCObjectVisitor v, void *c) {
    Effects *e = (Effects *)o;
    e->display = (NBTString *)v((MCObject *)e->display, c);
    e->drops = (ItemStackList *)v((MCObject *)e->drops, c);
    e->last = (ItemStack *)v((MCObject *)e->last, c);
}
static const MCObjectClass effectsClass = {"native crafting test actor effects",
                                           MCObjectHeap_plainClone, trace, NULL};
static mc_crafting_dispatch dispatch;
static NBTString *display(MCObject *o, const ItemStack *s) {
    CHECK(o && s->object.heap == o->heap);
    return ((Effects *)o)->display;
}
static bool add_stat(MCObject *o, const Item *item, int32_t amount) {
    MCGameplayPlayer *p = (MCGameplayPlayer *)o;
    int32_t id = ItemStack_registryId(item);
    CHECK(id >= 0 && id < 2268);
    StatBase *stat = ((MCGameplayWorld *)(p->living.entity.worldObj))->craftStats[id]; /* Original EntityPlayerMP.addStat ignores a null
                                                     source craft stat. */
    return !stat || StatFileWriter_increaseStat(p->stats, o, stat, amount);
}
static bool created(ItemStack *s, MCObject *w, MCObject *p) {
    MCGameplayWorld *world = (MCGameplayWorld *)w;
    if (s->item == ItemStack_registryItem(358))
        return ItemMap_onCreated(s, world, p); /* Original base Item.onCreated has an empty body. */
    return true;
}
static const ItemStackCraftingDispatch sourceCrafting = {add_stat, created};
static bool crafted(ItemStack *s, MCObject *w, MCObject *p, int32_t amount) {
    Effects *e = (Effects *)((MCGameplayPlayer *)p)->effects;
    e->crafted++;
    e->last = s;
    MCObjectHeap_touch(p->heap);
    return ItemStack_onCrafting(s, w, p, amount, &sourceCrafting);
}
static bool achieve(MCObject *o, mc_crafting_achievement value) {
    static const char *const names[] = {
        "buildWorkBench",     "buildPickaxe", "buildFurnace", "buildHoe", "makeBread",  "bakeCake",
        "buildBetterPickaxe", "buildSword",   "enchantments", "bookcase", "overpowered"};
    MCGameplayPlayer *p = (MCGameplayPlayer *)o;
    Effects *e = (Effects *)p->effects;
    CHECK(value >= MC_ACH_BUILD_WORKBENCH && value <= MC_ACH_OVERPOWERED);
    e->achievements++;
    MCObjectHeap_touch(o->heap);
    char name[80];
    snprintf(name, sizeof(name), "achievement.%s", names[value]);
    StatBase *stat = StatList_getOneShotStat_ascii(((MCGameplayWorld *)(p->living.entity.worldObj))->statList, name);
    CHECK(stat);
    return StatFileWriter_increaseStat(p->stats, o, stat, 1);
}
static bool drop(MCObject *o, ItemStack *s, bool scatter) {
    (void)scatter;
    Effects *e = (Effects *)((MCGameplayPlayer *)o)->effects;
    CHECK(s);
    return ItemStackList_add(e->drops, s);
}
static const MCGameplayCraftingEffects effects = {crafted, achieve, drop};
typedef struct {
    MCGameplay game;
    mc_world terrain;
    MCGameplayWorld *world;
    MCGameplayPlayer *player;
    Effects *effects;
    MCObjectRootScope scope;
} Fixture;
static void init(Fixture *f) {
    memset(f, 0, sizeof(*f));
    CHECK(MCGameplay_init(&f->game, 64 * 1024 * 1024));
    mc_world_init(&f->terrain, 919);
    CHECK(mc_world_set(&f->terrain, 0, 10, 0, 58 << 4));
    CHECK(MCObjectRootScope_begin(&f->scope, f->game.heap));
    f->world = MCGameplayWorld_new(f->game.heap, MCGameplay_get(&f->game), &f->terrain, NULL);
    CHECK(f->world && MCGameplay_setWorld(&f->game, (MCObject *)f->world));
    f->effects = (Effects *)MCObjectHeap_alloc(f->game.heap, sizeof(Effects), &effectsClass);
    CHECK(f->effects);
    f->effects->display = NBTString_fromUTF8(f->game.heap, "日本語の名前");
    f->effects->drops = ItemStackList_new(f->game.heap);
    CHECK(f->effects->display && f->effects->drops);
    CHECK(MCGameplayCrafting_configureWorld(f->world, display, (MCObject *)f->effects));
    f->player = MCGameplayPlayer_new(f->world, NBTString_fromASCII(f->game.heap, "Player"),
                                     StatFileWriter_new(f->game.heap), &dispatch);
    CHECK(f->player);
    f->player->effects = (MCObject *)f->effects;
    f->player->living.entity.posY = 10;
    CHECK(MCGameplay_setPlayer(&f->game, 0, "11111111-1111-1111-1111-111111111111",
                               (MCObject *)f->player));
}
static void finish(Fixture *f) {
    CHECK(!MCObjectHeap_failed(f->game.heap));
    MCObjectRootScope_end(&f->scope);
    CHECK(MCGameplay_free(&f->game));
    mc_world_free(&f->terrain);
}
static ItemStack *item(Fixture *f, int32_t id, int32_t n, int32_t damage) {
    ItemStack *s = ItemStack_new(f->game.heap, ItemStack_registryItem(id), n, damage);
    CHECK(s);
    return s;
}
static void put(InventoryCrafting *g, int32_t i, ItemStack *s) {
    CHECK(InventoryCrafting_setInventorySlotContents(g, i, s));
}
static void clear(Fixture *f, InventoryCrafting *g) {
    InventoryCrafting_clear(g);
    CHECK(InventoryPlayer_setItemStack(f->player->inventory, NULL));
}
static ItemStack *take(Fixture *f, Container *c) {
    ItemStack *preview = Slot_getStack(Container_getSlot(c, 0));
    CHECK(preview);
    ItemStack *returned = Container_slotClick(c, 0, 0, 0, f->player->inventory);
    CHECK(returned);
    ItemStack *s = InventoryPlayer_getItemStack(f->player->inventory);
    CHECK(s);
    return s;
}
static NBTTagCompound *patterns(Fixture *f, ItemStack *s, int32_t color) {
    NBTTagCompound *root = NBTTagCompound_new(f->game.heap),
                   *bet = NBTTagCompound_new(f->game.heap), *p = NBTTagCompound_new(f->game.heap);
    NBTTagList *l = NBTTagList_new(f->game.heap);
    CHECK(root && bet && p && l);
    CHECK(NBTTagCompound_setString_ascii(p, "Pattern", NBTString_fromASCII(f->game.heap, "bs")));
    CHECK(NBTTagCompound_setInteger_ascii(p, "Color", color));
    CHECK(NBTTagList_appendTag(l, (NBTBase *)p));
    CHECK(NBTTagCompound_setTag_ascii(bet, "Patterns", (NBTBase *)l));
    CHECK(NBTTagCompound_setTag_ascii(root, "BlockEntityTag", (NBTBase *)bet));
    CHECK(ItemStack_setTagCompound(s, root));
    return root;
}
static void registration_and_all_static(void) {
    Fixture f;
    init(&f);
    RecipeList *list = CraftingManager_getRecipeList(f.world->manager);
    CHECK(RecipeList_size(list) == 373);
    CHECK(f.world->statList && f.world->furnace &&
          FurnaceRecipeMap_size(FurnaceRecipes_getSmeltingList(f.world->furnace)) == 26);
    unsigned craftStats = 0;
    for (size_t i = 0; i < MC_GAMEPLAY_CRAFT_STAT_COUNT; i++) {
        CHECK(f.world->craftStats[i] == f.world->statList->objectCraftStats[i]);
        if (f.world->craftStats[i]) ++craftStats;
    }
    CHECK(craftStats == 232 && !f.world->craftStats[358] && !f.world->craftStats[387] &&
          !f.world->craftStats[401] && !f.world->craftStats[402] && f.world->craftStats[395]);
    CHECK(f.world->statList->dropStat &&
          StatList_getOneShotStat_ascii(f.world->statList, "achievement.openInventory"));
    static const int indices[] = {0, 1, 2, 70, 71, 72, 216, 280};
    static const char *names[] = {"RecipesArmorDyes",
                                  "RecipeFireworks",
                                  "RecipesBanners.RecipeAddPattern",
                                  "RecipeBookCloning",
                                  "RecipesMapCloning",
                                  "RecipesMapExtending",
                                  "RecipeRepairItem",
                                  "RecipesBanners.RecipeDuplicatePattern"};
    for (size_t i = 0; i < sizeof(indices) / sizeof(*indices); i++) {
        IRecipe r = RecipeList_get(list, indices[i]);
        CHECK(r.instance && strcmp(r.instance->klass->name, names[i]) == 0);
    }
    ContainerWorkbench *bench = ContainerWorkbench_new(
        f.player->inventory, (MCObject *)f.world, &(mc_crafting_position){0, 10, 0}, &dispatch);
    CHECK(bench);
    f.player->openContainer = &bench->container;
    CHECK(ContainerWorkbench_canInteractWith(bench, f.player->inventory));
    for (size_t i = 0; i < mc_crafting_static_recipe_count(); i++) {
        mc_crafting_recipe_fact fact;
        CHECK(mc_crafting_static_recipe(i, &fact));
        InventoryCrafting_clear(bench->craftMatrix);
        for (int j = 0; j < fact.count; j++)
            if (fact.input[j].id >= 0) {
                int index = fact.width ? (j / fact.width) * 3 + j % fact.width : j;
                put(bench->craftMatrix, index,
                    item(&f, fact.input[j].id, 1,
                         fact.input[j].damage == 32767 ? 0 : fact.input[j].damage));
            }
        ItemStack *out =
            MCGameplayCrafting_findMatchingRecipe(bench->craftMatrix, (MCObject *)f.world);
        CHECK(out && out->item == ItemStack_registryItem(fact.output) &&
              out->stackSize == fact.amount && out->itemDamage == fact.damage);
    }
    finish(&f);
}
static void native_player_crafts(void) {
    Fixture f;
    init(&f);
    ContainerPlayer *c = ((ContainerPlayer *)(f.player->inventoryContainer));
    InventoryCrafting *g = c->craftMatrix;
    put(g, 3, item(&f, 17, 2, 2));
    ItemStack *out = take(&f, &c->container);
    CHECK(out->item == ItemStack_registryItem(5) && out->stackSize == 4 && out->itemDamage == 2);
    CHECK(InventoryCrafting_getStackInSlot(g, 3)->stackSize == 1);
    CHECK(StatFileWriter_readStat(f.player->stats, f.world->craftStats[5]) == 4);
    clear(&f, g);
    put(g, 0, item(&f, 298, 1, 7));
    put(g, 1, item(&f, 351, 1, 1));
    out = take(&f, &c->container);
    CHECK(out->item == ItemStack_registryItem(298) && out->stackSize == 1 &&
          ItemArmor_hasColor(f.game.heap, out->item, out));
    clear(&f, g);
    put(g, 0, item(&f, 289, 1, 0));
    put(g, 1, item(&f, 339, 1, 0));
    out = take(&f, &c->container);
    CHECK(out->item == ItemStack_registryItem(401) && !out->stackTagCompound);
    clear(&f, g);
    ItemStack *book = item(&f, 387, 2, 7);
    NBTTagCompound *tag = NBTTagCompound_new(f.game.heap);
    CHECK(tag && NBTTagCompound_setInteger_ascii(tag, "generation", 0));
    CHECK(ItemStack_setTagCompound(book, tag));
    CHECK(ItemStack_setStackDisplayName(book, NBTString_fromUTF8(f.game.heap, "原本")));
    put(g, 0, book);
    put(g, 1, item(&f, 386, 1, 0));
    out = take(&f, &c->container);
    CHECK(out->item == ItemStack_registryItem(387) && out->itemDamage == 0 && out->stackSize == 1);
    CHECK(InventoryCrafting_getStackInSlot(g, 0) == book && book->stackSize == 0);
    CHECK(InventoryPlayer_getStackInSlot(f.player->inventory, 0)->stackSize == 1);
    clear(&f, g);
    put(g, 0, item(&f, 358, 1, 1234));
    put(g, 1, item(&f, 395, 1, 0));
    out = take(&f, &c->container);
    CHECK(out->item == ItemStack_registryItem(358) && out->stackSize == 2 &&
          out->itemDamage == 1234);
    clear(&f, g);
    put(g, 0, item(&f, 276, 1, 1500));
    put(g, 1, item(&f, 276, 1, 1500));
    out = take(&f, &c->container);
    CHECK(out->item == ItemStack_registryItem(276) && out->itemDamage == 1361 &&
          !out->stackTagCompound);
    clear(&f, g);
    put(g, 0, item(&f, 425, 1, 3));
    put(g, 1, item(&f, 45, 1, 0));
    out = take(&f, &c->container);
    CHECK(TileEntityBanner_getPatterns(out) == 1);
    clear(&f, g);
    ItemStack *banner = item(&f, 425, 1, 3);
    NBTTagCompound *original = patterns(&f, banner, 4);
    put(g, 0, banner);
    put(g, 1, item(&f, 425, 1, 3));
    out = take(&f, &c->container);
    CHECK(out->stackTagCompound != original && TileEntityBanner_getPatterns(out) == 1);
    CHECK(InventoryCrafting_getStackInSlot(g, 0) != banner &&
          TileEntityBanner_getPatterns(InventoryCrafting_getStackInSlot(g, 0)) == 1);
    CHECK(f.effects->crafted == 8 && ItemStackList_size(f.effects->drops) == 0);
    finish(&f);
}
static void source_maps_and_snapshot(void) {
    Fixture f;
    init(&f);
    f.world->spawnX = -65;
    f.world->spawnZ = 1024;
    f.world->dimension = 255;
    CHECK(World_nativeImportMapNextProjection(f.world, 10));
    ItemStack *s = item(&f, 358, 0, 999);
    NBTTagCompound *tag = NBTTagCompound_new(f.game.heap);
    CHECK(tag && NBTTagCompound_setInteger_ascii(tag, "custom", 42) &&
          ItemStack_setTagCompound(s, tag));
    CHECK(InventoryPlayer_setInventorySlotContents(f.player->inventory, 0, s));
    CHECK(InventoryPlayer_setItemStack(f.player->inventory, s));
    mc_map_info *map = ItemMap_getMapData(s, f.world);
    CHECK(map && s->itemDamage == 10 && s->stackSize == 0 && s->stackTagCompound == tag);
    CHECK(map->center_x == -576 && map->center_z == 1472 && map->scale == 3 &&
          map->dimension == -1 && map->dirty && map_next(f.world) == 11);
    map->metadata_known = false;
    CHECK(ItemMap_getMapData(s, f.world) == map);
    f.world->remote = true;
    ItemStack *missing = item(&f, 1, -1, 500);
    CHECK(!ItemMap_getMapData(missing, f.world) && missing->itemDamage == 500 &&
          map_next(f.world) == 11 && !MCObjectHeap_failed(f.game.heap));
    f.world->remote = false;
    map->metadata_known = true;
    MCObjectRootScope_end(&f.scope);
    MCGameplayTransaction tx = {0};
    CHECK(MCGameplay_begin(&f.game, &tx));
    CHECK(MCObjectRootScope_begin(&f.scope, tx.working.heap));
    MCGameplayPlayer *p = (MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
    CHECK(((MCGameplayWorld *)(p->living.entity.worldObj))->manager != f.world->manager &&
          ((MCGameplayWorld *)(p->living.entity.worldObj))->itemDisplayContext == p->effects &&
          ((MCGameplayWorld *)(p->living.entity.worldObj))->itemDisplayContext != (MCObject *)f.effects);
    CHECK(((MCGameplayWorld *)(p->living.entity.worldObj))->statList != f.world->statList && ((MCGameplayWorld *)(p->living.entity.worldObj))->furnace != f.world->furnace);
    CHECK(((MCGameplayWorld *)(p->living.entity.worldObj))->craftStats[5] != f.world->craftStats[5] &&
          ((MCGameplayWorld *)(p->living.entity.worldObj))->craftStats[5] == ((MCGameplayWorld *)(p->living.entity.worldObj))->statList->objectCraftStats[5]);
    CHECK(((MCGameplayWorld *)(p->living.entity.worldObj))->statList->dropStat != f.world->statList->dropStat &&
          StatList_getOneShotStat_ascii(((MCGameplayWorld *)(p->living.entity.worldObj))->statList, "stat.drop") ==
              ((MCGameplayWorld *)(p->living.entity.worldObj))->statList->dropStat);
    ItemStack *alias = InventoryPlayer_getItemStack(p->inventory);
    CHECK(alias == InventoryPlayer_getStackInSlot(p->inventory, 0) && alias != s);
    alias->itemDamage = 1000;
    CHECK(ItemMap_getMapData(alias, ((MCGameplayWorld *)(p->living.entity.worldObj))) && alias->itemDamage == 11 &&
          ((MCGameplayWorld *)(p->living.entity.worldObj))->maps.count == 2);
    CHECK(s->itemDamage == 10 && f.world->maps.count == 1);
    MCObjectRootScope_end(&f.scope);
    CHECK(MCGameplay_abort(&tx));
    CHECK(MCObjectRootScope_begin(&f.scope, f.game.heap));
    finish(&f);
    init(&f);
    f.world->spawnX = INT32_MAX;
    f.world->spawnZ = INT32_MIN;
    CHECK(World_nativeImportMapNextProjection(f.world, 65535));
    s = item(&f, 1, -1, 999);
    map = ItemMap_getMapData(s, f.world);
    CHECK(map && s->itemDamage == 0 && map_next(f.world) == 0 && map->center_x == -2147483200 &&
          map->center_z == -2147483200);
    map->colors[0] = 42;
    mc_map_info *originalMap = map;
    CHECK(World_nativeImportMapNextProjection(f.world, 32768));
    s->itemDamage = 999;
    map = ItemMap_getMapData(s, f.world);
    CHECK(map && map == originalMap && s->itemDamage == 0 && map_next(f.world) == 32769 &&
          f.world->maps.count == 1 && map->colors[0] == 0);
    mc_nbt encoded = {0};
    mc_maps exportView=f.world->maps;exportView.next_id=map_next(f.world);
    CHECK(mc_maps_encode(&exportView, &encoded)); /* Ephemeral native export only. */
    mc_maps decoded = {0};
    CHECK(mc_maps_decode(&encoded, &decoded) && decoded.next_id == 32769);
    mc_nbt_free(&encoded);
    mc_maps_free(&decoded);
    finish(&f);
}
static void workbench_map_recipe(void) {
    Fixture f;
    init(&f);
    ContainerWorkbench *c = ContainerWorkbench_new(f.player->inventory, (MCObject *)f.world,
                                                   &(mc_crafting_position){0, 10, 0}, &dispatch);
    CHECK(c);
    f.player->openContainer = &c->container;
    mc_map_info map = {0};
    map.id = 4;
    map.center_x = 100;
    map.center_z = -50;
    map.scale = 1;
    map.metadata_known = false;
    CHECK(ItemMapData_nativeSetItemData(f.world, &map));
    CHECK(World_nativeImportMapNextProjection(f.world, 10));
    for (int i = 0; i < 9; i++)
        put(c->craftMatrix, i, item(&f, i == 4 ? 358 : 339, i == 4 ? 0 : 1, i == 4 ? 4 : 0));
    ItemStack *preview = InventoryCraftResult_getStackInSlot(c->craftResult, 0);
    CHECK(preview && preview->itemDamage == 4 &&
          NBTTagCompound_getBoolean_ascii(preview->stackTagCompound, "map_is_scaling"));
    CHECK(!mc_maps_find(&f.world->maps, 4)->metadata_known);
    InventoryCrafting_getStackInSlot(c->craftMatrix, 4)->stackSize = 1;
    ItemStack *out = take(&f, &c->container);
    CHECK(out->itemDamage == 10 && map_next(f.world) == 11 && f.world->maps.count == 2 &&
          mc_maps_find(&f.world->maps, 10)->scale == 2);
    finish(&f);
}
static void native_failures(void) {
    Fixture f;
    init(&f);
    CraftingManager *saved = f.world->manager;
    StatList *savedStats = f.world->statList;
    FurnaceRecipes *savedFurnace = f.world->furnace;
    CHECK(!MCGameplayCrafting_configureWorld(f.world, NULL, (MCObject *)f.effects));
    CHECK(MCObjectHeap_failed(f.game.heap) && f.world->manager == saved &&
          f.world->itemDisplayContext == (MCObject *)f.effects);
    CHECK(f.world->statList == savedStats && f.world->furnace == savedFurnace &&
          f.world->craftStats[5] == savedStats->objectCraftStats[5]);
    MCObjectRootScope_end(&f.scope);
    CHECK(MCGameplay_free(&f.game));
    mc_world_free(&f.terrain);
    init(&f);
    CHECK(!MCGameplayCrafting_findMatchingRecipe(((ContainerPlayer *)(f.player->inventoryContainer))->craftMatrix,
                                                 (MCObject *)f.effects));
    CHECK(MCObjectHeap_failed(f.game.heap));
    MCObjectRootScope_end(&f.scope);
    CHECK(MCGameplay_free(&f.game));
    mc_world_free(&f.terrain);
    init(&f);
    for (int32_t i = 0; i < (int32_t)MC_MAX_MAPS; i++) {
        mc_map_info map = {0};
        map.id = i;
        map.scale = 3;
        map.metadata_known = true;
        CHECK(ItemMapData_nativeSetItemData(f.world, &map));
    }
    CHECK(World_nativeImportMapNextProjection(f.world, MC_MAX_MAPS));
    ItemStack *s = item(&f, 358, -1, 1000);
    CHECK(InventoryPlayer_setItemStack(f.player->inventory, s));
    MCObjectRootScope_end(&f.scope);
    MCGameplayTransaction tx = {0};
    CHECK(MCGameplay_begin(&f.game, &tx));
    CHECK(MCObjectRootScope_begin(&f.scope, tx.working.heap));
    MCGameplayPlayer *p = (MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
    ItemStack *ws = InventoryPlayer_getItemStack(p->inventory);
    CHECK(!ItemMap_getMapData(ws, ((MCGameplayWorld *)(p->living.entity.worldObj))) && MCObjectHeap_failed(tx.working.heap));
    CHECK(ws->itemDamage == MC_MAX_MAPS && map_next((MCGameplayWorld *)p->living.entity.worldObj) == MC_MAX_MAPS + 1 &&
          ((MCGameplayWorld *)(p->living.entity.worldObj))->maps.count == MC_MAX_MAPS);
    CHECK(s->itemDamage == 1000 && map_next(f.world) == MC_MAX_MAPS);
    MCObjectRootScope_end(&f.scope);
    CHECK(MCGameplay_abort(&tx));
    CHECK(MCObjectRootScope_begin(&f.scope, f.game.heap));
    finish(&f);
}
static void original_map_vectors(const char *path) {
    FILE *input = fopen(path, "r");
    CHECK(input);
    Fixture f;
    init(&f);
    int32_t next, x, z, dimension, remote, damage, after, present, cx, cz, dim, scale;
    unsigned vectors = 0;
    while (fscanf(input, "%d %d %d %d %d %d %d %d %d %d %d %d", &next, &x, &z, &dimension, &remote,
                  &damage, &after, &present, &cx, &cz, &dim, &scale) == 12) {
        mc_maps_free(&f.world->maps);
        mc_maps_init(&f.world->maps);
        CHECK(World_nativeImportMapNextProjection(f.world, next));
        f.world->spawnX = x;
        f.world->spawnZ = z;
        f.world->dimension = dimension;
        f.world->remote = remote != 0;
        ItemStack *s = item(&f, 358, 0, 999);
        mc_map_info *map = ItemMap_getMapData(s, f.world);
        CHECK((map != NULL) == (present != 0));
        CHECK(s->itemDamage == damage && s->stackSize == 0 && map_next(f.world) == after);
        if (map)
            CHECK(map->center_x == cx && map->center_z == cz && map->dimension == dim &&
                  map->scale == scale && map->dirty);
        CHECK(!MCObjectHeap_failed(f.game.heap));
        vectors++;
    }
    CHECK(feof(input) && fclose(input) == 0);
    finish(&f);
    printf("Independent original ItemMapData vectors: %u passed\n", vectors);
}
int main(int argc, char **argv) {
    CHECK(MCGameplayCrafting_nativeDispatch(&dispatch, &effects));
    registration_and_all_static();
    native_player_crafts();
    source_maps_and_snapshot();
    workbench_map_recipe();
    native_failures();
    if (argc > 1)
        original_map_vectors(argv[1]);
    printf("Native gameplay crafting: %u checks passed\n", checks);
    return 0;
}
