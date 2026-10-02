#include "entity/item/EntityItemFrame.h"
#include "item/ItemStackFrame.h"
#include "nbt/NBTTagCompound.h"
#include "world/World.h"

static bool same(const MCObject *o,void *context){return o==context;}
static bool tracked(MCObjectHeap *h,MCObject *o) {
    return !o||(o->heap==h&&MCObjectHeap_findObject(h,o->klass,same,o)==o);
}
static void trace(MCObject *o,MCObjectVisitor v,void *context) {
    if(MCObjectHeap_objectSize(o)<sizeof(EntityItemFrame)){MCObjectHeap_fail(o->heap);return;}
    EntityItemFrame *f=(EntityItemFrame *)o;
    EntityHanging_traceFields(&f->hanging,v,context);
    f->nativeContext=v(f->nativeContext,context);
}
static const MCObjectClass klass={"net.minecraft.entity.item.EntityItemFrame",MCObjectHeap_plainClone,trace,NULL};
bool EntityItemFrame_isInstance(const MCObject *o) {
    return o&&o->klass==&klass&&MCObjectHeap_objectSize(o)>=sizeof(EntityItemFrame);
}
static const NativeJavaClassDescriptor *const parents[]={&EntityHanging_Class};
const NativeJavaClassDescriptor EntityItemFrame_Class={"net.minecraft.entity.item.EntityItemFrame",parents,1,EntityItemFrame_isInstance};
NativeJavaClass *EntityItemFrame_nativeClass(MCObjectHeap *h){return NativeJavaClass_literal(h,&EntityItemFrame_Class);}
static EntityFrameResult fail(EntityItemFrame *f) {
    MCObjectHeap_fail(f?f->hanging.entity.object.heap:NULL);return ENTITY_FRAME_FAILURE;
}
static bool pin(EntityItemFrame *f,MCObjectRootScope *s,MCObject *o) {
    return tracked(f->hanging.entity.object.heap,o)&&MCObjectRootScope_pin(s,o);
}
static bool pin_stack(EntityItemFrame *f,MCObjectRootScope *s,ItemStack *item) {
    return pin(f,s,(MCObject *)item)&&(!item||(ItemStack_isInstance((MCObject *)item)&&
        MCObjectHeap_objectSize((MCObject *)item)>=sizeof(ItemStack)));
}
static bool begin(EntityItemFrame *f,MCObjectRootScope *s) {
    if(!f||!tracked(f->hanging.entity.object.heap,(MCObject *)f)||!EntityItemFrame_isInstance((MCObject *)f)||
       MCObjectHeap_failed(f->hanging.entity.object.heap)||!MCObjectRootScope_begin(s,f->hanging.entity.object.heap)) {
        fail(f);return false;
    }
    if(pin(f,s,(MCObject *)f)&&pin(f,s,f->nativeContext)&&pin(f,s,f->hanging.nativeContext))return true;
    fail(f);MCObjectRootScope_end(s);return false;
}
static EntityFrameResult end(EntityItemFrame *f,MCObjectRootScope *s,EntityFrameResult result) {
    MCObjectHeap *h=f->hanging.entity.object.heap;
    if((result!=ENTITY_FRAME_OK&&result!=ENTITY_FRAME_EXCEPTION&&result!=ENTITY_FRAME_FAILURE)||
       !tracked(h,f->nativeContext)||!tracked(h,f->hanging.nativeContext)||
       !tracked(h,f->hanging.entity.entityContext)||MCObjectHeap_failed(h))result=fail(f);
    if(result==ENTITY_FRAME_FAILURE)fail(f);
    MCObjectRootScope_end(s);return result;
}
EntityItemFrame *EntityItemFrame_nativeAllocate(MCObjectHeap *h,const EntityHangingDependencies *d,
    MCObject *context,const EntityItemFrameDependencies *f,MCObject *frameContext) {
    if(!tracked(h,context)||!tracked(h,frameContext)){MCObjectHeap_fail(h);return NULL;}
    EntityItemFrame *out=(EntityItemFrame *)MCObjectHeap_alloc(h,sizeof(*out),&klass);
    if(out){out->hanging.nativeDependencies=d;out->hanging.nativeContext=context;out->nativeDependencies=f;out->nativeContext=frameContext;}
    return out;
}
static bool init(MCObject *context,Entity *e){(void)context;return EntityItemFrame_entityInit((EntityItemFrame *)e);}
static bool position(MCObject *context,Entity *e,double x,double y,double z) {
    (void)context;return EntityHanging_setPosition((EntityHanging *)e,x,y,z)==ENTITY_FRAME_OK;
}
static bool bounds(MCObject *context,Entity *e,AxisAlignedBB *box){(void)context;return Entity_setEntityBoundingBox(e,box);}
static bool dimension(MCObject *context,MCObject *world,int32_t *out) {
    (void)context;if(!world||!World_isInstance(world)||!out){MCObjectHeap_fail(world?world->heap:NULL);return false;}
    WorldProvider *p=((World *)world)->provider;
    if(!p||!WorldProvider_isInstance((MCObject *)p)||p->object.heap!=world->heap){MCObjectHeap_fail(world->heap);return false;}
    *out=WorldProvider_getDimensionId(p);return !MCObjectHeap_failed(world->heap);
}
static bool remote(MCObject *context,MCObject *world,bool *out) {
    (void)context;if(!world||!World_isInstance(world)||!out){MCObjectHeap_fail(world?world->heap:NULL);return false;}
    *out=((World *)world)->isRemote;return !MCObjectHeap_failed(world->heap);
}
static const EntityDependencies entity_dependencies={.entityInit=init,.setPosition=position,
    .setEntityBoundingBox=bounds,.getDimensionId=dimension,.isRemote=remote};
const EntityDependencies *EntityItemFrame_nativeEntityDependencies(void){return &entity_dependencies;}
EntityFrameResult EntityItemFrame_construct(EntityItemFrame *f,World *w,const EntityDependencies *d,
    MCObject *context,NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    MCObjectRootScope s={0};if(!begin(f,&s))return ENTITY_FRAME_FAILURE;
    EntityFrameResult r=EntityHanging_construct(&f->hanging,w,d?d:&entity_dependencies,context,random,ids);
    if(r==ENTITY_FRAME_OK){f->itemDropChance=1.0f;MCObjectHeap_touch(f->hanging.entity.object.heap);}
    return end(f,&s,r);
}
EntityFrameResult EntityItemFrame_constructPosition(EntityItemFrame *f,World *w,BlockPos *p,
    const NativeHangingFacing *direction,const EntityDependencies *d,MCObject *context,
    NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    MCObjectRootScope s={0};if(!begin(f,&s))return ENTITY_FRAME_FAILURE;
    EntityFrameResult r=EntityHanging_constructPosition(&f->hanging,w,p,d?d:&entity_dependencies,context,random,ids);
    if(r==ENTITY_FRAME_OK) {
        f->itemDropChance=1.0f;MCObjectHeap_touch(f->hanging.entity.object.heap);
        r=EntityHanging_updateFacingWithBoundingBox(&f->hanging,direction);
    }
    return end(f,&s,r);
}
static EntityFrameResult watcher(EntityItemFrame *f,MCObjectRootScope *s,DataWatcher **out) {
    DataWatcher *w=Entity_getDataWatcher(&f->hanging.entity);
    if(MCObjectHeap_failed(f->hanging.entity.object.heap))return ENTITY_FRAME_FAILURE;
    if(!w)return ENTITY_FRAME_EXCEPTION;
    if(!pin(f,s,(MCObject *)w)||!DataWatcher_isInstance((MCObject *)w))return fail(f);
    *out=w;return ENTITY_FRAME_OK;
}
bool EntityItemFrame_entityInit(EntityItemFrame *f) {
    MCObjectRootScope s={0};if(!begin(f,&s))return false;
    DataWatcher *w=NULL;EntityFrameResult r=watcher(f,&s,&w);
    if(r==ENTITY_FRAME_OK&&!DataWatcher_addObjectByDataType(w,8,5))r=ENTITY_FRAME_FAILURE;
    if(r==ENTITY_FRAME_OK) {
        /* Source evaluates the second getDataWatcher receiver before Byte.valueOf. */
        r=watcher(f,&s,&w);
        MCObject *zero=r==ENTITY_FRAME_OK?DataWatcher_boxByte(f->hanging.entity.object.heap,0):NULL;
        if(r==ENTITY_FRAME_OK&&(!zero||!DataWatcher_addObject(w,9,zero)))r=ENTITY_FRAME_FAILURE;
    }
    /* EntityDependencies bool cannot carry healthy Java exceptions. */
    if(r!=ENTITY_FRAME_OK)r=fail(f);
    return end(f,&s,r)==ENTITY_FRAME_OK;
}
int32_t EntityItemFrame_getWidthPixels(EntityItemFrame *f) {
    if(!f||!tracked(f->hanging.entity.object.heap,(MCObject *)f)||!EntityItemFrame_isInstance((MCObject *)f)||MCObjectHeap_failed(f->hanging.entity.object.heap)){fail(f);return 0;}
    return 12;
}
int32_t EntityItemFrame_getHeightPixels(EntityItemFrame *f) {
    if(!f||!tracked(f->hanging.entity.object.heap,(MCObject *)f)||!EntityItemFrame_isInstance((MCObject *)f)||MCObjectHeap_failed(f->hanging.entity.object.heap)){fail(f);return 0;}
    return 12;
}
float EntityItemFrame_getCollisionBorderSize(EntityItemFrame *f) {
    if(!f||!tracked(f->hanging.entity.object.heap,(MCObject *)f)||!EntityItemFrame_isInstance((MCObject *)f)||MCObjectHeap_failed(f->hanging.entity.object.heap))fail(f);
    return 0.0f;
}
EntityFrameResult EntityItemFrame_getDisplayedItem(EntityItemFrame *f,ItemStack **out) {
    MCObjectRootScope s={0};if(!begin(f,&s))return ENTITY_FRAME_FAILURE;
    EntityFrameResult r=ENTITY_FRAME_FAILURE;DataWatcher *w=NULL;ItemStack *item=NULL;
    if(!out)goto done;
    r=watcher(f,&s,&w);if(r!=ENTITY_FRAME_OK)goto done;
    item=DataWatcher_getWatchableObjectItemStack(w,8);
    if(!pin_stack(f,&s,item)||MCObjectHeap_failed(f->hanging.entity.object.heap))r=ENTITY_FRAME_FAILURE;
done:r=end(f,&s,r);if(r==ENTITY_FRAME_OK)*out=item;return r;
}
/* Called only when the Source conditional has reached the world invocation.
   Capture world/position before resolving Blocks.air and invoking the native
   required leaf; callbacks may mutate current fields and contexts. */
static EntityFrameResult comparator(EntityItemFrame *f,MCObjectRootScope *s) {
    World *world=(World *)f->hanging.entity.worldObj;BlockPos *pos=f->hanging.hangingPosition;
    if(!pin(f,s,(MCObject *)world)||!pin(f,s,(MCObject *)pos)||
       (world&&!World_isInstance((MCObject *)world))||(pos&&!BlockPos_isInstance((MCObject *)pos)))return fail(f);
    NativeBlockStateRuntime *registry=NativeBlockStateRuntime_get(f->hanging.entity.object.heap);
    if(!registry)return fail(f);
    NativeBlock *air=registry->air;
    if(!world)return ENTITY_FRAME_EXCEPTION;
    const EntityItemFrameDependencies *d=f->nativeDependencies;MCObject *context=f->nativeContext;
    if(!pin(f,s,context)||!d||!d->updateComparatorOutputLevel)return fail(f);
    return d->updateComparatorOutputLevel(context,world,pos,air);
}
static EntityFrameResult displayed(EntityItemFrame *f,ItemStack *item,bool update) {
    MCObjectRootScope s={0};if(!begin(f,&s))return ENTITY_FRAME_FAILURE;
    EntityFrameResult r=ENTITY_FRAME_FAILURE;MCObjectHeap *heap=f->hanging.entity.object.heap;
    if(!pin_stack(f,&s,item))goto done;
    if(item) {
        item=ItemStack_copy(heap,item);if(!item||!pin_stack(f,&s,item))goto done;
        item->stackSize=1;MCObjectHeap_touch(heap);
        if(!ItemStack_setItemFrame(item,f))goto done;
    }
    DataWatcher *w=NULL;r=watcher(f,&s,&w);if(r!=ENTITY_FRAME_OK)goto done;
    if(!DataWatcher_updateObject(w,8,(MCObject *)item)){r=ENTITY_FRAME_FAILURE;goto done;}
    r=watcher(f,&s,&w);if(r!=ENTITY_FRAME_OK)goto done;
    if(!DataWatcher_setObjectWatched(w,8)){r=ENTITY_FRAME_FAILURE;goto done;}
    if(update&&f->hanging.hangingPosition)r=comparator(f,&s);
done:return end(f,&s,r);
}
EntityFrameResult EntityItemFrame_setDisplayedItem(EntityItemFrame *f,ItemStack *item){return displayed(f,item,true);}
EntityFrameResult EntityItemFrame_getRotation(EntityItemFrame *f,int32_t *out) {
    MCObjectRootScope s={0};if(!begin(f,&s))return ENTITY_FRAME_FAILURE;
    EntityFrameResult r=ENTITY_FRAME_FAILURE;DataWatcher *w=NULL;int32_t value=0;
    if(!out)goto done;
    r=watcher(f,&s,&w);if(r!=ENTITY_FRAME_OK)goto done;
    value=DataWatcher_getWatchableObjectByte(w,9);if(MCObjectHeap_failed(f->hanging.entity.object.heap))r=ENTITY_FRAME_FAILURE;
done:r=end(f,&s,r);if(r==ENTITY_FRAME_OK)*out=value;return r;
}
static EntityFrameResult rotation(EntityItemFrame *f,int32_t value,bool update) {
    MCObjectRootScope s={0};if(!begin(f,&s))return ENTITY_FRAME_FAILURE;
    EntityFrameResult r;DataWatcher *w=NULL;r=watcher(f,&s,&w);if(r!=ENTITY_FRAME_OK)goto done;
    MCObject *boxed=DataWatcher_boxByte(f->hanging.entity.object.heap,value%8);
    if(!boxed||!DataWatcher_updateObject(w,9,boxed)){r=ENTITY_FRAME_FAILURE;goto done;}
    if(update&&f->hanging.hangingPosition)r=comparator(f,&s);
done:return end(f,&s,r);
}
EntityFrameResult EntityItemFrame_setItemRotation(EntityItemFrame *f,int32_t value){return rotation(f,value,true);}
EntityFrameResult EntityItemFrame_writeEntityToNBT(EntityItemFrame *f,NBTTagCompound *tag) {
    MCObjectRootScope s={0};if(!begin(f,&s))return ENTITY_FRAME_FAILURE;
    EntityFrameResult r=ENTITY_FRAME_FAILURE;ItemStack *item=NULL;
    if(!pin(f,&s,(MCObject *)tag)||(tag&&!NBTTagCompound_isInstance((MCObject *)tag)))goto done;
    r=EntityItemFrame_getDisplayedItem(f,&item);if(r!=ENTITY_FRAME_OK)goto done;
    if(item) {
        /* Capture tag receiver before the second displayed-item getter. */
        r=EntityItemFrame_getDisplayedItem(f,&item);if(r!=ENTITY_FRAME_OK)goto done;
        NBTTagCompound *stored=NBTTagCompound_new(f->hanging.entity.object.heap);
        if(!stored||!item||!ItemStack_writeToNBT(item,stored)){r=ENTITY_FRAME_FAILURE;goto done;}
        if(!tag){r=ENTITY_FRAME_EXCEPTION;goto done;}
        if(!NBTTagCompound_setTag_ascii(tag,"Item",(NBTBase *)stored)){r=ENTITY_FRAME_FAILURE;goto done;}
        int32_t value;r=EntityItemFrame_getRotation(f,&value);if(r!=ENTITY_FRAME_OK)goto done;
        if(!NBTTagCompound_setByte_ascii(tag,"ItemRotation",(int8_t)value)||
           !NBTTagCompound_setFloat_ascii(tag,"ItemDropChance",f->itemDropChance)){r=ENTITY_FRAME_FAILURE;goto done;}
    }
    r=EntityHanging_writeEntityToNBT(&f->hanging,tag);
done:return end(f,&s,r);
}
EntityFrameResult EntityItemFrame_readEntityFromNBT(EntityItemFrame *f,NBTTagCompound *tag) {
    MCObjectRootScope s={0};if(!begin(f,&s))return ENTITY_FRAME_FAILURE;
    EntityFrameResult r=ENTITY_FRAME_FAILURE;MCObjectHeap *heap=f->hanging.entity.object.heap;
    if(!tag){r=ENTITY_FRAME_EXCEPTION;goto done;}
    if(!pin(f,&s,(MCObject *)tag)||!NBTTagCompound_isInstance((MCObject *)tag))goto done;
    NBTTagCompound *stored=NBTTagCompound_getCompoundTag_ascii(tag,"Item");
    if(MCObjectHeap_failed(heap))goto done;
    if(stored&&!NBTBase_hasNoTags((NBTBase *)stored)) {
        ItemStackNBTResult status;ItemStack *item=ItemStack_loadItemStackFromNBT(heap,stored,&status);
        if(status!=ITEMSTACK_NBT_OK){r=ENTITY_FRAME_FAILURE;goto done;}
        r=displayed(f,item,false);if(r!=ENTITY_FRAME_OK)goto done;
        int32_t value=NBTTagCompound_getByte_ascii(tag,"ItemRotation");if(MCObjectHeap_failed(heap)){r=ENTITY_FRAME_FAILURE;goto done;}
        r=rotation(f,value,false);if(r!=ENTITY_FRAME_OK)goto done;
        if(NBTTagCompound_hasKeyType_ascii(tag,"ItemDropChance",99)) {
            float chance=NBTTagCompound_getFloat_ascii(tag,"ItemDropChance");if(MCObjectHeap_failed(heap)){r=ENTITY_FRAME_FAILURE;goto done;}
            f->itemDropChance=chance;MCObjectHeap_touch(heap);
        }
        if(NBTTagCompound_hasKey_ascii(tag,"Direction")) {
            r=EntityItemFrame_getRotation(f,&value);if(r!=ENTITY_FRAME_OK)goto done;
            r=rotation(f,value*2,false);if(r!=ENTITY_FRAME_OK)goto done;
        }
    }
    r=EntityHanging_readEntityFromNBT(&f->hanging,tag);
done:return end(f,&s,r);
}
