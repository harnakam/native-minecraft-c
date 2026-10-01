#include "util/MCGameplayPlayer.h"
#include "util/MathHelper.h"
#include <math.h>

static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    MCGameplayPlayer *player=(MCGameplayPlayer *)object;
    player->worldObj=(MCGameplayWorld *)visitor((MCObject *)player->worldObj,context);
    player->inventory=(InventoryPlayer *)visitor((MCObject *)player->inventory,context);
    player->inventoryContainer=(ContainerPlayer *)visitor((MCObject *)player->inventoryContainer,context);
    player->openContainer=(Container *)visitor((MCObject *)player->openContainer,context);
    player->name=(NBTString *)visitor((MCObject *)player->name,context);
    player->stats=(StatFileWriter *)visitor((MCObject *)player->stats,context);
    player->savedFields=(NBTTagCompound *)visitor((MCObject *)player->savedFields,context);
    player->savedRootName=(NBTString *)visitor((MCObject *)player->savedRootName,context);
    player->rand=(NativeJavaRandom *)visitor((MCObject *)player->rand,context);
    player->entityUniqueID=(NativeJavaUUID *)visitor((MCObject *)player->entityUniqueID,context);
    player->gameProfileUUID=(NativeJavaUUID *)visitor((MCObject *)player->gameProfileUUID,context);
    player->handler=visitor(player->handler,context);
    player->effects=visitor(player->effects,context);
    player->pendingPackets=visitor(player->pendingPackets,context);
}
static const MCObjectClass klass={"C919.native.GameplayPlayer",MCObjectHeap_plainClone,trace,NULL};
bool MCGameplayPlayer_isInstance(const MCObject *object) { return object && object->klass==&klass; }
static bool dependencies_ready(const mc_crafting_dispatch *d) {
    return d && d->inventory && d->world && d->findMatchingRecipe &&
        d->getRemainingItems && d->onCrafting && d->triggerAchievement &&
        d->drop && d->isPickaxe && d->isHoe && d->isSword && d->isWoodPickaxe &&
        d->armorType && d->isRemote && d->isCraftingTable && d->getDistanceSq;
}
static bool inherited_random_segment(MCGameplayPlayer *player) {
    MCObjectHeap *heap=player->object.heap;
    NativeJavaRandomRuntime *runtime=player->worldObj->randomRuntime;
    player->rand=NativeJavaRandomRuntime_newRandom(runtime,heap);
    if (player->rand) player->entityUniqueID=MathHelper_getRandomUuid(player->rand);
    if (!player->entityUniqueID) {MCObjectHeap_fail(heap);return false;}
    /* Original EntityLivingBase's three shared Math.random draws and exact
       expressions. No draw is substituted with this entity's Random. */
    double value;
    if (!NativeJavaRandomRuntime_mathRandom(runtime,&value)) {MCObjectHeap_fail(heap);return false;}
    volatile double first=value+1.0; first=first*0.009999999776482582;
    player->randomUnused1=(float)first;
    if (!NativeJavaRandomRuntime_mathRandom(runtime,&value)) {MCObjectHeap_fail(heap);return false;}
    volatile float second=(float)value; second=second*12398.0f;
    player->randomUnused2=second;
    if (!NativeJavaRandomRuntime_mathRandom(runtime,&value)) {MCObjectHeap_fail(heap);return false;}
    volatile double yaw=value*3.141592653589793; yaw=yaw*2.0;
    player->rotationYaw=(float)yaw; player->rotationYawHead=player->rotationYaw;
    MCObjectHeap_touch(heap); return true;
}
MCGameplayPlayer *MCGameplayPlayer_new(MCGameplayWorld *world,NBTString *name,
    StatFileWriter *stats,const mc_crafting_dispatch *dependencies) {
    MCObjectHeap *heap=world ? world->object.heap : NULL;
    if (!heap || !name || ((MCObject *)name)->heap!=heap ||
        (stats && ((MCObject *)stats)->heap!=heap) || !dependencies_ready(dependencies)) {
        MCObjectHeap_fail(heap); return NULL;
    }
    MCObjectRootScope scope={0};
    if (!MCObjectRootScope_begin(&scope,heap)) return NULL;
    MCGameplayPlayer *player=(MCGameplayPlayer *)MCObjectHeap_alloc(heap,sizeof(*player),&klass);
    if (player) {
        player->worldObj=world; player->name=name; player->stats=stats;
        bool randomReady=inherited_random_segment(player);
        player->savedFields=randomReady?NBTTagCompound_new(heap):NULL;
        player->inventory=randomReady?InventoryPlayer_new(heap,(MCObject *)player,MCGameplayPlayer_isCreativeMode):NULL;
        if (player->savedFields && player->inventory) {
            player->inventoryContainer=ContainerPlayer_new(player->inventory,!world->remote,(MCObject *)player,dependencies);
            if (player->inventoryContainer) {
                player->openContainer=&player->inventoryContainer->container;
                /* Native inherited field view of EntityPlayer's later
                   setLocationAndAngles(...,0,0). Its captured head yaw remains
                   the EntityLivingBase random result; spawn/physics are still
                   supplied by the native actor assembly, not a full ctor. */
                player->rotationYaw=0;player->rotationPitch=0;MCObjectHeap_touch(heap);
            }
        }
        if (!player->savedFields || !player->inventory || !player->inventoryContainer || MCObjectHeap_failed(heap)) {
            MCObjectHeap_fail(heap); player=NULL;
        }
    }
    MCObjectRootScope_end(&scope);
    return player;
}
static MCGameplayPlayer *player_object(const MCObject *object) {
    if (!object) return NULL;
    if (object->klass!=&klass) { MCObjectHeap_fail(object->heap); return NULL; }
    return (MCGameplayPlayer *)object;
}
InventoryPlayer *MCGameplayPlayer_inventory(MCObject *object) {
    MCGameplayPlayer *player=player_object(object);
    return player ? player->inventory : NULL;
}
MCObject *MCGameplayPlayer_world(MCObject *object) {
    MCGameplayPlayer *player=player_object(object);
    return player ? (MCObject *)player->worldObj : NULL;
}
bool MCGameplayPlayer_isCreativeMode(const MCObject *object) {
    const MCGameplayPlayer *player=player_object(object);
    return player && player->creative;
}
double MCGameplayPlayer_getDistanceSq(const MCObject *object,double x,double y,double z) {
    const MCGameplayPlayer *player=player_object(object);
    if (!player) return NAN;
    double dx=player->posX-x,dy=player->posY-y,dz=player->posZ-z;
    return dx*dx+dy*dy+dz*dz;
}
