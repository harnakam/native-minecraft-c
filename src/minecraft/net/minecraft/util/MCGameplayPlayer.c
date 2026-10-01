#include "util/MCGameplayPlayer.h"
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
        player->savedFields=NBTTagCompound_new(heap);
        player->inventory=InventoryPlayer_new(heap,(MCObject *)player,MCGameplayPlayer_isCreativeMode);
        if (player->savedFields && player->inventory) {
            player->inventoryContainer=ContainerPlayer_new(player->inventory,world->remote,(MCObject *)player,dependencies);
            if (player->inventoryContainer) player->openContainer=&player->inventoryContainer->container;
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
