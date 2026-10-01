#include "entity/EntityLivingBase.h"
#include "entity/SharedMonsterAttributes.h"
#include "entity/ai/attributes/ServersideAttributeMap.h"
#include "entity/ai/attributes/AttributeModifier.h"
#include "util/CombatTracker.h"
#include "util/MCGameplayPlayer.h"
#include "util/MathHelper.h"

static void statics_trace(MCObject *object, MCObjectVisitor visitor,
                          void *context) {
  EntityLivingBaseStaticFields *fields = (EntityLivingBaseStaticFields *)object;
  fields->sprintingSpeedBoostModifierUUID = (NativeJavaUUID *)visitor(
      (MCObject *)fields->sprintingSpeedBoostModifierUUID, context);
  fields->sprintingSpeedBoostModifier = (AttributeModifier *)visitor(
      (MCObject *)fields->sprintingSpeedBoostModifier, context);
}
static const MCObjectClass statics_class = {"native.EntityLivingBaseStatics",
                                            MCObjectHeap_plainClone,
                                            statics_trace, NULL};
static bool statics_size(const MCObject *object, void *context) {
  (void)context;
  return MCObjectHeap_objectSize(object) >=
         sizeof(EntityLivingBaseStaticFields);
}
const EntityLivingBaseStaticFields *
EntityLivingBase_getStaticFields(MCObjectHeap *heap) {
  if (!heap || MCObjectHeap_failed(heap))
    return NULL;
  EntityLivingBaseStaticFields *fields =
      (EntityLivingBaseStaticFields *)MCObjectHeap_findObject(
          heap, &statics_class, statics_size, NULL);
  if (fields)
    return fields;
  MCObjectRootScope scope = {0};
  if (!MCObjectRootScope_begin(&scope, heap))
    return NULL;
  fields = (EntityLivingBaseStaticFields *)MCObjectHeap_alloc(
      heap, sizeof *fields, &statics_class);
  if (!fields)
    goto fail;
  NBTString *id =
      NBTString_literalASCII(heap, "662A6B8D-DA3E-4C1C-8813-96EA6097278D");
  fields->sprintingSpeedBoostModifierUUID =
      id ? NativeJavaUUID_fromString(heap, id) : NULL;
  if (!fields->sprintingSpeedBoostModifierUUID)
    goto fail;
  NBTString *name = NBTString_literalASCII(heap, "Sprinting speed boost");
  AttributeModifier *modifier =
      name
          ? AttributeModifier_new(heap, fields->sprintingSpeedBoostModifierUUID,
                                  name, 0.30000001192092896, 2)
          : NULL;
  if (!modifier)
    goto fail;
  fields->sprintingSpeedBoostModifier =
      AttributeModifier_setSaved(modifier, false);
  if (!fields->sprintingSpeedBoostModifier)
    goto fail;
  /* Permanent native root models this initialized Java class's static refs.
     The complete native heap snapshot preserves both edges and aliases. */
  MCObjectRoot root = {0};
  if (!MCObjectRoot_init(&root, heap, (MCObject *)fields))
    goto fail;
  MCObjectRootScope_end(&scope);
  return fields;
fail:
  MCObjectHeap_fail(heap);
  MCObjectRootScope_end(&scope);
  return NULL;
}

bool EntityLivingBase_isInstance(const MCObject *object) {
    return MCGameplayPlayer_isInstance(object)&&MCObjectHeap_objectSize(object)>=sizeof(EntityLivingBase);
}
static bool valid(EntityLivingBase *self) {
    if(!EntityLivingBase_isInstance((MCObject *)self)) {
        MCObjectHeap_fail(self?self->entity.object.heap:NULL);return false;
    }
    return !MCObjectHeap_failed(self->entity.object.heap);
}
static bool begin(EntityLivingBase *self,MCObjectRootScope *scope) {
    if(!valid(self)||!MCObjectRootScope_begin(scope,self->entity.object.heap))return false;
    if(MCObjectRootScope_pin(scope,(MCObject *)self))return true;
    MCObjectRootScope_end(scope);return false;
}
static bool effect(EntityLivingBase *self,bool completed) {
    if(!completed)MCObjectHeap_fail(self->entity.object.heap);
    return completed&&!MCObjectHeap_failed(self->entity.object.heap);
}
static bool valid_map(EntityLivingBase *self,BaseAttributeMap *map) {
    return effect(self,BaseAttributeMap_isInstance((MCObject *)map)&&map->object.heap==self->entity.object.heap);
}
static bool valid_attribute(EntityLivingBase *self,IAttributeInstance *attribute) {
    return effect(self,IAttributeInstance_isInstance((MCObject *)attribute)&&attribute->object.heap==self->entity.object.heap);
}
static bool valid_watcher(EntityLivingBase *self,DataWatcher *watcher) {
    return effect(self,DataWatcher_isInstance((MCObject *)watcher)&&((MCObject *)watcher)->heap==self->entity.object.heap);
}
static void potion_trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    LivingPotionMap *map=(LivingPotionMap *)object;
    map->table=visitor(map->table,context);map->entrySet=visitor(map->entrySet,context);
}
static const MCObjectClass potion_class={"native.EntityLivingBase.PotionHashMap",MCObjectHeap_plainClone,potion_trace,NULL};
bool LivingPotionMap_isInstance(const MCObject *object) {
    return object&&object->klass==&potion_class&&MCObjectHeap_objectSize(object)>=sizeof(LivingPotionMap);
}
static LivingPotionMap *potion_map_new(MCObjectHeap *heap) {
    LivingPotionMap *map=(LivingPotionMap *)MCObjectHeap_alloc(heap,sizeof *map,&potion_class);
    if(map)map->loadFactor=0.75f;
    return map;
}
void EntityLivingBase_traceFields(EntityLivingBase *self,MCObjectVisitor visitor,void *context) {
    Entity_traceFields(&self->entity,visitor,context);
    self->attributeMap=(BaseAttributeMap *)visitor((MCObject *)self->attributeMap,context);
    self->_combatTracker=(CombatTracker *)visitor((MCObject *)self->_combatTracker,context);
    self->activePotionsMap=(LivingPotionMap *)visitor((MCObject *)self->activePotionsMap,context);
    self->previousEquipment=(ItemStackArray *)visitor((MCObject *)self->previousEquipment,context);
    self->attackingPlayer=(MCGameplayPlayer *)visitor((MCObject *)self->attackingPlayer,context);
    self->entityLivingToAttack=(EntityLivingBase *)visitor((MCObject *)self->entityLivingToAttack,context);
    self->lastAttacker=(EntityLivingBase *)visitor((MCObject *)self->lastAttacker,context);
    self->livingContext=visitor(self->livingContext,context);
}

bool EntityLivingBase_construct(EntityLivingBase *self,MCObject *world,
    const EntityDependencies *entity,MCObject *entityContext,
    const EntityLivingBaseDependencies *living,MCObject *livingContext,
    NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    MCObjectRootScope scope={0};if(!begin(self,&scope))return false;
    MCObjectHeap *heap=self->entity.object.heap;bool ok=false;double draw;
    if(!EntityLivingBase_getStaticFields(heap))goto done;
    /* Native virtual dispatch must already be available when Entity invokes
       entityInit. These adapter fields are not Java declaration initializers. */
    if(!living||(livingContext&&livingContext->heap!=heap)||
       !MCObjectRootScope_pin(&scope,livingContext))goto done;
    self->livingDependencies=living;self->livingContext=livingContext;MCObjectHeap_touch(heap);
    if(!Entity_construct(&self->entity,world,entity,entityContext,random,ids))goto done;
    CombatTracker *tracker=CombatTracker_new(heap,self);if(!tracker)goto done;
    self->_combatTracker=tracker;MCObjectHeap_touch(heap);
    LivingPotionMap *potions=potion_map_new(heap);if(!potions)goto done;
    self->activePotionsMap=potions;MCObjectHeap_touch(heap);
    ItemStackArray *equipment=ItemStackArray_new(heap,5);if(!equipment)goto done;
    self->previousEquipment=equipment;MCObjectHeap_touch(heap);
    self->maxHurtResistantTime=20;self->jumpMovementFactor=0.02f;self->potionsNeedUpdate=true;
    MCObjectHeap_touch(heap);
    if(!living->applyEntityAttributes||!effect(self,living->applyEntityAttributes(livingContext,self)))goto done;
    float maximum=EntityLivingBase_getMaxHealth(self);
    if(MCObjectHeap_failed(heap)||!living->setHealth||!effect(self,living->setHealth(livingContext,self,maximum)))goto done;
    self->entity.preventEntitySpawning=true;MCObjectHeap_touch(heap);
    if(!living->mathRandom||!effect(self,living->mathRandom(livingContext,&draw)))goto done;
    volatile double first=draw+1.0;
    self->randomUnused1=(float)(first*0.009999999776482582);MCObjectHeap_touch(heap);
    const EntityDependencies *d=self->entity.entityDependencies;
    if(!d||!d->setPosition||!effect(self,d->setPosition(self->entity.entityContext,&self->entity,
        self->entity.posX,self->entity.posY,self->entity.posZ)))goto done;
    if(!effect(self,living->mathRandom(livingContext,&draw)))goto done;
    volatile float narrowed=(float)draw;
    self->randomUnused2=narrowed*12398.0f;MCObjectHeap_touch(heap);
    if(!effect(self,living->mathRandom(livingContext,&draw)))goto done;
    volatile double angle=draw*3.141592653589793;
    self->entity.rotationYaw=(float)(angle*2.0);
    self->rotationYawHead=self->entity.rotationYaw;self->entity.stepHeight=0.6f;
    MCObjectHeap_touch(heap);ok=true;
done:
    if(!ok)MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap);
}

bool EntityLivingBase_setSprinting(EntityLivingBase *self, bool sprinting) {
  MCObjectRootScope scope = {0};
  if (!begin(self, &scope))
    return false;
  MCObjectHeap *heap = self->entity.object.heap;
  bool ok = false;
  const EntityLivingBaseStaticFields *fields =
      EntityLivingBase_getStaticFields(heap);
  if (!fields || !Entity_setSprinting(&self->entity, sprinting))
    goto done;
  const EntityLivingBaseDependencies *d = self->livingDependencies;
  SharedMonsterAttributes *attributes = SharedMonsterAttributes_get(heap);
  if (!attributes || !d || !d->getEntityAttribute)
    goto done;
  IAttributeInstance *instance = d->getEntityAttribute(
      self->livingContext, self, attributes->movementSpeed);
  if (MCObjectHeap_failed(heap) || !valid_attribute(self, instance) ||
      !MCObjectRootScope_pin(&scope, (MCObject *)instance))
    goto done;
  AttributeModifier *existing = IAttributeInstance_getModifier(
      instance, fields->sprintingSpeedBoostModifierUUID);
  if (MCObjectHeap_failed(heap))
    goto done;
  if (existing &&
      !effect(self, IAttributeInstance_removeModifier(
                        instance, fields->sprintingSpeedBoostModifier)))
    goto done;
  if (sprinting &&
      !effect(self, IAttributeInstance_applyModifier(
                        instance, fields->sprintingSpeedBoostModifier)))
    goto done;
  ok = true;
done:
  if (!ok)
    MCObjectHeap_fail(heap);
  MCObjectRootScope_end(&scope);
  return ok && !MCObjectHeap_failed(heap);
}

bool EntityLivingBase_entityInit(EntityLivingBase *self) {
    MCObjectRootScope scope={0};if(!begin(self,&scope))return false;
    MCObjectHeap *heap=self->entity.object.heap;bool ok=false;
    /* Receiver evaluation precedes each boxed argument. This scope forbids
       collection/adoption while the exact references are borrowed. */
    DataWatcher *watcher=self->entity.dataWatcher;
    MCObject *value=DataWatcher_boxInt(heap,0);
    if(!value||!valid_watcher(self,watcher)||!DataWatcher_addObject(watcher,7,value))goto done;
    watcher=self->entity.dataWatcher;value=DataWatcher_boxByte(heap,0);
    if(!value||!valid_watcher(self,watcher)||!DataWatcher_addObject(watcher,8,value))goto done;
    watcher=self->entity.dataWatcher;value=DataWatcher_boxByte(heap,0);
    if(!value||!valid_watcher(self,watcher)||!DataWatcher_addObject(watcher,9,value))goto done;
    watcher=self->entity.dataWatcher;value=DataWatcher_boxFloat(heap,1.0f);
    ok=value&&valid_watcher(self,watcher)&&DataWatcher_addObject(watcher,6,value);
done:
    if(!ok)MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap);
}
bool EntityLivingBase_applyEntityAttributes(EntityLivingBase *self) {
    MCObjectRootScope scope={0};if(!begin(self,&scope))return false;
    MCObjectHeap *heap=self->entity.object.heap;bool ok=false;
    const EntityLivingBaseDependencies *d=self->livingDependencies;
    for(unsigned i=0;i<3;i++) {
        if(!d||!d->getAttributeMap)goto done;
        BaseAttributeMap *map=d->getAttributeMap(self->livingContext,self);
        if(MCObjectHeap_failed(heap))goto done;
        SharedMonsterAttributes *s=SharedMonsterAttributes_get(heap);if(!s)goto done;
        IAttribute *attribute=i==0?s->maxHealth:(i==1?s->knockbackResistance:s->movementSpeed);
        if(!valid_map(self,map)||!BaseAttributeMap_registerAttribute(map,attribute))goto done;
    }
    ok=true;
done:
    if(!ok)MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap);
}
BaseAttributeMap *EntityLivingBase_getAttributeMap(EntityLivingBase *self) {
    MCObjectRootScope scope={0};if(!begin(self,&scope))return NULL;
    BaseAttributeMap *out=NULL;
    if(!self->attributeMap) {
        ServersideAttributeMap *map=ServersideAttributeMap_new(self->entity.object.heap);
        if(!map)goto done;
        self->attributeMap=&map->base;MCObjectHeap_touch(self->entity.object.heap);
    }
    if(valid_map(self,self->attributeMap))out=self->attributeMap;
done:
    MCObjectRootScope_end(&scope);return out;
}
IAttributeInstance *EntityLivingBase_getEntityAttribute(EntityLivingBase *self,IAttribute *attribute) {
    MCObjectRootScope scope={0};if(!begin(self,&scope))return NULL;
    const EntityLivingBaseDependencies *d=self->livingDependencies;IAttributeInstance *out=NULL;
    if(!d||!d->getAttributeMap)goto done;
    BaseAttributeMap *map=d->getAttributeMap(self->livingContext,self);
    if(MCObjectHeap_failed(self->entity.object.heap)||!valid_map(self,map))goto done;
    out=BaseAttributeMap_getAttributeInstance(map,attribute);
    /* An unregistered attribute returns null here; its caller determines
       whether that null is subsequently dereferenced. */
done:
    if(!d||!d->getAttributeMap)MCObjectHeap_fail(self->entity.object.heap);
    MCObjectRootScope_end(&scope);return out;
}
float EntityLivingBase_getMaxHealth(EntityLivingBase *self) {
    MCObjectRootScope scope={0};if(!begin(self,&scope))return 0.0f;
    MCObjectHeap *heap=self->entity.object.heap;float value=0.0f;
    const EntityLivingBaseDependencies *d=self->livingDependencies;
    SharedMonsterAttributes *s=SharedMonsterAttributes_get(heap);if(!s)goto done;
    if(!d||!d->getEntityAttribute){MCObjectHeap_fail(heap);goto done;}
    IAttributeInstance *attribute=d->getEntityAttribute(self->livingContext,self,s->maxHealth);
    if(MCObjectHeap_failed(heap)||!valid_attribute(self,attribute))goto done;
    value=(float)IAttributeInstance_getAttributeValue(attribute);
done:
    MCObjectRootScope_end(&scope);return value;
}
float EntityLivingBase_getHealth(EntityLivingBase *self) {
    if(!valid(self)||!valid_watcher(self,self->entity.dataWatcher))return 0.0f;
    return DataWatcher_getWatchableObjectFloat(self->entity.dataWatcher,6);
}
bool EntityLivingBase_setHealth(EntityLivingBase *self,float health) {
    MCObjectRootScope scope={0};if(!begin(self,&scope))return false;
    MCObjectHeap *heap=self->entity.object.heap;
    DataWatcher *watcher=self->entity.dataWatcher;
    float maximum=EntityLivingBase_getMaxHealth(self);bool ok=false;
    if(MCObjectHeap_failed(heap))goto done;
    MCObject *boxed=DataWatcher_boxFloat(heap,MathHelper_clamp_float(health,0.0f,maximum));
    ok=boxed&&valid_watcher(self,watcher)&&effect(self,DataWatcher_updateObject(watcher,6,boxed));
done:
    if(!ok)MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap);
}
