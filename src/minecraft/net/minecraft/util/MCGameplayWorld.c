#include "util/MCGameplayWorld.h"

static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    MCGameplayWorld *world=(MCGameplayWorld *)object;
    world->owners=(MCGameplayObjects *)visitor((MCObject *)world->owners,context);
    world->manager=(CraftingManager *)visitor((MCObject *)world->manager,context);
    world->furnace=(FurnaceRecipes *)visitor((MCObject *)world->furnace,context);
    world->statList=(StatList *)visitor((MCObject *)world->statList,context);
    world->itemDisplayContext=visitor(world->itemDisplayContext,context);
    world->savedItemFields=(NBTTagCompound *)visitor((MCObject *)world->savedItemFields,context);
    world->savedItemRootName=(NBTString *)visitor((MCObject *)world->savedItemRootName,context);
    for (size_t i=0;i<MC_GAMEPLAY_CRAFT_STAT_COUNT;i++)
        world->craftStats[i]=(StatBase *)visitor((MCObject *)world->craftStats[i],context);
}
static void destroy(MCObject *object) {
    /* The heap owns every managed edge; terrain belongs to the caller. */
    mc_maps_free(&((MCGameplayWorld *)object)->maps);
}
static MCObject *clone(MCObjectHeap *destination,const MCObject *source) {
    MCGameplayWorld *copy=(MCGameplayWorld *)MCObjectHeap_plainClone(destination,source);
    if (!copy) return NULL;
    /* Detach every copied native pointer before an allocation can fail. The
       failed-snapshot destructor must never release the original map store. */
    mc_maps_init(&copy->maps);
    if (!mc_maps_copy(&copy->maps,&((const MCGameplayWorld *)source)->maps)) {
        MCObjectHeap_fail(destination); return NULL;
    }
    return (MCObject *)copy;
}
static const MCObjectClass klass={"C919.native.GameplayWorld",clone,trace,destroy};
bool MCGameplayWorld_isInstance(const MCObject *object) { return object && object->klass==&klass; }
MCGameplayWorld *MCGameplayWorld_new(MCObjectHeap *heap,MCGameplayObjects *owners,
    const mc_world *terrain,CraftingManager *manager) {
    if (!heap || !owners || owners->object.heap!=heap ||
        (manager && manager->object.heap!=heap)) {
        MCObjectHeap_fail(heap); return NULL;
    }
    MCGameplayWorld *world=(MCGameplayWorld *)MCObjectHeap_alloc(heap,sizeof(*world),&klass);
    if (!world) return NULL;
    world->owners=owners; world->terrain=terrain; world->manager=manager;
    mc_maps_init(&world->maps);
    return world;
}
static const MCGameplayWorld *world_object(const MCObject *object) {
    if (!object) return NULL;
    if (object->klass!=&klass) { MCObjectHeap_fail(object->heap); return NULL; }
    return (const MCGameplayWorld *)object;
}
bool MCGameplayWorld_isRemote(const MCObject *object) {
    const MCGameplayWorld *world=world_object(object);
    return world && world->remote;
}
bool MCGameplayWorld_isCraftingTable(const MCObject *object,int32_t x,int32_t y,int32_t z) {
    const MCGameplayWorld *world=world_object(object);
    if (!world) return false;
    if (!world->terrain) { MCObjectHeap_fail(object->heap); return false; }
    return (mc_world_get(world->terrain,x,y,z)>>4)==58;
}
