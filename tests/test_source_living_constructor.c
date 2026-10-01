#include "entity/EntityLivingBase.h"
#include "util/CombatTracker.h"
#include "util/MCGameplayPlayer.h"
#include "entity/SharedMonsterAttributes.h"
#include "util/MathHelper.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"living constructor check %u line %d: %s\n",checks,__LINE__,#x);exit(1);}} while(0)
/* Break caught: the real CombatTracker constructor must allocate one empty
   managed list and retain the passed fighter, including the legal null case. */
static void tracker_constructor(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    CombatTracker *a=CombatTracker_new(heap,NULL);CHECK(a);
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,heap,(MCObject *)a));
    CombatTracker *b=CombatTracker_new(heap,NULL);CHECK(b&&a!=b);
    CHECK(a->fighter==NULL&&a->combatEntries&&a->combatEntries!=b->combatEntries);
    CHECK(a->combatEntries->size==0&&a->combatEntries->modCount==0);
    CHECK(a->combatEntries->elementData&&a->combatEntries->elementData->length==0);
    CHECK(a->combatEntries->elementData==b->combatEntries->elementData);
    CHECK(a->field_94555_c==0&&a->field_152775_d==0&&a->field_152776_e==0);
    CHECK(!a->field_94552_d&&!a->field_94553_e&&!a->field_94551_f);
    MCObjectHeap *copy=MCObjectHeap_clone(heap);CHECK(copy);
    MCObjectRoot copied={0};CHECK(MCObjectRoot_rebind(&copied,copy,&root));
    CombatTracker *other=(CombatTracker *)MCObjectRoot_get(&copied);CHECK(other&&other!=a);
    CHECK(other->combatEntries!=a->combatEntries&&other->combatEntries->elementData!=a->combatEntries->elementData);
    CHECK(MCObjectHeap_collect(copy)&&other->combatEntries->elementData->length==0);
    MCObjectRoot_drop(&copied);MCObjectHeap_free(copy);MCObjectRoot_drop(&root);MCObjectHeap_free(heap);
}
enum {POSITION=1,BOUNDS,DIMENSION,INIT,APPLY,GETMAP,GETATTRIBUTE,SETHEALTH,NOTIFY,MATH};
typedef struct {
    MCObject object;NativeJavaRandomRuntime *runtime;NativeEntityIDRuntime *ids;
    NativeJavaRandomState mathState;unsigned events[64],count,failAt,mathCount;
    int32_t dimension;bool playerAttributes,swapWatcher,mutateInit,exhaustAfterInit;
    EntityLivingBase *last;DataWatcher *replacement;BaseAttributeMap *earlyMap;
} Witness;
static void witness_trace(MCObject *o,MCObjectVisitor visitor,void *context) {
    Witness *w=(Witness *)o;w->last=(EntityLivingBase *)visitor((MCObject *)w->last,context);
    w->replacement=(DataWatcher *)visitor((MCObject *)w->replacement,context);
    w->earlyMap=(BaseAttributeMap *)visitor((MCObject *)w->earlyMap,context);
}
static const MCObjectClass witness_class={"fixture.Living.constructor",MCObjectHeap_plainClone,witness_trace,NULL};
static bool event(Witness *w,unsigned id) {
    CHECK(w->count<64);w->events[w->count++]=id;return !w->failAt||w->count!=w->failAt;
}
static bool clock_nano(void *context,int64_t *out){(void)context;*out=123;return true;}
static const NativeJavaRandomRuntimeDependencies clock_methods={clock_nano};
static bool position(MCObject *context,Entity *e,double x,double y,double z) {
    return event((Witness *)context,POSITION)&&Entity_setPosition(e,x,y,z);
}
static bool bounds(MCObject *context,Entity *e,AxisAlignedBB *box) {
    return event((Witness *)context,BOUNDS)&&Entity_setEntityBoundingBox(e,box);
}
static bool dimension(MCObject *context,MCObject *world,int32_t *out) {
    Witness *w=(Witness *)context;CHECK(context==world);
    if(!event(w,DIMENSION))return false;
    *out=w->dimension;return true;
}
static bool watcher_update(MCObject *context,MCObject *owner,int32_t id) {
    Witness *w=(Witness *)context;CHECK(owner==(MCObject *)w->last&&id==6);
    return event(w,NOTIFY);
}
static const DataWatcherDependencies watcher_methods={.onDataWatcherUpdate=watcher_update};
static bool init(MCObject *context,Entity *entity) {
    Witness *w=(Witness *)context;EntityLivingBase *l=(EntityLivingBase *)entity;
    if(!event(w,INIT))return false;
    CHECK(l->attributeMap==NULL&&l->_combatTracker==NULL&&l->activePotionsMap==NULL&&l->previousEquipment==NULL);
    CHECK(l->maxHurtResistantTime==0&&l->jumpMovementFactor==0&&!l->potionsNeedUpdate);
    CHECK(entity->rand&&entity->entityUniqueID&&entity->cmdResultStats&&entity->worldObj==context);
    CHECK(WatchableObjectList_size(DataWatcher_getAllWatched(entity->dataWatcher))==5);
    CHECK(EntityLivingBase_entityInit(l));
    CHECK(WatchableObjectList_size(DataWatcher_getAllWatched(entity->dataWatcher))==9);
    CHECK(DataWatcher_getWatchableObjectInt(entity->dataWatcher,7)==0&&DataWatcher_getWatchableObjectByte(entity->dataWatcher,8)==0&&DataWatcher_getWatchableObjectByte(entity->dataWatcher,9)==0);
    CHECK(EntityLivingBase_getHealth(l)==1);
    if(w->mutateInit) {
        /* A real source virtual hook can write future superclass fields.
           Only declared initializers may overwrite those earlier writes. */
        ServersideAttributeMap *map=ServersideAttributeMap_new(context->heap);if(!map)return false;
        w->earlyMap=&map->base;l->attributeMap=w->earlyMap;
        l->scoreValue=23;l->maxHurtResistantTime=-7;l->jumpMovementFactor=-3;l->potionsNeedUpdate=false;
        MCObjectHeap_touch(context->heap);
    }
    if(w->exhaustAfterInit) {
        /* A virtual hook can mutate later-declared fields before their
           initializers. Failed RHS allocation must not erase those refs. */
        l->_combatTracker=CombatTracker_new(context->heap,l);CHECK(l->_combatTracker);
        l->previousEquipment=ItemStackArray_new(context->heap,3);CHECK(l->previousEquipment);
        size_t remaining=16*1024*1024-MCObjectHeap_liveBytes(context->heap);
        CHECK(remaining>=sizeof(MCObject));
        CHECK(MCObjectHeap_alloc(context->heap,remaining,&witness_class));
        MCObjectHeap_touch(context->heap);
    }
    return true;
}
static const EntityDependencies entity_methods={.entityInit=init,.setPosition=position,.setEntityBoundingBox=bounds,
    .getDimensionId=dimension,.watcher=&watcher_methods};
static BaseAttributeMap *get_map(MCObject *context,EntityLivingBase *l) {
    if(!event((Witness *)context,GETMAP))return NULL;
    return EntityLivingBase_getAttributeMap(l);
}
static IAttributeInstance *get_attribute(MCObject *context,EntityLivingBase *l,IAttribute *attribute) {
    Witness *w=(Witness *)context;if(!event(w,GETATTRIBUTE))return NULL;
    if(w->swapWatcher){l->entity.dataWatcher=w->replacement;MCObjectHeap_touch(context->heap);}
    return EntityLivingBase_getEntityAttribute(l,attribute);
}
static bool apply(MCObject *context,EntityLivingBase *l) {
    Witness *w=(Witness *)context;if(!event(w,APPLY))return false;
    CHECK(l->_combatTracker&&l->_combatTracker->fighter==l&&l->activePotionsMap&&l->previousEquipment);
    CHECK(l->_combatTracker->combatEntries->size==0&&l->activePotionsMap->size==0&&l->activePotionsMap->table==NULL&&l->activePotionsMap->loadFactor==0.75f);
    CHECK(l->previousEquipment->length==5&&l->maxHurtResistantTime==20&&l->jumpMovementFactor==0.02f&&l->potionsNeedUpdate);
    for(int i=0;i<5;i++)CHECK(l->previousEquipment->items[i]==NULL);
    CHECK(!l->entity.preventEntitySpawning&&l->randomUnused1==0&&l->randomUnused2==0&&l->rotationYawHead==0);
    if(!EntityLivingBase_applyEntityAttributes(l))return false;
    if(w->playerAttributes) {
        SharedMonsterAttributes *s=SharedMonsterAttributes_get(context->heap);CHECK(s);
        IAttributeInstance *a=BaseAttributeMap_registerAttribute(get_map(context,l),s->attackDamage);
        if(!a||!IAttributeInstance_setBaseValue(a,1))return false;
        a=get_attribute(context,l,s->movementSpeed);
        if(!a||!IAttributeInstance_setBaseValue(a,0.10000000149011612))return false;
    }
    return true;
}
static bool set_health(MCObject *context,EntityLivingBase *l,float value) {
    return event((Witness *)context,SETHEALTH)&&EntityLivingBase_setHealth(l,value);
}
static bool math_random(MCObject *context,double *out) {
    Witness *w=(Witness *)context;if(!event(w,MATH))return false;
    ++w->mathCount;return NativeJavaRandomState_nextDouble(&w->mathState,out);
}
static const EntityLivingBaseDependencies living_methods={apply,get_map,get_attribute,set_health,math_random};
static Witness *setup(MCObjectHeap *heap) {
    Witness *w=(Witness *)MCObjectHeap_alloc(heap,sizeof *w,&witness_class);CHECK(w);
    w->runtime=NativeJavaRandomRuntime_new(&clock_methods,NULL);w->ids=NativeEntityIDRuntime_new(100);
    CHECK(w->runtime&&w->ids&&NativeJavaRandomState_setSeed(&w->mathState,0));w->dimension=42;return w;
}
static void release(Witness *w) {CHECK(NativeJavaRandomRuntime_free(w->runtime)&&NativeEntityIDRuntime_free(w->ids));}
static EntityLivingBase *allocate(MCObjectHeap *heap,Witness *w) {
    MCGameplayPlayer *p=MCGameplayPlayer_nativeAllocate(heap);CHECK(p);
    w->last=(EntityLivingBase *)p;MCObjectHeap_touch(heap);return w->last;
}
static bool construct(Witness *w,EntityLivingBase *l) {
    return EntityLivingBase_construct(l,(MCObject *)w,&entity_methods,(MCObject *)w,&living_methods,(MCObject *)w,w->runtime,w->ids);
}
static uint32_t float_bits(float f){uint32_t bits;memcpy(&bits,&f,sizeof bits);return bits;}
static void living_constructor(void) {
    MCObjectHeap *heap=MCObjectHeap_new(16*1024*1024);CHECK(heap);Witness *w=setup(heap);
    EntityLivingBase *l=allocate(heap,w);MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,heap,(MCObject *)l));
    CHECK(construct(w,l));
    CHECK(l->entity.entityId==100&&l->entity.dimension==42&&l->entity.preventEntitySpawning&&l->entity.stepHeight==0.6f);
    CHECK(EntityLivingBase_getHealth(l)==20&&EntityLivingBase_getMaxHealth(l)==20&&w->mathCount==3);
    /* Numeric facts from the unchanged original constructor with Math seed0. */
    CHECK(l->rotationYawHead==l->entity.rotationYaw&&float_bits(l->randomUnused1)==UINT32_C(0x3c8dcd06));
    CHECK(float_bits(l->randomUnused2)==UINT32_C(0x453a62bb)&&float_bits(l->rotationYawHead)==UINT32_C(0x4080290f));
    CHECK(l->entity.boundingBox&&l->entity.boundingBox->minX==-(double)(0.6f/2));
    SharedMonsterAttributes *s=SharedMonsterAttributes_get(heap);CHECK(s);
    CHECK(IAttributeInstance_getAttribute(BaseAttributeMap_getAttributeInstance(l->attributeMap,s->maxHealth))==s->maxHealth);
    CHECK(EntityLivingBase_getAttributeMap(l)==l->attributeMap);
    MCGameplayPlayer *referenced=MCGameplayPlayer_nativeAllocate(heap);CHECK(referenced);
    l->attackingPlayer=referenced;l->entityLivingToAttack=&referenced->living;l->lastAttacker=&referenced->living;
    MCObjectHeap_touch(heap);
    MCObjectHeap *copy=MCObjectHeap_clone(heap);CHECK(copy);MCObjectRoot copied={0};CHECK(MCObjectRoot_rebind(&copied,copy,&root));
    EntityLivingBase *other=(EntityLivingBase *)MCObjectRoot_get(&copied);CHECK(other&&other!=l&&other->_combatTracker->fighter==other);
    CHECK(other->previousEquipment!=l->previousEquipment&&other->attributeMap!=l->attributeMap);
    CHECK(other->attackingPlayer&&other->attackingPlayer!=referenced&&other->attackingPlayer->living.entity.object.heap==copy);
    CHECK(other->entityLivingToAttack==&other->attackingPlayer->living&&other->lastAttacker==other->entityLivingToAttack);
    CHECK(MCObjectHeap_collect(copy)&&EntityLivingBase_getHealth(other)==20);
    MCObjectRoot_drop(&copied);MCObjectHeap_free(copy);release(w);MCObjectRoot_drop(&root);MCObjectHeap_free(heap);
}
static void initializer_allocation_failure(void) {
    MCObjectHeap *heap=MCObjectHeap_new(16*1024*1024);CHECK(heap);Witness *w=setup(heap);
    w->exhaustAfterInit=true;EntityLivingBase *l=allocate(heap,w);
    CHECK(!construct(w,l)&&MCObjectHeap_failed(heap));
    CHECK(l->_combatTracker&&l->_combatTracker->fighter==l&&l->previousEquipment&&l->previousEquipment->length==3);
    CHECK(l->activePotionsMap==NULL&&l->maxHurtResistantTime==0&&l->jumpMovementFactor==0&&!l->potionsNeedUpdate);
    CHECK(w->count==4&&w->mathCount==0&&!l->entity.preventEntitySpawning);
    release(w);MCObjectHeap_free(heap);
}
/* Break caught: a failed invoked dependency must stop at the original prefix;
   graph fields already written and external ID/Math effects are not undone. */
static void constructor_failures(void) {
    unsigned baseline[64],count;
    MCObjectHeap *heap=MCObjectHeap_new(16*1024*1024);CHECK(heap);Witness *w=setup(heap);
    EntityLivingBase *l=allocate(heap,w);CHECK(construct(w,l));count=w->count;CHECK(count==19);
    memcpy(baseline,w->events,count*sizeof *baseline);release(w);MCObjectHeap_free(heap);
    for(unsigned fail=1;fail<=count;fail++) {
        heap=MCObjectHeap_new(16*1024*1024);CHECK(heap);w=setup(heap);w->failAt=fail;l=allocate(heap,w);
        CHECK(!construct(w,l)&&MCObjectHeap_failed(heap)&&w->count==fail);
        CHECK(memcmp(baseline,w->events,fail*sizeof *baseline)==0);
        int32_t next;CHECK(NativeEntityIDRuntime_next(w->ids,&next)&&next==101);
        CHECK(l->entity.entityId==100);
        if(fail<=4)CHECK(!l->_combatTracker&&!l->activePotionsMap&&!l->previousEquipment);
        else CHECK(l->_combatTracker&&l->_combatTracker->fighter==l&&l->maxHurtResistantTime==20);
        if(fail<=14)CHECK(!l->entity.preventEntitySpawning&&w->mathCount==0);
        else CHECK(l->entity.preventEntitySpawning);
        if(fail<=15)CHECK(l->randomUnused1==0);
        else CHECK(float_bits(l->randomUnused1)==UINT32_C(0x3c8dcd06));
        if(fail<=18)CHECK(l->randomUnused2==0);
        else CHECK(float_bits(l->randomUnused2)==UINT32_C(0x453a62bb));
        CHECK(l->entity.rotationYaw==0&&l->rotationYawHead==0&&l->entity.stepHeight==0);
        release(w);MCObjectHeap_free(heap);
    }
}
static float from_bits(uint32_t bits){float f;memcpy(&f,&bits,sizeof f);return f;}
/* Break caught: health uses the live actual max-health attribute, exact float
   clamp, original boxed-float semantics, and the evaluated watcher receiver. */
static void health_and_initializer_aliases(void) {
    MCObjectHeap *heap=MCObjectHeap_new(16*1024*1024);CHECK(heap);Witness *w=setup(heap);
    w->mutateInit=true;w->playerAttributes=true;EntityLivingBase *l=allocate(heap,w);CHECK(construct(w,l));
    CHECK(l->attributeMap==w->earlyMap&&l->scoreValue==23&&l->maxHurtResistantTime==20&&l->jumpMovementFactor==0.02f&&l->potionsNeedUpdate);
    SharedMonsterAttributes *s=SharedMonsterAttributes_get(heap);CHECK(s);
    CHECK(IAttributeInstance_getAttributeValue(BaseAttributeMap_getAttributeInstance(l->attributeMap,s->attackDamage))==1);
    CHECK(IAttributeInstance_getAttributeValue(BaseAttributeMap_getAttributeInstance(l->attributeMap,s->movementSpeed))==0.10000000149011612);
    IAttributeInstance *max=BaseAttributeMap_getAttributeInstance(l->attributeMap,s->maxHealth);CHECK(max);
    const float maximums[]={0,7.25f,20,1024};
    const uint32_t input[]={0xff800000,0x80000001,0x80000000,0,1,0x40e80000,0x41a00000,0x7f800000,0x7fc01234,0xffc05678};
    for(unsigned m=0;m<sizeof maximums/sizeof *maximums;m++)for(unsigned i=0;i<sizeof input/sizeof *input;i++) {
        w->count=0;CHECK(IAttributeInstance_setBaseValue(max,maximums[m]));
        CHECK(EntityLivingBase_setHealth(l,1));w->count=0;
        float value=from_bits(input[i]);CHECK(EntityLivingBase_setHealth(l,value));
        uint32_t expected=input[i];
        if(!isnan(value)){if(value<0)expected=0;else if(value>maximums[m])expected=float_bits(maximums[m]);}
        CHECK(float_bits(EntityLivingBase_getHealth(l))==expected);
    }
    w->count=0;CHECK(IAttributeInstance_setBaseValue(max,20));CHECK(EntityLivingBase_setHealth(l,1));
    DataWatcher *original=l->entity.dataWatcher;
    w->replacement=DataWatcher_new(heap,(MCObject *)l,&watcher_methods,(MCObject *)w);CHECK(w->replacement);
    l->entity.dataWatcher=w->replacement;CHECK(EntityLivingBase_entityInit(l));l->entity.dataWatcher=original;
    w->swapWatcher=true;w->count=0;CHECK(EntityLivingBase_setHealth(l,7.25f));
    CHECK(l->entity.dataWatcher==w->replacement&&DataWatcher_getWatchableObjectFloat(original,6)==7.25f&&EntityLivingBase_getHealth(l)==1);
    release(w);MCObjectHeap_free(heap);
}
static void invalid_watcher_ownership(void) {
    for(unsigned mode=0;mode<2;mode++) {
        MCObjectHeap *heap=MCObjectHeap_new(16*1024*1024);CHECK(heap);Witness *w=setup(heap);
        EntityLivingBase *l=allocate(heap,w);CHECK(construct(w,l));w->count=0;
        MCObjectHeap *foreign=MCObjectHeap_new(1024*1024);CHECK(foreign);
        /* The foreign watcher is never invoked; its actual constructor still
           receives the required immutable method table. */
        DataWatcher *other=DataWatcher_new(foreign,NULL,&watcher_methods,NULL);CHECK(other);
        l->entity.dataWatcher=mode?other:NULL;
        CHECK(!EntityLivingBase_setHealth(l,3)&&MCObjectHeap_failed(heap));
        /* Source captures the watcher first, but evaluates the max-health
           argument before the failing update invocation. */
        CHECK(w->count==2&&w->events[0]==GETATTRIBUTE&&w->events[1]==GETMAP);
        CHECK(!MCObjectHeap_failed(foreign));release(w);MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
    }
}
static void nullable_attribute_and_health_failure(void) {
    MCObjectHeap *heap=MCObjectHeap_new(16*1024*1024);CHECK(heap);Witness *w=setup(heap);
    EntityLivingBase *l=allocate(heap,w);CHECK(construct(w,l));w->count=0;
    SharedMonsterAttributes *s=SharedMonsterAttributes_get(heap);CHECK(s);
    CHECK(EntityLivingBase_getEntityAttribute(l,s->followRange)==NULL&&!MCObjectHeap_failed(heap));
    CHECK(w->count==1&&w->events[0]==GETMAP);
    l->entity.dataWatcher=NULL;
    CHECK(EntityLivingBase_getHealth(l)==0&&MCObjectHeap_failed(heap));
    release(w);MCObjectHeap_free(heap);
}
int main(void) {tracker_constructor();living_constructor();constructor_failures();initializer_allocation_failure();health_and_initializer_aliases();invalid_watcher_ownership();nullable_attribute_and_health_failure();printf("source Living constructor: %u checks GREEN\n",checks);return 0;}
