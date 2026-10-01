#include "entity/player/EntityPlayerDrops.h"
#include "util/MathHelper.h"

static float multiply(float a,float b) {volatile float out=a*b;return out;}
static float divide(float a,float b) {volatile float out=a/b;return out;}
static float add(float a,float b) {volatile float out=a+b;return out;}
static bool completed(MCObjectHeap *h,bool ok) {if(!ok)MCObjectHeap_fail(h);return ok&&!MCObjectHeap_failed(h);}
static bool dependencies_ready(const EntityPlayerDropsDependencies *d) {
    return d&&d->entity&&d->constructor&&d->getEyeHeight&&d->nextFloat&&d->getName&&
        d->joinEntityItemWithWorld&&d->triggerDropStat&&d->mathSin&&d->mathCos;
}
EntityItem *EntityPlayer_dropItem(MCGameplayPlayer *player,ItemStack *stack,bool around,bool trace,
    const EntityPlayerDropsDependencies *d,MCObject *context) {
    if (!player) return NULL;
    MCObjectHeap *h=player->object.heap;
    if (!stack) return NULL;
    if (stack->object.heap!=h) {MCObjectHeap_fail(h);return NULL;}
    if (!stack->stackSize) return NULL;
    if (!MCGameplayPlayer_isInstance((MCObject *)player)||!dependencies_ready(d)) {MCObjectHeap_fail(h);return NULL;}
    MCObjectRootScope scope={0}; if(!MCObjectRootScope_begin(&scope,h))return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)player)&&MCObjectRootScope_pin(&scope,(MCObject *)stack)&&MCObjectRootScope_pin(&scope,context);
    EntityItem *entity=NULL;
    if (ok) {
        double y=player->posY-0.30000001192092896;
        float eye=d->getEyeHeight(context,player); y=y+(double)eye;
        if (!MCObjectHeap_failed(h)) entity=EntityItem_new_stack(h,(MCObject *)player->worldObj,context,d->entity,d->constructor,player->posX,y,player->posZ,stack);
        ok=entity!=NULL&&!MCObjectHeap_failed(h);
    }
    if (ok) {
        EntityItem_setPickupDelay(entity,40);
        if (trace) {
            NBTString *name=d->getName(context,player);
            ok=!MCObjectHeap_failed(h)&&EntityItem_setThrower(entity,name);
        }
    }
    if (ok && around) {
        float speed=multiply(d->nextFloat(context,player),0.5f);
        if (!MCObjectHeap_failed(h)) {
            float angle=multiply(multiply(d->nextFloat(context,player),3.1415927f),2.0f);
            if (!MCObjectHeap_failed(h)) {
                entity->motionX=(double)multiply(-MathHelper_sin(angle),speed);
                entity->motionZ=(double)multiply(MathHelper_cos(angle),speed);
                entity->motionY=0.20000000298023224; MCObjectHeap_touch(h);
            }
        }
    } else if (ok) {
        float speed=0.3f;
        float yaw=multiply(divide(player->rotationYaw,180.0f),3.1415927f);
        float pitch=multiply(divide(player->rotationPitch,180.0f),3.1415927f);
        entity->motionX=(double)multiply(multiply(-MathHelper_sin(yaw),MathHelper_cos(pitch)),speed);
        entity->motionZ=(double)multiply(multiply(MathHelper_cos(yaw),MathHelper_cos(pitch)),speed);
        entity->motionY=(double)add(multiply(-MathHelper_sin(pitch),speed),0.1f); MCObjectHeap_touch(h);
        float angle=multiply(multiply(d->nextFloat(context,player),3.1415927f),2.0f);
        if (!MCObjectHeap_failed(h)) speed=multiply(0.02f,d->nextFloat(context,player));
        if (!MCObjectHeap_failed(h)) {
            double cos=d->mathCos(context,(double)angle);
            if (!MCObjectHeap_failed(h)) {volatile double delta=cos*(double)speed;entity->motionX+=delta;MCObjectHeap_touch(h);}
        }
        float first=0,second=0;
        if (!MCObjectHeap_failed(h)) first=d->nextFloat(context,player);
        if (!MCObjectHeap_failed(h)) second=d->nextFloat(context,player);
        if (!MCObjectHeap_failed(h)) {
            entity->motionY+=(double)multiply(add(first,-second),0.1f);
            double sin=d->mathSin(context,(double)angle);
            if (!MCObjectHeap_failed(h)) {volatile double delta=sin*(double)speed;entity->motionZ+=delta;MCObjectHeap_touch(h);}
        }
    }
    ok=ok&&!MCObjectHeap_failed(h);
    if (ok) ok=completed(h,d->joinEntityItemWithWorld(context,player,entity));
    if (ok && trace) ok=completed(h,d->triggerDropStat(context,player));
    MCObjectRootScope_end(&scope); return ok?entity:NULL;
}
EntityItem *EntityPlayer_dropPlayerItemWithRandomChoice(MCGameplayPlayer *player,ItemStack *stack,bool unused,
    const EntityPlayerDropsDependencies *d,MCObject *context) {
    (void)unused;return EntityPlayer_dropItem(player,stack,false,false,d,context);
}
