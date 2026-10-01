#include "entity/item/EntityItem.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"entity constructor check %u line %d: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
enum {POSITION=1,BOUNDS,DIMENSION,INIT,REMOTE,MOVE};
typedef struct {
    MCObject object;int32_t dimension;bool remote;unsigned events[64],count,failAt;
    EntityItem *last;AxisAlignedBB *zero;
    NativeJavaRandomRuntime *random;NativeEntityIDRuntime *ids;int64_t nano;
    const EntityDependencies *methods;
    bool leanInit;
} Witness;
static void witness_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    Witness *w=(Witness *)object;w->last=(EntityItem *)visitor((MCObject *)w->last,context);
    w->zero=(AxisAlignedBB *)visitor((MCObject *)w->zero,context);
}
static const MCObjectClass witness_class={"fixture.Entity.constructor",MCObjectHeap_plainClone,witness_trace,NULL};
static bool event(Witness *w,unsigned value) {CHECK(w->count<64);w->events[w->count++]=value;return w->failAt!=value;}
static bool nano(void *context,int64_t *out) {(void)context;*out=123;return true;}
static const NativeJavaRandomRuntimeDependencies random_clock={nano};
static bool position(MCObject *context,Entity *entity,double x,double y,double z) {
    Witness *w=(Witness *)context;
    if(!w->zero)w->zero=entity->boundingBox;
    if(entity->width==0.6f&&entity->boundingBox->minX==0&&entity->boundingBox->maxX==0)CHECK(entity->boundingBox==w->zero);
    if(!event(w,POSITION))return false;
    return Entity_setPosition(entity,x,y,z);
}
static bool bounds(MCObject *context,Entity *entity,AxisAlignedBB *box) {
    return event((Witness *)context,BOUNDS)&&Entity_setEntityBoundingBox(entity,box);
}
static bool dimension(MCObject *context,MCObject *world,int32_t *out) {
    Witness *w=(Witness *)context;CHECK(world==context);
    if(!event(w,DIMENSION))return false;
    *out=w->dimension;return true;
}
static bool remote(MCObject *context,MCObject *world,bool *out) {
    Witness *w=(Witness *)context;CHECK(world==context);
    if(!event(w,REMOTE))return false;
    *out=w->remote;return true;
}
static bool move(MCObject *context,Entity *entity,double x,double y,double z) {
    CHECK(y==0&&x==z);
    return event((Witness *)context,MOVE)&&Entity_setPosition(entity,entity->posX+x,entity->posY+y,entity->posZ+z);
}
static bool location(MCObject *context,Entity *entity,double x,double y,double z,float yaw,float pitch) {
    (void)context;return Entity_setLocationAndAngles(entity,x,y,z,yaw,pitch);
}
static bool init(MCObject *context,Entity *entity) {
    Witness *w=(Witness *)context;if(!event(w,INIT))return false;
    EntityItem *item=(EntityItem *)entity;
    if(w->leanInit)return EntityItem_entityInit(item);
    CHECK(entity->renderDistanceWeight==1&&entity->width==0.6f&&entity->height==1.8f);
    CHECK(entity->nextStepDistance==1&&entity->fireResistance==1&&entity->firstUpdate);
#define ZERO(field) CHECK(entity->field==0)
    ZERO(preventEntitySpawning);ZERO(riddenByEntity);ZERO(ridingEntity);ZERO(forceSpawn);
    ZERO(prevPosX);ZERO(prevPosY);ZERO(prevPosZ);ZERO(motionX);ZERO(motionY);ZERO(motionZ);
    ZERO(rotationYaw);ZERO(rotationPitch);ZERO(prevRotationYaw);ZERO(prevRotationPitch);
    ZERO(onGround);ZERO(isCollidedHorizontally);ZERO(isCollidedVertically);ZERO(isCollided);ZERO(velocityChanged);ZERO(isInWeb);ZERO(isOutsideBorder);ZERO(isDead);
    ZERO(prevDistanceWalkedModified);ZERO(distanceWalkedModified);ZERO(distanceWalkedOnStepModified);ZERO(fallDistance);
    ZERO(lastTickPosX);ZERO(lastTickPosY);ZERO(lastTickPosZ);ZERO(stepHeight);ZERO(noClip);ZERO(entityCollisionReduction);
    ZERO(ticksExisted);ZERO(fire);ZERO(inWater);ZERO(hurtResistantTime);ZERO(isImmuneToFire);ZERO(entityRiderPitchDelta);ZERO(entityRiderYawDelta);
    ZERO(addedToChunk);ZERO(chunkCoordX);ZERO(chunkCoordY);ZERO(chunkCoordZ);ZERO(serverPosX);ZERO(serverPosY);ZERO(serverPosZ);
    ZERO(ignoreFrustumCheck);ZERO(isAirBorne);ZERO(timeUntilPortal);ZERO(inPortal);ZERO(portalCounter);ZERO(lastPortalPos);ZERO(lastPortalVec);ZERO(teleportDirection);ZERO(invulnerable);
#undef ZERO
    CHECK(item->health==0&&item->hoverStart==0&&entity->ticksExisted==0&&!entity->isDead);
    CHECK(entity->rand&&entity->entityUniqueID&&entity->cmdResultStats&&entity->boundingBox!=w->zero);
    CHECK(entity->cmdResultStats->entitiesID==entity->cmdResultStats->objectives);
    CHECK(DataWatcher_getWatchableObjectByte(entity->dataWatcher,0)==0);
    CHECK(DataWatcher_getWatchableObjectShort(entity->dataWatcher,1)==300);
    CHECK(DataWatcher_getWatchableObjectByte(entity->dataWatcher,3)==0&&DataWatcher_getWatchableObjectByte(entity->dataWatcher,4)==0);
    CHECK(NBTString_equalsASCII(DataWatcher_getWatchableObjectString(entity->dataWatcher,2),""));
    CHECK(WatchableObjectList_size(DataWatcher_getAllWatched(entity->dataWatcher))==5);
    return EntityItem_entityInit(item);
}
static const EntityDependencies entity_methods={.entityInit=init,.setPosition=position,.setEntityBoundingBox=bounds,
    .getDimensionId=dimension,.isRemote=remote,.moveEntity=move,.setLocationAndAngles=location};
static bool base(MCObject *context,EntityItem *item,MCObject *world) {
    Witness *w=(Witness *)context;w->last=item;MCObjectHeap_touch(context->heap);
    CHECK(!item->entity.worldObj&&!item->entity.rand&&!item->entity.dataWatcher&&!item->entity.boundingBox&&!item->entity.cmdResultStats);
    return Entity_construct(&item->entity,world,w->methods,context,w->random,w->ids);
}
static double math_random(MCObject *context) {
    double out=0;if(!NativeJavaRandomRuntime_mathRandom(((Witness *)context)->random,&out))MCObjectHeap_fail(context->heap);return out;
}
static bool size_item(MCObject *context,EntityItem *item,float width,float height) {(void)context;return Entity_setSize(&item->entity,width,height);}
static bool position_item(MCObject *context,EntityItem *item,double x,double y,double z) {return position(context,&item->entity,x,y,z);}
static const EntityItemConstructorDependencies constructors={base,math_random,size_item,position_item};
/* These record-only dependencies belong to the fixture. No production stub is
   bound; this suite invokes constructor/metadata helpers rather than pickup. */
static bool log_item(MCObject *c,int32_t id){(void)c;(void)id;return true;}
static bool item_remote(MCObject *c,MCObject *world){(void)c;return ((Witness *)world)->remote;}
static InventoryPlayer *inventory(MCObject *c,MCObject *p){(void)c;(void)p;return NULL;}
static const NBTString *name(MCObject *c,MCObject *p){(void)p;return NBTString_literalASCII(c->heap,"Witness");}
static MCObject *find(MCObject *c,MCObject *w,const NBTString *n){(void)c;(void)w;(void)n;return NULL;}
static bool achievement(MCObject *c,MCObject *p,EntityItemAchievement a){(void)c;(void)p;(void)a;return true;}
static bool silent(MCObject *c,const EntityItem *e){(void)c;return Entity_isSilent((Entity *)&e->entity);}
static float next_float(MCObject *c,EntityItem *e){float out=0;if(!NativeJavaRandom_nextFloat(e->entity.rand,&out))MCObjectHeap_fail(c->heap);return out;}
static bool sound(MCObject *c,MCObject *w,MCObject *p,const char *n,float v,float t){(void)c;(void)w;(void)p;(void)n;(void)v;(void)t;return true;}
static bool pickup(MCObject *c,MCObject *p,EntityItem *e,int32_t n){(void)c;(void)p;(void)e;(void)n;return true;}
static bool dead(MCObject *c,EntityItem *e){(void)c;e->entity.isDead=true;return true;}
static const EntityItemDependencies item_methods={log_item,item_remote,inventory,name,find,achievement,silent,next_float,sound,pickup,dead};
static Witness *setup(MCObjectHeap *heap,int32_t counter) {
    Witness *w=(Witness *)MCObjectHeap_alloc(heap,sizeof *w,&witness_class);CHECK(w);
    w->random=NativeJavaRandomRuntime_new(&random_clock,NULL);w->ids=NativeEntityIDRuntime_new(counter);
    CHECK(w->random&&w->ids);w->dimension=-1;w->methods=&entity_methods;return w;
}
static void release(Witness *w) {CHECK(NativeJavaRandomRuntime_free(w->random)&&NativeEntityIDRuntime_free(w->ids));}
static uint64_t bits(double x){uint64_t out;memcpy(&out,&x,sizeof out);return out;}
static void constructors_and_helpers(void) {
    MCObjectHeap *heap=MCObjectHeap_new(4*1024*1024);CHECK(heap);Witness *w=setup(heap,INT32_MAX);
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,heap));
    EntityItem *world=EntityItem_new_world(heap,(MCObject *)w,(MCObject *)w,&item_methods,&constructors);CHECK(world);
    CHECK(world->entity.entityId==INT32_MAX&&world->entity.dimension==-1);
    CHECK(w->events[0]==POSITION&&w->events[1]==BOUNDS&&w->events[2]==DIMENSION&&w->events[3]==INIT&&w->events[4]==BOUNDS&&w->count==5);
    CHECK(world->entity.width==0.25f&&world->entity.height==0.25f&&world->entity.posX==0);
    double original=-(double)(0.6f/2.0f);
    CHECK(world->entity.boundingBox->minX==original&&world->entity.boundingBox->maxX==original+0.25);
    CHECK(EntityItem_getEntityItem(world)->stackSize==0&&EntityItem_getEntityItem(world)->item==ItemStack_registryItem(0));
    CHECK(Entity_getEntityBoundingBox(&world->entity)==world->entity.boundingBox&&Entity_getCommandStats(&world->entity)==world->entity.cmdResultStats);
    w->count=0;EntityItem *positioned=EntityItem_new_position(heap,NULL,(MCObject *)w,&item_methods,&constructors,1,2,3);CHECK(positioned);
    CHECK(positioned->entity.entityId==INT32_MIN&&positioned->entity.dimension==0);
    CHECK(w->events[0]==POSITION&&w->events[1]==BOUNDS&&w->events[2]==INIT&&w->count==6);
    CHECK(positioned->entity.boundingBox->minX==0.875&&positioned->entity.boundingBox->maxX==1.125&&positioned->entity.boundingBox->minY==2&&positioned->entity.boundingBox->maxY==2.25);
    CHECK(positioned->entity.cmdResultStats!=world->entity.cmdResultStats&&positioned->entity.cmdResultStats->entitiesID==world->entity.cmdResultStats->entitiesID);
    for(unsigned i=0;i<5;i++)CHECK(positioned->entity.cmdResultStats->entitiesID->items[i]==NULL);
    CHECK(Entity_getUniqueID(&positioned->entity)==positioned->entity.entityUniqueID&&Entity_getDataWatcher(&positioned->entity)==positioned->entity.dataWatcher);
    Entity_setEntityId(&positioned->entity,-7);CHECK(Entity_getEntityId(&positioned->entity)==-7);
    positioned->entity.worldObj=(MCObject *)w;positioned->entity.firstUpdate=false;w->count=0;
    CHECK(Entity_setSize(&positioned->entity,0.5f,0.75f));CHECK(w->count==4&&w->events[0]==BOUNDS&&w->events[1]==REMOTE&&w->events[2]==MOVE&&w->events[3]==BOUNDS);
    CHECK(positioned->entity.posX==0.75&&positioned->entity.posZ==2.75);
    w->remote=true;w->count=0;CHECK(Entity_setSize(&positioned->entity,0.6f,0.8f));CHECK(w->count==2&&w->events[1]==REMOTE);
    DataWatcherBlockPos *pos=DataWatcher_blockPos(heap,INT32_MIN,INT32_MAX,-1);CHECK(pos);
    CHECK(Entity_moveToBlockPosAndAngles(&positioned->entity,pos,30,-15));
    CHECK(positioned->entity.posX==(double)INT32_MIN+0.5&&positioned->entity.posY==(double)INT32_MAX&&positioned->entity.posZ==-0.5);
    CHECK(positioned->entity.lastTickPosX==positioned->entity.prevPosX&&positioned->entity.prevPosX==positioned->entity.posX&&positioned->entity.rotationYaw==30&&positioned->entity.rotationPitch==-15);
    CHECK(DataWatcher_updateObject(positioned->entity.dataWatcher,4,DataWatcher_boxByte(heap,1))&&Entity_isSilent(&positioned->entity));
    CHECK(DataWatcher_updateObject(positioned->entity.dataWatcher,4,DataWatcher_boxByte(heap,4))&&!Entity_isSilent(&positioned->entity));
    release(w);MCObjectRootScope_end(&scope);MCObjectHeap_free(heap);
}
static void axis_values(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    AxisAlignedBB *a=AxisAlignedBB_new(heap,3,2,1,-3,-2,-1);CHECK(a);
    CHECK(a->minX==-3&&a->maxX==3&&a->minY==-2&&a->maxY==2&&a->minZ==-1&&a->maxZ==1);
    a=AxisAlignedBB_new(heap,0.0,-0.0,INFINITY,-0.0,0.0,-INFINITY);CHECK(a);
    CHECK(bits(a->minX)==UINT64_C(0x8000000000000000)&&bits(a->maxX)==0&&bits(a->minY)==UINT64_C(0x8000000000000000)&&bits(a->maxY)==0);
    CHECK(isinf(a->minZ)&&signbit(a->minZ)&&isinf(a->maxZ)&&!signbit(a->maxZ));
    uint64_t raw=UINT64_C(0x7ff8000000001234);double nan;memcpy(&nan,&raw,sizeof nan);
    a=AxisAlignedBB_new(heap,nan,1,0,4,nan,0);CHECK(a);
    CHECK(bits(a->minX)==raw&&bits(a->maxX)==raw&&bits(a->minY)==raw&&bits(a->maxY)==raw);MCObjectHeap_free(heap);
}
static void failure_order(void) {
    for(unsigned fail=POSITION;fail<=INIT;fail++) {
        MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);Witness *w=setup(heap,77);w->failAt=fail;
        CHECK(!EntityItem_new_position(heap,(MCObject *)w,(MCObject *)w,&item_methods,&constructors,1,2,3));
        CHECK(MCObjectHeap_failed(heap)&&w->events[w->count-1]==fail&&w->last->health==0&&w->last->hoverStart==0);
        CHECK(w->last->entity.rand&&w->last->entity.entityUniqueID&&w->last->entity.cmdResultStats);
        int32_t next;CHECK(NativeEntityIDRuntime_next(w->ids,&next)&&next==78);
        CHECK(!MCObjectHeap_hasBorrowers(heap));release(w);MCObjectHeap_free(heap);
    }
}
static void graph_identity(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);Witness *w=setup(heap,10);
    EntityItem *a=EntityItem_new_position(heap,(MCObject *)w,(MCObject *)w,&item_methods,&constructors,1,2,3);CHECK(a);
    a->entity.ridingEntity=&a->entity;a->entity.riddenByEntity=&a->entity;
    a->entity.cmdResultStats->entitiesID->items[3]=NBTString_literalASCII(heap,"shared-command-slot");CHECK(a->entity.cmdResultStats->entitiesID->items[3]);
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,heap,(MCObject *)w));CHECK(MCObjectHeap_collect(heap));
    MCObjectHeap *copy=MCObjectHeap_clone(heap);CHECK(copy);MCObjectRoot branch={0};CHECK(MCObjectRoot_rebind(&branch,copy,&root));
    Witness *cw=(Witness *)MCObjectRoot_get(&branch);EntityItem *ca=cw->last;
    CHECK(cw!=w&&ca!=a&&ca->entity.entityContext==(MCObject *)cw&&ca->entity.worldObj==(MCObject *)cw);
    CHECK(ca->entity.boundingBox!=a->entity.boundingBox&&ca->entity.rand!=a->entity.rand&&ca->entity.entityUniqueID!=a->entity.entityUniqueID);
    CHECK(ca->entity.ridingEntity==&ca->entity&&ca->entity.riddenByEntity==&ca->entity);
    CHECK(ca->entity.cmdResultStats->entitiesID==ca->entity.cmdResultStats->objectives&&ca->entity.cmdResultStats->entitiesID!=a->entity.cmdResultStats->entitiesID);
    CHECK(cw->zero!=w->zero&&cw->random==w->random&&cw->ids==w->ids);
    cw->count=0;EntityItem *b=EntityItem_new_position(copy,(MCObject *)cw,(MCObject *)cw,&item_methods,&constructors,4,5,6);CHECK(b&&b->entity.entityId==11);
    CHECK(b->entity.cmdResultStats->entitiesID==ca->entity.cmdResultStats->entitiesID);MCObjectHeap_free(copy);
    int32_t id;CHECK(NativeEntityIDRuntime_next(w->ids,&id)&&id==12);CHECK(a->entity.entityId==10);
    CHECK(MCObjectHeap_collect(heap));release(w);MCObjectHeap_free(heap);
}
static void branch_dependencies(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);Witness *w=setup(heap,20);
    EntityDependencies limited=entity_methods;limited.getDimensionId=NULL;limited.isRemote=NULL;limited.moveEntity=NULL;
    w->methods=&limited;
    EntityItem *a=EntityItem_new_position(heap,NULL,(MCObject *)w,&item_methods,&constructors,1,2,3);CHECK(a);
    CHECK(Entity_setSize(&a->entity,0.2f,0.3f));
    CHECK(Entity_setSize(&a->entity,0.3f,0.4f)); /* firstUpdate short-circuits world */
    a->entity.firstUpdate=false;CHECK(!Entity_setSize(&a->entity,0.4f,0.5f));
    CHECK(MCObjectHeap_failed(heap)&&a->entity.width==0.4f&&a->entity.boundingBox->maxX-a->entity.boundingBox->minX==(double)0.4f);
    CHECK(!MCObjectHeap_hasBorrowers(heap));release(w);MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(1024*1024);CHECK(heap);w=setup(heap,30);limited=entity_methods;limited.entityInit=NULL;w->methods=&limited;
    CHECK(!EntityItem_new_world(heap,(MCObject *)w,(MCObject *)w,&item_methods,&constructors));
    CHECK(w->last->entity.dataWatcher&&w->count==3&&w->events[2]==DIMENSION);
    /* The failed heap forbids inspection getters; retained source fields still
       prove that failure occurred after Watcher assignment, before subclass. */
    CHECK(w->last->health==0&&w->last->hoverStart==0&&MCObjectHeap_failed(heap));
    CHECK(!MCObjectHeap_hasBorrowers(heap));release(w);MCObjectHeap_free(heap);
}
static void allocation_prefix(void) {
    unsigned beforeId=0,afterId=0,succeeded=0;
    for(size_t budget=512;budget<=8192;budget+=128) {
        MCObjectHeap *heap=MCObjectHeap_new(budget);CHECK(heap);Witness *w=setup(heap,100);w->leanInit=true;
        EntityItem *item=EntityItem_new_position(heap,(MCObject *)w,(MCObject *)w,&item_methods,&constructors,1,2,3);
        bool idConsumed=w->last&&w->last->entity.renderDistanceWeight==1.0;
        int32_t next;CHECK(NativeEntityIDRuntime_next(w->ids,&next)&&next==(idConsumed?101:100));
        if(item){++succeeded;CHECK(!MCObjectHeap_failed(heap)&&item->health==5&&item->entity.dataWatcher);}
        else {
            CHECK(MCObjectHeap_failed(heap));if(idConsumed)++afterId;else++beforeId;
            if(w->last&&!w->last->entity.rand)CHECK(w->last->entity.fireResistance==0&&!w->last->entity.firstUpdate);
            if(w->last&&!w->last->entity.entityUniqueID)CHECK(!w->last->entity.cmdResultStats&&!w->last->entity.dataWatcher);
        }
        CHECK(!MCObjectHeap_hasBorrowers(heap));release(w);MCObjectHeap_free(heap);
    }
    CHECK(beforeId>0&&afterId>0&&succeeded>0);
}
static void invalid_watcher(void) {
    for(unsigned mode=0;mode<2;mode++) {
        MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);Witness *w=setup(heap,0);
        EntityItem *e=EntityItem_nativeNew(heap,(MCObject *)w,(MCObject *)w,&item_methods);CHECK(e);
        MCObjectHeap *foreign=NULL;Witness *other=NULL;
        if(mode) {
            foreign=MCObjectHeap_new(1024*1024);CHECK(foreign);other=setup(foreign,0);
            EntityItem *foreignEntity=EntityItem_new_world(foreign,(MCObject *)other,(MCObject *)other,&item_methods,&constructors);CHECK(foreignEntity);
            e->entity.dataWatcher=foreignEntity->entity.dataWatcher;
        }
        CHECK(!Entity_isSilent(&e->entity)&&MCObjectHeap_failed(heap)&&!MCObjectHeap_hasBorrowers(heap));
        if(foreign){CHECK(!MCObjectHeap_failed(foreign));release(other);MCObjectHeap_free(foreign);}
        release(w);MCObjectHeap_free(heap);
    }
}
static void watcher_flags(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);Witness *w=setup(heap,0);
    EntityItem *item=EntityItem_new_world(heap,(MCObject *)w,(MCObject *)w,&item_methods,&constructors);CHECK(item);
    Entity *e=&item->entity;
    CHECK(!Entity_isSilent(e)&&!Entity_isSneaking(e));
    CHECK(Entity_setSilent(e,true)&&Entity_isSilent(e));
    CHECK(Entity_setSilent(e,false)&&!Entity_isSilent(e));
    CHECK(Entity_setSneaking(e,true)&&Entity_isSneaking(e));
    CHECK(DataWatcher_getWatchableObjectByte(e->dataWatcher,0)==2);
    CHECK(Entity_setFlag(e,7,true)&&DataWatcher_getWatchableObjectByte(e->dataWatcher,0)==-126);
    CHECK(Entity_getFlag(e,-1)&&Entity_getFlag(e,31)&&Entity_getFlag(e,63));
    CHECK(Entity_getFlag(e,8)&&!Entity_getFlag(e,32));
    CHECK(Entity_setFlag(e,31,false)&&DataWatcher_getWatchableObjectByte(e->dataWatcher,0)==-126);
    CHECK(Entity_setFlag(e,39,false)&&DataWatcher_getWatchableObjectByte(e->dataWatcher,0)==2);
    CHECK(Entity_setSneaking(e,false)&&!Entity_isSneaking(e));
    CHECK(!MCObjectHeap_failed(heap)&&!MCObjectHeap_hasBorrowers(heap));release(w);MCObjectHeap_free(heap);
}
int main(void){watcher_flags();constructors_and_helpers();axis_values();failure_order();graph_identity();branch_dependencies();allocation_prefix();invalid_watcher();printf("source Entity constructor: %u checks passed\n",checks);return 0;}
