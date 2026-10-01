#include "util/MCGameplayPlayer.h"
#include "entity/player/EntityPlayer.h"
#include "client/entity/EntityPlayerSP.h"
#include "entity/player/EntityPlayerMP.h"
#include <math.h>

void MCGameplayPlayer_traceFields(MCGameplayPlayer *p,MCObjectVisitor visit,void *context) {
    EntityPlayer_traceFields(p,visit,context);
    p->stats=(StatFileWriter *)visit((MCObject *)p->stats,context);
    p->savedFields=(NBTTagCompound *)visit((MCObject *)p->savedFields,context);
    p->savedRootName=(NBTString *)visit((MCObject *)p->savedRootName,context);
    p->handler=visit(p->handler,context);
    p->effects=visit(p->effects,context);
    p->pendingPackets=visit(p->pendingPackets,context);
}
static void trace(MCObject *object,MCObjectVisitor visit,void *context) {
    MCGameplayPlayer_traceFields((MCGameplayPlayer *)object,visit,context);
}
static const MCObjectClass klass={"C919.native.GameplayPlayer",MCObjectHeap_plainClone,trace,NULL};
bool MCGameplayPlayer_isInstance(const MCObject *object) {
    return object&&(object->klass==&klass||EntityPlayerSP_isInstance(object)||EntityPlayerMP_isInstance(object))&&
        MCObjectHeap_objectSize(object)>=sizeof(MCGameplayPlayer);
}
StatFileWriter *MCGameplayPlayer_statFile(MCGameplayPlayer *p) {
    if(!MCGameplayPlayer_isInstance((MCObject *)p)){MCObjectHeap_fail(p?((MCObject *)p)->heap:NULL);return NULL;}
    return EntityPlayerMP_isInstance((MCObject *)p)?(StatFileWriter *)((EntityPlayerMP *)p)->statsFile:p?p->stats:NULL;
}
MCObject *MCGameplayPlayer_handler(MCGameplayPlayer *p) {
    if(!MCGameplayPlayer_isInstance((MCObject *)p)){MCObjectHeap_fail(p?((MCObject *)p)->heap:NULL);return NULL;}
    return EntityPlayerMP_isInstance((MCObject *)p)?(MCObject *)((EntityPlayerMP *)p)->playerNetServerHandler:p?p->handler:NULL;
}
bool MCGameplayPlayer_isChangingQuantityOnly(MCGameplayPlayer *p) {
    if(!MCGameplayPlayer_isInstance((MCObject *)p)){MCObjectHeap_fail(p?((MCObject *)p)->heap:NULL);return false;}
    return EntityPlayerMP_isInstance((MCObject *)p)?((EntityPlayerMP *)p)->isChangingQuantityOnly:p&&p->isChangingQuantityOnly;
}
bool MCGameplayPlayer_setChangingQuantityOnly(MCGameplayPlayer *p,bool value) {
    if(!MCGameplayPlayer_isInstance((MCObject *)p)){MCObjectHeap_fail(p?((MCObject *)p)->heap:NULL);return false;}
    if(EntityPlayerMP_isInstance((MCObject *)p))((EntityPlayerMP *)p)->isChangingQuantityOnly=value;
    else p->isChangingQuantityOnly=value;
    MCObjectHeap_touch(((MCObject *)p)->heap);return !MCObjectHeap_failed(((MCObject *)p)->heap);
}
MCGameplayPlayer *MCGameplayPlayer_nativeAllocate(MCObjectHeap *heap) {
    return (MCGameplayPlayer *)MCObjectHeap_alloc(heap,sizeof(MCGameplayPlayer),&klass);
}
static bool entity_init(MCObject *context,Entity *e) {(void)context;return EntityPlayer_entityInit((MCGameplayPlayer *)e);}
static bool position(MCObject *context,Entity *e,double x,double y,double z) {(void)context;return Entity_setPosition(e,x,y,z);}
static bool box(MCObject *context,Entity *e,AxisAlignedBB *b) {(void)context;return Entity_setEntityBoundingBox(e,b);}
static bool location(MCObject *context,Entity *e,double x,double y,double z,float yaw,float pitch) {
    (void)context;return Entity_setLocationAndAngles(e,x,y,z,yaw,pitch);
}
static bool dimension(MCObject *context,MCObject *w,int32_t *out) {
    (void)context;if(!MCGameplayWorld_isInstance(w)||!out){MCObjectHeap_fail(w?w->heap:NULL);return false;}
    *out=WorldProvider_getDimensionId(((World *)w)->provider);return !MCObjectHeap_failed(w->heap);
}
static bool remote(MCObject *context,MCObject *w,bool *out) {
    (void)context;if(!MCGameplayWorld_isInstance(w)||!out){MCObjectHeap_fail(w?w->heap:NULL);return false;}
    *out=((World *)w)->isRemote;return true;
}
static bool attributes(MCObject *context,EntityLivingBase *e) {(void)context;return EntityPlayer_applyEntityAttributes((MCGameplayPlayer *)e);}
static BaseAttributeMap *attribute_map(MCObject *context,EntityLivingBase *e) {(void)context;return EntityLivingBase_getAttributeMap(e);}
static IAttributeInstance *attribute(MCObject *context,EntityLivingBase *e,IAttribute *a) {(void)context;return EntityLivingBase_getEntityAttribute(e,a);}
static bool health(MCObject *context,EntityLivingBase *e,float f) {(void)context;return EntityLivingBase_setHealth(e,f);}
static bool math_random(MCObject *context,double *out) {
    MCGameplayPlayer *p=(MCGameplayPlayer *)context;
    if(!MCGameplayPlayer_isInstance(context)||!MCGameplayWorld_isInstance(p->living.entity.worldObj)) {
        MCObjectHeap_fail(context?context->heap:NULL);return false;
    }
    return NativeJavaRandomRuntime_mathRandom(((MCGameplayWorld *)p->living.entity.worldObj)->randomRuntime,out);
}
static DataWatcherBlockPos *spawn_point(MCObject *context,MCObject *w) {
    (void)context;if(!MCGameplayWorld_isInstance(w)){MCObjectHeap_fail(w?w->heap:NULL);return NULL;}
    return World_getSpawnPoint((World *)w);
}
static bool player_location(MCObject *context,MCGameplayPlayer *p,double x,double y,double z,float yaw,float pitch) {
    return location(context,&p->living.entity,x,y,z,yaw,pitch);
}
static const EntityDependencies entity_dependencies={
    .entityInit=entity_init,.setPosition=position,.setEntityBoundingBox=box,
    .getDimensionId=dimension,.isRemote=remote,.setLocationAndAngles=location
};
static const EntityLivingBaseDependencies living_dependencies={
    .applyEntityAttributes=attributes,.getAttributeMap=attribute_map,.getEntityAttribute=attribute,
    .setHealth=health,.mathRandom=math_random
};
const EntityPlayerDependencies MCGameplayPlayer_nativeConstructorBindings={
    .entity=&entity_dependencies,.living=&living_dependencies,.isRemote=remote,
    .getSpawnPoint=spawn_point,.setLocationAndAngles=player_location
};
const EntityPlayerDependencies *MCGameplayPlayer_nativeConstructorDependencies(void) {
    return &MCGameplayPlayer_nativeConstructorBindings;
}
bool MCGameplayPlayer_nativeAttachEnvironment(MCGameplayPlayer *p,StatFileWriter *stats) {
    MCObjectHeap *heap=p?p->living.entity.object.heap:NULL;
    if(!MCGameplayPlayer_isInstance((MCObject *)p)||
        (stats&&((MCObject *)stats)->heap!=heap)) {MCObjectHeap_fail(heap);return false;}
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return false;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)p)&&MCObjectRootScope_pin(&scope,(MCObject *)stats);
    if(ok) {
        p->stats=stats;p->savedFields=NBTTagCompound_new(heap);MCObjectHeap_touch(heap);
        ok=p->savedFields!=NULL&&!MCObjectHeap_failed(heap);
    }
    if(!ok)MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);return ok;
}
static bool dependencies_ready(const mc_crafting_dispatch *d) {
    return d&&d->inventory&&d->world&&d->findMatchingRecipe&&d->getRemainingItems&&
        d->onCrafting&&d->triggerAchievement&&d->drop&&d->isPickaxe&&d->isHoe&&
        d->isSword&&d->isWoodPickaxe&&d->armorType&&d->isRemote&&d->isCraftingTable&&d->getDistanceSq;
}
MCGameplayPlayer *MCGameplayPlayer_newWithProfile(MCGameplayWorld *world,NativeGameProfile *profile,
    StatFileWriter *stats,const mc_crafting_dispatch *crafting) {
    MCObjectHeap *heap=world?world->object.heap:NULL;
    if(!MCGameplayWorld_isInstance((MCObject *)world)||!NativeGameProfile_isInstance((MCObject *)profile)||
       profile->object.heap!=heap||(stats&&((MCObject *)stats)->heap!=heap)||!dependencies_ready(crafting)) {
        MCObjectHeap_fail(heap);return NULL;
    }
    MCObjectRootScope scope={0};
    if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)world)&&MCObjectRootScope_pin(&scope,(MCObject *)profile)&&
        MCObjectRootScope_pin(&scope,(MCObject *)stats);
    MCGameplayPlayer *p=ok?MCGameplayPlayer_nativeAllocate(heap):NULL;
    if(p) {
        ok=EntityPlayer_construct(p,(MCObject *)world,profile,&MCGameplayPlayer_nativeConstructorBindings,crafting,(MCObject *)p,
            world->randomRuntime,NativeEntityIDRuntime_process());
        if(ok)ok=MCGameplayPlayer_nativeAttachEnvironment(p,stats);
    } else ok=false;
    if(!ok)MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap)?p:NULL;
}
MCGameplayPlayer *MCGameplayPlayer_new(MCGameplayWorld *world,NBTString *name,
    StatFileWriter *stats,const mc_crafting_dispatch *crafting) {
    MCObjectHeap *heap=world?world->object.heap:NULL;
    MCObjectRootScope scope={0};
    if(!heap||!MCObjectRootScope_begin(&scope,heap))return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)world)&&MCObjectRootScope_pin(&scope,(MCObject *)name)&&
        MCObjectRootScope_pin(&scope,(MCObject *)stats);
    NativeGameProfile *profile=ok?NativeGameProfile_new(heap,NULL,name):NULL;
    MCGameplayPlayer *p=profile?MCGameplayPlayer_newWithProfile(world,profile,stats,crafting):NULL;
    MCObjectRootScope_end(&scope);return p;
}
static MCGameplayPlayer *player_object(const MCObject *object) {
    if(!MCGameplayPlayer_isInstance(object)){MCObjectHeap_fail(object?object->heap:NULL);return NULL;}
    return (MCGameplayPlayer *)object;
}
InventoryPlayer *MCGameplayPlayer_inventory(MCObject *object) {
    MCGameplayPlayer *p=player_object(object);return p?p->inventory:NULL;
}
MCObject *MCGameplayPlayer_world(MCObject *object) {
    MCGameplayPlayer *p=player_object(object);return p?p->living.entity.worldObj:NULL;
}
PlayerCapabilities *MCGameplayPlayer_capabilities(const MCObject *object) {
    MCGameplayPlayer *p=player_object(object);PlayerCapabilities *caps=p?p->capabilities:NULL;
    if(!caps||caps->object.heap!=object->heap||!PlayerCapabilities_isInstance((MCObject *)caps)) {
        MCObjectHeap_fail(object?object->heap:NULL);return NULL;
    }
    return caps;
}
bool MCGameplayPlayer_isCreativeMode(const MCObject *object) {
    PlayerCapabilities *caps=MCGameplayPlayer_capabilities(object);return caps&&caps->isCreativeMode;
}
double MCGameplayPlayer_getDistanceSq(const MCObject *object,double x,double y,double z) {
    MCGameplayPlayer *p=player_object(object);if(!p)return NAN;
    double dx=p->living.entity.posX-x,dy=p->living.entity.posY-y,dz=p->living.entity.posZ-z;
    return dx*dx+dy*dy+dz*dz;
}
