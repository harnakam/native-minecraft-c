#include "server/native_gameplay.h"
#include "server/management/ItemInWorldManagerUse.h"
#include "util/MCGameplayPackets.h"
#include "entity/player/EntityPlayerDrops.h"
#include "entity/player/EntityPlayerMPStats.h"
#include "item/ItemStackCrafting.h"
#include "item/ItemAnimation.h"
#include "item/ItemMap.h"
#include "entity/player/InventoryPlayerAnimations.h"
#include "item/ItemMapCreated.h"
#include "item/ItemEmptyMap.h"
#include "item/item.h"
#include "inventory/ContainerWorkbench.h"
#include "stats/StatisticsFile.h"
#include "stats/StatList.h"
#include "util/MathHelper.h"
#include "world/WorldSettingsGameType.h"
#include "entity/item/NativeItemMotion.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>

/* Registry subclass resolution is a native binding. In this original version
   ItemMap is the sole Item.onUpdate override; all other registered Items inherit
   the actual empty Item body. The source stack/inventory methods own ordering. */
static bool item_on_update(MCObject *context, const Item *item, ItemStack *stack,
                          MCObject *world, MCObject *entity, int32_t slot, bool selected) {
    (void)context;
    if (item == ItemStack_registryItem(358)) {
        bool changed = false;
        return ItemMap_onUpdate(stack, (MCGameplayWorld *)world, entity, slot, selected, &changed);
    }
    if (!ItemStack_registryIsKnownItem(item)) {
        MCObjectHeap_fail(stack->object.heap);
        return false;
    }
    Item_onUpdate(item, stack, world, entity, slot, selected);
    return !MCObjectHeap_failed(stack->object.heap);
}

bool mc_server_graph_tick_inventory(MCGameplayPlayer *player) {
    static const ItemStackAnimationDependencies item = {item_on_update};
    static const InventoryPlayerAnimationDependencies inventory = {MCGameplayPlayer_world, &item};
    if (!MCGameplayPlayer_isInstance((MCObject *)player)) {
        MCObjectHeap_fail(player ? player->object.heap : NULL);
        return false;
    }
    return InventoryPlayer_decrementAnimations(player->inventory, &inventory, NULL);
}

#ifdef _WIN32
#include <windows.h>
typedef DWORD RuntimeThread;
static RuntimeThread current_thread(void) {
    return GetCurrentThreadId();
}
static bool same_thread(RuntimeThread a, RuntimeThread b) {
    return a == b;
}
#else
#include <pthread.h>
typedef pthread_t RuntimeThread;
static RuntimeThread current_thread(void) {
    return pthread_self();
}
static bool same_thread(RuntimeThread a, RuntimeThread b) {
    return pthread_equal(a, b) != 0;
}
#endif

/* Native storage for the real initially empty world scoreboard. Its objective
   collection is managed state, rather than an empty iterator callback. Commands,
   persistence and the complete Scoreboard/ObjectiveStat classes remain ports. */
typedef struct {
    MCObject object;
    MCObject *criteria;
    NBTString *name;
} RuntimeObjective;
typedef struct {
    MCObject object;
    RuntimeObjective *objective;
    NBTString *name;
    int32_t points;
} RuntimeScore;
typedef struct {
    MCObject object;
    MCObject *objectives[128];
    size_t objectiveCount;
    RuntimeScore *scores[1024];
    size_t scoreCount;
} RuntimeScoreboard;
typedef struct {
    MCObject object;
    MCObject *values[128];
    size_t count, index;
} RuntimeIterator;
typedef struct {
    MCObject object;
    RuntimeThread thread;
    RuntimeScoreboard *scoreboard;
    bool announceAchievements;
} RuntimeWorld;
typedef struct {
    MCObject object;
    MCGameplayPlayer *actor;
    int32_t currentWindowId;
    int64_t lastActiveTick;
    bool managerCreative, managerSpectator, usingItem;
} RuntimeActor;
static bool fail(MCObjectHeap *heap) {
    MCObjectHeap_fail(heap);
    return false;
}
static void trace_board(MCObject *o, MCObjectVisitor visit, void *ctx) {
    RuntimeScoreboard *b = (RuntimeScoreboard *)o;
    for (size_t i = 0; i < b->objectiveCount; i++)
        b->objectives[i] = visit(b->objectives[i], ctx);
    for (size_t i = 0; i < b->scoreCount; i++)
        b->scores[i] = (RuntimeScore *)visit((MCObject *)b->scores[i], ctx);
}
static void trace_iterator(MCObject *o, MCObjectVisitor v, void *c) {
    RuntimeIterator *i = (RuntimeIterator *)o;
    for (size_t n = 0; n < i->count; n++)
        i->values[n] = v(i->values[n], c);
}
static void trace_world(MCObject *o, MCObjectVisitor v, void *c) {
    RuntimeWorld *w = (RuntimeWorld *)o;
    w->scoreboard = (RuntimeScoreboard *)v((MCObject *)w->scoreboard, c);
}
static void trace_actor(MCObject *o, MCObjectVisitor v, void *c) {
    RuntimeActor *a = (RuntimeActor *)o;
    a->actor = (MCGameplayPlayer *)v((MCObject *)a->actor, c);
}
static void trace_score(MCObject *o, MCObjectVisitor v, void *c) {
    RuntimeScore *s = (RuntimeScore *)o;
    s->objective = (RuntimeObjective *)v((MCObject *)s->objective, c);
    s->name = (NBTString *)v((MCObject *)s->name, c);
}
static const MCObjectClass boardClass = {"C919.native.ScoreboardState", MCObjectHeap_plainClone,
                                         trace_board, NULL};
static const MCObjectClass iteratorClass = {"C919.native.ScoreObjectives", MCObjectHeap_plainClone,
                                            trace_iterator, NULL};
static const MCObjectClass worldClass = {"C919.native.ServerWorldBindings", MCObjectHeap_plainClone,
                                         trace_world, NULL};
static const MCObjectClass actorClass = {"C919.native.ServerActorBindings", MCObjectHeap_plainClone,
                                         trace_actor, NULL};
static const MCObjectClass scoreClass = {"C919.native.ScoreState", MCObjectHeap_plainClone,
                                         trace_score, NULL};
static RuntimeWorld *bindings(MCGameplayWorld *w) {
    if (!w || !w->nativeContext || w->nativeContext->klass != &worldClass) {
        fail(w ? w->object.heap : NULL);
        return NULL;
    }
    return (RuntimeWorld *)w->nativeContext;
}
static RuntimeActor *actor_bindings(MCGameplayPlayer *p) {
    if (!p || !p->effects || p->effects->klass != &actorClass) {
        fail(p ? p->object.heap : NULL);
        return NULL;
    }
    return (RuntimeActor *)p->effects;
}
MCGameplayWorld *mc_server_graph_world(MCGameplay *game) {
    MCGameplayObjects *o = MCGameplay_get(game);
    return o && MCGameplayWorld_isInstance(o->world) ? (MCGameplayWorld *)o->world : NULL;
}
MCGameplayPlayer *mc_server_graph_player(MCGameplay *game, size_t index) {
    MCGameplayObjects *o = MCGameplay_get(game);
    return o && index < MC_TRANSFER_MAX_PLAYERS && MCGameplayPlayer_isInstance(o->players[index])
               ? (MCGameplayPlayer *)o->players[index]
               : NULL;
}
static bool emit(MCGameplayWorld *w, mc_buf *packet) {
    bool ok = !packet->failed;
    for (size_t i = 0; i < MC_TRANSFER_MAX_PLAYERS && ok; i++) {
        MCGameplayPlayer *p = (MCGameplayPlayer *)w->owners->players[i];
        if (p && !p->isDead)
            ok = MCGameplayPackets_sendNative(p, packet);
    }
    mc_buf_free(packet);
    return ok || fail(w->object.heap);
}
static void start(mc_buf *p, int32_t id) {
    mc_buf_init(p);
    mc_put_varint(p, id);
}
static int16_t velocity(double value) {
    if (value < -3.9)
        value = -3.9;
    if (value > 3.9)
        value = 3.9;
    return (int16_t)(value * 8000.0);
}
bool mc_server_graph_send_item(MCGameplayPlayer *recipient, EntityItem *item) {
    if (!NativeItemMotion_validate(item))
        return false;
    MCObjectHeap *heap = item->entity.object.heap;
    mc_buf spawn;
    start(&spawn, 0x0e);
    mc_put_varint(&spawn, item->entity.entityId);
    mc_put_u8(&spawn, 2);
    mc_put_i32(&spawn, (int32_t)floor(item->entity.posX * 32.0));
    mc_put_i32(&spawn, (int32_t)floor(item->entity.posY * 32.0));
    mc_put_i32(&spawn, (int32_t)floor(item->entity.posZ * 32.0));
    mc_put_u8(&spawn, 0);
    mc_put_u8(&spawn, 0);
    mc_put_i32(&spawn, 1);
    mc_put_i16(&spawn, velocity(item->entity.motionX));
    mc_put_i16(&spawn, velocity(item->entity.motionY));
    mc_put_i16(&spawn, velocity(item->entity.motionZ));
    bool ok = MCGameplayPackets_sendNative(recipient, &spawn);
    mc_buf_free(&spawn);
    S1CPacketEntityMetadata *metadata =
        ok ? S1CPacketEntityMetadata_new(heap, item->entity.entityId, item->entity.dataWatcher, true) : NULL;
    if (ok)
        ok = metadata && MCGameplayPackets_sendMetadata(recipient, metadata);
    return ok || fail(heap);
}
bool mc_server_graph_send_motion(EntityItem *e) {
    if (!NativeItemMotion_validate(e))
        return false;
    MCGameplayWorld *w = (MCGameplayWorld *)e->entity.worldObj;
    mc_buf p;
    start(&p, 0x18);
    mc_put_varint(&p, e->entity.entityId);
    mc_put_i32(&p, (int32_t)floor(e->entity.posX * 32.0));
    mc_put_i32(&p, (int32_t)floor(e->entity.posY * 32.0));
    mc_put_i32(&p, (int32_t)floor(e->entity.posZ * 32.0));
    mc_put_u8(&p, 0);
    mc_put_u8(&p, 0);
    mc_put_u8(&p, e->entity.onGround);
    if (!emit(w, &p))
        return false;
    start(&p, 0x12);
    mc_put_varint(&p, e->entity.entityId);
    mc_put_i16(&p, velocity(e->entity.motionX));
    mc_put_i16(&p, velocity(e->entity.motionY));
    mc_put_i16(&p, velocity(e->entity.motionZ));
    return emit(w, &p);
}
static MCObject *get_board(MCObject *ctx, MCGameplayPlayer *p) {
    (void)ctx;
    RuntimeWorld *r = bindings(p->worldObj);
    return r ? (MCObject *)r->scoreboard : NULL;
}
static MCObject *criteria(MCObject *ctx, StatBase *stat) {
    (void)ctx;
    return (MCObject *)stat;
}
static MCObject *objectives(MCObject *ctx, MCObject *board, MCObject *key) {
    (void)ctx;
    RuntimeScoreboard *b = (RuntimeScoreboard *)board;
    if (!board || board->klass != &boardClass)
        return fail(board ? board->heap : NULL), NULL;
    RuntimeIterator *out =
        (RuntimeIterator *)MCObjectHeap_alloc(board->heap, sizeof(*out), &iteratorClass);
    if (!out)
        return NULL;
    for (size_t i = 0; i < b->objectiveCount; i++) {
        RuntimeObjective *o = (RuntimeObjective *)b->objectives[i];
        if (o->criteria == key)
            out->values[out->count++] = (MCObject *)o;
    }
    return (MCObject *)out;
}
static MCObject *iterator(MCObject *ctx, MCObject *list) {
    (void)ctx;
    return list;
}
static bool has_next(MCObject *ctx, MCObject *list) {
    (void)ctx;
    RuntimeIterator *i = (RuntimeIterator *)list;
    return i->index < i->count;
}
static MCObject *next(MCObject *ctx, MCObject *list) {
    (void)ctx;
    RuntimeIterator *i = (RuntimeIterator *)list;
    if (i->index >= i->count)
        return fail(list->heap), NULL;
    return i->values[i->index++];
}
static NBTString *player_name(MCObject *ctx, MCGameplayPlayer *p) {
    (void)ctx;
    return p->name;
}
static MCObject *score(MCObject *ctx, MCObject *board, const NBTString *name, MCObject *objective) {
    (void)ctx;
    RuntimeScoreboard *b = (RuntimeScoreboard *)board;
    for (size_t i = 0; i < b->scoreCount; i++)
        if (b->scores[i]->objective == (RuntimeObjective *)objective &&
            NBTString_equals(b->scores[i]->name, name))
            return (MCObject *)b->scores[i];
    if (b->scoreCount == 1024)
        return fail(board->heap), NULL;
    RuntimeScore *s = (RuntimeScore *)MCObjectHeap_alloc(board->heap, sizeof(*s), &scoreClass);
    if (!s)
        return NULL;
    s->objective = (RuntimeObjective *)objective;
    s->name = (NBTString *)name;
    b->scores[b->scoreCount++] = s;
    return (MCObject *)s;
}
static bool increment(MCObject *ctx, MCObject *o, int32_t amount) {
    (void)ctx;
    RuntimeScore *s = (RuntimeScore *)o;
    uint32_t value = (uint32_t)s->points + (uint32_t)amount;
    s->points = value <= INT32_MAX ? (int32_t)value : -1 - (int32_t)(UINT32_MAX - value);
    MCObjectHeap_touch(o->heap);
    return true;
}
static bool points(MCObject *ctx, MCObject *o, int32_t value) {
    (void)ctx;
    ((RuntimeScore *)o)->points = value;
    MCObjectHeap_touch(o->heap);
    return true;
}
static const EntityPlayerMPStatsDependencies statDependencies = {
    get_board, criteria,    objectives, iterator,  has_next,
    next,      player_name, score,      increment, points};
static bool add_stat(MCGameplayPlayer *p, StatBase *stat, int32_t amount) {
    return EntityPlayerMP_addStat(p, stat, amount, (MCObject *)p->worldObj, &statDependencies);
}
static bool announcing(MCObject *ctx, MCObject *server) {
    (void)ctx;
    return ((RuntimeWorld *)server)->announceAchievements;
}
static bool announcement(MCObject *ctx, MCObject *server, MCObject *p, StatBase *stat, bool taken) {
    (void)server;
    (void)p;
    (void)stat;
    (void)taken;
    return fail(ctx->heap);
}
static int32_t tick_counter(MCObject *ctx, MCObject *server) {
    (void)server;
    return (int32_t)((MCGameplayWorld *)ctx)->worldTime;
}
static bool send_stats(MCObject *ctx, MCObject *object, StatisticsFileIntMap *map) {
    (void)ctx;
    MCGameplayPlayer *p = (MCGameplayPlayer *)object;
    mc_buf out;
    start(&out, 0x37);
    size_t count = StatisticsFileIntMap_size(map);
    mc_put_varint(&out, (int32_t)count);
    PacketBuffer buffer;
    bool ok = PacketBuffer_init(&buffer, p->object.heap, &out);
    for (size_t i = 0; i < count && ok; i++) {
        StatBase *s = NULL;
        int32_t value = 0;
        ok = StatisticsFileIntMap_entry(map, i, &s, &value) && s &&
             PacketBuffer_writeString(&buffer, s->statId) &&
             PacketBuffer_writeVarIntToBuffer(&buffer, value);
    }
    if (ok)
        ok = MCGameplayPackets_sendNative(p, &out);
    mc_buf_free(&out);
    return ok;
}
static const StatisticsFileDependencies statisticsDependencies = {announcing, announcement,
                                                                  tick_counter, send_stats};
static StatBase *achievement_stat(MCGameplayPlayer *p, const char *id) {
    return StatList_getOneShotStat_ascii(p->worldObj->statList, id);
}
static bool craft_stat(MCObject *object, const Item *item, int32_t amount) {
    MCGameplayPlayer *p = (MCGameplayPlayer *)object;
    int32_t id = ItemStack_registryId(item);
    StatBase *stat =
        id >= 0 && id < (int32_t)MC_GAMEPLAY_CRAFT_STAT_COUNT ? p->worldObj->craftStats[id] : NULL;
    return add_stat(p, stat, amount);
}
static bool created(ItemStack *stack, MCObject *world, MCObject *player) {
    if (ItemStack_getItem(stack) == ItemStack_registryItem(358))
        return ItemMap_onCreated(stack, (MCGameplayWorld *)world, player);
    /* The original base Item.onCreated body is empty. Other overrides are
       dispatched separately as their classes are translated. */
    return true;
}
static bool crafting(ItemStack *s, MCObject *w, MCObject *p, int32_t amount) {
    static const ItemStackCraftingDispatch d = {craft_stat, created};
    return ItemStack_onCrafting(s, w, p, amount, &d);
}
static bool craft_achievement(MCObject *object, mc_crafting_achievement achievement) {
    static const char *ids[] = {
        "achievement.buildWorkBench",     "achievement.buildPickaxe", "achievement.buildFurnace",
        "achievement.buildHoe",           "achievement.makeBread",    "achievement.bakeCake",
        "achievement.buildBetterPickaxe", "achievement.buildSword",   "achievement.enchantments",
        "achievement.bookcase",           "achievement.overpowered"};
    if ((unsigned)achievement >= sizeof ids / sizeof *ids)
        return fail(object->heap);
    MCGameplayPlayer *p = (MCGameplayPlayer *)object;
    StatBase *s = achievement_stat(p, ids[achievement]);
    return s && add_stat(p, s, 1);
}
static double random_double(MCObject *ctx) {
    double value=0;
    if (!MCGameplayWorld_isInstance(ctx)||
        !NativeJavaRandomRuntime_mathRandom(((MCGameplayWorld *)ctx)->randomRuntime,&value)) {
        MCObjectHeap_fail(ctx?ctx->heap:NULL);return 0;
    }
    return value;
}
static bool entity_init(MCObject *c,Entity *e) {
    return MCGameplayWorld_isInstance(c) && EntityItem_entityInit((EntityItem *)e);
}
static bool entity_position(MCObject *c,Entity *e,double x,double y,double z) {
    return MCGameplayWorld_isInstance(c) && Entity_setPosition(e,x,y,z);
}
static bool entity_box(MCObject *c,Entity *e,AxisAlignedBB *box) {
    return MCGameplayWorld_isInstance(c) && Entity_setEntityBoundingBox(e,box);
}
static bool entity_dimension(MCObject *c,MCObject *world,int32_t *out) {
    if (!MCGameplayWorld_isInstance(c) || !MCGameplayWorld_isInstance(world) || !out) return false;
    *out=((MCGameplayWorld *)world)->dimension;return true;
}
static bool entity_remote(MCObject *c,MCObject *world,bool *out) {
    if (!MCGameplayWorld_isInstance(c) || !MCGameplayWorld_isInstance(world) || !out) return false;
    *out=((MCGameplayWorld *)world)->remote;return true;
}
static bool entity_location(MCObject *c,Entity *e,double x,double y,double z,float yaw,float pitch) {
    return MCGameplayWorld_isInstance(c) && Entity_setLocationAndAngles(e,x,y,z,yaw,pitch);
}
static const EntityDependencies entity_base_dependencies={.entityInit=entity_init,.setPosition=entity_position,
    .setEntityBoundingBox=entity_box,.getDimensionId=entity_dimension,.isRemote=entity_remote,.setLocationAndAngles=entity_location};
static bool base_entity(MCObject *c,EntityItem *e,MCObject *w) {
    return MCGameplayWorld_isInstance(c) && MCGameplayWorld_isInstance(w) &&
        Entity_construct(&e->entity,w,&entity_base_dependencies,c,((MCGameplayWorld *)w)->randomRuntime,NativeEntityIDRuntime_process());
}
static bool size_entity(MCObject *c,EntityItem *e,float w,float h) {return MCGameplayWorld_isInstance(c)&&Entity_setSize(&e->entity,w,h);}
static bool position_entity(MCObject *c,EntityItem *e,double x,double y,double z) {
    if (!MCGameplayWorld_isInstance(c)||!isfinite(x)||!isfinite(y)||!isfinite(z)) return fail(e->entity.object.heap);
    return Entity_setPosition(&e->entity,x,y,z);
}
static const EntityItemConstructorDependencies constructors = {base_entity, random_double,
                                                               size_entity, position_entity};
static bool missing_item(MCObject *ctx, int32_t id) {
    (void)ctx;
    fprintf(stderr, "EntityItem %d has no watcher stack.\n", id);
    return true;
}
static bool remote(MCObject *ctx, MCObject *world) {
    (void)ctx;
    return MCGameplayWorld_isRemote(world);
}
static InventoryPlayer *inventory(MCObject *ctx, MCObject *p) {
    (void)ctx;
    return MCGameplayPlayer_inventory(p);
}
static const NBTString *name(MCObject *ctx, MCObject *p) {
    (void)ctx;
    return ((MCGameplayPlayer *)p)->name;
}
static MCObject *find_player(MCObject *ctx, MCObject *object, const NBTString *n) {
    (void)ctx;
    MCGameplayWorld *w = (MCGameplayWorld *)object;
    for (size_t i = 0; i < MC_TRANSFER_MAX_PLAYERS; i++) {
        MCGameplayPlayer *p = (MCGameplayPlayer *)w->owners->players[i];
        if (p && !p->isDead && NBTString_equals(p->name, n))
            return (MCObject *)p;
    }
    return NULL;
}
static bool item_achievement(MCObject *ctx, MCObject *object, EntityItemAchievement value) {
    (void)ctx;
    static const char *ids[] = {"achievement.mineWood", "achievement.killCow",
                                "achievement.diamonds", "achievement.blazeRod",
                                "achievement.diamondsToYou"};
    if ((unsigned)value >= sizeof ids / sizeof *ids)
        return fail(object->heap);
    MCGameplayPlayer *p = (MCGameplayPlayer *)object;
    StatBase *s = achievement_stat(p, ids[value]);
    return s && add_stat(p, s, 1);
}
static bool silent(MCObject *ctx, const EntityItem *e) {
    if (!MCGameplayWorld_isInstance(ctx)||!EntityItem_isInstance((const MCObject *)e)||e->entity.object.heap!=ctx->heap)
        return fail(ctx?ctx->heap:NULL);
    return Entity_isSilent((Entity *)&e->entity);
}
static float entity_float(MCObject *ctx, EntityItem *e) {
    (void)ctx;
    float value=0;
    if (!EntityItem_isInstance((MCObject *)e)||!e->entity.rand||e->entity.rand->object.heap!=e->entity.object.heap||
        !NativeJavaRandom_nextFloat(e->entity.rand,&value)) {
        MCObjectHeap_fail(e?e->entity.object.heap:NULL);return 0;
    }
    return value;
}
static bool sound(MCObject *ctx, MCObject *world, MCObject *object, const char *soundName,
                  float volume, float pitch) {
    (void)ctx;
    MCGameplayPlayer *p = (MCGameplayPlayer *)object;
    mc_buf out;
    start(&out, 0x29);
    mc_put_string(&out, soundName);
    mc_put_i32(&out, (int32_t)(p->posX * 8));
    mc_put_i32(&out, (int32_t)(p->posY * 8));
    mc_put_i32(&out, (int32_t)(p->posZ * 8));
    mc_put_f32(&out, volume);
    int32_t encoded = (int32_t)(pitch * 63.0F);
    if (encoded > 255)
        encoded = 255;
    if (encoded < 0)
        encoded = 0;
    mc_put_u8(&out, (uint8_t)encoded);
    return emit((MCGameplayWorld *)world, &out);
}
static bool pickup(MCObject *ctx, MCObject *object, EntityItem *e, int32_t count) {
    (void)ctx;
    (void)count;
    MCGameplayPlayer *p = (MCGameplayPlayer *)object;
    mc_buf out;
    start(&out, 0x0d);
    mc_put_varint(&out, e->entity.entityId);
    mc_put_varint(&out, p->entityId);
    return emit(p->worldObj, &out);
}
static bool dead(MCObject *ctx, EntityItem *e) {
    (void)ctx;
    bool alreadyDead = e->entity.isDead;
    e->entity.isDead = true;
    MCObjectHeap_touch(e->entity.object.heap);
    if (alreadyDead)
        return true;
    mc_buf out;
    start(&out, 0x13);
    mc_put_varint(&out, 1);
    mc_put_varint(&out, e->entity.entityId);
    return emit((MCGameplayWorld *)e->entity.worldObj, &out);
}
bool mc_server_graph_kill_item(EntityItem *e) {
    return dead(e->dependencyContext, e);
}
static const EntityItemDependencies itemDependencies = {
    missing_item, remote,       inventory, name,   find_player, item_achievement,
    silent,       entity_float, sound,     pickup, dead};
const EntityItemDependencies *mc_server_graph_item_dependencies(void) {
    return &itemDependencies;
}
const EntityItemConstructorDependencies *mc_server_graph_item_constructors(void) {
    return &constructors;
}
static float eye_height(MCObject *ctx, MCGameplayPlayer *p) {
    (void)ctx;
    return EntityPlayer_getEyeHeight(p);
}
static float player_float(MCObject *ctx, MCGameplayPlayer *p) {
    (void)ctx;
    float value=0;
    if (!MCGameplayPlayer_isInstance((MCObject *)p)||!p->rand||p->rand->object.heap!=p->object.heap||
        !NativeJavaRandom_nextFloat(p->rand,&value)) {
        MCObjectHeap_fail(p?p->object.heap:NULL);return 0;
    }
    return value;
}
int32_t mc_server_graph_allocate_entity(MCGameplayWorld *w) {
    for (size_t tries = 0; tries < MC_GAMEPLAY_MAX_ITEMS + MC_TRANSFER_MAX_PLAYERS + 1; tries++) {
        int32_t id = w->nextEntityId;
        w->nextEntityId = id == INT32_MAX ? 1 : id + 1;
        bool used = id <= 0;
        for (size_t i = 0; i < w->owners->itemCount && !used; i++)
            used = ((EntityItem *)w->owners->items[i])->entity.entityId == id;
        for (size_t i = 0; i < MC_TRANSFER_MAX_PLAYERS && !used; i++) {
            MCGameplayPlayer *p = (MCGameplayPlayer *)w->owners->players[i];
            used = p && p->entityId == id;
        }
        if (!used)
            return id;
    }
    fail(w->object.heap);
    return 0;
}
static bool join_item(MCObject *ctx, MCGameplayPlayer *p, EntityItem *e) {
    (void)ctx;
    MCGameplayWorld *w = p->worldObj;
    MCGameplayObjects *o = w->owners;
    if (o->itemCount == MC_GAMEPLAY_MAX_ITEMS)
        return fail(e->entity.object.heap);
    o->items[o->itemCount++] = (MCObject *)e;
    MCObjectHeap_touch(e->entity.object.heap);
    for (size_t i = 0; i < MC_TRANSFER_MAX_PLAYERS; i++) {
        MCGameplayPlayer *recipient = (MCGameplayPlayer *)o->players[i];
        if (recipient && !recipient->isDead && !mc_server_graph_send_item(recipient, e))
            return false;
    }
    return true;
}
static bool drop_stat(MCObject *ctx, MCGameplayPlayer *p) {
    (void)ctx;
    return add_stat(p, p->worldObj->statList->dropStat, 1);
}
static double sine(MCObject *ctx, double v) {
    (void)ctx;
    return sin(v);
}
static double cosine(MCObject *ctx, double v) {
    (void)ctx;
    return cos(v);
}
static const EntityPlayerDropsDependencies dropDependencies = {
    &itemDependencies, &constructors, eye_height, player_float, player_name,
    join_item,         drop_stat,     sine,       cosine};
static bool drop(MCObject *object, ItemStack *stack, bool scatter) {
    MCGameplayPlayer *p = (MCGameplayPlayer *)object;
    EntityPlayer_dropItem(p, stack, scatter, false, &dropDependencies, (MCObject *)p->worldObj);
    return !MCObjectHeap_failed(p->object.heap);
}
static mc_crafting_dispatch craftingDispatch;
const mc_crafting_dispatch *mc_server_graph_crafting(void) {
    return &craftingDispatch;
}
static MCPacketThreadResult packet_thread(MCObject *ctx, NetHandlerPlayServer *h,
                                          MCObject *packet) {
    (void)ctx;
    (void)packet;
    RuntimeWorld *w = bindings(h->playerEntity->worldObj);
    return w && same_thread(w->thread, current_thread()) ? MC_PACKET_THREAD_EXECUTE
                                                         : MC_PACKET_THREAD_FAILED;
}
static bool active(MCObject *ctx, MCGameplayPlayer *p) {
    (void)ctx;
    RuntimeActor *a = actor_bindings(p);
    if (!a)
        return false;
    a->lastActiveTick = p->worldObj->worldTime;
    MCObjectHeap_touch(p->object.heap);
    return true;
}
static bool close_container(MCObject *ctx, MCGameplayPlayer *p) {
    (void)ctx;
    return EntityPlayerMPWindows_closeContainer(p);
}
static bool confirm(MCObject *ctx, MCGameplayPlayer *p, int32_t window, int16_t action,
                    bool accepted) {
    (void)ctx;
    return MCGameplayPackets_sendConfirmTransaction(p, window, action, accepted);
}
static bool update_container(MCObject *ctx, MCGameplayPlayer *p, Container *c,
                             ContainerList *stacks) {
    (void)ctx;
    return EntityPlayerMPWindows_updateCraftingInventory(p, c, stacks);
}
static bool held(MCObject *ctx, MCGameplayPlayer *p) {
    (void)ctx;
    return EntityPlayerMPWindows_updateHeldItem(p);
}
static MCObject *tile(MCObject *ctx, MCGameplayWorld *w, int32_t x, int32_t y, int32_t z) {
    (void)ctx;
    (void)x;
    (void)y;
    (void)z;
    /* The current native terrain adapter has no tile-entity collection. */
    if (!w->terrain)
        fail(w->object.heap);
    return NULL;
}
static bool tile_nbt(MCObject *ctx, MCObject *object, NBTTagCompound *out) {
    (void)object;
    (void)out;
    return fail(ctx->heap);
}
static EntityItem *drop_choice(MCObject *ctx, MCGameplayPlayer *p, ItemStack *s, bool unused) {
    (void)ctx;
    return EntityPlayer_dropPlayerItemWithRandomChoice(p, s, unused, &dropDependencies,
                                                       (MCObject *)p->worldObj);
}
static const NetHandlerPlayServerDependencies handlerDependencies = {
    packet_thread, active, close_container, confirm,    update_container,
    held,          tile,   tile_nbt,        drop_choice};
static NBTString *display_name(MCObject *ctx, const ItemStack *stack) {
    return NBTString_fromUTF8(ctx->heap, mc_item_name((int16_t)ItemStack_registryId(stack->item)));
}
bool mc_server_graph_init(MCGameplay *game, const mc_world *terrain, int32_t spawnX, int32_t spawnZ,
                          uint64_t seed) {
    if (!MCGameplay_init(game, 256u * 1024u * 1024u))
        return false;
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, game->heap))
        return false;
    static const MCGameplayCraftingEffects effects = {crafting, craft_achievement, drop};
    bool ok = MCGameplayCrafting_nativeDispatch(&craftingDispatch, &effects);
    MCGameplayWorld *w =
        ok ? MCGameplayWorld_new(game->heap, MCGameplay_get(game), terrain, NULL) : NULL;
    RuntimeWorld *r =
        w ? (RuntimeWorld *)MCObjectHeap_alloc(game->heap, sizeof(*r), &worldClass) : NULL;
    if (r)
        r->scoreboard = (RuntimeScoreboard *)MCObjectHeap_alloc(game->heap, sizeof(*r->scoreboard),
                                                                &boardClass);
    ok = r && r->scoreboard;
    if (ok) {
        r->thread = current_thread();
        w->nativeContext = (MCObject *)r;
        w->spawnX = spawnX;
        w->spawnZ = spawnZ;
        w->nextEntityId = 1;
        /* The terrain generation seed is not World.rand/Entity.rand or Math's
           process seed. Native no-argument construction owns those streams. */
        (void)seed;
        ok = MCGameplayCrafting_configureWorld(w, display_name, (MCObject *)w) &&
             MCGameplay_setWorld(game, (MCObject *)w);
    }
    MCObjectRootScope_end(&scope);
    return ok && !MCObjectHeap_failed(game->heap);
}
static bool add_player(MCGameplay *game, size_t index, const char *uuid,
                                const char *nameText, int32_t entityId, double x, double y,
                                double z, bool creative,bool preserveSourceID) {
    MCGameplayWorld *w = mc_server_graph_world(game);
    MCObjectHeap *heap = game->heap;
    if (!w || !game->snapshot || index >= MC_TRANSFER_MAX_PLAYERS || w->owners->players[index])
        return fail(heap);
    NBTString *n = NBTString_fromASCII(heap, nameText);
    RuntimeWorld *r = bindings(w);
    StatisticsFile *stats =
        r ? StatisticsFile_nativeNew(heap, (MCObject *)r, NBTString_literalASCII(heap, ""),
                                     (MCObject *)w, &statisticsDependencies)
          : NULL;
    MCGameplayPlayer *p =
        stats && n ? MCGameplayPlayer_new(w, n, (StatFileWriter *)stats, &craftingDispatch) : NULL;
    if (p) {
        NBTString *profile=NBTString_fromASCII(heap,uuid);
        p->gameProfileUUID=profile?NativeJavaUUID_fromString(heap,profile):NULL;
        if (!p->gameProfileUUID) return false;
        p->entityUniqueID=p->gameProfileUUID;
        MCObjectHeap_touch(heap);
    }
    RuntimeActor *a = p ? (RuntimeActor *)MCObjectHeap_alloc(heap, sizeof(*a), &actorClass) : NULL;
    if (!a)
        return false;
    a->actor = p;
    p->effects = (MCObject *)a;
    if (!WorldSettingsGameType_configurePlayerCapabilities(creative?&WorldSettingsGameType_CREATIVE:&WorldSettingsGameType_SURVIVAL,p->capabilities)) return false;
    a->managerCreative = creative;
    if (!preserveSourceID) p->entityId = entityId;
    p->posX = x;
    p->posY = y;
    p->posZ = z;
    p->dimension = w->dimension;
    if (!MCGameplay_setPlayer(game, index, uuid, (MCObject *)p) || !MCGameplayPackets_bind(p) ||
        !NetHandlerPlayServer_nativeNew(p, (MCObject *)w, &handlerDependencies))
        return false;
    return true;
}
bool mc_server_graph_add_player(MCGameplay *game,size_t index,const char *uuid,const char *name,
    int32_t id,double x,double y,double z,bool creative) {
    return add_player(game,index,uuid,name,id,x,y,z,creative,false);
}
bool mc_server_graph_add_player_auto(MCGameplay *game,size_t index,const char *uuid,const char *name,
    double x,double y,double z,bool creative) {
    return add_player(game,index,uuid,name,0,x,y,z,creative,true);
}
bool mc_server_graph_drop(MCGameplayPlayer *p, bool all) {
    EntityPlayer_dropOneItem(p, all, &dropDependencies, (MCObject *)p->worldObj);
    return !MCObjectHeap_failed(p->object.heap);
}
static StatBase *use_stat(MCObject *ctx, const Item *item) {
    (void)item;
    MCGameplayWorld *w = (MCGameplayWorld *)ctx;
    return w->emptyMapUseStat;
}
static bool trigger(MCObject *ctx, MCGameplayPlayer *p, StatBase *stat) {
    (void)ctx;
    return add_stat(p, stat, 1);
}
static bool empty_drop(MCObject *ctx, MCGameplayPlayer *p, ItemStack *stack, bool unused,
                       EntityItem **out) {
    *out = drop_choice(ctx, p, stack, unused);
    return !MCObjectHeap_failed(p->object.heap);
}
static bool item_right_click(MCObject *ctx, const Item *item, ItemStack *stack, MCObject *world,
                             MCObject *player, ItemStack **out) {
    (void)ctx;
    MCGameplayPlayer *p = (MCGameplayPlayer *)player;
    if (item == ItemStack_registryItem(395)) {
        static const ItemEmptyMapDependencies d = {use_stat, trigger, empty_drop};
        *out = ItemEmptyMap_onItemRightClick(item, stack, (MCGameplayWorld *)world, p, &d, world);
        return !MCObjectHeap_failed(stack->object.heap);
    }
    if (item == ItemStack_registryItem(358)) {
        *out = stack;
        return true;
    }
    return fail(stack->object.heap);
}
static bool manager_spectator(MCObject *ctx) {
    return ((RuntimeActor *)ctx)->managerSpectator;
}
static bool manager_creative(MCObject *ctx) {
    return ((RuntimeActor *)ctx)->managerCreative;
}
static int32_t item_duration(MCObject *ctx, ItemStack *stack) {
    (void)ctx;
    if (stack->item == ItemStack_registryItem(395) || stack->item == ItemStack_registryItem(358))
        return 0;
    fail(stack->object.heap);
    return 0;
}
static bool using_item(MCObject *ctx, MCGameplayPlayer *p) {
    (void)p;
    return ((RuntimeActor *)ctx)->usingItem;
}
static bool send_container(MCObject *ctx, MCGameplayPlayer *p, Container *c) {
    (void)ctx;
    return EntityPlayerMPWindows_sendContainerToPlayer(p, c);
}
bool mc_server_graph_use_item(MCGameplayPlayer *p) {
    ItemStack *stack = InventoryPlayer_getCurrentItem(p->inventory);
    if (!stack)
        return true;
    /* Only the map Item paths are translated in this runtime slice. Block,
       food/combat and other subclass use remain explicit native boundaries. */
    if (stack->item != ItemStack_registryItem(395) && stack->item != ItemStack_registryItem(358))
        return true;
    static const ItemStackUseDependencies use = {item_right_click};
    static const ItemInWorldManagerUseDependencies d = {
        manager_spectator, manager_creative, &use, item_duration, using_item, send_container};
    bool result = false;
    RuntimeActor *a = actor_bindings(p);
    return a && ItemInWorldManager_tryUseItem(p, p->worldObj, stack, &d, (MCObject *)a, &result);
}
bool mc_server_graph_close(MCGameplayPlayer *p, bool sendClose) {
    return sendClose ? EntityPlayerMPWindows_closeScreen(p)
                     : EntityPlayerMPWindows_closeContainer(p);
}
bool mc_server_graph_open_workbench(MCGameplayPlayer *p, int32_t x, int32_t y, int32_t z) {
    if (p->openContainer != &p->inventoryContainer->container && !mc_server_graph_close(p, true))
        return false;
    RuntimeActor *a = actor_bindings(p);
    if (!a)
        return false;
    a->currentWindowId = a->currentWindowId % 100 + 1;
    mc_buf open;
    start(&open, 0x2d);
    mc_put_u8(&open, (uint8_t)a->currentWindowId);
    mc_put_string(&open, "minecraft:crafting_table");
    mc_put_string(&open, "{\"translate\":\"tile.workbench.name\"}");
    mc_put_u8(&open, 0);
    bool ok = MCGameplayPackets_sendNative(p, &open);
    mc_buf_free(&open);
    mc_crafting_position pos = {x, y, z};
    ContainerWorkbench *bench =
        ok ? ContainerWorkbench_new(p->inventory, (MCObject *)p->worldObj, &pos, &craftingDispatch)
           : NULL;
    if (!bench)
        return false;
    p->openContainer = &bench->container;
    bench->container.windowId = a->currentWindowId;
    MCObjectHeap_touch(p->object.heap);
    return Container_onCraftGuiOpened(&bench->container, EntityPlayerMPWindows_listener(p));
}
bool mc_server_graph_detect_changes(MCGameplayObjects *o) {
    for (size_t i = 0; i < MC_TRANSFER_MAX_PLAYERS; i++) {
        MCGameplayPlayer *p = (MCGameplayPlayer *)o->players[i];
        if (p && !p->isDead && !Container_detectAndSendChanges(p->openContainer))
            return false;
    }
    return true;
}
bool mc_server_graph_preflight_inventory(MCGameplayObjects *o) {
    const size_t budget = 2097152u - 8192u;
    for (size_t i = 0; i < MC_TRANSFER_MAX_PLAYERS; i++) {
        MCGameplayPlayer *p = (MCGameplayPlayer *)o->players[i];
        if (!p || p->isDead)
            continue;
        mc_buf packet;
        start(&packet, 0x30);
        int32_t count = ContainerList_size(p->openContainer->inventorySlots);
        mc_put_u8(&packet, (uint8_t)p->openContainer->windowId);
        mc_put_i16(&packet, (int16_t)count);
        PacketBuffer buffer;
        bool ok = PacketBuffer_init(&buffer, o->object.heap, &packet);
        for (int32_t n = 0; n < count && ok; n++) {
            Slot *slot = Container_getSlot(p->openContainer, n);
            ok = slot && PacketBuffer_writeItemStackToBuffer(&buffer, Slot_getStack(slot));
        }
        /* Cursor's own S2F framing also belongs to the native aggregate cap. */
        mc_put_varint(&packet, 0x2f);
        mc_put_u8(&packet, 255);
        mc_put_i16(&packet, -1);
        if (ok)
            ok = PacketBuffer_writeItemStackToBuffer(&buffer,
                                                     InventoryPlayer_getItemStack(p->inventory));
        ok = ok && !packet.failed && packet.len <= budget;
        mc_buf_free(&packet);
        if (!ok)
            return fail(o->object.heap);
    }
    return !MCObjectHeap_failed(o->object.heap);
}
EntityItem *mc_server_graph_find_item(MCGameplayObjects *o, int32_t id) {
    for (size_t i = 0; i < o->itemCount; i++)
        if (((EntityItem *)o->items[i])->entity.entityId == id)
            return (EntityItem *)o->items[i];
    return NULL;
}
bool mc_server_graph_remove_dead(MCGameplayObjects *o) {
    for (size_t i = 0; i < o->itemCount;) {
        EntityItem *e = (EntityItem *)o->items[i];
        if (!e->entity.isDead) {
            i++;
            continue;
        }
        for (size_t n = i + 1; n < o->itemCount; n++)
            o->items[n - 1] = o->items[n];
        o->items[--o->itemCount] = NULL;
        MCObjectHeap_touch(o->object.heap);
    }
    return !MCObjectHeap_failed(o->object.heap);
}
