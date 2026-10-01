#ifndef C919_SOURCE_ENTITY_LIVING_BASE_H
#define C919_SOURCE_ENTITY_LIVING_BASE_H
#include "entity/Entity.h"
#include "item/ItemStack.h"

typedef struct BaseAttributeMap BaseAttributeMap;
typedef struct IAttribute IAttribute;
typedef struct IAttributeInstance IAttributeInstance;
typedef struct CombatTracker CombatTracker;
typedef struct MCGameplayPlayer MCGameplayPlayer;
typedef struct EntityLivingBase EntityLivingBase;
/* Native storage for the original empty HashMap<Integer,PotionEffect> instance.
   PotionEffect and later Map operations are unported; no empty successful
   potion/combat update method is provided by this constructor subset. */
typedef struct LivingPotionMap {
    MCObject object;
    MCObject *table,*entrySet;
    int32_t size,modCount,threshold;
    float loadFactor;
} LivingPotionMap;
typedef struct EntityLivingBaseDependencies {
    bool (*applyEntityAttributes)(MCObject *context,EntityLivingBase *);
    BaseAttributeMap *(*getAttributeMap)(MCObject *context,EntityLivingBase *);
    IAttributeInstance *(*getEntityAttribute)(MCObject *context,EntityLivingBase *,IAttribute *);
    bool (*setHealth)(MCObject *context,EntityLivingBase *,float);
    bool (*mathRandom)(MCObject *context,double *out);
} EntityLivingBaseDependencies;
/* All original instance fields, with one first-member Entity state. Native
   inherited Player identity is currently the MCGameplayPlayer receiver. The
   complete constructor/reachable methods below do not implement combat,
   potion, movement-tick, Living NBT or the separate EntityLiving mob class. */
struct EntityLivingBase {
    Entity entity;
    BaseAttributeMap *attributeMap;
    CombatTracker *_combatTracker;
    LivingPotionMap *activePotionsMap;
    ItemStackArray *previousEquipment;
    bool isSwingInProgress;
    int32_t swingProgressInt,arrowHitTimer,hurtTime,maxHurtTime;
    float attackedAtYaw;
    int32_t deathTime;
    float prevSwingProgress,swingProgress,prevLimbSwingAmount,limbSwingAmount,limbSwing;
    int32_t maxHurtResistantTime;
    float prevCameraPitch,cameraPitch,randomUnused2,randomUnused1,renderYawOffset,prevRenderYawOffset;
    float rotationYawHead,prevRotationYawHead,jumpMovementFactor;
    MCGameplayPlayer *attackingPlayer;
    int32_t recentlyHit;
    bool dead;
    int32_t entityAge;
    float prevOnGroundSpeedFactor,onGroundSpeedFactor,movedDistance,prevMovedDistance,unused180;
    int32_t scoreValue;
    float lastDamage;
    bool isJumping;
    float moveStrafing,moveForward,randomYawVelocity;
    int32_t newPosRotationIncrements;
    double newPosX,newPosY,newPosZ,newRotationYaw,newRotationPitch;
    bool potionsNeedUpdate;
    EntityLivingBase *entityLivingToAttack;
    int32_t revengeTimer;
    EntityLivingBase *lastAttacker;
    int32_t lastAttackerTime;
    float landMovementFactor;
    int32_t jumpTicks;
    float absorptionAmount;
    const EntityLivingBaseDependencies *livingDependencies;
    MCObject *livingContext;
};
/* Abstract source constructor on a zeroed allocated actual Player subtype.
   Entity's virtual entityInit runs before Living initializers. Required
   immutable dispatch binds the actual subclass, not a default success hook.
   Process ID/seed/Math effects and failure prefixes retain their source order. */
bool EntityLivingBase_construct(EntityLivingBase *,MCObject *world,
    const EntityDependencies *,MCObject *entityContext,
    const EntityLivingBaseDependencies *,MCObject *livingContext,
    NativeJavaRandomRuntime *,NativeEntityIDRuntime *);
bool EntityLivingBase_isInstance(const MCObject *);
void EntityLivingBase_traceFields(EntityLivingBase *,MCObjectVisitor,void *context);
bool LivingPotionMap_isInstance(const MCObject *);
bool EntityLivingBase_entityInit(EntityLivingBase *);
bool EntityLivingBase_applyEntityAttributes(EntityLivingBase *);
BaseAttributeMap *EntityLivingBase_getAttributeMap(EntityLivingBase *);
IAttributeInstance *EntityLivingBase_getEntityAttribute(EntityLivingBase *,IAttribute *);
float EntityLivingBase_getMaxHealth(EntityLivingBase *);
float EntityLivingBase_getHealth(EntityLivingBase *);
bool EntityLivingBase_setHealth(EntityLivingBase *,float);
#endif
