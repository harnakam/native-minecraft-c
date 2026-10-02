#include "entity/Entity.h"
#include "entity/item/EntityItem.h"
#include "entity/item/EntityItemFrame.h"
#include "util/MCGameplayPlayer.h"
#include "util/MathHelper.h"

/* Native closed subclass classification of actual first-member descendants. */
bool Entity_isInstance(const MCObject *object) {
    return (EntityItem_isInstance(object)||EntityItemFrame_isInstance(object)||MCGameplayPlayer_isInstance(object))&&
        MCObjectHeap_objectSize(object)>=sizeof(Entity);
}
static bool valid(Entity *entity) {
    if(!Entity_isInstance((MCObject *)entity)){MCObjectHeap_fail(entity?entity->object.heap:NULL);return false;}
    return !MCObjectHeap_failed(entity->object.heap);
}
static bool begin(Entity *entity,MCObjectRootScope *scope) {
    if(!valid(entity)||!MCObjectRootScope_begin(scope,entity->object.heap))return false;
    if(MCObjectRootScope_pin(scope,(MCObject *)entity))return true;
    MCObjectRootScope_end(scope);return false;
}
static bool effect(Entity *entity,bool completed) {
    if(!completed)MCObjectHeap_fail(entity->object.heap);
    return completed&&!MCObjectHeap_failed(entity->object.heap);
}
void Entity_traceFields(Entity *entity,MCObjectVisitor visitor,void *context) {
    entity->riddenByEntity=(Entity *)visitor((MCObject *)entity->riddenByEntity,context);
    entity->ridingEntity=(Entity *)visitor((MCObject *)entity->ridingEntity,context);
    entity->worldObj=visitor(entity->worldObj,context);
    entity->boundingBox=(AxisAlignedBB *)visitor((MCObject *)entity->boundingBox,context);
    entity->rand=(NativeJavaRandom *)visitor((MCObject *)entity->rand,context);
    entity->dataWatcher=(DataWatcher *)visitor((MCObject *)entity->dataWatcher,context);
    entity->lastPortalPos=(DataWatcherBlockPos *)visitor((MCObject *)entity->lastPortalPos,context);
    entity->lastPortalVec=visitor(entity->lastPortalVec,context);
    entity->teleportDirection=visitor(entity->teleportDirection,context);
    entity->entityUniqueID=(NativeJavaUUID *)visitor((MCObject *)entity->entityUniqueID,context);
    entity->cmdResultStats=(CommandResultStats *)visitor((MCObject *)entity->cmdResultStats,context);
    entity->entityContext=visitor(entity->entityContext,context);
}
AxisAlignedBB *Entity_getEntityBoundingBox(Entity *entity) {return valid(entity)?entity->boundingBox:NULL;}
bool Entity_setEntityBoundingBox(Entity *entity,AxisAlignedBB *box) {
    if(!valid(entity))return false;
    if(box&&(!AxisAlignedBB_isInstance((MCObject *)box)||box->object.heap!=entity->object.heap))return effect(entity,false);
    entity->boundingBox=box;MCObjectHeap_touch(entity->object.heap);return true;
}
int32_t Entity_getEntityId(const Entity *entity) {return valid((Entity *)entity)?entity->entityId:0;}
bool Entity_equals(Entity *entity,MCObject *other) {
    if(!valid(entity))return false;
    if(other&&other->heap!=entity->object.heap)return effect(entity,false);
    return Entity_isInstance(other)&&((Entity *)other)->entityId==entity->entityId;
}
int32_t Entity_hashCode(Entity *entity) {return valid(entity)?entity->entityId:0;}
void Entity_setEntityId(Entity *entity,int32_t id) {if(valid(entity)){entity->entityId=id;MCObjectHeap_touch(entity->object.heap);}}
DataWatcher *Entity_getDataWatcher(Entity *entity) {return valid(entity)?entity->dataWatcher:NULL;}
NativeJavaUUID *Entity_getUniqueID(Entity *entity) {return valid(entity)?entity->entityUniqueID:NULL;}
CommandResultStats *Entity_getCommandStats(Entity *entity) {return valid(entity)?entity->cmdResultStats:NULL;}
bool Entity_isSilent(Entity *entity) {
    if(!valid(entity))return false;
    if(!DataWatcher_isInstance((MCObject *)entity->dataWatcher)||((MCObject *)entity->dataWatcher)->heap!=entity->object.heap)
        return effect(entity,false);
    return DataWatcher_getWatchableObjectByte(entity->dataWatcher,4)==1;
}
bool Entity_setSilent(Entity *entity,bool silent) {
    MCObjectRootScope scope={0};if(!begin(entity,&scope))return false;
    MCObject *value=DataWatcher_boxByte(entity->object.heap,silent?1:0);
    bool watcher=DataWatcher_isInstance((MCObject *)entity->dataWatcher)&&
        ((MCObject *)entity->dataWatcher)->heap==entity->object.heap;
    bool ok=effect(entity,value&&watcher&&DataWatcher_updateObject(entity->dataWatcher,4,value));
    MCObjectRootScope_end(&scope);return ok;
}
bool Entity_getFlag(Entity *entity,int32_t flag) {
    if(!valid(entity))return false;
    if(!DataWatcher_isInstance((MCObject *)entity->dataWatcher)||
       ((MCObject *)entity->dataWatcher)->heap!=entity->object.heap)return effect(entity,false);
    int32_t value=DataWatcher_getWatchableObjectByte(entity->dataWatcher,0);
    return ((uint32_t)value&(UINT32_C(1)<<((uint32_t)flag&31u)))!=0;
}
bool Entity_setFlag(Entity *entity,int32_t flag,bool set) {
    MCObjectRootScope scope={0};if(!begin(entity,&scope))return false;
    if(!DataWatcher_isInstance((MCObject *)entity->dataWatcher)||
       ((MCObject *)entity->dataWatcher)->heap!=entity->object.heap) {
        effect(entity,false);MCObjectRootScope_end(&scope);return false;
    }
    int32_t old=DataWatcher_getWatchableObjectByte(entity->dataWatcher,0);
    uint32_t mask=UINT32_C(1)<<((uint32_t)flag&31u);
    uint8_t bits=(uint8_t)(set?((uint32_t)old|mask):((uint32_t)old&~mask));
    int32_t value=bits<=INT8_MAX?(int32_t)bits:-1-(int32_t)(UINT8_MAX-bits);
    MCObject *boxed=!MCObjectHeap_failed(entity->object.heap)?DataWatcher_boxByte(entity->object.heap,value):NULL;
    bool ok=boxed&&effect(entity,DataWatcher_updateObject(entity->dataWatcher,0,boxed));
    MCObjectRootScope_end(&scope);return ok;
}
bool Entity_isSneaking(Entity *entity) {return Entity_getFlag(entity,1);}
bool Entity_setSneaking(Entity *entity,bool sneaking) {return Entity_setFlag(entity,1,sneaking);}
bool Entity_isSprinting(Entity *entity) {return Entity_getFlag(entity,3);}
bool Entity_setSprinting(Entity *entity,bool sprinting) {return Entity_setFlag(entity,3,sprinting);}
bool Entity_setPosition(Entity *entity,double x,double y,double z) {
    MCObjectRootScope scope={0};if(!begin(entity,&scope))return false;
    entity->posX=x;entity->posY=y;entity->posZ=z;MCObjectHeap_touch(entity->object.heap);
    float half=entity->width/2.0f,height=entity->height;
    AxisAlignedBB *box=AxisAlignedBB_new(entity->object.heap,x-(double)half,y,z-(double)half,x+(double)half,y+(double)height,z+(double)half);
    const EntityDependencies *d=entity->entityDependencies;
    bool ok=box&&d&&d->setEntityBoundingBox&&effect(entity,d->setEntityBoundingBox(entity->entityContext,entity,box));
    if(!ok)MCObjectHeap_fail(entity->object.heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(entity->object.heap);
}
bool Entity_setSize(Entity *entity,float width,float height) {
    MCObjectRootScope scope={0};if(!begin(entity,&scope))return false;
    bool ok=true;
    if(width!=entity->width||height!=entity->height) {
        float previous=entity->width;entity->width=width;entity->height=height;MCObjectHeap_touch(entity->object.heap);
        AxisAlignedBB *old=Entity_getEntityBoundingBox(entity);
        if(!old){ok=false;goto done;}
        AxisAlignedBB *box=AxisAlignedBB_new(entity->object.heap,old->minX,old->minY,old->minZ,
            old->minX+(double)entity->width,old->minY+(double)entity->height,old->minZ+(double)entity->width);
        const EntityDependencies *d=entity->entityDependencies;
        ok=box&&d&&d->setEntityBoundingBox&&effect(entity,d->setEntityBoundingBox(entity->entityContext,entity,box));
        if(ok&&entity->width>previous&&!entity->firstUpdate) {
            bool remote=false;
            ok=entity->worldObj&&d->isRemote&&effect(entity,d->isRemote(entity->entityContext,entity->worldObj,&remote));
            if(ok&&!remote)ok=d->moveEntity&&effect(entity,d->moveEntity(entity->entityContext,entity,(double)(previous-entity->width),0,(double)(previous-entity->width)));
        }
    }
done:
    if(!ok)MCObjectHeap_fail(entity->object.heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(entity->object.heap);
}
bool Entity_setLocationAndAngles(Entity *entity,double x,double y,double z,float yaw,float pitch) {
    MCObjectRootScope scope={0};if(!begin(entity,&scope))return false;
    entity->lastTickPosX=entity->prevPosX=entity->posX=x;
    entity->lastTickPosY=entity->prevPosY=entity->posY=y;
    entity->lastTickPosZ=entity->prevPosZ=entity->posZ=z;
    entity->rotationYaw=yaw;entity->rotationPitch=pitch;MCObjectHeap_touch(entity->object.heap);
    const EntityDependencies *d=entity->entityDependencies;
    bool ok=d&&d->setPosition&&effect(entity,d->setPosition(entity->entityContext,entity,entity->posX,entity->posY,entity->posZ));
    if(!ok)MCObjectHeap_fail(entity->object.heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(entity->object.heap);
}
bool Entity_moveToBlockPosAndAngles(Entity *entity,DataWatcherBlockPos *pos,float yaw,float pitch) {
    MCObjectRootScope scope={0};if(!begin(entity,&scope))return false;
    const EntityDependencies *d=entity->entityDependencies;
    bool ok=pos&&DataWatcher_blockPosIsInstance((MCObject *)pos)&&pos->object.heap==entity->object.heap&&d&&d->setLocationAndAngles&&
        effect(entity,d->setLocationAndAngles(entity->entityContext,entity,(double)pos->x+0.5,(double)pos->y,(double)pos->z+0.5,yaw,pitch));
    if(!ok)MCObjectHeap_fail(entity->object.heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(entity->object.heap);
}

/* Java class-static identity is represented by one registered root per heap.
   Whole-graph snapshots remap this root and all ZERO_AABB references together;
   there is no cross-heap pointer cache. This is the native class-init boundary,
   rather than part of the instance constructor's callback order. */
typedef struct {MCObject object;AxisAlignedBB *ZERO_AABB;} EntityStatics;
static void statics_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    EntityStatics *fields=(EntityStatics *)object;
    fields->ZERO_AABB=(AxisAlignedBB *)visitor((MCObject *)fields->ZERO_AABB,context);
}
static const MCObjectClass statics_class={"native.Entity.statics",MCObjectHeap_plainClone,statics_trace,NULL};
static bool any(const MCObject *object,void *context) {(void)object;(void)context;return true;}
static EntityStatics *class_statics(MCObjectHeap *heap) {
    EntityStatics *fields=(EntityStatics *)MCObjectHeap_findObject(heap,&statics_class,any,NULL);
    if(fields)return fields;
    fields=(EntityStatics *)MCObjectHeap_alloc(heap,sizeof *fields,&statics_class);
    if(!fields)return NULL;
    fields->ZERO_AABB=AxisAlignedBB_new(heap,0,0,0,0,0,0);
    MCObjectRoot root={0};
    return fields->ZERO_AABB&&MCObjectRoot_init(&root,heap,(MCObject *)fields)?fields:NULL;
}
static bool inherited_update(MCObject *context,MCObject *owner,int32_t id) {
    (void)context;
    if(!Entity_isInstance(owner)){MCObjectHeap_fail(owner?owner->heap:NULL);return false;}
    Entity_onDataWatcherUpdate(owner,id);return !MCObjectHeap_failed(owner->heap);
}
static const DataWatcherDependencies inherited_watcher={.onDataWatcherUpdate=inherited_update};
bool Entity_construct(Entity *entity,MCObject *world,const EntityDependencies *dependencies,MCObject *context,
                      NativeJavaRandomRuntime *randomRuntime,NativeEntityIDRuntime *ids) {
    MCObjectRootScope scope={0};if(!begin(entity,&scope))return false;
    MCObjectHeap *heap=entity->object.heap;bool ok=false;
    /* Native allocation/dispatch validation. Invoked virtual methods are checked
       at their source call sites, including branch-only provider/physics calls.
       Actual subclass allocation supplies Java's zero-initialized fields. */
    if(!dependencies||!randomRuntime||!ids||(world&&world->heap!=heap)||(context&&context->heap!=heap)||
       entity->rand||entity->dataWatcher||entity->entityUniqueID||entity->boundingBox||entity->cmdResultStats)goto done;
    if(!MCObjectRootScope_pin(&scope,world)||!MCObjectRootScope_pin(&scope,context))goto done;
    EntityStatics *fields=class_statics(heap);if(!fields)goto done;
    entity->entityDependencies=dependencies;entity->entityContext=context;
    if(!NativeEntityIDRuntime_next(ids,&entity->entityId))goto done;
    entity->renderDistanceWeight=1.0;entity->boundingBox=fields->ZERO_AABB;
    entity->width=0.6f;entity->height=1.8f;entity->nextStepDistance=1;MCObjectHeap_touch(heap);
    entity->rand=NativeJavaRandomRuntime_newRandom(randomRuntime,heap);
    if(!entity->rand)goto done;
    entity->fireResistance=1;entity->firstUpdate=true;MCObjectHeap_touch(heap);
    entity->entityUniqueID=MathHelper_getRandomUuid(entity->rand);
    if(!entity->entityUniqueID)goto done;
    entity->cmdResultStats=CommandResultStats_new(heap);
    if(!entity->cmdResultStats)goto done;
    entity->worldObj=world;MCObjectHeap_touch(heap);
    if(!dependencies->setPosition||!effect(entity,dependencies->setPosition(context,entity,0,0,0)))goto done;
    if(world) {
        int32_t dimension;
        if(!dependencies->getDimensionId||!effect(entity,dependencies->getDimensionId(context,world,&dimension)))goto done;
        entity->dimension=dimension;MCObjectHeap_touch(heap);
    }
    entity->dataWatcher=DataWatcher_new(heap,(MCObject *)entity,dependencies->watcher?dependencies->watcher:&inherited_watcher,context);
    if(!entity->dataWatcher)goto done;
    MCObjectHeap_touch(heap);
    MCObject *value=DataWatcher_boxByte(heap,0);
    if(!value||!DataWatcher_addObject(entity->dataWatcher,0,value))goto done;
    value=DataWatcher_boxShort(heap,300);
    if(!value||!DataWatcher_addObject(entity->dataWatcher,1,value))goto done;
    value=DataWatcher_boxByte(heap,0);
    if(!value||!DataWatcher_addObject(entity->dataWatcher,3,value))goto done;
    NBTString *empty=NBTString_literalASCII(heap,"");
    if(!empty||!DataWatcher_addObject(entity->dataWatcher,2,(MCObject *)empty))goto done;
    value=DataWatcher_boxByte(heap,0);
    if(!value||!DataWatcher_addObject(entity->dataWatcher,4,value))goto done;
    ok=dependencies->entityInit&&effect(entity,dependencies->entityInit(context,entity));
done:
    if(!ok)MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap);
}

void Entity_onDataWatcherUpdate(MCObject *entity, int32_t dataID) {
    (void)entity;
    (void)dataID;
}
