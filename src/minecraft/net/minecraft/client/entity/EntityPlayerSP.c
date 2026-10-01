#include "client/entity/EntityPlayerSP.h"
#include "network/play/client/C03PacketPlayer.h"
#include "network/play/client/C0BPacketEntityAction.h"
#include <string.h>

static void trace(MCObject *o, MCObjectVisitor v, void *ctx) {
    EntityPlayerSP *p = (EntityPlayerSP *)o;
    AbstractClientPlayer_traceFields(&p->clientPlayer,v,ctx);
    p->statWriter=(StatFileWriter *)v((MCObject *)p->statWriter,ctx);
    p->clientBrand=(NBTString *)v((MCObject *)p->clientBrand,ctx);
    p->sendQueue = (NetHandlerPlayClient *)v((MCObject *)p->sendQueue, ctx);
    p->mc = v(p->mc, ctx);
    p->dependencyContext = v(p->dependencyContext, ctx);
    p->movementInput = (MovementInput *)v((MCObject *)p->movementInput,ctx);
    p->walkingContext = v(p->walkingContext,ctx);
}
static const MCObjectClass klass = {"net.minecraft.client.entity.EntityPlayerSP",
                                    MCObjectHeap_plainClone, trace, NULL};
bool EntityPlayerSP_isInstance(const MCObject *o) { return o && o->klass == &klass && MCObjectHeap_objectSize(o)>=sizeof(EntityPlayerSP); }
MCGameplayPlayer *EntityPlayerSP_asPlayer(EntityPlayerSP *p) {return p? &p->clientPlayer.player:NULL;}
MCObject *EntityPlayerSP_asObject(EntityPlayerSP *p) {return (MCObject *)p;}
EntityPlayerSP *EntityPlayerSP_nativeAllocate(MCObjectHeap *heap) {
    return (EntityPlayerSP *)MCObjectHeap_alloc(heap,sizeof(EntityPlayerSP),&klass);
}
static bool failure(MCObjectHeap *h) {
    MCObjectHeap_fail(h);
    return false;
}
static bool begin(EntityPlayerSP *p, MCObjectRootScope *scope) {
    if (!p || !EntityPlayerSP_isInstance((MCObject *)p))
        return failure(p ? EntityPlayerSP_asObject(p)->heap : NULL);
    return MCObjectRootScope_begin(scope, EntityPlayerSP_asObject(p)->heap) &&
           MCObjectRootScope_pin(scope, (MCObject *)p);
}
static bool end(EntityPlayerSP *p, MCObjectRootScope *scope, bool ok) {
    ok = ok && !MCObjectHeap_failed(EntityPlayerSP_asObject(p)->heap);
    if (!ok)
        failure(EntityPlayerSP_asObject(p)->heap);
    MCObjectRootScope_end(scope);
    return ok;
}
bool EntityPlayerSP_construct(EntityPlayerSP *p,MCObject *mc,MCObject *world,
    NetHandlerPlayClient *handler,StatFileWriter *stats,const EntityPlayerSPConstructorDependencies *d,
    const mc_crafting_dispatch *crafting,MCObject *context,NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *heap=EntityPlayerSP_asObject(p)->heap;
    bool ok=false;
    if(!d||!d->player||!d->getGameProfile||
       !MCObjectRootScope_pin(&scope,mc)||!MCObjectRootScope_pin(&scope,world)||
       !MCObjectRootScope_pin(&scope,(MCObject *)handler)||
       !MCObjectRootScope_pin(&scope,(MCObject *)stats)||!MCObjectRootScope_pin(&scope,context))goto done;
    /* This virtual call is evaluated as the super-constructor argument, before
       any Entity allocation effects. Its nullable result is not prevalidated. */
    if(!NetHandlerPlayClient_isInstance((MCObject *)handler))goto done;
    NativeGameProfile *profile=d->getGameProfile(context,handler);
    if(MCObjectHeap_failed(heap)||!MCObjectRootScope_pin(&scope,(MCObject *)profile))goto done;
    if(!AbstractClientPlayer_construct(&p->clientPlayer,world,profile,d->player,crafting,context,random,ids))goto done;
    p->sendQueue=handler;MCObjectHeap_touch(heap);
    p->statWriter=stats;MCObjectHeap_touch(heap);
    p->mc=mc;MCObjectHeap_touch(heap);
    EntityPlayerSP_asPlayer(p)->living.entity.dimension=0;MCObjectHeap_touch(heap);
    ok=true;
done:
    return end(p,&scope,ok);
}
bool EntityPlayerSP_bindActions(EntityPlayerSP *p,MCObject *context,const EntityPlayerSPDependencies *d) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    bool ok=d&&d->addToSendQueue&&d->blockPosOrigin&&d->displayGuiScreenNull&&
        MCObjectRootScope_pin(&scope,context);
    if(ok){p->dependencyContext=context;p->dependencies=d;MCObjectHeap_touch(EntityPlayerSP_asObject(p)->heap);}
    return end(p,&scope,ok);
}
static bool queue_packet(EntityPlayerSP *p, MCObject *packet, MCObjectRootScope *scope) {
    return packet && p->dependencies && p->dependencies->addToSendQueue &&
           NetHandlerPlayClient_isInstance((MCObject *)p->sendQueue) &&
           MCObjectRootScope_pin(scope, (MCObject *)p->sendQueue) &&
           MCObjectRootScope_pin(scope, p->dependencyContext) &&
           p->dependencies->addToSendQueue(p->dependencyContext, p->sendQueue, packet);
}
bool EntityPlayerSP_dropOneItem(EntityPlayerSP *p, bool all, EntityItem **out) {
    MCObjectRootScope scope = {0};
    if (!out || !begin(p, &scope))
        return false;
    bool ok = p->dependencies && p->dependencies->blockPosOrigin &&
              MCObjectRootScope_pin(&scope, p->dependencyContext);
    DataWatcherBlockPos *pos = ok ? p->dependencies->blockPosOrigin(p->dependencyContext) : NULL;
    ok = ok && !MCObjectHeap_failed(EntityPlayerSP_asObject(p)->heap) && pos &&
         DataWatcher_blockPosIsInstance((MCObject *)pos) &&
         MCObjectRootScope_pin(&scope, (MCObject *)pos);
    C07PacketPlayerDigging *packet =
        ok ? C07PacketPlayerDigging_new(
                 EntityPlayerSP_asObject(p)->heap,
                 C07PacketPlayerDigging_action(all ? C07_DROP_ALL_ITEMS : C07_DROP_ITEM), pos,
                 C07PacketPlayerDigging_facing(0))
           : NULL;
    ok = queue_packet(p, (MCObject *)packet, &scope);
    if (ok && !MCObjectHeap_failed(EntityPlayerSP_asObject(p)->heap))
        *out = NULL;
    return end(p, &scope, ok);
}
bool EntityPlayerSP_closeScreen(EntityPlayerSP *p) {
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    MCGameplayPlayer *actor = EntityPlayerSP_asPlayer(p);
    bool ok = MCGameplayPlayer_isInstance((MCObject *)actor) &&
              MCObjectRootScope_pin(&scope, (MCObject *)actor) && actor->openContainer &&
              MCObjectRootScope_pin(&scope, (MCObject *)actor->openContainer);
    C0DPacketCloseWindow *packet =
        ok ? C0DPacketCloseWindow_new(EntityPlayerSP_asObject(p)->heap, actor->openContainer->windowId) : NULL;
    ok = queue_packet(p, (MCObject *)packet, &scope) && !MCObjectHeap_failed(EntityPlayerSP_asObject(p)->heap);
    if (ok)
        ok = EntityPlayerSP_closeScreenAndDropStack(p);
    return end(p, &scope, ok);
}
bool EntityPlayerSP_closeScreenAndDropStack(EntityPlayerSP *p) {
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    MCGameplayPlayer *actor = EntityPlayerSP_asPlayer(p);
    bool ok = MCGameplayPlayer_isInstance((MCObject *)actor) &&
              MCObjectRootScope_pin(&scope, (MCObject *)actor) && actor->inventory &&
              MCObjectRootScope_pin(&scope, (MCObject *)actor->inventory);
    if (ok)
        ok = InventoryPlayer_setItemStack(actor->inventory, NULL);
    if (ok) {
        /* Original EntityPlayer.closeScreen superclass body: only this assignment.
           Minecraft's GUI dependency subsequently closes its retained old GUI. */
        actor->openContainer =
            ((ContainerPlayer *)(actor->inventoryContainer)) ? actor->inventoryContainer : NULL;
        MCObjectHeap_touch(EntityPlayerSP_asObject(p)->heap);
        ok = p->dependencies && p->dependencies->displayGuiScreenNull && p->mc &&
             MCObjectRootScope_pin(&scope, p->mc) &&
             MCObjectRootScope_pin(&scope, p->dependencyContext) &&
             p->dependencies->displayGuiScreenNull(p->dependencyContext, p->mc);
    }
    return end(p, &scope, ok);
}
bool EntityPlayerSP_joinEntityItemWithWorld(EntityPlayerSP *p, EntityItem *item) {
    (void)item;
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    /* The original SP override has an empty body, including for a NULL argument. */
    return end(p, &scope, true);
}
bool EntityPlayerSP_addStat(EntityPlayerSP *p, StatBase *stat, int32_t amount) {
    (void)amount;
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    bool ok = MCObjectRootScope_pin(&scope, (MCObject *)stat);
    if (ok && stat && stat->isIndependent) {
        /* Original EntityPlayer.addStat superclass body is empty. */
    }
    return end(p, &scope, ok);
}
bool EntityPlayerSP_triggerAchievement(EntityPlayerSP *p, StatBase *stat) {
    /* Original inherited EntityPlayer method dynamically calls this.addStat. */
    return EntityPlayerSP_addStat(p, stat, 1);
}
bool EntityPlayerSP_bindWalking(EntityPlayerSP *p,MCObject *context,const EntityPlayerSPWalkingDependencies *d) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    bool ok=d&&d->isSprinting&&d->isSneaking&&d->isCurrentViewEntity&&d->getEntityBoundingBox&&
        MCObjectRootScope_pin(&scope,context);
    if(ok){p->walkingContext=context;p->walkingDependencies=d;MCObjectHeap_touch(EntityPlayerSP_asObject(p)->heap);}
    return end(p,&scope,ok);
}
bool EntityPlayerSP_isSneaking(EntityPlayerSP *p) {
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    bool flag = false, ok = true;
    MovementInput *input = p->movementInput;
    if (input) {
        ok = MovementInput_isInstance((MCObject *)input) &&
             MCObjectRootScope_pin(&scope, (MCObject *)input);
        if (ok)
            flag = input->sneak;
    }
    if (ok && flag) {
        MCGameplayPlayer *actor = EntityPlayerSP_asPlayer(p);
        ok = MCGameplayPlayer_isInstance((MCObject *)actor) &&
             MCObjectRootScope_pin(&scope, (MCObject *)actor);
        if (ok)
            flag = !actor->sleeping;
    }
    return end(p, &scope, ok) && flag;
}
static bool walking_actor(EntityPlayerSP *p, MCGameplayPlayer *actor) {
    return EntityPlayerSP_asPlayer(p) == actor && MCGameplayPlayer_isInstance((MCObject *)actor) &&
           actor->living.entity.object.heap == EntityPlayerSP_asObject(p)->heap &&
           !MCObjectHeap_failed(EntityPlayerSP_asObject(p)->heap);
}
static bool walking_boolean(EntityPlayerSP *p, MCGameplayPlayer *actor,
                            bool (*method)(MCObject *, EntityPlayerSP *, bool *), bool *out,
                            MCObjectRootScope *scope) {
    return method && MCObjectRootScope_pin(scope, p->walkingContext) &&
           method(p->walkingContext, p, out) && walking_actor(p, actor);
}
static AxisAlignedBB *walking_box(EntityPlayerSP *p, MCGameplayPlayer *actor,
                                  const EntityPlayerSPWalkingDependencies *d,
                                  MCObjectRootScope *scope) {
    AxisAlignedBB *box = d->getEntityBoundingBox && MCObjectRootScope_pin(scope, p->walkingContext)
                             ? d->getEntityBoundingBox(p->walkingContext, p)
                             : NULL;
    if (!walking_actor(p, actor) || !AxisAlignedBB_isInstance((MCObject *)box) ||
        !MCObjectRootScope_pin(scope, (MCObject *)box))
        return NULL;
    return box;
}
static bool walking_queue(EntityPlayerSP *p, MCGameplayPlayer *actor,
                          NetHandlerPlayClient *captured, MCObject *packet,
                          MCObjectRootScope *scope) {
    return packet && !MCObjectHeap_failed(EntityPlayerSP_asObject(p)->heap) && p->dependencies &&
           p->dependencies->addToSendQueue &&
           NetHandlerPlayClient_isInstance((MCObject *)captured) &&
           MCObjectHeap_objectSize((MCObject *)captured) >= sizeof *captured &&
           MCObjectRootScope_pin(scope, p->dependencyContext) &&
           p->dependencies->addToSendQueue(p->dependencyContext, captured, packet) &&
           walking_actor(p, actor);
}
static int32_t increment32(int32_t value) {
    uint32_t bits = (uint32_t)value + UINT32_C(1);
    int32_t result;
    memcpy(&result, &bits, sizeof result);
    return result;
}
bool EntityPlayerSP_onUpdateWalkingPlayer(EntityPlayerSP *p) {
    MCObjectRootScope scope = {0};
    if (!begin(p, &scope))
        return false;
    bool ok = false, flag, flag1, current;
    MCGameplayPlayer *actor = EntityPlayerSP_asPlayer(p);
    const EntityPlayerSPWalkingDependencies *d = p->walkingDependencies;
    if (!walking_actor(p, actor) || !MCObjectRootScope_pin(&scope, (MCObject *)actor) ||
        !MCObjectRootScope_pin(&scope, p->walkingContext) || !d)
        goto done;
    Entity *e = &actor->living.entity;
    if (!walking_boolean(p, actor, d->isSprinting, &flag, &scope))
        goto done;
    if (flag != p->serverSprintState) {
        /* Java captures the sendQueue receiver before constructor arguments.
           A null receiver still evaluates arguments/new before invocation. */
        NetHandlerPlayClient *sendQueue = p->sendQueue;
        if (!MCObjectRootScope_pin(&scope, (MCObject *)sendQueue))
            goto done;
        C0BPacketEntityAction *packet = C0BPacketEntityAction_new(
            EntityPlayerSP_asObject(p)->heap, e,
            C0BPacketEntityAction_nativeAction(flag ? C0B_START_SPRINTING : C0B_STOP_SPRINTING));
        if (!walking_queue(p, actor, sendQueue, (MCObject *)packet, &scope))
            goto done;
        p->serverSprintState = flag;
        MCObjectHeap_touch(EntityPlayerSP_asObject(p)->heap);
    }
    if (!walking_boolean(p, actor, d->isSneaking, &flag1, &scope))
        goto done;
    if (flag1 != p->serverSneakState) {
        NetHandlerPlayClient *sendQueue = p->sendQueue;
        if (!MCObjectRootScope_pin(&scope, (MCObject *)sendQueue))
            goto done;
        C0BPacketEntityAction *packet = C0BPacketEntityAction_new(
            EntityPlayerSP_asObject(p)->heap, e,
            C0BPacketEntityAction_nativeAction(flag1 ? C0B_START_SNEAKING : C0B_STOP_SNEAKING));
        if (!walking_queue(p, actor, sendQueue, (MCObject *)packet, &scope))
            goto done;
        p->serverSneakState = flag1;
        MCObjectHeap_touch(EntityPlayerSP_asObject(p)->heap);
    }
    if (!walking_boolean(p, actor, d->isCurrentViewEntity, &current, &scope))
        goto done;
    if (current) {
        double d0 = e->posX - p->lastReportedPosX;
        AxisAlignedBB *box = walking_box(p, actor, d, &scope);
        if (!box)
            goto done;
        double d1 = box->minY - p->lastReportedPosY;
        double d2 = e->posZ - p->lastReportedPosZ;
        volatile float yawDifference = e->rotationYaw - p->lastReportedYaw;
        volatile float pitchDifference = e->rotationPitch - p->lastReportedPitch;
        double d3 = (double)yawDifference, d4 = (double)pitchDifference;
        /* Source double multiplication/addition order, without contraction. */
        volatile double xx = d0 * d0, yy = d1 * d1, zz = d2 * d2;
        volatile double xy = xx + yy;
        double distance = xy + zz;
        bool flag2 = distance > 9.0E-4 || p->positionUpdateTicks >= 20;
        bool flag3 = d3 != 0.0 || d4 != 0.0;
        NetHandlerPlayClient *sendQueue;
        C03PacketPlayer *packet;
        if (!e->ridingEntity) {
            if (flag2 && flag3) {
                sendQueue = p->sendQueue;
                if (!MCObjectRootScope_pin(&scope, (MCObject *)sendQueue))
                    goto done;
                double x = e->posX;
                box = walking_box(p, actor, d, &scope);
                if (!box)
                    goto done;
                double y = box->minY, z = e->posZ;
                float yaw = e->rotationYaw, pitch = e->rotationPitch;
                bool ground = e->onGround;
                packet = C06PacketPlayerPosLook_new(EntityPlayerSP_asObject(p)->heap, x, y, z, yaw, pitch, ground);
            } else if (flag2) {
                sendQueue = p->sendQueue;
                if (!MCObjectRootScope_pin(&scope, (MCObject *)sendQueue))
                    goto done;
                double x = e->posX;
                box = walking_box(p, actor, d, &scope);
                if (!box)
                    goto done;
                double y = box->minY, z = e->posZ;
                bool ground = e->onGround;
                packet = C04PacketPlayerPosition_new(EntityPlayerSP_asObject(p)->heap, x, y, z, ground);
            } else if (flag3) {
                sendQueue = p->sendQueue;
                if (!MCObjectRootScope_pin(&scope, (MCObject *)sendQueue))
                    goto done;
                packet = C05PacketPlayerLook_new(EntityPlayerSP_asObject(p)->heap, e->rotationYaw, e->rotationPitch,
                                                 e->onGround);
            } else {
                sendQueue = p->sendQueue;
                if (!MCObjectRootScope_pin(&scope, (MCObject *)sendQueue))
                    goto done;
                packet = C03PacketPlayer_new(EntityPlayerSP_asObject(p)->heap, e->onGround);
            }
            if (!walking_queue(p, actor, sendQueue, (MCObject *)packet, &scope))
                goto done;
        } else {
            sendQueue = p->sendQueue;
            if (!MCObjectRootScope_pin(&scope, (MCObject *)sendQueue))
                goto done;
            packet = C06PacketPlayerPosLook_new(EntityPlayerSP_asObject(p)->heap, e->motionX, -999.0, e->motionZ,
                                                e->rotationYaw, e->rotationPitch, e->onGround);
            if (!walking_queue(p, actor, sendQueue, (MCObject *)packet, &scope))
                goto done;
            flag2 = false;
        }
        p->positionUpdateTicks = increment32(p->positionUpdateTicks);
        MCObjectHeap_touch(EntityPlayerSP_asObject(p)->heap);
        if (flag2) {
            p->lastReportedPosX = e->posX;
            MCObjectHeap_touch(EntityPlayerSP_asObject(p)->heap);
            box = walking_box(p, actor, d, &scope);
            if (!box)
                goto done;
            p->lastReportedPosY = box->minY;
            p->lastReportedPosZ = e->posZ;
            p->positionUpdateTicks = 0;
            MCObjectHeap_touch(EntityPlayerSP_asObject(p)->heap);
        }
        if (flag3) {
            p->lastReportedYaw = e->rotationYaw;
            p->lastReportedPitch = e->rotationPitch;
            MCObjectHeap_touch(EntityPlayerSP_asObject(p)->heap);
        }
    }
    ok = true;
done:
    return end(p, &scope, ok);
}
