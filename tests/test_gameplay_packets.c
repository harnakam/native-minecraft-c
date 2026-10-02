#include "util/MCGameplayPackets.h"
#include "util/MCPacketQueue.h"
#include "item/ItemMapLoad.h"
#include "item/ItemMapPacket.h"
#include "world/storage/SaveDataMemoryStorage.h"
#include "util/Vec4b.h"
#include "nbt/nbt.h"
#include "nbt/NBTTagList.h"
#include "nbt/NBTTagByte.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(p) _mkdir(p)
#define remove_dir(p) _rmdir(p)
#else
#include <unistd.h>
#define make_dir(p) mkdir(p, 0700)
#define remove_dir(p) rmdir(p)
#endif
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "gameplay packet check %u at %d: %s\n", checks, __LINE__, #x);         \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static ItemStack *recipe(InventoryCrafting *g, MCObject *w) {
    return CraftingManager_findMatchingRecipe(((MCGameplayWorld *)w)->manager, g, w, NULL, NULL);
}
static ItemStackArray *remaining(InventoryCrafting *g, MCObject *w) {
    return CraftingManager_func_180303_b(((MCGameplayWorld *)w)->manager, g, w);
}
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
static const mc_crafting_dispatch deps = {MCGameplayPlayer_inventory,
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
static const char *base = "test-gameplay-packets.c919",
                  *uuid = "11111111-1111-1111-1111-111111111111";
static MCGameplayPlayer *setup(MCGameplay *game) {
    CHECK(MCGameplay_init(game, 32 * 1024 * 1024));
    CraftingManager *m = CraftingManager_newEmpty(game->heap);
    CHECK(m);
    MCGameplayWorld *w = MCGameplayWorld_new(game->heap, MCGameplay_get(game), NULL, m);
    CHECK(w && MCGameplay_setWorld(game, (MCObject *)w));
    MCGameplayPlayer *p =
        MCGameplayPlayer_new(w, NBTString_fromASCII(game->heap, "Player"), NULL, &deps);
    CHECK(p && MCGameplay_setPlayer(game, 0, uuid, (MCObject *)p));
    CHECK(MCGameplayPackets_bind(p));
    return p;
}
static bool encode_tag(NBTTagCompound *tag, mc_nbt *out) {
    mc_buf b = {0};
    bool ok = NBTWire_encodeCompound(&b, tag);
    if (ok)
        ok = mc_nbt_read(&b, out) && b.pos == b.len;
    mc_buf_free(&b);
    return ok;
}
typedef struct {
    bool fail;
    unsigned players;
} Encoder;
/* Real source InventoryPlayer storage and wire packet preflight are used. These
   fixture encoders cover this packet/inventory transaction only; no production
   world/entity/map serialization is implied. */
static bool player_encoder(const MCGameplayObjects *o, size_t index, mc_nbt *out, void *context) {
    Encoder *e = (Encoder *)context;
    ++e->players;
    if (e->fail)
        return false;
    MCGameplayPlayer *p = (MCGameplayPlayer *)o->players[index];
    if (!MCGameplayPackets_validate(p))
        return false;
    NBTTagCompound *tag = NBTTagCompound_new(o->object.heap);
    NBTTagList *list = NBTTagList_new(o->object.heap);
    CHECK(tag && list);
    return InventoryPlayer_writeToNBT(p->inventory, list) &&
           NBTTagCompound_setTag_ascii(tag, "Inventory", (NBTBase *)list) && encode_tag(tag, out);
}
static bool items_encoder(const MCGameplayObjects *o, mc_nbt *out, void *context) {
    (void)context;
    CHECK(o->itemCount == 0);
    NBTTagCompound *tag = NBTTagCompound_new(o->object.heap);
    NBTTagList *list = NBTTagList_new(o->object.heap);
    CHECK(tag && list);
    return NBTTagCompound_setTag_ascii(tag, "Items", (NBTBase *)list) && encode_tag(tag, out);
}
static const MCGameplayEncoders encoders = {player_encoder, items_encoder, NULL};
typedef struct {
    MCGameplay *game;
    unsigned calls, accepted;
    int failAt;
    bool reenter;
    bool appendDuringSend;
    int32_t lastId;
} Sink;
static bool sink(const mc_buf *b, void *context) {
    Sink *s = (Sink *)context;
    ++s->calls;
    CHECK(b->len && MCObjectHeap_hasBorrowers(s->game->heap));
    CHECK(!MCObjectHeap_collect(s->game->heap));
    if (s->reenter) {
        s->reenter = false;
        CHECK(MCGameplayPackets_flush(s->game, 0, sink, s) == MC_GAMEPLAY_PACKETS_FAILED);
    }
    if ((int)s->calls == s->failAt)
        return false;
    if (s->appendDuringSend) {
        MCGameplayPlayer *player = (MCGameplayPlayer *)MCGameplay_get(s->game)->players[0];
        CHECK(!MCGameplayPackets_sendCloseWindow(player, 42));
    }
    mc_buf view = *b;
    s->lastId = mc_get_varint(&view);
    CHECK(!view.failed);
    ++s->accepted;
    return true;
}
static void cleanup(void) {
    char path[256];
    snprintf(path, sizeof path, "%s.players/%s.dat", base, uuid);
    CHECK(remove(path) == 0);
    CHECK(remove("test-gameplay-packets.c919.items.dat") == 0);
    CHECK(remove_dir("test-gameplay-packets.c919.players") == 0);
}
static void source_map_packets(void) {
    MCGameplay game = {0};
    MCGameplayPlayer *player = setup(&game);
    MCObjectRootScope scope = {0}; CHECK(MCObjectRootScope_begin(&scope, game.heap));
    MCGameplayWorld *world = (MCGameplayWorld *)player->living.entity.worldObj;
    world->mapStorage = (MapStorage *)SaveDataMemoryStorage_new(game.heap);
    CHECK(world->mapStorage);
    MapData *map = NULL;
    CHECK(ItemMap_loadMapData(41, world, &map) == WORLD_SAVED_DATA_OK && map && world->maps.count == 0);
    Vec4b *icon = Vec4b_new(game.heap, 1, 2, 3, 4); CHECK(icon);
    CHECK(NativeLinkedHashMap_put(map->mapDecorations,
        (MCObject *)NBTString_fromASCII(game.heap, "player"), (MCObject *)icon));
    map->scale = 2; map->colors->values[0] = 19;
    MapInfo *info = NULL; S34PacketMaps *packet = NULL;
    CHECK(MapData_getMapInfo(map, player, &info) == WORLD_SAVED_DATA_OK && info);
    ItemStack *stack = ItemStack_new(game.heap, ItemStack_registryItem(358), 1, 41); CHECK(stack);
    CHECK(ItemMap_createMapDataPacket(stack, world, player, &packet) == WORLD_SAVED_DATA_OK && packet);
    CHECK(MCPacketQueue_append(player->pendingPackets, (MCObject *)packet, 0x34));
    MCGameplayPacketKind kind;
    CHECK(MCGameplayPackets_packetAt(player, 0, &kind) == (MCObject *)packet && kind == MC_GAMEPLAY_PACKET_MAP);
    map->colors->values[0] = 99;
    mc_buf wire = {0}; CHECK(MCGameplayPackets_encodeAt(player, 0, &wire));
    CHECK(mc_get_varint(&wire) == 0x34 && mc_get_varint(&wire) == 41 && mc_get_u8(&wire) == 2 &&
        mc_get_varint(&wire) == 1 && mc_get_u8(&wire) == 0x14 && mc_get_u8(&wire) == 2 && mc_get_u8(&wire) == 3);
    CHECK(mc_get_u8(&wire) == 128 && mc_get_u8(&wire) == 128 && mc_get_u8(&wire) == 0 &&
        mc_get_u8(&wire) == 0 && mc_get_varint(&wire) == 16384 && mc_get_u8(&wire) == 19);
    mc_buf_free(&wire); MCObjectRootScope_end(&scope);
    MCGameplayTransaction tx = {0}; CHECK(MCGameplay_begin(&game, &tx));
    CHECK(MCObjectRootScope_begin(&scope, tx.working.heap));
    MCGameplayPlayer *copyPlayer = (MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
    MapData *copyMap = NULL;
    CHECK(ItemMap_loadMapData(41, (World *)copyPlayer->living.entity.worldObj, &copyMap) == WORLD_SAVED_DATA_OK);
    S34PacketMaps *copyPacket = (S34PacketMaps *)MCGameplayPackets_packetAt(copyPlayer, 0, &kind);
    CHECK(copyPacket && copyPacket != packet && copyMap != map && copyMap->colors->values[0] == 99 &&
        copyPacket->mapDataBytes->values[0] == 19);
    MCObject *copyIcon = NativeLinkedHashMap_get(copyMap->mapDecorations,
        (MCObject *)NBTString_fromASCII(tx.working.heap, "player"));
    CHECK(copyIcon == copyPacket->mapVisiblePlayersVec4b->values[0] && copyIcon != (MCObject *)icon);
    CHECK(Vec4b_construct((Vec4b *)copyIcon, 6, 7, 8, 9));
    CHECK(MCGameplayPackets_encodeAt(copyPlayer, 0, &wire));
    wire.pos = 4; CHECK(mc_get_u8(&wire) == 0x69 && mc_get_u8(&wire) == 7 && mc_get_u8(&wire) == 8);
    mc_buf_free(&wire); MCObjectRootScope_end(&scope); CHECK(MCGameplay_abort(&tx));
    CHECK(icon->field_176117_a == 1 && map->colors->values[0] == 99 && !MCObjectHeap_failed(game.heap));
    CHECK(MCGameplay_free(&game));
}
int main(void) {
    source_map_packets();
    MCGameplay game = {0};
    MCGameplayPlayer *p = setup(&game);
    MCObjectRootScope scope = {0};
    CHECK(MCObjectRootScope_begin(&scope, game.heap));
    ItemStack *shared = ItemStack_new(game.heap, ItemStack_registryItem(1), 5, 0);
    CHECK(shared);
    NBTTagCompound *tag = NBTTagCompound_new(game.heap);
    CHECK(tag && NBTTagCompound_setInteger_ascii(tag, "value", 7) &&
          ItemStack_setTagCompound(shared, tag));
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory, 0, shared) &&
          InventoryPlayer_setInventorySlotContents(p->inventory, 1, shared));
    CHECK(Container_onCraftGuiOpened(p->openContainer, EntityPlayerMPWindows_listener(p)));
    CHECK(MCGameplayPackets_count(p) == 4);
    MCGameplayPacketKind kind;
    S30PacketWindowItems *items = (S30PacketWindowItems *)MCGameplayPackets_packetAt(p, 0, &kind);
    CHECK(kind == MC_GAMEPLAY_PACKET_ITEMS);
    ItemStack *a = items->itemStacks->items[36], *b = items->itemStacks->items[37];
    CHECK(a && b && a != b && a != shared && a->stackTagCompound != b->stackTagCompound);
    CHECK(a->stackSize == 5 && NBTTagCompound_getInteger_ascii(a->stackTagCompound, "value") == 7);
    shared->stackSize = 0;
    CHECK(NBTTagCompound_setInteger_ascii(tag, "value", 9));
    CHECK(a->stackSize == 5 && NBTTagCompound_getInteger_ascii(a->stackTagCompound, "value") == 7);
    for (int32_t i = 0; i < 70; i++)
        CHECK(MCGameplayPackets_sendCloseWindow(p, 256 + i));
    CHECK(MCGameplayPackets_count(p) == 74 && MCGameplayPackets_validate(p));
    mc_buf encoded = {0};
    CHECK(MCGameplayPackets_encodeAt(p, 73, &encoded));
    CHECK(encoded.len == 2 && encoded.data[0] == 0x2e && encoded.data[1] == 69);
    mc_buf_free(&encoded);
    Sink s = {&game, 0, 0, 0, false, false, 0};
    CHECK(MCGameplayPackets_flush(&game, 0, sink, &s) == MC_GAMEPLAY_PACKETS_UNCOMMITTED &&
          s.calls == 0);
    MCObjectRootScope_end(&scope);
    CHECK(make_dir("test-gameplay-packets.c919.players") == 0);
    MCGameplayTransaction tx = {0};
    CHECK(MCGameplay_begin(&game, &tx));
    MCGameplayPlayer *wp = (MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
    CHECK(wp != p && wp->pendingPackets != p->pendingPackets);
    CHECK(MCGameplayPackets_sendConfirmTransaction(wp, 0, 17, true));
    CHECK(MCGameplayPackets_flush(&tx.working, 0, sink, &s) == MC_GAMEPLAY_PACKETS_FAILED &&
          s.calls == 0);
    Encoder e = {true, 0};
    char error[256];
    CHECK(MCGameplay_commit(&tx, base, &encoders, &e, error, sizeof error) ==
          MC_GAMEPLAY_NOT_COMMITTED);
    CHECK(MCGameplay_get(&game)->commitSerial == 0 && MCGameplayPackets_count(p) == 74);
    CHECK(MCGameplayPackets_flush(&game, 0, sink, &s) == MC_GAMEPLAY_PACKETS_UNCOMMITTED &&
          s.calls == 0);
    /* Packet serialization failure must stop before the journal and leave the
       original graph/fence/queue untouched, just like encoder failure. */
    CHECK(MCGameplay_begin(&game, &tx));
    wp = (MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
    ItemStack *invalid = ItemStack_new(tx.working.heap, NULL, 1, 0);
    CHECK(invalid && MCGameplayPackets_sendSetSlot(wp, 0, 0, invalid));
    e = (Encoder){false, 0};
    CHECK(MCGameplay_commit(&tx, base, &encoders, &e, error, sizeof error) ==
          MC_GAMEPLAY_NOT_COMMITTED);
    CHECK(!game.fatal && !MCObjectHeap_failed(game.heap) &&
          MCGameplay_get(&game)->commitSerial == 0 && MCGameplayPackets_count(p) == 74 &&
          s.calls == 0);
    CHECK(MCGameplay_begin(&game, &tx));
    wp = (MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
    CHECK(MCGameplayPackets_sendConfirmTransaction(wp, 0, 17, true));
    e = (Encoder){false, 0};
    CHECK(MCGameplay_commit(&tx, base, &encoders, &e, error, sizeof error) ==
          MC_GAMEPLAY_COMMITTED);
    CHECK(e.players == 1 && MCGameplay_get(&game)->commitSerial == 1);
    p = (MCGameplayPlayer *)MCGameplay_get(&game)->players[0];
    CHECK(MCGameplayPackets_count(p) == 75);
    CHECK(InventoryPlayer_getStackInSlot(p->inventory, 0) ==
              InventoryPlayer_getStackInSlot(p->inventory, 1) &&
          InventoryPlayer_getStackInSlot(p->inventory, 0)->stackSize == 0);
    items = (S30PacketWindowItems *)MCGameplayPackets_packetAt(p, 0, &kind);
    CHECK(items->itemStacks->items[36]->stackSize == 5 &&
          items->itemStacks->items[36] != items->itemStacks->items[37]);
    s.failAt = 2;
    s.reenter = true;
    CHECK(MCGameplayPackets_flush(&game, 0, sink, &s) == MC_GAMEPLAY_PACKETS_FAILED);
    CHECK(s.calls == 2 && s.accepted == 1 && s.lastId == 0x30 && MCGameplayPackets_count(p) == 74 &&
          !game.fatal);
    s.failAt = 0;
    CHECK(MCGameplayPackets_flush(&game, 0, sink, &s) == MC_GAMEPLAY_PACKETS_SENT);
    CHECK(s.accepted == 75 && s.lastId == 0x32 && MCGameplayPackets_count(p) == 0);
    CHECK(MCGameplayPackets_sendCloseWindow(p, 5));
    unsigned before = s.calls;
    CHECK(MCGameplayPackets_flush(&game, 0, sink, &s) == MC_GAMEPLAY_PACKETS_UNCOMMITTED &&
          s.calls == before);
    CHECK(MCGameplay_begin(&game, &tx));
    e = (Encoder){false, 0};
    CHECK(MCGameplay_commit(&tx, base, &encoders, &e, error, sizeof error) ==
          MC_GAMEPLAY_COMMITTED);
    CHECK(MCGameplay_get(&game)->commitSerial == 2);
    p = (MCGameplayPlayer *)MCGameplay_get(&game)->players[0];
    CHECK(MCGameplayPackets_flush(&game, 0, sink, &s) == MC_GAMEPLAY_PACKETS_SENT &&
          s.calls == before + 1 && s.lastId == 0x2e);
    CHECK(MCGameplayPackets_sendCloseWindow(p, 6));
    CHECK(MCGameplayPackets_sendCloseWindow(p, 7));
    CHECK(MCGameplay_begin(&game, &tx));
    e = (Encoder){false, 0};
    CHECK(MCGameplay_commit(&tx, base, &encoders, &e, error, sizeof error) ==
          MC_GAMEPLAY_COMMITTED);
    p = (MCGameplayPlayer *)MCGameplay_get(&game)->players[0];
    before = s.accepted;
    s.appendDuringSend = true;
    CHECK(MCGameplayPackets_flush(&game, 0, sink, &s) == MC_GAMEPLAY_PACKETS_FAILED);
    CHECK(game.fatal && s.accepted == before + 1 && MCGameplayPackets_count(p) == 1);
    CHECK(!MCGameplay_begin(&game, &tx));
    CHECK(!MCObjectHeap_collect(game.heap));
    CHECK(MCGameplay_free(&game));
    cleanup();
    /* The generic owner supports other MCObject actors. A packet adapter must
       check the native player class before accessing its larger field layout. */
    CHECK(MCGameplay_init(&game, 1024 * 1024));
    NBTTagByte *small = NBTTagByte_new(game.heap, 1);
    CHECK(small && MCGameplay_setPlayer(&game, 0, uuid, (MCObject *)small));
    before = s.calls;
    CHECK(MCGameplayPackets_flush(&game, 0, sink, &s) == MC_GAMEPLAY_PACKETS_FAILED &&
          s.calls == before && !MCObjectHeap_failed(game.heap));
    CHECK(MCGameplay_free(&game));
    printf("gameplay packets: %u checks\n", checks);
    return 0;
}
