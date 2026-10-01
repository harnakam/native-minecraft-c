#include "entity/item/EntityItem.h"
#include "nbt/NBTTagCompound.h"
#include <limits.h>
#include <string.h>

static int32_t java_add(int32_t a,int32_t b) {
    uint32_t value=(uint32_t)a+(uint32_t)b;
    return value<=INT32_MAX?(int32_t)value:-1-(int32_t)(UINT32_MAX-value);
}
static int32_t java_subtract(int32_t a,int32_t b) {
    uint32_t value=(uint32_t)a-(uint32_t)b;
    return value<=INT32_MAX?(int32_t)value:-1-(int32_t)(UINT32_MAX-value);
}
static bool effect(EntityItem *entity,bool completed) {
    if (!completed) MCObjectHeap_fail(entity->object.heap);
    return completed&&!MCObjectHeap_failed(entity->object.heap);
}
static void trace(MCObject *o,MCObjectVisitor visitor,void *context) {
    EntityItem *entity=(EntityItem *)o;
    entity->worldObj=visitor(entity->worldObj,context);
    entity->dependencyContext=visitor(entity->dependencyContext,context);
    entity->watchedItem=(ItemStack *)visitor((MCObject *)entity->watchedItem,context);
    entity->owner=(NBTString *)visitor((MCObject *)entity->owner,context);
    entity->thrower=(NBTString *)visitor((MCObject *)entity->thrower,context);
    entity->savedFields=(NBTTagCompound *)visitor((MCObject *)entity->savedFields,context);
}
static const MCObjectClass klass={"net.minecraft.entity.item.EntityItem",MCObjectHeap_plainClone,trace,NULL};
bool EntityItem_isInstance(const MCObject *object) {return object&&object->klass==&klass;}
static EntityItem *allocate(MCObjectHeap *heap,MCObject *world,MCObject *context,const EntityItemDependencies *d) {
    if ((world&&world->heap!=heap)||(context&&context->heap!=heap)||!d||
        !d->markWatched||!d->logMissingItem||!d->isRemote||!d->inventory||!d->name||!d->findPlayer||
        !d->triggerAchievement||!d->isSilent||!d->nextFloat||!d->playSoundAtEntity||!d->onItemPickup||!d->setDead) {
        MCObjectHeap_fail(heap);return NULL;
    }
    EntityItem *entity=(EntityItem *)MCObjectHeap_alloc(heap,sizeof(*entity),&klass);
    if (entity) {entity->worldObj=world;entity->dependencyContext=context;entity->dependencies=d;}
    return entity;
}
EntityItem *EntityItem_nativeNew(MCObjectHeap *heap,MCObject *world,MCObject *context,const EntityItemDependencies *d) {
    EntityItem *entity=allocate(heap,world,context,d);
    if (entity) {entity->health=5;MCObjectHeap_touch(heap);}
    return entity;
}
static bool constructor_begin(MCObjectRootScope *scope,MCObjectHeap *heap,MCObject *world,MCObject *context,const EntityItemConstructorDependencies *d) {
    if (!d||!d->baseConstructor||!d->mathRandom||!d->setSize||!d->setPosition) {MCObjectHeap_fail(heap);return false;}
    if (!MCObjectRootScope_begin(scope,heap)) return false;
    if (!MCObjectRootScope_pin(scope,world)||!MCObjectRootScope_pin(scope,context)) {MCObjectRootScope_end(scope);return false;}
    return true;
}
static EntityItem *constructor_base(MCObjectHeap *heap,MCObject *world,MCObject *context,const EntityItemDependencies *d,const EntityItemConstructorDependencies *construct) {
    EntityItem *entity=allocate(heap,world,context,d); if (!entity) return NULL;
    if (!effect(entity,construct->baseConstructor(context,entity,world))) return NULL;
    entity->health=5; MCObjectHeap_touch(heap);
    double random=construct->mathRandom(context); if (MCObjectHeap_failed(heap)) return NULL;
    volatile double hover=random*3.141592653589793; hover=hover*2.0;
    entity->hoverStart=(float)hover; MCObjectHeap_touch(heap);
    return effect(entity,construct->setSize(context,entity,0.25f,0.25f))?entity:NULL;
}
EntityItem *EntityItem_new_position(MCObjectHeap *heap,MCObject *world,MCObject *context,const EntityItemDependencies *d,const EntityItemConstructorDependencies *construct,double x,double y,double z) {
    MCObjectRootScope scope={0}; if (!constructor_begin(&scope,heap,world,context,construct)) return NULL;
    EntityItem *entity=constructor_base(heap,world,context,d,construct);
    if (entity && !effect(entity,construct->setPosition(context,entity,x,y,z))) entity=NULL;
    if (entity) {
        double random=construct->mathRandom(context);
        if (!MCObjectHeap_failed(heap)) {entity->rotationYaw=(float)(random*360.0);MCObjectHeap_touch(heap);}
        if (!MCObjectHeap_failed(heap)) {
            random=construct->mathRandom(context); volatile double motion=random*0.20000000298023224; motion=motion-0.10000000149011612;
            if (!MCObjectHeap_failed(heap)) {entity->motionX=(double)(float)motion;entity->motionY=0.20000000298023224;MCObjectHeap_touch(heap);}
        }
        if (!MCObjectHeap_failed(heap)) {
            random=construct->mathRandom(context); volatile double motion=random*0.20000000298023224; motion=motion-0.10000000149011612;
            if (!MCObjectHeap_failed(heap)) {entity->motionZ=(double)(float)motion;MCObjectHeap_touch(heap);}
        }
    }
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(heap)?NULL:entity;
}
EntityItem *EntityItem_new_stack(MCObjectHeap *heap,MCObject *world,MCObject *context,const EntityItemDependencies *d,const EntityItemConstructorDependencies *construct,double x,double y,double z,ItemStack *stack) {
    MCObjectRootScope scope={0}; if (!constructor_begin(&scope,heap,world,context,construct)) return NULL;
    EntityItem *entity=NULL;
    if (MCObjectRootScope_pin(&scope,(MCObject *)stack)) {
        entity=EntityItem_new_position(heap,world,context,d,construct,x,y,z);
        if (entity && !EntityItem_setEntityItemStack(entity,stack)) entity=NULL;
    }
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(heap)?NULL:entity;
}
EntityItem *EntityItem_new_world(MCObjectHeap *heap,MCObject *world,MCObject *context,const EntityItemDependencies *d,const EntityItemConstructorDependencies *construct) {
    MCObjectRootScope scope={0}; if (!constructor_begin(&scope,heap,world,context,construct)) return NULL;
    EntityItem *entity=constructor_base(heap,world,context,d,construct);
    if (entity) {
        ItemStack *air=ItemStack_new(heap,ItemStack_registryItem(0),0,0);
        if (!air||!EntityItem_setEntityItemStack(entity,air)) entity=NULL;
    }
    MCObjectRootScope_end(&scope); return MCObjectHeap_failed(heap)?NULL:entity;
}
ItemStack *EntityItem_getEntityItem(EntityItem *entity) {
    if (MCObjectHeap_failed(entity->object.heap)) return NULL;
    ItemStack *stack=entity->watchedItem;
    if (!stack) {
        if (entity->worldObj&&!effect(entity,entity->dependencies->logMissingItem(entity->dependencyContext,entity->entityId))) return NULL;
        return ItemStack_new_item(entity->object.heap,ItemStack_registryItem(1));
    }
    return stack;
}
bool EntityItem_setEntityItemStack(EntityItem *entity,ItemStack *stack) {
    if (MCObjectHeap_failed(entity->object.heap)) return false;
    if (stack&&stack->object.heap!=entity->object.heap) {MCObjectHeap_fail(entity->object.heap);return false;}
    /* Native index10 watcher storage. Source updateObject retains this exact
       reference; setObjectWatched is unconditional, including same/NULL refs. */
    entity->watchedItem=stack;MCObjectHeap_touch(entity->object.heap);
    return effect(entity,entity->dependencies->markWatched(entity->dependencyContext,entity,10));
}
static int16_t java_short(int32_t value) {uint16_t bits=(uint16_t)value;int16_t out;memcpy(&out,&bits,sizeof(out));return out;}
static int8_t java_byte(int32_t value) {uint8_t bits=(uint8_t)value;int8_t out;memcpy(&out,&bits,sizeof(out));return out;}
bool EntityItem_writeEntityToNBT(EntityItem *entity,NBTTagCompound *tag) {
    MCObjectHeap *heap=entity?entity->object.heap:NULL;MCObjectRootScope scope={0};
    if (!entity||!tag||((MCObject *)tag)->heap!=heap) {MCObjectHeap_fail(heap);return false;}
    if (!MCObjectRootScope_begin(&scope,heap))return false;
    bool ok=NBTTagCompound_setShort_ascii(tag,"Health",(int16_t)java_byte(entity->health))&&
        NBTTagCompound_setShort_ascii(tag,"Age",java_short(entity->age))&&
        NBTTagCompound_setShort_ascii(tag,"PickupDelay",java_short(entity->delayBeforeCanPickup));
    if (ok&&EntityItem_getThrower(entity))ok=NBTTagCompound_setString_ascii(tag,"Thrower",entity->thrower);
    if (ok&&EntityItem_getOwner(entity))ok=NBTTagCompound_setString_ascii(tag,"Owner",entity->owner);
    if (ok&&EntityItem_getEntityItem(entity)) {
        ItemStack *item=EntityItem_getEntityItem(entity);
        NBTTagCompound *output=item?NBTTagCompound_new(heap):NULL;
        ok=output&&ItemStack_writeToNBT(item,output)&&NBTTagCompound_setTag_ascii(tag,"Item",(NBTBase *)output);
    }
    ok=ok&&!MCObjectHeap_failed(heap);MCObjectRootScope_end(&scope);return ok;
}
ItemStackNBTResult EntityItem_readEntityFromNBT(EntityItem *entity,NBTTagCompound *tag) {
    MCObjectHeap *heap=entity?entity->object.heap:NULL;MCObjectRootScope scope={0};
    if (!entity||!tag||((MCObject *)tag)->heap!=heap) {MCObjectHeap_fail(heap);return ITEMSTACK_NBT_FAILURE;}
    if (!MCObjectRootScope_begin(&scope,heap))return ITEMSTACK_NBT_FAILURE;
    entity->health=NBTTagCompound_getShort_ascii(tag,"Health")&255;
    entity->age=NBTTagCompound_getShort_ascii(tag,"Age");MCObjectHeap_touch(heap);
    if (NBTTagCompound_hasKey_ascii(tag,"PickupDelay")) {entity->delayBeforeCanPickup=NBTTagCompound_getShort_ascii(tag,"PickupDelay");MCObjectHeap_touch(heap);}
    if (NBTTagCompound_hasKey_ascii(tag,"Owner")) {
        NBTString *owner=NBTTagCompound_getString_ascii(tag,"Owner");
        if (!MCObjectHeap_failed(heap)) {entity->owner=owner;MCObjectHeap_touch(heap);}
    }
    if (!MCObjectHeap_failed(heap)&&NBTTagCompound_hasKey_ascii(tag,"Thrower")) {
        NBTString *thrower=NBTTagCompound_getString_ascii(tag,"Thrower");
        if (!MCObjectHeap_failed(heap)) {entity->thrower=thrower;MCObjectHeap_touch(heap);}
    }
    NBTTagCompound *item=MCObjectHeap_failed(heap)?NULL:NBTTagCompound_getCompoundTag_ascii(tag,"Item");
    ItemStackNBTResult result=ITEMSTACK_NBT_FAILURE;
    ItemStack *loaded=item?ItemStack_loadItemStackFromNBT(heap,item,&result):NULL;
    if (result==ITEMSTACK_NBT_OK) {
        if (!EntityItem_setEntityItemStack(entity,loaded))result=ITEMSTACK_NBT_FAILURE;
        else {
            ItemStack *stack=EntityItem_getEntityItem(entity);
            if (MCObjectHeap_failed(heap))result=ITEMSTACK_NBT_FAILURE;
            else if (!stack&&!effect(entity,entity->dependencies->setDead(entity->dependencyContext,entity)))result=ITEMSTACK_NBT_FAILURE;
        }
    }
    if (MCObjectHeap_failed(heap))result=ITEMSTACK_NBT_FAILURE;
    MCObjectRootScope_end(&scope);return result;
}
bool EntityItem_combineItems(EntityItem *entity,EntityItem *other) {
    MCObjectHeap *heap=entity->object.heap;
    if (!other||other->object.heap!=heap) {MCObjectHeap_fail(heap);return false;}
    if (MCObjectHeap_failed(heap)||other==entity||other->isDead||entity->isDead) return false;
    ItemStack *stack=EntityItem_getEntityItem(entity),*otherStack=EntityItem_getEntityItem(other);
    if (MCObjectHeap_failed(heap)||!stack||!otherStack) return false;
    if (entity->delayBeforeCanPickup==32767||other->delayBeforeCanPickup==32767||entity->age==-32768||other->age==-32768) return false;
    if (ItemStack_getItem(otherStack)!=ItemStack_getItem(stack)) return false;
    if (ItemStack_hasTagCompound(otherStack)!=ItemStack_hasTagCompound(stack)) return false;
    if (ItemStack_hasTagCompound(otherStack)&&!NBTBase_equals((NBTBase *)ItemStack_getTagCompound(otherStack),(NBTBase *)ItemStack_getTagCompound(stack))) return false;
    if (!ItemStack_getItem(otherStack)) return false;
    if (ItemStack_getHasSubtypes(otherStack)&&ItemStack_getMetadata(otherStack)!=ItemStack_getMetadata(stack)) return false;
    if (otherStack->stackSize<stack->stackSize) return EntityItem_combineItems(other,entity);
    if (java_add(otherStack->stackSize,stack->stackSize)>ItemStack_getMaxStackSize(otherStack)) return false;
    otherStack->stackSize=java_add(otherStack->stackSize,stack->stackSize);
    if (entity->delayBeforeCanPickup>other->delayBeforeCanPickup) other->delayBeforeCanPickup=entity->delayBeforeCanPickup;
    if (entity->age<other->age) other->age=entity->age;
    MCObjectHeap_touch(heap);
    if (!EntityItem_setEntityItemStack(other,otherStack)) return false;
    return effect(entity,entity->dependencies->setDead(entity->dependencyContext,entity));
}
static bool achievement(EntityItem *entity,MCObject *player,EntityItemAchievement value) {
    return effect(entity,entity->dependencies->triggerAchievement(entity->dependencyContext,player,value));
}
bool EntityItem_onCollideWithPlayer(EntityItem *entity,MCObject *player) {
    MCObjectHeap *heap=entity->object.heap;const EntityItemDependencies *d=entity->dependencies;MCObject *context=entity->dependencyContext;
    if (MCObjectHeap_failed(heap)) return false;
    if (!entity->worldObj||entity->worldObj->heap!=heap) {MCObjectHeap_fail(heap);return false;}
    bool remote=d->isRemote(context,entity->worldObj);
    if (MCObjectHeap_failed(heap)) return false;
    if (remote) return true;
    ItemStack *stack=EntityItem_getEntityItem(entity);
    if (!stack||MCObjectHeap_failed(heap)) return false;
    int32_t count=stack->stackSize;
    if (entity->delayBeforeCanPickup!=0) return true;
    if (entity->owner&&java_subtract(6000,entity->age)>200) {
        if (!player||player->heap!=heap) {MCObjectHeap_fail(heap);return false;}
        const NBTString *playerName=d->name(context,player);
        if (!playerName||((const MCObject *)playerName)->heap!=heap) {MCObjectHeap_fail(heap);return false;}
        if (MCObjectHeap_failed(heap)) return false;
        if (!NBTString_equals(entity->owner,playerName)) return true;
    }
    if (!player||player->heap!=heap) {MCObjectHeap_fail(heap);return false;}
    InventoryPlayer *inventory=d->inventory(context,player);
    if (!inventory||inventory->object.heap!=heap) {MCObjectHeap_fail(heap);return false;}
    bool inserted=InventoryPlayer_addItemStackToInventory(inventory,stack);
    if (MCObjectHeap_failed(heap)) return false;
    /* A false original result can still leave a smaller shared stack. Do not
       erase it, copy it, mark metadata, or run pickup effects in that case. */
    if (!inserted) return true;
    if (ItemStack_getItem(stack)==ItemStack_registryItem(17)&&!achievement(entity,player,ENTITYITEM_ACH_MINE_WOOD)) return false;
    if (ItemStack_getItem(stack)==ItemStack_registryItem(162)&&!achievement(entity,player,ENTITYITEM_ACH_MINE_WOOD)) return false;
    if (ItemStack_getItem(stack)==ItemStack_registryItem(334)&&!achievement(entity,player,ENTITYITEM_ACH_KILL_COW)) return false;
    if (ItemStack_getItem(stack)==ItemStack_registryItem(264)&&!achievement(entity,player,ENTITYITEM_ACH_DIAMONDS)) return false;
    if (ItemStack_getItem(stack)==ItemStack_registryItem(369)&&!achievement(entity,player,ENTITYITEM_ACH_BLAZE_ROD)) return false;
    if (ItemStack_getItem(stack)==ItemStack_registryItem(264)&&EntityItem_getThrower(entity)) {
        MCObject *thrower=d->findPlayer(context,entity->worldObj,EntityItem_getThrower(entity));
        if (MCObjectHeap_failed(heap)) return false;
        if (thrower&&thrower->heap!=heap) {MCObjectHeap_fail(heap);return false;}
        if (thrower&&thrower!=player&&!achievement(entity,thrower,ENTITYITEM_ACH_DIAMONDS_TO_YOU)) return false;
    }
    bool silent=d->isSilent(context,entity);
    if (MCObjectHeap_failed(heap)) return false;
    if (!silent) {
        float first=d->nextFloat(context,entity);
        if (MCObjectHeap_failed(heap)) return false;
        float second=d->nextFloat(context,entity);
        if (MCObjectHeap_failed(heap)) return false;
        float pitch=((first-second)*0.7F+1.0F)*2.0F;
        if (!effect(entity,d->playSoundAtEntity(context,entity->worldObj,player,"random.pop",0.2F,pitch))) return false;
    }
    if (!effect(entity,d->onItemPickup(context,player,entity,count))) return false;
    if (stack->stackSize<=0&&!effect(entity,d->setDead(context,entity))) return false;
    return !MCObjectHeap_failed(heap);
}
NBTString *EntityItem_getOwner(const EntityItem *entity) {return entity->owner;}
bool EntityItem_setOwner(EntityItem *entity,NBTString *owner) {
    if (MCObjectHeap_failed(entity->object.heap)) return false;
    if (owner&&((MCObject *)owner)->heap!=entity->object.heap) {MCObjectHeap_fail(entity->object.heap);return false;}
    entity->owner=owner;MCObjectHeap_touch(entity->object.heap);return !MCObjectHeap_failed(entity->object.heap);
}
NBTString *EntityItem_getThrower(const EntityItem *entity) {return entity->thrower;}
bool EntityItem_setThrower(EntityItem *entity,NBTString *thrower) {
    if (MCObjectHeap_failed(entity->object.heap)) return false;
    if (thrower&&((MCObject *)thrower)->heap!=entity->object.heap) {MCObjectHeap_fail(entity->object.heap);return false;}
    entity->thrower=thrower;MCObjectHeap_touch(entity->object.heap);return !MCObjectHeap_failed(entity->object.heap);
}
int32_t EntityItem_getAge(const EntityItem *entity) {return entity->age;}
void EntityItem_setAgeToCreativeDespawnTime(EntityItem *entity) {entity->age=4800;MCObjectHeap_touch(entity->object.heap);}
void EntityItem_setDefaultPickupDelay(EntityItem *entity) {entity->delayBeforeCanPickup=10;MCObjectHeap_touch(entity->object.heap);}
void EntityItem_setNoPickupDelay(EntityItem *entity) {entity->delayBeforeCanPickup=0;MCObjectHeap_touch(entity->object.heap);}
void EntityItem_setInfinitePickupDelay(EntityItem *entity) {entity->delayBeforeCanPickup=32767;MCObjectHeap_touch(entity->object.heap);}
void EntityItem_setPickupDelay(EntityItem *entity,int32_t ticks) {entity->delayBeforeCanPickup=ticks;MCObjectHeap_touch(entity->object.heap);}
bool EntityItem_cannotPickup(const EntityItem *entity) {return entity->delayBeforeCanPickup>0;}
void EntityItem_setNoDespawn(EntityItem *entity) {entity->age=-6000;MCObjectHeap_touch(entity->object.heap);}
void EntityItem_func_174870_v(EntityItem *entity) {EntityItem_setInfinitePickupDelay(entity);entity->age=5999;MCObjectHeap_touch(entity->object.heap);}
