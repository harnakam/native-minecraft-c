#include "entity/player/EntityPlayerMP.h"
#include "util/MCGameplayPlayer.h"
#include "util/MathHelper.h"
#include <string.h>

static void trace(MCObject *o,MCObjectVisitor visit,void *context) {
    EntityPlayerMP *p=(EntityPlayerMP *)o;
    MCGameplayPlayer_traceFields(&p->player,visit,context);
    p->translator=(NBTString *)visit((MCObject *)p->translator,context);
    p->playerNetServerHandler=(NetHandlerPlayServer *)visit((MCObject *)p->playerNetServerHandler,context);
    p->mcServer=visit(p->mcServer,context);
    p->theItemInWorldManager=(ItemInWorldManager *)visit((MCObject *)p->theItemInWorldManager,context);
    p->loadedChunks=visit(p->loadedChunks,context);
    p->destroyedItemsNetCache=visit(p->destroyedItemsNetCache,context);
    p->statsFile=(StatisticsFile *)visit((MCObject *)p->statsFile,context);
    p->chatVisibility=visit(p->chatVisibility,context);
    p->spectatingEntity=(Entity *)visit((MCObject *)p->spectatingEntity,context);
    p->constructorContext=visit(p->constructorContext,context);
}
static const MCObjectClass klass={"net.minecraft.entity.player.EntityPlayerMP",MCObjectHeap_plainClone,trace,NULL};
bool EntityPlayerMP_isInstance(const MCObject *o) {
    return o&&o->klass==&klass&&MCObjectHeap_objectSize(o)>=sizeof(EntityPlayerMP);
}
EntityPlayerMP *EntityPlayerMP_nativeAllocate(MCObjectHeap *heap) {
    return (EntityPlayerMP *)MCObjectHeap_alloc(heap,sizeof(EntityPlayerMP),&klass);
}
MCGameplayPlayer *EntityPlayerMP_asPlayer(EntityPlayerMP *p) {return p?&p->player:NULL;}
static bool failed(EntityPlayerMP *p) {MCObjectHeap_fail(p?((MCObject *)p)->heap:NULL);return false;}
StatisticsFile *EntityPlayerMP_getStatFile(EntityPlayerMP *p) {
    if(!EntityPlayerMP_isInstance((MCObject *)p)) {failed(p);return NULL;}
    return p->statsFile;
}
bool EntityPlayerMP_isSpectator(EntityPlayerMP *p) {
    if(!EntityPlayerMP_isInstance((MCObject *)p)||!ItemInWorldManager_isInstance((MCObject *)p->theItemInWorldManager)||
       p->theItemInWorldManager->object.heap!=((MCObject *)p)->heap)return failed(p);
    return ItemInWorldManager_getGameType(p->theItemInWorldManager)==&WorldSettingsGameType_SPECTATOR;
}
bool EntityPlayerMP_markPlayerActive(EntityPlayerMP *p) {
    if(!EntityPlayerMP_isInstance((MCObject *)p))return failed(p);
    MCObjectHeap *heap=((MCObject *)p)->heap;
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return false;
    const EntityPlayerMPConstructorDependencies *d=p->constructorDependencies;
    MCObject *context=p->constructorContext;
    int64_t time;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)p)&&MCObjectRootScope_pin(&scope,context)&&
        d&&d->currentTimeMillis&&d->currentTimeMillis(context,&time)&&!MCObjectHeap_failed(heap);
    if(ok){p->playerLastActiveTime=time;MCObjectHeap_touch(heap);}
    else failed(p);
    MCObjectRootScope_end(&scope);return ok;
}
int64_t EntityPlayerMP_getLastActiveTime(EntityPlayerMP *p) {
    if(!EntityPlayerMP_isInstance((MCObject *)p)) {failed(p);return 0;}
    return p->playerLastActiveTime;
}
bool EntityPlayerMP_construct(EntityPlayerMP *p,MCObject *server,MCObject *world,NativeGameProfile *profile,
    ItemInWorldManager *manager,const EntityPlayerMPConstructorDependencies *d,const mc_crafting_dispatch *crafting,
    MCObject *context,NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    if(!EntityPlayerMP_isInstance((MCObject *)p))return failed(p);
    MCObjectHeap *heap=((MCObject *)p)->heap;
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return false;
    bool ok=false;
#define REF(o) MCObjectRootScope_pin(&scope,(MCObject *)(o))
#define EFFECT(call) ((call)&&!MCObjectHeap_failed(heap))
    if(!d||!d->player||!REF(p)||!REF(server)||!REF(world)||!REF(profile)||!REF(manager)||!REF(context))goto done;
    /* Native dispatch is installed before source superclass virtual calls.
       No original leaf field is cleared or initialized at this boundary. */
    p->constructorDependencies=d;p->constructorContext=context;
    if(!EntityPlayer_construct(&p->player,world,profile,d->player,crafting,context,random,ids))goto done;
    NBTString *translator=NBTString_literalASCII(heap,"en_US");
    if(!translator)goto done;
    p->translator=translator;MCObjectHeap_touch(heap);
    if(!d->newLinkedList)goto done;
    MCObject *list=d->newLinkedList(context);
    if(!list||MCObjectHeap_failed(heap)||!REF(list))goto done;
    p->loadedChunks=list;MCObjectHeap_touch(heap);
    list=d->newLinkedList(context);
    if(!list||MCObjectHeap_failed(heap)||!REF(list))goto done;
    p->destroyedItemsNetCache=list;MCObjectHeap_touch(heap);
    uint32_t minimumBits=1;memcpy(&p->combinedHealth,&minimumBits,sizeof minimumBits);
    p->lastHealth=-1.0E8f;p->lastFoodLevel=-99999999;p->wasHungry=true;
    p->lastExperience=-99999999;p->respawnInvulnerabilityTicks=60;p->chatColours=true;
    int64_t time;
    if(!d->currentTimeMillis||!EFFECT(d->currentTimeMillis(context,&time)))goto done;
    p->playerLastActiveTime=time;p->spectatingEntity=NULL;MCObjectHeap_touch(heap);
    /* A null interactionManager fails here, after parent and initializers. */
    if(!ItemInWorldManager_isInstance((MCObject *)manager))goto done;
    manager->thisPlayerMP=p;MCObjectHeap_touch(heap);
    p->theItemInWorldManager=manager;MCObjectHeap_touch(heap);
    if(!world||!d->getSpawnPoint)goto done;
    BlockPos *position=d->getSpawnPoint(context,world);
    if(MCObjectHeap_failed(heap)||!REF(position))goto done;
    if(!d->getProvider)goto done;
    MCObject *provider=d->getProvider(context,world);
    if(!provider||MCObjectHeap_failed(heap)||!REF(provider)||!d->getHasNoSky)goto done;
    bool noSky;
    if(!EFFECT(d->getHasNoSky(context,provider,&noSky)))goto done;
    bool ordinary=false;
    if(!noSky) {
        if(!d->getWorldInfo)goto done;
        MCObject *info=d->getWorldInfo(context,world);
        if(!info||MCObjectHeap_failed(heap)||!REF(info)||!d->getWorldGameType)goto done;
        const WorldSettingsGameType *type=NULL;
        if(!EFFECT(d->getWorldGameType(context,info,&type)))goto done;
        ordinary=type!=&WorldSettingsGameType_ADVENTURE;
    }
    if(ordinary) {
        int32_t protection;
        if(!server||!d->getSpawnProtectionSize||!EFFECT(d->getSpawnProtectionSize(context,server,&protection)))goto done;
        uint32_t bits=(uint32_t)protection-6;
        int32_t range=bits<=INT32_MAX?(int32_t)bits:-1-(int32_t)(UINT32_MAX-bits);
        if(range<5)range=5;
        if(!d->getWorldBorder)goto done;
        MCObject *border=d->getWorldBorder(context,world);
        if(!border||MCObjectHeap_failed(heap)||!REF(border)||!BlockPos_isInstance((MCObject *)position)||!d->getClosestDistance)goto done;
        double distance;
        int32_t positionX,positionZ;
        if(Vec3i_getX(&position->vec3i,&positionX)!=NATIVE_ARRAY_OK||Vec3i_getZ(&position->vec3i,&positionZ)!=NATIVE_ARRAY_OK)goto done;
        if(!EFFECT(d->getClosestDistance(context,border,(double)positionX,(double)positionZ,&distance)))goto done;
        int32_t closest=MathHelper_floor_double(distance);
        if(closest<range)range=closest;
        if(closest<=1)range=1;
        bits=(uint32_t)range*2;
        int32_t bound=bits<=INT32_MAX?(int32_t)bits:-1-(int32_t)(UINT32_MAX-bits);
        /* Java captures the add receiver before reading this.rand for each
           virtual bounded draw. The top-solid world receiver is worldIn. */
        BlockPos *addReceiver=position;
        int32_t dx,dz;
        NativeJavaRandom *rand=p->player.living.entity.rand;
        if(!d->nextInt||!rand||!REF(rand)||!EFFECT(d->nextInt(context,rand,bound,&dx)))goto done;
        bits=(uint32_t)dx-(uint32_t)range;
        dx=bits<=INT32_MAX?(int32_t)bits:-1-(int32_t)(UINT32_MAX-bits);
        rand=p->player.living.entity.rand;
        if(!rand||!REF(rand)||!EFFECT(d->nextInt(context,rand,bound,&dz)))goto done;
        bits=(uint32_t)dz-(uint32_t)range;
        dz=bits<=INT32_MAX?(int32_t)bits:-1-(int32_t)(UINT32_MAX-bits);
        BlockPos *added=BlockPos_add(addReceiver,dx,0,dz);
        if(!added||!REF(added)||!d->getTopSolidOrLiquidBlock)goto done;
        position=d->getTopSolidOrLiquidBlock(context,world,added);
        if(MCObjectHeap_failed(heap)||!REF(position))goto done;
    }
    p->mcServer=server;MCObjectHeap_touch(heap);
    if(!server||!d->getConfigurationManager)goto done;
    MCObject *configuration=d->getConfigurationManager(context,server);
    if(!configuration||MCObjectHeap_failed(heap)||!REF(configuration)||!d->getPlayerStatsFile)goto done;
    StatisticsFile *stats=d->getPlayerStatsFile(context,configuration,p);
    if(MCObjectHeap_failed(heap)||!REF(stats))goto done;
    p->statsFile=stats;MCObjectHeap_touch(heap);
    p->player.living.entity.stepHeight=0.0f;MCObjectHeap_touch(heap);
    if(!d->moveToBlockPosAndAngles||!EFFECT(d->moveToBlockPosAndAngles(context,p,position,0.0f,0.0f)))goto done;
    for(;;) {
        if(!d->getEntityBoundingBox)goto done;
        AxisAlignedBB *box=d->getEntityBoundingBox(context,p);
        if(MCObjectHeap_failed(heap)||!REF(box)||!d->getCollidingBoundingBoxes)goto done;
        MCObject *collisions=d->getCollidingBoundingBoxes(context,world,p,box);
        if(!collisions||MCObjectHeap_failed(heap)||!REF(collisions)||!d->isCollisionListEmpty)goto done;
        bool empty;
        if(!EFFECT(d->isCollisionListEmpty(context,collisions,&empty)))goto done;
        if(empty||!(p->player.living.entity.posY<255.0))break;
        double x=p->player.living.entity.posX,y=p->player.living.entity.posY+1.0,z=p->player.living.entity.posZ;
        if(!d->setPosition||!EFFECT(d->setPosition(context,p,x,y,z)))goto done;
    }
    ok=true;
done:
    if(!ok)failed(p);
    MCObjectRootScope_end(&scope);
#undef REF
#undef EFFECT
    return ok&&!MCObjectHeap_failed(heap);
}
