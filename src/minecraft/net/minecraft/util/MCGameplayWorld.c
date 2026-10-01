#include "util/MCGameplayWorld.h"
#include "world/storage/MapData.h"

static void trace(MCObject *object,MCObjectVisitor visitor,void *context) {
    MCGameplayWorld *world=(MCGameplayWorld *)object;
    world->owners=(MCGameplayObjects *)visitor((MCObject *)world->owners,context);
    world->manager=(CraftingManager *)visitor((MCObject *)world->manager,context);
    world->furnace=(FurnaceRecipes *)visitor((MCObject *)world->furnace,context);
    world->statList=(StatList *)visitor((MCObject *)world->statList,context);
    world->itemDisplayContext=visitor(world->itemDisplayContext,context);
    world->nativeContext=visitor(world->nativeContext,context);
    world->mapStorage=(MapStorage *)visitor((MCObject *)world->mapStorage,context);
    world->rand=(NativeJavaRandom *)visitor((MCObject *)world->rand,context);
    world->emptyMapUseStat=(StatBase *)visitor((MCObject *)world->emptyMapUseStat,context);
    world->savedItemFields=(NBTTagCompound *)visitor((MCObject *)world->savedItemFields,context);
    world->savedItemRootName=(NBTString *)visitor((MCObject *)world->savedItemRootName,context);
    MapData_traceReferences(&world->maps,visitor,context);
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
    return MCGameplayWorld_newWithRandomRuntime(heap,owners,terrain,manager,NativeJavaRandomRuntime_process());
}
MCGameplayWorld *MCGameplayWorld_newWithRandomRuntime(MCObjectHeap *heap,MCGameplayObjects *owners,
    const mc_world *terrain,CraftingManager *manager,NativeJavaRandomRuntime *runtime) {
    if (!heap || !owners || owners->object.heap!=heap ||
        !runtime || (manager && manager->object.heap!=heap)) {
        MCObjectHeap_fail(heap); return NULL;
    }
    MCObjectRootScope scope={0};
    if (!MCObjectRootScope_begin(&scope,heap)) return NULL;
    MCGameplayWorld *world=(MCGameplayWorld *)MCObjectHeap_alloc(heap,sizeof(*world),&klass);
    bool ok=world!=NULL;
    if (world) {
        world->owners=owners; world->terrain=terrain; world->manager=manager;
        world->randomRuntime=runtime; mc_maps_init(&world->maps);
        NativeJavaRandom *temporary=NativeJavaRandomRuntime_newRandom(runtime,heap);
        ok=temporary&&NativeJavaRandom_nextInt(temporary,&world->updateLCG);
        if (ok) world->rand=NativeJavaRandomRuntime_newRandom(runtime,heap);
        ok=ok&&world->rand&&NativeJavaRandom_nextIntBound(world->rand,12000,&world->ambientTickCountdown);
        MCObjectHeap_touch(heap);
    }
    if(ok) {
        /* Native partial World factory: base counter's original no-save-handler
           branch. Journal persistence is the surrounding native environment. */
        world->mapStorage=MapStorage_nativeNewCounterProvider(heap,NULL,NULL,NULL);
        ok=world->mapStorage!=NULL;
    }
    if (!ok) MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);
    return ok&&!MCObjectHeap_failed(heap)?world:NULL;
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
