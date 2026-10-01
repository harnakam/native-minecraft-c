#include "entity/player/EntityPlayer.h"
#include "util/MCGameplayPlayer.h"
#include "entity/SharedMonsterAttributes.h"
#include <limits.h>

void EntityPlayer_traceFields(MCGameplayPlayer *p,MCObjectVisitor visit,void *context) {
    EntityLivingBase_traceFields(&p->living,visit,context);
    p->inventory=(InventoryPlayer *)visit((MCObject *)p->inventory,context);
    p->theInventoryEnderChest=(InventoryEnderChest *)visit((MCObject *)p->theInventoryEnderChest,context);
    p->inventoryContainer=(Container *)visit((MCObject *)p->inventoryContainer,context);
    p->openContainer=(Container *)visit((MCObject *)p->openContainer,context);
    p->foodStats=(FoodStats *)visit((MCObject *)p->foodStats,context);
    p->playerLocation=(DataWatcherBlockPos *)visit((MCObject *)p->playerLocation,context);
    p->spawnChunk=(DataWatcherBlockPos *)visit((MCObject *)p->spawnChunk,context);
    p->startMinecartRidingCoordinate=(DataWatcherBlockPos *)visit((MCObject *)p->startMinecartRidingCoordinate,context);
    p->capabilities=(PlayerCapabilities *)visit((MCObject *)p->capabilities,context);
    p->itemInUse=(ItemStack *)visit((MCObject *)p->itemInUse,context);
    p->gameProfile=(NativeGameProfile *)visit((MCObject *)p->gameProfile,context);
    p->fishEntity=visit(p->fishEntity,context);
    p->playerContext=visit(p->playerContext,context);
}

static bool begin(MCGameplayPlayer *p,MCObjectRootScope *scope) {
    if(!MCGameplayPlayer_isInstance((MCObject *)p)) {
        MCObjectHeap_fail(p?((MCObject *)p)->heap:NULL);return false;
    }
    if(!MCObjectRootScope_begin(scope,((MCObject *)p)->heap))return false;
    if(MCObjectRootScope_pin(scope,(MCObject *)p))return true;
    MCObjectRootScope_end(scope);return false;
}
static bool effect(MCGameplayPlayer *p,bool ok) {
    MCObjectHeap *heap=((MCObject *)p)->heap;
    if(!ok)MCObjectHeap_fail(heap);
    return ok&&!MCObjectHeap_failed(heap);
}
bool EntityPlayer_entityInit(MCGameplayPlayer *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *heap=((MCObject *)p)->heap;
    bool ok=EntityLivingBase_entityInit(&p->living);
    MCObject *value=ok?DataWatcher_boxByte(heap,0):NULL;
    ok=ok&&value&&DataWatcher_addObject(p->living.entity.dataWatcher,16,value);
    value=ok?DataWatcher_boxFloat(heap,0):NULL;
    ok=ok&&value&&DataWatcher_addObject(p->living.entity.dataWatcher,17,value);
    value=ok?DataWatcher_boxInt(heap,0):NULL;
    ok=ok&&value&&DataWatcher_addObject(p->living.entity.dataWatcher,18,value);
    value=ok?DataWatcher_boxByte(heap,0):NULL;
    ok=ok&&value&&DataWatcher_addObject(p->living.entity.dataWatcher,10,value);
    ok=effect(p,ok);MCObjectRootScope_end(&scope);return ok;
}
bool EntityPlayer_applyEntityAttributes(MCGameplayPlayer *p) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *heap=((MCObject *)p)->heap;
    const EntityLivingBaseDependencies *d=p->living.livingDependencies;
    bool ok=EntityLivingBase_applyEntityAttributes(&p->living);
    BaseAttributeMap *map=ok&&d&&d->getAttributeMap?d->getAttributeMap(p->living.livingContext,&p->living):NULL;
    if(map&&!MCObjectRootScope_pin(&scope,(MCObject *)map))map=NULL;
    SharedMonsterAttributes *fields=map?SharedMonsterAttributes_get(heap):NULL;
    IAttributeInstance *attack=fields?BaseAttributeMap_registerAttribute(map,fields->attackDamage):NULL;
    ok=ok&&attack&&IAttributeInstance_setBaseValue(attack,1.0);
    IAttributeInstance *movement=ok&&d->getEntityAttribute?
        d->getEntityAttribute(p->living.livingContext,&p->living,fields->movementSpeed):NULL;
    ok=ok&&movement&&MCObjectRootScope_pin(&scope,(MCObject *)movement)&&
        IAttributeInstance_setBaseValue(movement,0.10000000149011612);
    ok=effect(p,ok);MCObjectRootScope_end(&scope);return ok;
}
bool EntityPlayer_construct(MCGameplayPlayer *p,MCObject *world,NativeGameProfile *profile,
    const EntityPlayerDependencies *d,const mc_crafting_dispatch *crafting,MCObject *context,
    NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    MCObjectRootScope scope={0};if(!begin(p,&scope))return false;
    MCObjectHeap *heap=((MCObject *)p)->heap;bool ok=false;
    if(!d||(context&&context->heap!=heap)||(profile&&profile->object.heap!=heap)||
       !MCObjectRootScope_pin(&scope,world)||!MCObjectRootScope_pin(&scope,(MCObject *)profile)||
       !MCObjectRootScope_pin(&scope,context))goto done;
    p->playerDependencies=d;p->playerContext=context;MCObjectHeap_touch(heap);
    if(!EntityLivingBase_construct(&p->living,world,d->entity,context,d->living,context,random,ids))goto done;
    /* Only declared instance initializers overwrite fields previously written
       by a superclass virtual call; other Java-default fields are left alone. */
    InventoryPlayer *inventory=InventoryPlayer_new(heap,(MCObject *)p,MCGameplayPlayer_isCreativeMode);
    if(!inventory)goto done;
    p->inventory=inventory;MCObjectHeap_touch(heap);
    InventoryEnderChest *ender=InventoryEnderChest_new(heap,d->enderBasic,d->enderChest,context);
    if(!ender)goto done;
    p->theInventoryEnderChest=ender;MCObjectHeap_touch(heap);
    FoodStats *food=FoodStats_new(heap);if(!food)goto done;
    p->foodStats=food;MCObjectHeap_touch(heap);
    PlayerCapabilities *caps=PlayerCapabilities_new(heap);if(!caps)goto done;
    p->capabilities=caps;MCObjectHeap_touch(heap);
    p->speedOnGround=0.1f;p->speedInAir=0.02f;p->hasReducedDebug=false;MCObjectHeap_touch(heap);
    NativeJavaUUID *uuid=EntityPlayer_getUUID(profile);if(!uuid)goto done;
    p->living.entity.entityUniqueID=uuid;p->gameProfile=profile;MCObjectHeap_touch(heap);
    bool isRemote;
    if(!world||!d->isRemote||!effect(p,d->isRemote(context,world,&isRemote)))goto done;
    ContainerPlayer *container=ContainerPlayer_new(p->inventory,!isRemote,(MCObject *)p,crafting);
    if(!container)goto done;
    p->inventoryContainer=&container->container;p->openContainer=p->inventoryContainer;MCObjectHeap_touch(heap);
    DataWatcherBlockPos *pos=d->getSpawnPoint?d->getSpawnPoint(context,world):NULL;
    if(!DataWatcher_blockPosIsInstance((MCObject *)pos)||pos->object.heap!=heap||
       !MCObjectRootScope_pin(&scope,(MCObject *)pos))goto done;
    uint32_t ybits=(uint32_t)pos->y+1u;
    int32_t y=ybits<=INT32_MAX?(int32_t)ybits:-1-(int32_t)(UINT32_MAX-ybits);
    if(!d->setLocationAndAngles||!effect(p,d->setLocationAndAngles(context,p,
       (double)pos->x+0.5,(double)y,(double)pos->z+0.5,0,0)))goto done;
    p->living.unused180=180.0f;p->living.entity.fireResistance=20;MCObjectHeap_touch(heap);ok=true;
done:
    ok=effect(p,ok);MCObjectRootScope_end(&scope);return ok;
}
