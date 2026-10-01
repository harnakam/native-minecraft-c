#include "entity/SharedMonsterAttributes.h"
#include "network/play/client/C0BPacketEntityAction.h"
#include "server/native_gameplay.h"
#include "util/MCGameplayPlayer.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "entity actions check %u at %d: %s\n", checks, __LINE__, #x);          \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static void constructors(void) {
    MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
    CHECK(heap);
    MCGameplayPlayer *player = MCGameplayPlayer_nativeAllocate(heap);
    CHECK(player);
    player->living.entity.entityId = INT32_MIN;
    const C0BPacketEntityActionAction *action = C0BPacketEntityAction_nativeAction(C0B_RIDING_JUMP);
    C0BPacketEntityAction *p =
        C0BPacketEntityAction_new_aux(heap, &player->living.entity, action, -1);
    CHECK(p);
    CHECK(p->entityID == INT32_MIN && p->action == action && p->auxData == -1);
    p = C0BPacketEntityAction_new(heap, &player->living.entity, NULL);
    CHECK(p && p->entityID == INT32_MIN && !p->action && !p->auxData);
    p = C0BPacketEntityAction_new_empty(heap);
    CHECK(p && !p->entityID && !p->action && !p->auxData);
    MCObjectHeap_free(heap);
}
static void codec(void) {
    /* A mutation omitting signed VarInt aux data or normalizing entity ID
       breaks this literal independently specified packet. */
    static const uint8_t wire[] = {0x80, 0x80, 0x80, 0x80, 0x08, 0x05,
                                   0xff, 0xff, 0xff, 0xff, 0x0f};
    for (size_t length = 0; length <= sizeof(wire); length++) {
        MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
        CHECK(heap);
        C0BPacketEntityAction *p = C0BPacketEntityAction_new_empty(heap);
        CHECK(p);
        p->entityID = 123;
        p->action = C0BPacketEntityAction_nativeAction(1);
        p->auxData = 99;
        mc_buf bytes;
        mc_buf_init(&bytes);
        mc_put_bytes(&bytes, wire, length);
        PacketBuffer buffer;
        CHECK(PacketBuffer_init(&buffer, heap, &bytes));
        CHECK(C0BPacketEntityAction_readPacketData(p, &buffer) == (length == sizeof(wire)));
        CHECK(p->entityID == (length >= 5 ? INT32_MIN : 123));
        CHECK(p->action == C0BPacketEntityAction_nativeAction(length >= 6 ? 5 : 1));
        CHECK(p->auxData == (length == sizeof(wire) ? -1 : 99));
        CHECK(!MCObjectHeap_hasBorrowers(heap));
        mc_buf_free(&bytes);
        MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
    CHECK(heap);
    C0BPacketEntityAction *p = C0BPacketEntityAction_new_empty(heap);
    CHECK(p);
    p->entityID = INT32_MIN;
    p->action = C0BPacketEntityAction_nativeAction(5);
    p->auxData = -1;
    mc_buf bytes;
    mc_buf_init(&bytes);
    PacketBuffer b;
    CHECK(PacketBuffer_init(&b, heap, &bytes));
    CHECK(C0BPacketEntityAction_writePacketData(p, &b));
    CHECK(bytes.len == sizeof(wire) && !memcmp(bytes.data, wire, sizeof(wire)));
    mc_buf_free(&bytes);
    MCObjectHeap_free(heap);
    for (int32_t ordinal = -1; ordinal <= 7; ordinal += 8) {
        heap = MCObjectHeap_new(1024 * 1024);
        CHECK(heap);
        p = C0BPacketEntityAction_new_empty(heap);
        CHECK(p);
        p->action = C0BPacketEntityAction_nativeAction(1);
        p->auxData = 17;
        mc_buf_init(&bytes);
        mc_put_u8(&bytes, 42);
        mc_put_varint(&bytes, ordinal);
        mc_put_u8(&bytes, 3);
        CHECK(PacketBuffer_init(&b, heap, &bytes));
        CHECK(!C0BPacketEntityAction_readPacketData(p, &b));
        CHECK(p->entityID == 42 && p->action == C0BPacketEntityAction_nativeAction(1) &&
              p->auxData == 17);
        mc_buf_free(&bytes);
        MCObjectHeap_free(heap);
    }
    heap = MCObjectHeap_new(1024 * 1024);
    CHECK(heap);
    p = C0BPacketEntityAction_new_empty(heap);
    CHECK(p);
    p->entityID = 128;
    mc_buf_init(&bytes);
    CHECK(PacketBuffer_init(&b, heap, &bytes));
    CHECK(!C0BPacketEntityAction_writePacketData(p, &b));
    CHECK(bytes.len == 2 && bytes.data[0] == 0x80 && bytes.data[1] == 1 &&
          !MCObjectHeap_hasBorrowers(heap));
    mc_buf_free(&bytes);
    MCObjectHeap_free(heap);
}
static void handler_sneaking(void) {
    MCGameplay base = {0};
    mc_world terrain;
    mc_world_init(&terrain, 919);
    /* Real loaded air chunks close the SourceMP top-solid/collision dependency. */
    for(int cz=-1;cz<=1;cz++)for(int cx=-1;cx<=1;cx++)CHECK(mc_world_chunk(&terrain,cx,cz,true));
    CHECK(mc_server_graph_init(&base, &terrain, 0, 0, 1));
    MCGameplayTransaction tx = {0};
    CHECK(MCGameplay_begin(&base, &tx));
    MCGameplay *game = &tx.working;
    MCObjectRootScope scope = {0};
    CHECK(MCObjectRootScope_begin(&scope, game->heap));
    CHECK(mc_server_graph_add_player_auto(game, 0, "00000000-0000-0000-0000-000000000001", "Alice",
                                          0, 80, 0, true));
    MCGameplayPlayer *p = mc_server_graph_player(game, 0);
    CHECK(p);
    NetHandlerPlayServer *h = (NetHandlerPlayServer *)MCGameplayPlayer_handler(p);
    CHECK(h && h->hasMoved);
    C0BPacketEntityAction *packet = C0BPacketEntityAction_new(
        game->heap, &p->living.entity, C0BPacketEntityAction_nativeAction(0));
    CHECK(packet);
    CHECK(NetHandlerPlayServer_processEntityAction(h, packet));
    CHECK(Entity_isSneaking(&p->living.entity));
    MCObjectRootScope_end(&scope);
    CHECK(MCGameplay_abort(&tx));
    CHECK(MCGameplay_free(&base));
    mc_world_free(&terrain);
}
typedef struct {
    MCObject object;
    NetHandlerPlayServer *handler;
    MCGameplayPlayer *first, *second, *lastPlayer;
    C0BPacketEntityAction *packet;
    MCObject *lastHorse;
    char calls[64], failure;
    size_t used;
    MCPacketThreadResult thread;
    bool swapActive, horse, wakeImmediately, wakeWorld, wakeSpawn;
    int32_t replaceAction, jump;
} Effects;
static void effect_trace(MCObject *o, MCObjectVisitor v, void *c) {
    Effects *e = (Effects *)o;
    e->handler = (NetHandlerPlayServer *)v((MCObject *)e->handler, c);
    e->first = (MCGameplayPlayer *)v((MCObject *)e->first, c);
    e->second = (MCGameplayPlayer *)v((MCObject *)e->second, c);
    e->lastPlayer = (MCGameplayPlayer *)v((MCObject *)e->lastPlayer, c);
    e->packet = (C0BPacketEntityAction *)v((MCObject *)e->packet, c);
    e->lastHorse = v(e->lastHorse, c);
}
static const MCObjectClass effectsClass = {"fixture.EntityActions", MCObjectHeap_plainClone,
                                           effect_trace, NULL};
static bool event(Effects *e, char call) {
    CHECK(e->used + 1 < sizeof(e->calls));
    e->calls[e->used++] = call;
    e->calls[e->used] = 0;
    CHECK(!MCObjectHeap_collect(e->object.heap));
    CHECK(!MCObjectHeap_failed(e->object.heap));
    MCObjectHeap_touch(e->object.heap);
    return e->failure != call;
}
static MCPacketThreadResult thread_action(MCObject *o, NetHandlerPlayServer *h, MCObject *packet) {
    Effects *e = (Effects *)o;
    CHECK(h == e->handler && packet == (MCObject *)e->packet);
    return event(e, 'T') ? e->thread : MC_PACKET_THREAD_FAILED;
}
static bool active_action(MCObject *o, MCGameplayPlayer *p) {
    Effects *e = (Effects *)o;
    CHECK(p == e->first);
    bool ok = event(e, 'A');
    e->lastPlayer = p;
    if (e->swapActive)
        e->handler->playerEntity = e->second;
    if (e->replaceAction >= 0)
        e->packet->action = C0BPacketEntityAction_nativeAction(e->replaceAction);
    MCObjectHeap_touch(o->heap);
    return ok;
}
static bool wake_action(MCObject *o, MCGameplayPlayer *p, bool immediate, bool world, bool spawn) {
    Effects *e = (Effects *)o;
    bool ok = event(e, 'W');
    e->lastPlayer = p;
    e->wakeImmediately = immediate;
    e->wakeWorld = world;
    e->wakeSpawn = spawn;
    p->sleeping = false;
    MCObjectHeap_touch(o->heap);
    return ok;
}
static bool horse_action(MCObject *o, MCObject *entity, bool *out) {
    Effects *e = (Effects *)o;
    bool ok = event(e, 'H');
    e->lastHorse = entity;
    *out = e->horse;
    return ok;
}
static bool jump_action(MCObject *o, MCObject *entity, int32_t power) {
    Effects *e = (Effects *)o;
    bool ok = event(e, 'J');
    e->lastHorse = entity;
    e->jump = power;
    return ok;
}
static bool gui_action(MCObject *o, MCObject *entity, MCGameplayPlayer *p) {
    Effects *e = (Effects *)o;
    bool ok = event(e, 'O');
    e->lastHorse = entity;
    e->lastPlayer = p;
    return ok;
}
static const NetHandlerPlayServerEntityActionDependencies actionDeps = {wake_action, horse_action,
                                                                        jump_action, gui_action};
typedef struct {
    MCGameplay base;
    MCGameplayTransaction tx;
    mc_world terrain;
    MCObjectRootScope scope;
    NetHandlerPlayServerDependencies deps;
    MCGameplayPlayer *p, *q;
    NetHandlerPlayServer *h;
    Effects *effects;
} Fixture;
static void fixture(Fixture *f, int32_t action) {
    memset(f, 0, sizeof(*f));
    mc_world_init(&f->terrain, 919);
    for(int cz=-1;cz<=1;cz++)for(int cx=-1;cx<=1;cx++)CHECK(mc_world_chunk(&f->terrain,cx,cz,true));
    CHECK(mc_server_graph_init(&f->base, &f->terrain, 0, 0, 1));
    CHECK(MCGameplay_begin(&f->base, &f->tx));
    MCGameplay *game = &f->tx.working;
    CHECK(MCObjectRootScope_begin(&f->scope, game->heap));
    CHECK(mc_server_graph_add_player_auto(game, 0, "00000000-0000-0000-0000-000000000001", "Alice",
                                          0, 80, 0, true));
    CHECK(mc_server_graph_add_player_auto(game, 1, "00000000-0000-0000-0000-000000000002", "Bob", 0,
                                          80, 0, true));
    f->p = mc_server_graph_player(game, 0);
    f->q = mc_server_graph_player(game, 1);
    f->h = (NetHandlerPlayServer *)MCGameplayPlayer_handler(f->p);
    f->effects = (Effects *)MCObjectHeap_alloc(game->heap, sizeof(Effects), &effectsClass);
    CHECK(f->effects);
    Effects *e = f->effects;
    e->handler = f->h;
    e->first = f->p;
    e->second = f->q;
    e->replaceAction = -1;
    e->thread = MC_PACKET_THREAD_EXECUTE;
    e->packet = C0BPacketEntityAction_new_aux(
        game->heap, &f->p->living.entity, C0BPacketEntityAction_nativeAction(action), INT32_MIN);
    CHECK(e->packet);
    f->deps = *f->h->dependencies;
    f->deps.checkThreadAndEnqueue = thread_action;
    f->deps.markPlayerActive = active_action;
    f->h->dependencies = &f->deps;
    f->h->dependencyContext = (MCObject *)e;
    CHECK(NetHandlerPlayServer_nativeBindEntityActions(f->h, &actionDeps, (MCObject *)e));
    MCObjectHeap_touch(game->heap);
}
static void cleanup(Fixture *f) {
    MCObjectRootScope_end(&f->scope);
    CHECK(!MCObjectHeap_hasBorrowers(f->tx.working.heap));
    CHECK(MCGameplay_abort(&f->tx));
    CHECK(MCGameplay_free(&f->base));
    mc_world_free(&f->terrain);
}
static void handler_order_and_effects(void) {
    /* Caching player before markActive, caching action before that callback,
       silently skipping required wake/horse methods, or assigning hasMoved
       before a failed wake all break independent observable state below. */
    Fixture f;
    fixture(&f, 0);
    f.effects->swapActive = true;
    CHECK(C0BPacketEntityAction_processPacket(f.effects->packet,
                                              NetHandlerPlayServer_asHandler(f.h)));
    CHECK(!strcmp(f.effects->calls, "TA"));
    CHECK(!Entity_isSneaking(&f.p->living.entity) && Entity_isSneaking(&f.q->living.entity));
    cleanup(&f);
    fixture(&f, 0);
    f.effects->replaceAction = 1;
    CHECK(Entity_setSneaking(&f.p->living.entity, true));
    CHECK(NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet));
    CHECK(!Entity_isSneaking(&f.p->living.entity));
    cleanup(&f);
    fixture(&f, 2);
    f.p->sleeping = true;
    CHECK(f.h->hasMoved);
    CHECK(NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet));
    CHECK(!strcmp(f.effects->calls, "TAW"));
    CHECK(!f.p->sleeping && !f.h->hasMoved && !f.effects->wakeImmediately && f.effects->wakeWorld &&
          f.effects->wakeSpawn);
    cleanup(&f);
    fixture(&f, 2);
    f.p->sleeping = true;
    f.effects->failure = 'W';
    CHECK(!NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet));
    CHECK(!f.p->sleeping && f.h->hasMoved && MCObjectHeap_failed(f.tx.working.heap));
    cleanup(&f);
    fixture(&f, 2);
    CHECK(NetHandlerPlayServer_nativeBindEntityActions(f.h, NULL, NULL));
    CHECK(!NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet));
    CHECK(!strcmp(f.effects->calls, "TA") && f.h->hasMoved);
    cleanup(&f);
    for (int32_t action = 5; action <= 6; action++) {
        fixture(&f, action);
        CHECK(NetHandlerPlayServer_nativeBindEntityActions(f.h, NULL, NULL));
        CHECK(NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet));
        CHECK(!strcmp(f.effects->calls, "TA"));
        cleanup(&f);
        fixture(&f, action);
        f.p->living.entity.ridingEntity = &f.q->living.entity;
        CHECK(NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet));
        CHECK(!strcmp(f.effects->calls, "TAH"));
        cleanup(&f);
        fixture(&f, action);
        f.p->living.entity.ridingEntity = &f.q->living.entity;
        f.effects->horse = true;
        CHECK(NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet));
        CHECK(!strcmp(f.effects->calls, action == 5 ? "TAHJ" : "TAHO"));
        CHECK(f.effects->lastHorse == (MCObject *)f.q);
        if (action == 5)
            CHECK(f.effects->jump == INT32_MIN);
        else
            CHECK(f.effects->lastPlayer == f.p);
        cleanup(&f);
        fixture(&f, action);
        f.p->living.entity.ridingEntity = &f.q->living.entity;
        CHECK(NetHandlerPlayServer_nativeBindEntityActions(f.h, NULL, NULL));
        CHECK(!NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet));
        CHECK(!strcmp(f.effects->calls, "TA"));
        cleanup(&f);
    }
    for (int queued = 0; queued < 2; queued++) {
        fixture(&f, 0);
        f.effects->thread = queued ? MC_PACKET_THREAD_FAILED : MC_PACKET_THREAD_QUEUED;
        CHECK(NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet) == !queued);
        CHECK(!strcmp(f.effects->calls, "T"));
        CHECK(!Entity_isSneaking(&f.p->living.entity));
        cleanup(&f);
    }
    fixture(&f, 0);
    f.effects->failure = 'A';
    CHECK(!NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet));
    CHECK(!strcmp(f.effects->calls, "TA"));
    CHECK(!Entity_isSneaking(&f.p->living.entity));
    cleanup(&f);
    for (int invalid = 0; invalid < 2; invalid++) {
        C0BPacketEntityActionAction foreign = {0};
        fixture(&f, 0);
        f.effects->packet->action = invalid ? &foreign : NULL;
        CHECK(!NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet));
        CHECK(!strcmp(f.effects->calls, "TA"));
        cleanup(&f);
    }
    for (char failure = 'H'; failure <= 'O'; failure += (failure == 'H' ? 2 : 5)) {
        fixture(&f, failure == 'O' ? 6 : 5);
        f.p->living.entity.ridingEntity = &f.q->living.entity;
        f.effects->horse = true;
        f.effects->failure = failure;
        CHECK(!NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet));
        CHECK(!strcmp(f.effects->calls, failure == 'H' ? "TAH" : failure == 'J' ? "TAHJ" : "TAHO"));
        if (failure == 'J')
            CHECK(f.effects->jump == INT32_MIN);
        cleanup(&f);
    }
    fixture(&f, 3);
    const SharedMonsterAttributes *shared = SharedMonsterAttributes_get(f.tx.working.heap);
    CHECK(shared);
    IAttributeInstance *speed =
        EntityLivingBase_getEntityAttribute(&f.p->living, shared->movementSpeed);
    CHECK(speed);
    double base = IAttributeInstance_getAttributeValue(speed);
    CHECK(NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet));
    CHECK(Entity_isSprinting(&f.p->living.entity));
    CHECK(IAttributeInstance_getAttributeValue(speed) > base);
    f.effects->used = 0;
    f.effects->calls[0] = 0;
    f.effects->packet->action = C0BPacketEntityAction_nativeAction(4);
    CHECK(NetHandlerPlayServer_processEntityAction(f.h, f.effects->packet));
    CHECK(!Entity_isSprinting(&f.p->living.entity));
    CHECK(IAttributeInstance_getAttributeValue(speed) == base);
    cleanup(&f);
}
static void invalid_native_edges(void) {
    Fixture small;
    fixture(&small, 0);
    NetHandlerPlayServer *shortHandler = (NetHandlerPlayServer *)MCObjectHeap_alloc(
        small.tx.working.heap, sizeof(MCObject), small.h->object.klass);
    CHECK(shortHandler);
    CHECK(!NetHandlerPlayServer_nativeBindEntityActions(shortHandler, &actionDeps, NULL));
    cleanup(&small);
    Fixture f;
    fixture(&f, 0);
    MCObjectHeap *foreign = MCObjectHeap_new(1024 * 1024);
    CHECK(foreign);
    MCObject *context = MCObjectHeap_alloc(foreign, sizeof(MCObject), &effectsClass);
    CHECK(context);
    CHECK(!NetHandlerPlayServer_nativeBindEntityActions(f.h, &actionDeps, context));
    CHECK(f.h->entityActionContext == (MCObject *)f.effects &&
          MCObjectHeap_failed(f.tx.working.heap));
    cleanup(&f);
    MCObjectHeap_free(foreign);
    fixture(&f, 0);
    foreign = MCObjectHeap_new(1024 * 1024);
    CHECK(foreign);
    C0BPacketEntityAction *packet = C0BPacketEntityAction_new_empty(foreign);
    CHECK(packet);
    CHECK(!NetHandlerPlayServer_processEntityAction(f.h, packet));
    CHECK(!f.effects->used);
    cleanup(&f);
    MCObjectHeap_free(foreign);
    MCObjectHeap *heap = MCObjectHeap_new(1024 * 1024);
    CHECK(heap);
    packet = C0BPacketEntityAction_new_empty(heap);
    CHECK(packet);
    C0BPacketEntityAction *undersized =
        (C0BPacketEntityAction *)MCObjectHeap_alloc(heap, sizeof(MCObject), packet->object.klass);
    CHECK(undersized);
    mc_buf bytes;
    mc_buf_init(&bytes);
    PacketBuffer b;
    CHECK(PacketBuffer_init(&b, heap, &bytes));
    CHECK(!C0BPacketEntityAction_readPacketData(undersized, &b));
    CHECK(MCObjectHeap_failed(heap) && !MCObjectHeap_hasBorrowers(heap));
    mc_buf_free(&bytes);
    MCObjectHeap_free(heap);
}
static void lifetime(void) {
    Fixture f;
    fixture(&f, 0);
    MCObjectRoot handler = {0};
    CHECK(MCObjectRoot_init(&handler, f.tx.working.heap, (MCObject *)f.h));
    f.effects->thread = MC_PACKET_THREAD_QUEUED;
    CHECK(C0BPacketEntityAction_processPacket(f.effects->packet,
                                            NetHandlerPlayServer_asHandler(f.h)));
    CHECK(!Entity_isSneaking(&f.p->living.entity));
    MCObjectRootScope_end(&f.scope);
    CHECK(MCObjectHeap_collect(f.tx.working.heap));
    MCObjectHeap *clone = MCObjectHeap_clone(f.tx.working.heap);
    CHECK(clone);
    MCObjectRoot copy = {0};
    CHECK(MCObjectRoot_rebind(&copy, clone, &handler));
    NetHandlerPlayServer *h = (NetHandlerPlayServer *)MCObjectRoot_get(&copy);
    CHECK(h);
    Effects *e = (Effects *)h->entityActionContext;
    CHECK(e && (MCObject *)e == h->dependencyContext && e->handler == h &&
          h->playerEntity == e->first);
    CHECK(h->entityActionDependencies == &actionDeps && h->hasMoved);
    CHECK(!strcmp(e->calls, "T") && e->thread == MC_PACKET_THREAD_QUEUED);
    MCObjectRootScope resumed = {0};
    CHECK(MCObjectRootScope_begin(&resumed, clone));
    e->thread = MC_PACKET_THREAD_EXECUTE;
    e->used = 0;
    e->calls[0] = 0;
    MCObjectHeap_touch(clone);
    MCObjectRootScope_end(&resumed);
    CHECK(C0BPacketEntityAction_processPacket(e->packet, NetHandlerPlayServer_asHandler(h)));
    CHECK(Entity_isSneaking(&h->playerEntity->living.entity));
    CHECK(!Entity_isSneaking(&f.p->living.entity));
    CHECK(MCObjectHeap_adopt(f.tx.working.heap, clone));
    MCObjectHeap_free(clone);
    f.h = (NetHandlerPlayServer *)MCObjectRoot_get(&handler);
    CHECK(f.h && Entity_isSneaking(&f.h->playerEntity->living.entity));
    CHECK(MCObjectHeap_collect(f.tx.working.heap));
    cleanup(&f);
}
int main(void) {
    constructors();
    codec();
    handler_sneaking();
    handler_order_and_effects();
    invalid_native_edges();
    lifetime();
    printf("Source entity actions: %u checks passed\n", checks);
    return 0;
}
