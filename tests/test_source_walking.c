#include "entity/DataWatcher.h"
#include "client/entity/EntityPlayerSP.h"
#include "network/play/client/C03PacketPlayer.h"
#include "network/play/client/C0BPacketEntityAction.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      fprintf(stderr, "walking check %u line %d: %s\n", checks, __LINE__, #x); \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
enum { SPRINT = 1, SNEAK, VIEW, BOX, QUEUE };
typedef struct {
  MCObject object;
  MCGameplayPlayer *actor, *otherActor;
  MCGameplayWorld *world;
  EntityPlayerSP *sp;
  NetHandlerPlayClient *queue, *otherQueue;
  MCObject *packets[16];
  NetHandlerPlayClient *receivers[16];
  unsigned events[64], eventCount, packetCount, boxCount, failAt, failBox;
  bool sprint, sneak, view, useSourceSneak, mutateArgs, mutateSent, mutatePost,
      mutateActions, replaceFixtureActor, allowCollect, flipSprintState;
} Fixture;
static void trace(MCObject *o, MCObjectVisitor visit, void *context) {
  Fixture *f = (Fixture *)o;
  f->actor = (MCGameplayPlayer *)visit((MCObject *)f->actor, context);
  f->otherActor = (MCGameplayPlayer *)visit((MCObject *)f->otherActor, context);
  f->world = (MCGameplayWorld *)visit((MCObject *)f->world, context);
  f->sp = (EntityPlayerSP *)visit((MCObject *)f->sp, context);
  f->queue = (NetHandlerPlayClient *)visit((MCObject *)f->queue, context);
  f->otherQueue =
      (NetHandlerPlayClient *)visit((MCObject *)f->otherQueue, context);
  for (unsigned i = 0; i < f->packetCount; i++) {
    f->packets[i] = visit(f->packets[i], context);
    f->receivers[i] =
        (NetHandlerPlayClient *)visit((MCObject *)f->receivers[i], context);
  }
}
static const MCObjectClass fixtureClass = {
    "fixture.SourceWalking", MCObjectHeap_plainClone, trace, NULL};
static bool event(Fixture *f, unsigned id) {
  CHECK(f->eventCount < 64);
  f->events[f->eventCount++] = id;
  if (f->allowCollect)
    CHECK(!MCObjectHeap_collect(f->object.heap));
  return f->failAt == 0 || f->eventCount != f->failAt;
}
static bool sprint(MCObject *context, EntityPlayerSP *sp, bool *out) {
  Fixture *f = (Fixture *)context;
  CHECK(sp == f->sp);
  if (!event(f, SPRINT))
    return false;
  *out = f->sprint;
  if (f->flipSprintState)
    sp->serverSprintState = *out;
  return true;
}
static bool sneak(MCObject *context, EntityPlayerSP *sp, bool *out) {
  Fixture *f = (Fixture *)context;
  CHECK(sp == f->sp);
  if (!event(f, SNEAK))
    return false;
  *out = f->useSourceSneak ? EntityPlayerSP_isSneaking(sp) : f->sneak;
  return !MCObjectHeap_failed(context->heap);
}
static bool view(MCObject *context, EntityPlayerSP *sp, bool *out) {
  Fixture *f = (Fixture *)context;
  CHECK(sp == f->sp);
  if (!event(f, VIEW))
    return false;
  *out = f->view;
  return true;
}
static AxisAlignedBB *box(MCObject *context, EntityPlayerSP *sp) {
  Fixture *f = (Fixture *)context;
  CHECK(sp == f->sp);
  ++f->boxCount;
  if (!event(f, BOX)) {
    MCObjectHeap_fail(context->heap);
    return NULL;
  }
  if (f->failBox == f->boxCount)
    return NULL;
  Entity *e = &f->actor->living.entity;
  if (f->mutateArgs && f->boxCount == 2) {
    sp->sendQueue = f->otherQueue;
    e->posX = 17;
    e->posZ = 19;
    e->rotationYaw = 23;
    e->rotationPitch = 29;
    e->onGround = true;
    e->boundingBox = AxisAlignedBB_new(context->heap, 0, 13, 0, 1, 14, 1);
    CHECK(e->boundingBox);
  }
  if (f->mutatePost && f->boxCount == 3) {
    e->posX = 59;
    e->posZ = 61;
    e->rotationYaw = 67;
    e->rotationPitch = 71;
    e->boundingBox = AxisAlignedBB_new(context->heap, 0, 53, 0, 1, 54, 1);
    CHECK(e->boundingBox);
  }
  if (f->replaceFixtureActor)
    f->actor = f->otherActor;
  MCObjectHeap_touch(context->heap);
  return e->boundingBox;
}
static const EntityPlayerSPWalkingDependencies walking = {sprint, sneak, view,
                                                          box};
static bool queue(MCObject *context, NetHandlerPlayClient *receiver,
                  MCObject *packet) {
  Fixture *f = (Fixture *)context;
  CHECK(packet && packet->heap == context->heap);
  CHECK(receiver == f->queue || receiver == f->otherQueue);
  CHECK(f->packetCount < 16);
  f->packets[f->packetCount] = packet;
  f->receivers[f->packetCount++] = receiver;
  if (!event(f, QUEUE))
    return false;
  Entity *e = &f->actor->living.entity;
  if (f->mutateSent && C03PacketPlayer_isInstance(packet)) {
    e->posX = 37;
    e->posZ = 43;
    e->rotationYaw = 47;
    e->rotationPitch = 49;
    e->boundingBox = AxisAlignedBB_new(context->heap, 0, 41, 0, 1, 42, 1);
    CHECK(e->boundingBox);
    f->sp->positionUpdateTicks = 100;
  }
  if (f->mutateActions && C0BPacketEntityAction_isInstance(packet)) {
    C0BPacketEntityAction *p = (C0BPacketEntityAction *)packet;
    if (p->action == C0BPacketEntityAction_nativeAction(C0B_START_SPRINTING)) {
      f->sp->movementInput->sneak = true;
      f->sp->serverSprintState = false;
    }
  }
  MCObjectHeap_touch(context->heap);
  return true;
}
static DataWatcherBlockPos *unused_origin(MCObject *context) {
  (void)context;
  CHECK(false);
  return NULL;
}
static bool unused_gui(MCObject *context, MCObject *mc) {
  (void)context;
  (void)mc;
  CHECK(false);
  return false;
}
static const EntityPlayerSPDependencies spDependencies = {queue, unused_origin,
                                                          unused_gui};
static MCPacketThreadResult unused_thread(MCObject *c, NetHandlerPlayClient *h,
                                          MCObject *p) {
  (void)c;
  (void)h;
  (void)p;
  CHECK(false);
  return MC_PACKET_THREAD_FAILED;
}
static MCGameplayPlayer *unused_player(MCObject *c, MCObject *mc) {
  (void)c;
  (void)mc;
  CHECK(false);
  return NULL;
}
static bool unused_screen(MCObject *c, MCObject *mc) {
  (void)c;
  (void)mc;
  CHECK(false);
  return false;
}
static int32_t unused_tab(MCObject *c, MCObject *mc) {
  (void)c;
  (void)mc;
  CHECK(false);
  return 0;
}
static int32_t unused_inventory_tab(MCObject *c) {
  (void)c;
  CHECK(false);
  return 0;
}
static bool unused_close(MCObject *c, MCGameplayPlayer *p) {
  (void)c;
  (void)p;
  CHECK(false);
  return false;
}
static bool unused_confirm(MCObject *c, NetHandlerPlayClient *h,
                           C0FPacketConfirmTransaction *p) {
  (void)c;
  (void)h;
  (void)p;
  CHECK(false);
  return false;
}
static const NetHandlerPlayClientDependencies handlerDependencies = {
    .checkThreadAndEnqueue = unused_thread,
    .getPlayer = unused_player,
    .isCreativeScreen = unused_screen,
    .selectedCreativeTabIndex = unused_tab,
    .inventoryCreativeTabIndex = unused_inventory_tab,
    .closeScreenAndDropStack = unused_close,
    .addToSendQueue = unused_confirm};
/* Like the unchanged-JAR walking observer, this fixture assigns method fields
   on a zeroed most-derived SP. It tests walking, not the full SP tick/constructor.
   The owned Player and SP references are views of the same managed object. */
static Fixture *setup(MCGameplay *g, MCObjectRootScope *scope) {
  CHECK(MCGameplay_init(g, 32 * 1024 * 1024));
  CHECK(MCObjectRootScope_begin(scope, g->heap));
  Fixture *f = (Fixture *)MCObjectHeap_alloc(g->heap, sizeof *f, &fixtureClass);
  CHECK(f);
  f->world = MCGameplayWorld_new(g->heap, MCGameplay_get(g), NULL, NULL);
  CHECK(f->world);
  f->world->isRemote = true;
  CHECK(MCGameplay_setWorld(g, (MCObject *)f->world));
  f->sp = EntityPlayerSP_nativeAllocate(g->heap);
  CHECK(f->sp);
  f->actor = EntityPlayerSP_asPlayer(f->sp);
  CHECK((MCObject *)f->actor == EntityPlayerSP_asObject(f->sp));
  f->otherActor = MCGameplayPlayer_nativeAllocate(g->heap);
  CHECK(f->otherActor);
  Entity *e = &f->actor->living.entity;
  e->worldObj = (MCObject *)f->world;
  e->entityId = 42;
  e->boundingBox = AxisAlignedBB_new(g->heap, 0, 0, 0, 1, 1, 1);
  CHECK(e->boundingBox);
  CHECK(MCGameplay_setPlayer(g, 0, "11111111-1111-1111-1111-111111111111",
                             (MCObject *)f->actor));
  f->actor->effects = (MCObject *)f;
  f->queue = NetHandlerPlayClient_nativeNew(
      f->actor, (MCObject *)f, (MCObject *)f, &handlerDependencies);
  CHECK(f->queue);
  f->otherQueue = NetHandlerPlayClient_nativeNew(
      f->actor, (MCObject *)f, (MCObject *)f, &handlerDependencies);
  CHECK(f->otherQueue);
  f->sp->sendQueue=f->queue;f->sp->mc=(MCObject *)f;
  CHECK(EntityPlayerSP_bindActions(f->sp,(MCObject *)f,&spDependencies));
  f->sp->movementInput = MovementInput_new(g->heap);
  CHECK(f->sp->movementInput);
  CHECK(EntityPlayerSP_bindWalking(f->sp, (MCObject *)f, &walking));
  f->view = true;
  return f;
}
static void finish(MCGameplay *g, MCObjectRootScope *scope, bool failed) {
  CHECK(MCObjectHeap_failed(g->heap) == failed);
  MCObjectRootScope_end(scope);
  CHECK(MCGameplay_free(g));
}
static unsigned kind(MCObject *packet) {
  if (C03PacketPlayer_nativeBaseIsInstance(packet))
    return 3;
  if (C04PacketPlayerPosition_isInstance(packet))
    return 4;
  if (C05PacketPlayerLook_isInstance(packet))
    return 5;
  if (C06PacketPlayerPosLook_isInstance(packet))
    return 6;
  if (C0BPacketEntityAction_isInstance(packet))
    return 11;
  CHECK(false);
  return 0;
}
static void packet_bytes(Fixture *f, C03PacketPlayer *p) {
  mc_buf body;
  mc_buf_init(&body);
  PacketBuffer buffer;
  CHECK(PacketBuffer_init(&buffer, f->object.heap, &body));
  CHECK(C03PacketPlayer_writePacketData(p, &buffer));
  size_t expected = kind((MCObject *)p) == 3   ? 1
                    : kind((MCObject *)p) == 4 ? 25
                    : kind((MCObject *)p) == 5 ? 9
                                               : 33;
  CHECK(body.len == expected);
  mc_buf_free(&body);
}
static void defaults_and_sneak(void) {
  MCObjectHeap *h = MCObjectHeap_new(1024 * 1024);
  CHECK(h);
  MovementInput *m = MovementInput_new(h);
  CHECK(m);
  CHECK(!m->jump && !m->sneak && m->moveStrafe == 0 && m->moveForward == 0);
  m->jump = true;
  m->sneak = true;
  m->moveStrafe = -3;
  m->moveForward = 7;
  CHECK(MovementInput_updatePlayerMoveState(m));
  CHECK(m->jump && m->sneak && m->moveStrafe == -3 && m->moveForward == 7);
  MCObjectRoot root = {0};
  CHECK(MCObjectRoot_init(&root, h, (MCObject *)m));
  MCObjectHeap *copy = MCObjectHeap_clone(h);
  CHECK(copy);
  MCObjectRoot rebound = {0};
  CHECK(MCObjectRoot_rebind(&rebound, copy, &root));
  MovementInput *other = (MovementInput *)MCObjectRoot_get(&rebound);
  CHECK(other != m && other->sneak && other->moveForward == 7);
  MCObjectRoot_drop(&rebound);
  MCObjectHeap_free(copy);
  MCObjectRoot_drop(&root);
  MCObjectHeap_free(h);
  MCGameplay g = {0};
  MCObjectRootScope scope = {0};
  Fixture *f = setup(&g, &scope);
  CHECK(f->sp->positionUpdateTicks == 0 && !f->sp->serverSprintState &&
        !f->sp->serverSneakState);
  CHECK(!EntityPlayerSP_isSneaking(f->sp) && !MCObjectHeap_failed(g.heap));
  f->sp->movementInput->sneak = true;
  CHECK(EntityPlayerSP_isSneaking(f->sp));
  f->actor->sleeping = true;
  CHECK(!EntityPlayerSP_isSneaking(f->sp) && !MCObjectHeap_failed(g.heap));
  f->sp->movementInput = NULL;
  CHECK(!EntityPlayerSP_isSneaking(f->sp) && !MCObjectHeap_failed(g.heap));
  f->actor->sleeping = false;
  CHECK(!EntityPlayerSP_isSneaking(f->sp) && !MCObjectHeap_failed(g.heap));
  finish(&g, &scope, false);
}
static void branches_and_actions(void) {
  for (unsigned branch = 0; branch < 5; branch++) {
    MCGameplay g = {0};
    MCObjectRootScope scope = {0};
    Fixture *f = setup(&g, &scope);
    Entity *e = &f->actor->living.entity;
    if (branch == 1 || branch == 3)
      e->posX = 1;
    if (branch == 2 || branch == 3)
      e->rotationYaw = 15;
    if (branch == 4) {
      e->ridingEntity = &f->otherActor->living.entity;
      e->motionX = .25;
      e->motionZ = -.75;
      e->rotationYaw = 23;
    }
    e->onGround = true;
    CHECK(EntityPlayerSP_onUpdateWalkingPlayer(f->sp));
    CHECK(f->packetCount == 1 && kind(f->packets[0]) == (branch == 0   ? 3
                                                         : branch == 1 ? 4
                                                         : branch == 2 ? 5
                                                                       : 6));
    C03PacketPlayer *p = (C03PacketPlayer *)f->packets[0];
    CHECK(p->onGround);
    if (branch == 1 || branch == 3)
      CHECK(p->x == 1 && f->sp->lastReportedPosX == 1 &&
            f->sp->positionUpdateTicks == 0);
    else
      CHECK(f->sp->positionUpdateTicks == 1);
    if (branch == 4)
      CHECK(p->x == .25 && p->y == -999 && p->z == -.75 &&
            f->sp->lastReportedPosX == 0);
    if (branch >= 2)
      CHECK(f->sp->lastReportedYaw == e->rotationYaw);
    packet_bytes(f, p);
    finish(&g, &scope, false);
  }
  for (unsigned mask = 0; mask < 16; mask++) {
    MCGameplay g = {0};
    MCObjectRootScope scope = {0};
    Fixture *f = setup(&g, &scope);
    f->sprint = (mask & 1) != 0;
    f->sneak = (mask & 2) != 0;
    f->sp->serverSprintState = (mask & 4) != 0;
    f->sp->serverSneakState = (mask & 8) != 0;
    bool sprintChange = f->sprint != f->sp->serverSprintState,
         sneakChange = f->sneak != f->sp->serverSneakState;
    CHECK(EntityPlayerSP_onUpdateWalkingPlayer(f->sp));
    CHECK(f->packetCount == (unsigned)sprintChange + (unsigned)sneakChange + 1);
    unsigned at = 0;
    if (sprintChange) {
      C0BPacketEntityAction *p = (C0BPacketEntityAction *)f->packets[at++];
      CHECK(kind((MCObject *)p) == 11 && p->entityID == 42 && p->auxData == 0);
      CHECK(p->action ==
            C0BPacketEntityAction_nativeAction(f->sprint ? C0B_START_SPRINTING
                                                         : C0B_STOP_SPRINTING));
    }
    if (sneakChange) {
      C0BPacketEntityAction *p = (C0BPacketEntityAction *)f->packets[at++];
      CHECK(kind((MCObject *)p) == 11 && p->entityID == 42);
      CHECK(p->action ==
            C0BPacketEntityAction_nativeAction(f->sneak ? C0B_START_SNEAKING
                                                        : C0B_STOP_SNEAKING));
    }
    CHECK(kind(f->packets[at]) == 3 && f->sp->serverSprintState == f->sprint &&
          f->sp->serverSneakState == f->sneak);
    finish(&g, &scope, false);
  }
}
static void thresholds(void) {
  const double positions[] = {
      0, .03, nextafter(.03, 0), nextafter(.03, 1), NAN, INFINITY};
  for (unsigned x = 0; x < 6; x++)
    for (unsigned ticks = 0; ticks < 2; ticks++) {
      MCGameplay g = {0};
      MCObjectRootScope scope = {0};
      Fixture *f = setup(&g, &scope);
      f->actor->living.entity.posX = positions[x];
      f->sp->positionUpdateTicks = ticks ? 20 : 19;
      CHECK(EntityPlayerSP_onUpdateWalkingPlayer(f->sp));
      bool moving = ticks || x == 3 || x == 5;
      CHECK(kind(f->packets[0]) == (moving ? 4 : 3));
      CHECK(f->sp->positionUpdateTicks == (moving ? 0 : 20));
      finish(&g, &scope, false);
    }
  MCGameplay g = {0};
  MCObjectRootScope scope = {0};
  Fixture *f = setup(&g, &scope);
  f->actor->living.entity.ridingEntity = &f->otherActor->living.entity;
  f->sp->positionUpdateTicks = INT32_MAX;
  CHECK(EntityPlayerSP_onUpdateWalkingPlayer(f->sp));
  CHECK(f->sp->positionUpdateTicks == INT32_MIN);
  finish(&g, &scope, false);
  g = (MCGameplay){0};
  scope = (MCObjectRootScope){0};
  f = setup(&g, &scope);
  f->view = false;
  f->sprint = true;
  f->sneak = true;
  f->sp->positionUpdateTicks = -7;
  CHECK(EntityPlayerSP_onUpdateWalkingPlayer(f->sp));
  CHECK(f->packetCount == 2 && f->boxCount == 0 &&
        f->sp->positionUpdateTicks == -7);
  finish(&g, &scope, false);
}
static void mutation_and_failure(void) {
  MCGameplay g = {0};
  MCObjectRootScope scope = {0};
  Fixture *f = setup(&g, &scope);
  Entity *e = &f->actor->living.entity;
  e->posX = 1;
  e->rotationYaw = 1;
  f->mutateArgs = true;
  f->mutateSent = true;
  f->mutatePost = true;
  CHECK(EntityPlayerSP_onUpdateWalkingPlayer(f->sp));
  C03PacketPlayer *p = (C03PacketPlayer *)f->packets[0];
  CHECK(kind((MCObject *)p) == 6 && f->receivers[0] == f->queue &&
        f->sp->sendQueue == f->otherQueue);
  CHECK(p->x == 1 && p->y == 13 && p->z == 19 && p->yaw == 23 &&
        p->pitch == 29 && p->onGround);
  CHECK(f->sp->lastReportedPosX == 37 && f->sp->lastReportedPosY == 53 &&
        f->sp->lastReportedPosZ == 61);
  CHECK(f->sp->lastReportedYaw == 67 && f->sp->lastReportedPitch == 71 &&
        f->sp->positionUpdateTicks == 0);
  finish(&g, &scope, false);
  g = (MCGameplay){0};
  scope = (MCObjectRootScope){0};
  f = setup(&g, &scope);
  f->sprint = true;
  f->useSourceSneak = true;
  f->mutateActions = true;
  CHECK(EntityPlayerSP_onUpdateWalkingPlayer(f->sp));
  CHECK(f->packetCount == 3 && f->sp->serverSprintState &&
        f->sp->serverSneakState);
  finish(&g, &scope, false);
  g = (MCGameplay){0};
  scope = (MCObjectRootScope){0};
  f = setup(&g, &scope);
  f->sprint = true;
  f->flipSprintState = true;
  CHECK(EntityPlayerSP_onUpdateWalkingPlayer(f->sp));
  CHECK(f->packetCount == 1);
  finish(&g, &scope, false);
  for (unsigned failure = 1; failure <= 9; failure++) {
    g = (MCGameplay){0};
    scope = (MCObjectRootScope){0};
    f = setup(&g, &scope);
    e = &f->actor->living.entity;
    f->sprint = true;
    f->sneak = true;
    e->posX = 1;
    e->rotationYaw = 1;
    f->failAt = failure;
    CHECK(!EntityPlayerSP_onUpdateWalkingPlayer(f->sp) &&
          MCObjectHeap_failed(g.heap));
    CHECK(f->eventCount == failure);
    CHECK(f->sp->serverSprintState == (failure > 2));
    CHECK(f->sp->serverSneakState == (failure > 4));
    if (failure == 9)
      CHECK(f->sp->lastReportedPosX == 1 && f->sp->lastReportedPosY == 0 &&
            f->sp->positionUpdateTicks == 1);
    finish(&g, &scope, true);
  }
  for (unsigned failBox = 1; failBox <= 3; failBox++) {
    g = (MCGameplay){0};
    scope = (MCObjectRootScope){0};
    f = setup(&g, &scope);
    e = &f->actor->living.entity;
    e->posX = 7;
    e->rotationYaw = 3;
    f->failBox = failBox;
    CHECK(!EntityPlayerSP_onUpdateWalkingPlayer(f->sp) &&
          MCObjectHeap_failed(g.heap));
    CHECK(f->packetCount == (failBox == 3 ? 1 : 0));
    CHECK(f->sp->lastReportedPosX == (failBox == 3 ? 7 : 0));
    CHECK(f->sp->positionUpdateTicks == (failBox == 3 ? 1 : 0));
    finish(&g, &scope, true);
  }
  g = (MCGameplay){0};
  scope = (MCObjectRootScope){0};
  f = setup(&g, &scope);
  f->actor->living.entity.posX = 1;
  f->sp->sendQueue = NULL;
  f->mutateArgs = true;
  CHECK(!EntityPlayerSP_onUpdateWalkingPlayer(f->sp) &&
        MCObjectHeap_failed(g.heap));
  CHECK(f->boxCount == 2 && f->packetCount == 0 &&
        f->sp->sendQueue == f->otherQueue);
  finish(&g, &scope, true);
  g = (MCGameplay){0};
  scope = (MCObjectRootScope){0};
  f = setup(&g, &scope);
  f->replaceFixtureActor = true;
  EntityPlayerSP_asPlayer(f->sp)->living.entity.onGround=true;
  CHECK(EntityPlayerSP_onUpdateWalkingPlayer(f->sp));
  CHECK(f->actor==f->otherActor && EntityPlayerSP_asPlayer(f->sp)!=f->actor);
  CHECK(f->packetCount==1 && kind(f->packets[0])==3 && f->sp->positionUpdateTicks==1);
  CHECK(C03PacketPlayer_isOnGround((C03PacketPlayer *)f->packets[0]));
  finish(&g, &scope, false);
}
static void lifetime_and_native_bounds(void) {
  MCGameplay g = {0};
  MCObjectRootScope scope = {0};
  Fixture *f = setup(&g, &scope);
  f->allowCollect = true;
  CHECK(EntityPlayerSP_onUpdateWalkingPlayer(f->sp));
  CHECK(f->packetCount == 1);
  MCObjectRootScope_end(&scope);
  CHECK(MCObjectHeap_collect(g.heap));
  MCObjectHeap *copy = MCObjectHeap_clone(g.heap);
  CHECK(copy);
  MCObjectRoot copied = {0};
  CHECK(MCObjectRoot_rebind(&copied, copy, &g.root));
  MCGameplayObjects *owners = (MCGameplayObjects *)MCObjectRoot_get(&copied);
  Fixture *other = (Fixture *)((MCGameplayPlayer *)owners->players[0])->effects;
  CHECK(other && other != f);
  CHECK(EntityPlayerSP_asPlayer(other->sp) == other->actor &&
        other->sp->walkingContext == (MCObject *)other);
  CHECK(other->sp->movementInput != f->sp->movementInput &&
        other->packets[0] != f->packets[0]);
  CHECK(MCObjectRootScope_begin(&scope, copy));
  CHECK(EntityPlayerSP_onUpdateWalkingPlayer(other->sp));
  MCObjectRootScope_end(&scope);
  CHECK(other->sp->positionUpdateTicks == 2 && f->sp->positionUpdateTicks == 1);
  CHECK(MCObjectHeap_adopt(g.heap, copy));
  CHECK(MCObjectRoot_rebind(&copied, g.heap, &copied));
  MCObjectHeap_free(copy);
  /* g.root already owns this same adopted root ID; copied is the temporary
     alias handle, so dropping it would remove the surviving owner's root. */
  copied = (MCObjectRoot){0};
  CHECK(MCObjectHeap_collect(g.heap));
  f = (Fixture *)((MCGameplayPlayer *)MCGameplay_get(&g)->players[0])->effects;
  CHECK(f->sp->positionUpdateTicks == 2);
  CHECK(MCGameplay_free(&g));
  for (unsigned mode = 0; mode < 3; mode++) {
    g = (MCGameplay){0};
    scope = (MCObjectRootScope){0};
    f = setup(&g, &scope);
    if (mode == 0)
      f->sp->walkingDependencies = NULL;
    MCObjectHeap *foreign=NULL;
    if (mode == 1) {
      foreign=MCObjectHeap_new(1024*1024);CHECK(foreign);
      f->sp->walkingContext=MCObjectHeap_alloc(foreign,sizeof(Fixture),&fixtureClass);CHECK(f->sp->walkingContext);
    }
    if (mode == 2) {
      size_t remaining = 32 * 1024 * 1024 - MCObjectHeap_liveBytes(g.heap);
      static const MCObjectClass cls = {"fixture.exhaust.walk", MCObjectHeap_plainClone,
                                       NULL, NULL};
      CHECK(MCObjectHeap_alloc(g.heap, remaining, &cls));
    }
    CHECK(!EntityPlayerSP_onUpdateWalkingPlayer(f->sp) &&
          MCObjectHeap_failed(g.heap));
    CHECK(f->packetCount == 0);
    CHECK(f->sp->positionUpdateTicks == 0 && f->sp->lastReportedPosX == 0);
    finish(&g, &scope, true);
    MCObjectHeap_free(foreign);
  }
  g = (MCGameplay){0};
  scope = (MCObjectRootScope){0};
  f = setup(&g, &scope);
  EntityPlayerSP *tinySP=(EntityPlayerSP *)MCObjectHeap_alloc(g.heap,sizeof(MCObject),EntityPlayerSP_asObject(f->sp)->klass);CHECK(tinySP);
  CHECK(!EntityPlayerSP_isSneaking(tinySP) && MCObjectHeap_failed(g.heap));
  finish(&g, &scope, true);
  g = (MCGameplay){0};
  scope = (MCObjectRootScope){0};
  f = setup(&g, &scope);
  MovementInput *tiny = (MovementInput *)MCObjectHeap_alloc(
      g.heap, sizeof(MCObject), f->sp->movementInput->object.klass);
  CHECK(tiny);
  f->sp->movementInput = tiny;
  CHECK(!EntityPlayerSP_isSneaking(f->sp) && MCObjectHeap_failed(g.heap));
  finish(&g, &scope, true);
}
int main(void) {
  defaults_and_sneak();
  branches_and_actions();
  thresholds();
  mutation_and_failure();
  lifetime_and_native_bounds();
  printf("Source walking: %u checks passed\n", checks);
  return 0;
}
