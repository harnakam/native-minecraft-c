#include "entity/DataWatcher.h"
#include "entity/item/EntityItemFrame.h"
#include "item/ItemStackFrame.h"
#include "nbt/NBTTagCompound.h"
#include "world/World.h"
#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"item frame check %u line %d: %s\n",checks,__LINE__,#x); exit(1); } } while (0)
static bool nano(void *context,int64_t *value) { (void)context; *value=123; return true; }
static const NativeJavaRandomRuntimeDependencies clock_dependencies={nano};
typedef struct {
    MCObject object;EntityItemFrame *frame;World *world;BlockPos *position;
    NativeBlock *air;DataWatcher *captured,*replacement;MCObject *nextContext;
    unsigned events[64],count,widthCalls,heightCalls,comparatorCalls,boundsCalls;
    int mode,failEvent;EntityFrameResult comparatorResult;
} Witness;
enum { EV_POSITION=1,EV_DIMENSION,EV_INIT,EV_BOUNDS,EV_WIDTH,EV_HEIGHT,EV_WATCH,EV_COMPARATOR };
static void witness_trace(MCObject *o,MCObjectVisitor v,void *c) {
    Witness *w=(Witness *)o;
    w->frame=(EntityItemFrame *)v((MCObject *)w->frame,c);w->world=(World *)v((MCObject *)w->world,c);
    w->position=(BlockPos *)v((MCObject *)w->position,c);w->air=(NativeBlock *)v((MCObject *)w->air,c);
    w->captured=(DataWatcher *)v((MCObject *)w->captured,c);w->replacement=(DataWatcher *)v((MCObject *)w->replacement,c);
    w->nextContext=v(w->nextContext,c);
}
static const MCObjectClass witness_class={"fixture.itemFrame.dependencies",MCObjectHeap_plainClone,witness_trace,NULL};
static Witness *witness(MCObjectHeap *h) {
    Witness *w=(Witness *)MCObjectHeap_alloc(h,sizeof(*w),&witness_class);CHECK(w);return w;
}
static bool event(Witness *w,unsigned n) {CHECK(w->count<64);w->events[w->count++]=n;return w->failEvent!=(int)n;}
static bool position(MCObject *context,Entity *e,double x,double y,double z) {
    Witness *w=(Witness *)context;
    CHECK(e==(Entity *)w->frame);
    return event(w,EV_POSITION)&&EntityHanging_setPosition(&w->frame->hanging,x,y,z)==ENTITY_FRAME_OK;
}
static bool bounds(MCObject *context,Entity *e,AxisAlignedBB *box) {
    Witness *w=(Witness *)context;++w->boundsCalls;
    return event(w,EV_BOUNDS)&&Entity_setEntityBoundingBox(e,box);
}
static bool dimension(MCObject *context,MCObject *world,int32_t *out) {
    Witness *w=(Witness *)context;CHECK(world==(MCObject *)w->world);
    if(!event(w,EV_DIMENSION))return false;
    *out=42;return true;
}
static bool init(MCObject *context,Entity *e) {
    Witness *w=(Witness *)context;EntityItemFrame *f=(EntityItemFrame *)e;
    CHECK(e==(Entity *)w->frame&&f->itemDropChance==0&&e->width==0.6f&&e->height==1.8f);
    CHECK(e->rand&&e->entityUniqueID&&e->cmdResultStats&&e->dataWatcher);
    CHECK(WatchableObjectList_size(DataWatcher_getAllWatched(e->dataWatcher))==5);
    f->hanging.tickCounter1=77;f->itemDropChance=9;
    MCObjectHeap_touch(context->heap);
    return event(w,EV_INIT)&&EntityItemFrame_entityInit(f);
}
static bool on_watch(MCObject *context,MCObject *owner,int32_t id) {
    Witness *w=(Witness *)context;CHECK(owner==(MCObject *)w->frame&&id>=0);
    if(!event(w,EV_WATCH))return false;
    if(w->mode==3&&id==8)w->frame->hanging.hangingPosition=NULL;
    if(w->mode==4&&id==8)w->frame->hanging.entity.dataWatcher=w->replacement;
    MCObjectHeap_touch(context->heap);return true;
}
static const DataWatcherDependencies watcher_dependencies={.onDataWatcherUpdate=on_watch};
static const EntityDependencies entity_dependencies={.entityInit=init,.setPosition=position,
    .setEntityBoundingBox=bounds,.getDimensionId=dimension,.watcher=&watcher_dependencies};
static EntityFrameResult comparator(MCObject *context,World *world,BlockPos *p,NativeBlock *air) {
    Witness *w=(Witness *)context;++w->comparatorCalls;
    CHECK(world==w->world&&p==w->position&&air&&air->registeredId==0);
    w->air=air;MCObjectHeap_touch(context->heap);
    CHECK(!MCObjectHeap_collect(context->heap));
    if(!event(w,EV_COMPARATOR))return ENTITY_FRAME_FAILURE;
    if(w->mode==5){w->frame->nativeContext=w->nextContext;MCObjectHeap_touch(context->heap);}
    return w->comparatorResult;
}
static const EntityItemFrameDependencies frame_dependencies={comparator};
static bool pixels(MCObject *context,EntityHanging *h,bool height,int32_t *out) {
    Witness *w=(Witness *)context;CHECK(h==&w->frame->hanging);
    if(height)++w->heightCalls;else ++w->widthCalls;
    if(!event(w,height?EV_HEIGHT:EV_WIDTH))return false;
    if(w->mode==1) {
        if(height)*out=w->heightCalls==1?32:16;
        else {*out=w->widthCalls==2?64:32;if(w->widthCalls==1)h->facingDirection=&NativeHangingFacing_SOUTH;}
    } else *out=12;
    if(w->mode==6){h->nativeContext=w->nextContext;MCObjectHeap_touch(context->heap);}
    return true;
}
static bool width(MCObject *c,EntityHanging *h,int32_t *out){return pixels(c,h,false,out);}
static bool height(MCObject *c,EntityHanging *h,int32_t *out){return pixels(c,h,true,out);}
static const EntityHangingDependencies hanging_dependencies={width,height};
static EntityItemFrame *recording_frame(MCObjectHeap *heap,Witness *w,NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids,bool positioned) {
    w->frame=EntityItemFrame_nativeAllocate(heap,&hanging_dependencies,(MCObject *)w,&frame_dependencies,(MCObject *)w);
    CHECK(w->frame);
    w->world=World_nativeAllocate(heap,NULL,NULL);CHECK(w->world);
    w->position=DataWatcher_blockPos(heap,1,2,3);CHECK(w->position);
    EntityFrameResult r=positioned?EntityItemFrame_constructPosition(w->frame,w->world,w->position,&NativeHangingFacing_NORTH,
        &entity_dependencies,(MCObject *)w,random,ids):EntityItemFrame_construct(w->frame,w->world,&entity_dependencies,(MCObject *)w,random,ids);
    CHECK(r==ENTITY_FRAME_OK);
    if(!positioned)w->position=w->frame->hanging.hangingPosition;
    return w->frame;
}
static void clear_dirty(DataWatcher *w) {
    WatchableObjectList *all=DataWatcher_getAllWatched(w);CHECK(all);
    for(int32_t i=0;i<WatchableObjectList_size(all);i++)WatchableObject_setWatched(WatchableObjectList_get(all,i),false);
    DataWatcher_func_111144_e(w);
}
static void constructor_defaults(NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    MCObjectHeap *heap=MCObjectHeap_new(64u*1024u*1024u);
    EntityItemFrame *frame=EntityItemFrame_nativeAllocate(heap,NULL,NULL,NULL,NULL);
    CHECK(frame&&random&&ids);
    CHECK(MCObjectHeap_liveObjects(heap)==1); /* actual NEW does not allocate a Class literal */
    NativeJavaClass *runtimeClass=NativeJavaClass_getClass(heap,(MCObject *)frame);
    CHECK(runtimeClass&&runtimeClass->descriptor==&EntityItemFrame_Class);
    CHECK(EntityItemFrame_construct(frame,NULL,NULL,NULL,random,ids)==ENTITY_FRAME_OK);
    CHECK(frame->itemDropChance==1.0f&&frame->hanging.entity.width==0.5f&&frame->hanging.entity.height==0.5f);
    NativeJavaClass *concrete=EntityItemFrame_nativeClass(heap);
    NativeJavaClass *hanging=EntityHanging_nativeClass(heap);
    CHECK(hanging&&concrete&&NativeJavaClass_getClass(heap,(MCObject *)frame)==concrete);
    bool assignable=false;
    CHECK(NativeJavaClass_isAssignableFrom(NativeJavaClass_Entity(heap),concrete,&assignable)&&assignable);
    CHECK(NativeJavaClass_isAssignableFrom(hanging,concrete,&assignable)&&assignable);
    CHECK(Entity_isInstance((MCObject *)frame)&&EntityHanging_isInstance((MCObject *)frame));
    CHECK(frame->hanging.tickCounter1==0&&frame->hanging.facingDirection==NULL);
    BlockPos *p=EntityHanging_getHangingPosition(&frame->hanging);
    CHECK(p&&p->vec3i.x==0&&p->vec3i.y==0&&p->vec3i.z==0&&frame->hanging.entity.isAirBorne);
    AxisAlignedBB *b=Entity_getEntityBoundingBox(&frame->hanging.entity);
    CHECK(b&&b->minX==0&&b->minY==0&&b->minZ==0&&b->maxX==0.5&&b->maxY==0.5&&b->maxZ==0.5);
    DataWatcher *d=frame->hanging.entity.dataWatcher;
    CHECK(WatchableObjectList_size(DataWatcher_getAllWatched(d))==7);
    CHECK(WatchableObject_getObjectType(DataWatcher_nativeGetWatchedObject(d,8))==5);
    ItemStack *item=(ItemStack *)frame;int32_t rotation=123;
    CHECK(EntityItemFrame_getDisplayedItem(frame,&item)==ENTITY_FRAME_OK&&item==NULL);
    CHECK(EntityItemFrame_getRotation(frame,&rotation)==ENTITY_FRAME_OK&&rotation==0);
    CHECK(EntityItemFrame_getWidthPixels(frame)==12&&EntityItemFrame_getHeightPixels(frame)==12&&EntityItemFrame_getCollisionBorderSize(frame)==0);
    CHECK(NativeHangingFacing_getHorizontal(INT32_MIN)==&NativeHangingFacing_SOUTH);
    CHECK(NativeHangingFacing_getHorizontal(-3)==&NativeHangingFacing_EAST);
    const NativeHangingFacing invalid={0};CHECK(!NativeHangingFacing_isKnown(&invalid));
    CHECK(!NativeHangingFacing_rotateYCCW(&NativeHangingFacing_UP));
    MCObjectHeap_free(heap);
}
static void bounds_cases(NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    struct Case {const NativeHangingFacing *direction;float yaw;double x,z,minX,maxX,minZ,maxZ;} cases[]={
        {&NativeHangingFacing_NORTH,180,-1.5,7.96875,-1.875,-1.125,7.9375,8},
        {&NativeHangingFacing_SOUTH,0,-1.5,7.03125,-1.875,-1.125,7,7.0625},
        {&NativeHangingFacing_WEST,90,-1.03125,7.5,-1.0625,-1,7.125,7.875},
        {&NativeHangingFacing_EAST,270,-1.96875,7.5,-2,-1.9375,7.125,7.875}
    };
    for(size_t i=0;i<sizeof(cases)/sizeof(*cases);i++) {
        MCObjectHeap *h=MCObjectHeap_new(1u<<20);BlockPos *p=DataWatcher_blockPos(h,-2,64,7);
        EntityItemFrame *f=EntityItemFrame_nativeAllocate(h,NULL,NULL,NULL,NULL);CHECK(f&&p);
        CHECK(EntityItemFrame_constructPosition(f,NULL,p,cases[i].direction,NULL,NULL,random,ids)==ENTITY_FRAME_OK);
        CHECK(f->hanging.hangingPosition==p&&f->hanging.facingDirection==cases[i].direction);
        CHECK(f->hanging.entity.posX==cases[i].x&&f->hanging.entity.posY==64.5&&f->hanging.entity.posZ==cases[i].z);
        CHECK(f->hanging.entity.rotationYaw==cases[i].yaw&&f->hanging.entity.prevRotationYaw==cases[i].yaw);
        AxisAlignedBB *b=f->hanging.entity.boundingBox;
        CHECK(b->minX==cases[i].minX&&b->maxX==cases[i].maxX&&b->minY==64.125&&b->maxY==64.875&&b->minZ==cases[i].minZ&&b->maxZ==cases[i].maxZ);
        AxisAlignedBB *old=b;
        CHECK(EntityHanging_updateFacingWithBoundingBox(&f->hanging,NULL)==ENTITY_FRAME_EXCEPTION&&!MCObjectHeap_failed(h));
        CHECK(EntityHanging_updateFacingWithBoundingBox(&f->hanging,&NativeHangingFacing_UP)==ENTITY_FRAME_EXCEPTION&&!MCObjectHeap_failed(h));
        CHECK(f->hanging.entity.boundingBox==old&&f->hanging.facingDirection==cases[i].direction);
        BlockPos *before=f->hanging.hangingPosition;f->hanging.entity.isAirBorne=false;
        CHECK(EntityHanging_setPosition(&f->hanging,-1.1,64.8,7.2)==ENTITY_FRAME_OK);
        CHECK(f->hanging.hangingPosition!=before&&f->hanging.hangingPosition->vec3i.x==-2&&f->hanging.hangingPosition->vec3i.y==64&&f->hanging.hangingPosition->vec3i.z==7);
        CHECK(f->hanging.entity.boundingBox==old&&!f->hanging.entity.isAirBorne&&f->hanging.entity.posX==-1.1);
        CHECK(EntityHanging_setPosition(&f->hanging,4,8,9)==ENTITY_FRAME_OK&&f->hanging.entity.isAirBorne);
        CHECK(f->hanging.entity.boundingBox!=old);
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h=MCObjectHeap_new(1u<<20);EntityItemFrame *f=EntityItemFrame_nativeAllocate(h,NULL,NULL,NULL,NULL);
    CHECK(EntityItemFrame_constructPosition(f,NULL,NULL,&NativeHangingFacing_NORTH,NULL,NULL,random,ids)==ENTITY_FRAME_EXCEPTION);
    CHECK(!MCObjectHeap_failed(h)&&f->itemDropChance==1&&f->hanging.facingDirection==&NativeHangingFacing_NORTH&&f->hanging.entity.rotationYaw==180);
    CHECK(f->hanging.entity.boundingBox->maxX==0.5&&f->hanging.hangingPosition==NULL);
    MCObjectHeap_free(h);
}
static void callback_order(NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    MCObjectHeap *h=MCObjectHeap_new(1u<<20);Witness *w=witness(h);
    EntityItemFrame *f=recording_frame(h,w,random,ids,true);
    CHECK(f->hanging.tickCounter1==77&&f->itemDropChance==1&&f->hanging.entity.dimension==42);
    const unsigned expected[]={EV_POSITION,EV_DIMENSION,EV_INIT,EV_BOUNDS,EV_WIDTH,EV_HEIGHT,EV_WIDTH,EV_HEIGHT,EV_WIDTH,EV_BOUNDS};
    CHECK(w->count==sizeof(expected)/sizeof(*expected)&&memcmp(w->events,expected,sizeof(expected))==0);
    w->count=w->widthCalls=w->heightCalls=0;w->mode=1;
    CHECK(EntityHanging_updateFacingWithBoundingBox(&f->hanging,&NativeHangingFacing_NORTH)==ENTITY_FRAME_OK);
    CHECK(w->widthCalls==3&&w->heightCalls==2&&f->hanging.facingDirection==&NativeHangingFacing_SOUTH);
    CHECK(f->hanging.entity.rotationYaw==180&&f->hanging.entity.posX==2&&f->hanging.entity.posY==3&&f->hanging.entity.posZ==3.03125);
    AxisAlignedBB *b=f->hanging.entity.boundingBox;
    CHECK(b->minX==0&&b->maxX==4&&b->minY==2.5&&b->maxY==3.5&&b->minZ==3&&b->maxZ==3.0625);
    MCObjectHeap_free(h);
    for(int failed=EV_POSITION;failed<=EV_BOUNDS;failed++) {
        h=MCObjectHeap_new(1u<<20);w=witness(h);w->failEvent=failed;
        w->frame=EntityItemFrame_nativeAllocate(h,NULL,NULL,NULL,NULL);w->world=World_nativeAllocate(h,NULL,NULL);CHECK(w->frame&&w->world);
        CHECK(EntityItemFrame_construct(w->frame,w->world,&entity_dependencies,(MCObject *)w,random,ids)==ENTITY_FRAME_FAILURE&&MCObjectHeap_failed(h));
        CHECK(w->events[w->count-1]==(unsigned)failed);
        CHECK(w->frame->itemDropChance==(failed>=EV_INIT?9.0f:0.0f));
        CHECK(w->frame->hanging.entity.width==(failed==EV_BOUNDS?0.5f:0.6f));
        MCObjectHeap_free(h);
    }
}
static void displayed_and_rotation(NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    MCObjectHeap *h=MCObjectHeap_new(64u<<20);Witness *w=witness(h);
    EntityItemFrame *f=recording_frame(h,w,random,ids,true);
    ItemStack *original=ItemStack_new(h,ItemStack_registryItem(358),-4,7);CHECK(original);
    NBTTagCompound *tag=NBTTagCompound_new(h);CHECK(tag&&NBTTagCompound_setInteger_ascii(tag,"custom",17));
    CHECK(ItemStack_setTagCompound(original,tag));w->count=0;
    CHECK(EntityItemFrame_setDisplayedItem(f,original)==ENTITY_FRAME_OK);
    ItemStack *stored=NULL;CHECK(EntityItemFrame_getDisplayedItem(f,&stored)==ENTITY_FRAME_OK&&stored&&stored!=original);
    CHECK(stored->stackSize==1&&stored->itemDamage==7&&stored->itemFrame==f&&ItemStack_isOnItemFrame(stored));
    CHECK(original->stackSize==-4&&original->itemFrame==NULL&&stored->stackTagCompound!=tag);
    CHECK(NBTBase_equals((NBTBase *)stored->stackTagCompound,(NBTBase *)tag));
    CHECK(w->events[0]==EV_WATCH&&w->events[1]==EV_COMPARATOR&&w->comparatorCalls==1);
    CHECK(ItemStack_getItemFrame(stored)==f);
    ItemStack *copy=ItemStack_copy(h,stored);CHECK(copy&&copy->itemFrame==NULL);
    int32_t values[]={INT32_MIN,-17,-8,-1,0,7,8,9,INT32_MAX};
    int32_t expected[]={0,-1,0,-1,0,7,0,1,7};
    for(size_t i=0;i<sizeof(values)/sizeof(*values);i++) {
        CHECK(EntityItemFrame_setItemRotation(f,values[i])==ENTITY_FRAME_OK);
        int32_t rotation=55;CHECK(EntityItemFrame_getRotation(f,&rotation)==ENTITY_FRAME_OK&&rotation==expected[i]);
    }
    DataWatcher *watch=f->hanging.entity.dataWatcher;clear_dirty(watch);w->count=0;
    CHECK(EntityItemFrame_setItemRotation(f,7)==ENTITY_FRAME_OK&&!DataWatcher_hasObjectChanged(watch));
    CHECK(w->count==1&&w->events[0]==EV_COMPARATOR);
    CHECK(EntityItemFrame_setDisplayedItem(f,NULL)==ENTITY_FRAME_OK&&DataWatcher_hasObjectChanged(watch));
    CHECK(EntityItemFrame_getDisplayedItem(f,&copy)==ENTITY_FRAME_OK&&copy==NULL&&stored->itemFrame==f);
    clear_dirty(watch);w->count=0;
    CHECK(EntityItemFrame_setDisplayedItem(f,NULL)==ENTITY_FRAME_OK&&DataWatcher_hasObjectChanged(watch));
    CHECK(w->count==1&&w->events[0]==EV_COMPARATOR); /* mark watched is unconditional for 8 */
    w->mode=3;w->count=0;
    CHECK(EntityItemFrame_setDisplayedItem(f,original)==ENTITY_FRAME_OK&&f->hanging.hangingPosition==NULL);
    CHECK(w->count==1&&w->events[0]==EV_WATCH);
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(64u<<20);w=witness(h);f=recording_frame(h,w,random,ids,true);
    w->captured=f->hanging.entity.dataWatcher;
    w->replacement=DataWatcher_new(h,(MCObject *)f,&watcher_dependencies,(MCObject *)w);CHECK(w->replacement);
    CHECK(DataWatcher_addObjectByDataType(w->replacement,8,5));
    CHECK(DataWatcher_addObject(w->replacement,9,DataWatcher_boxByte(h,0)));
    clear_dirty(w->replacement);w->mode=4;w->count=0;
    original=ItemStack_new(h,ItemStack_registryItem(1),9,0);CHECK(original);
    CHECK(EntityItemFrame_setDisplayedItem(f,original)==ENTITY_FRAME_OK);
    CHECK(f->hanging.entity.dataWatcher==w->replacement&&DataWatcher_hasObjectChanged(w->replacement));
    CHECK(WatchableObject_getObject(DataWatcher_nativeGetWatchedObject(w->captured,8))!=NULL);
    CHECK(WatchableObject_getObject(DataWatcher_nativeGetWatchedObject(w->replacement,8))==NULL);
    CHECK(EntityItemFrame_getDisplayedItem(f,&stored)==ENTITY_FRAME_OK&&stored==NULL);
    MCObjectHeap_free(h);
}
static void nbt_and_lifetime(NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    MCObjectHeap *h=MCObjectHeap_new(64u<<20);Witness *w=witness(h);
    EntityItemFrame *f=recording_frame(h,w,random,ids,true);
    ItemStack *s=ItemStack_new(h,ItemStack_registryItem(358),9,23);NBTTagCompound *custom=NBTTagCompound_new(h);
    CHECK(s&&custom&&NBTTagCompound_setInteger_ascii(custom,"marker",123)&&ItemStack_setTagCompound(s,custom));
    CHECK(EntityItemFrame_setDisplayedItem(f,s)==ENTITY_FRAME_OK&&EntityItemFrame_setItemRotation(f,-3)==ENTITY_FRAME_OK);
    NBTTagCompound *out=NBTTagCompound_new(h);CHECK(out&&EntityItemFrame_writeEntityToNBT(f,out)==ENTITY_FRAME_OK);
    CHECK(NBTTagCompound_getByte_ascii(out,"Facing")==2&&NBTTagCompound_getInteger_ascii(out,"TileX")==1&&NBTTagCompound_getInteger_ascii(out,"TileY")==2&&NBTTagCompound_getInteger_ascii(out,"TileZ")==3);
    CHECK(NBTTagCompound_getByte_ascii(out,"ItemRotation")==-3&&NBTTagCompound_getFloat_ascii(out,"ItemDropChance")==1);
    ItemStack *stored=NULL;CHECK(EntityItemFrame_getDisplayedItem(f,&stored)==ENTITY_FRAME_OK);
    NBTTagCompound *item=NBTTagCompound_getCompoundTag_ascii(out,"Item");
    CHECK(NBTTagCompound_getTag_ascii(item,"tag")== (NBTBase *)stored->stackTagCompound);
    CHECK(NBTTagCompound_setByte_ascii(out,"Direction",-1));
    CHECK(NBTTagCompound_setFloat_ascii(out,"ItemDropChance",0.25f));
    EntityItemFrame *loaded=EntityItemFrame_nativeAllocate(h,NULL,NULL,NULL,NULL);CHECK(loaded);
    CHECK(EntityItemFrame_construct(loaded,NULL,NULL,NULL,random,ids)==ENTITY_FRAME_OK);
    CHECK(EntityItemFrame_readEntityFromNBT(loaded,out)==ENTITY_FRAME_OK);
    ItemStack *loadedItem=NULL;int32_t rotation=0;
    CHECK(EntityItemFrame_getDisplayedItem(loaded,&loadedItem)==ENTITY_FRAME_OK&&loadedItem&&loadedItem->itemFrame==loaded&&loadedItem->stackSize==1);
    CHECK(loadedItem->stackTagCompound!=stored->stackTagCompound&&NBTBase_equals((NBTBase *)loadedItem->stackTagCompound,(NBTBase *)stored->stackTagCompound));
    CHECK(EntityItemFrame_getRotation(loaded,&rotation)==ENTITY_FRAME_OK&&rotation==-6&&loaded->itemDropChance==0.25f);
    CHECK(loaded->hanging.facingDirection==&NativeHangingFacing_WEST&&loaded->hanging.hangingPosition->vec3i.x==0&&loaded->hanging.hangingPosition->vec3i.y==2&&loaded->hanging.hangingPosition->vec3i.z==3);
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)loaded));
    CHECK(MCObjectHeap_collect(h));loaded=(EntityItemFrame *)MCObjectRoot_get(&root);
    CHECK(EntityItemFrame_getDisplayedItem(loaded,&loadedItem)==ENTITY_FRAME_OK&&loadedItem->itemFrame==loaded);
    MCObjectHeap *clone=MCObjectHeap_clone(h);CHECK(clone);MCObjectRoot cloneRoot={0};CHECK(MCObjectRoot_rebind(&cloneRoot,clone,&root));
    EntityItemFrame *branch=(EntityItemFrame *)MCObjectRoot_get(&cloneRoot);ItemStack *branchItem=NULL;
    CHECK(branch!=loaded&&EntityItemFrame_getDisplayedItem(branch,&branchItem)==ENTITY_FRAME_OK&&branchItem!=loadedItem&&branchItem->itemFrame==branch);
    CHECK(NativeJavaClass_getClass(clone,(MCObject *)branch)==EntityItemFrame_nativeClass(clone));
    CHECK(MCObjectHeap_collect(clone)&&MCObjectHeap_adopt(h,clone));MCObjectHeap_free(clone);
    loaded=(EntityItemFrame *)MCObjectRoot_get(&root);
    CHECK(EntityItemFrame_getDisplayedItem(loaded,&loadedItem)==ENTITY_FRAME_OK&&ItemStack_getItemFrame(loadedItem)==loaded);
    CHECK(!MCObjectHeap_failed(h)&&MCObjectHeap_collect(h));
    MCObjectRoot_drop(&root);CHECK(MCObjectHeap_collect(h));MCObjectHeap_free(h);
}
static void healthy_and_native_failures(NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    MCObjectHeap *h=MCObjectHeap_new(64u<<20);EntityItemFrame *f=EntityItemFrame_nativeAllocate(h,NULL,NULL,NULL,NULL);
    ItemStack *out=(ItemStack *)f;int32_t rotation=99;
    CHECK(EntityItemFrame_getDisplayedItem(f,&out)==ENTITY_FRAME_EXCEPTION&&out==(ItemStack *)f&&!MCObjectHeap_failed(h));
    CHECK(EntityItemFrame_getRotation(f,&rotation)==ENTITY_FRAME_EXCEPTION&&rotation==99&&!MCObjectHeap_failed(h));
    CHECK(EntityItemFrame_construct(f,NULL,NULL,NULL,random,ids)==ENTITY_FRAME_OK);
    ItemStack *s=ItemStack_new(h,ItemStack_registryItem(1),3,0);CHECK(s);
    CHECK(EntityItemFrame_setDisplayedItem(f,s)==ENTITY_FRAME_EXCEPTION&&!MCObjectHeap_failed(h));
    CHECK(EntityItemFrame_getDisplayedItem(f,&out)==ENTITY_FRAME_OK&&out&&out->itemFrame==f&&out->stackSize==1);
    size_t before=MCObjectHeap_liveObjects(h);
    CHECK(EntityItemFrame_writeEntityToNBT(f,NULL)==ENTITY_FRAME_EXCEPTION&&!MCObjectHeap_failed(h));
    CHECK(MCObjectHeap_liveObjects(h)>before); /* argument NBT construction precedes NULL tag invocation */
    CHECK(EntityItemFrame_readEntityFromNBT(f,NULL)==ENTITY_FRAME_EXCEPTION&&!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    for(int result=ENTITY_FRAME_OK;result<=ENTITY_FRAME_FAILURE;result++) {
        h=MCObjectHeap_new(64u<<20);Witness *w=witness(h);f=recording_frame(h,w,random,ids,true);
        w->comparatorResult=(EntityFrameResult)result;
        s=ItemStack_new(h,ItemStack_registryItem(1),4,0);CHECK(s);
        CHECK(EntityItemFrame_setDisplayedItem(f,s)==(EntityFrameResult)result);
        ItemStack *stored=(ItemStack *)WatchableObject_getObject(DataWatcher_nativeGetWatchedObject(f->hanging.entity.dataWatcher,8));
        CHECK(stored&&stored->stackSize==1&&stored->itemFrame==f); /* no rollback on reached callback failure */
        CHECK(MCObjectHeap_failed(h)==(result==ENTITY_FRAME_FAILURE));
        MCObjectHeap_free(h);
    }
    h=MCObjectHeap_new(1u<<20);f=EntityItemFrame_nativeAllocate(h,NULL,NULL,NULL,NULL);CHECK(f);
    MCObject *small=MCObjectHeap_alloc(h,sizeof(Entity),f->hanging.entity.object.klass);CHECK(small&&!EntityItemFrame_isInstance(small));
    CHECK(EntityItemFrame_construct((EntityItemFrame *)small,NULL,NULL,NULL,random,ids)==ENTITY_FRAME_FAILURE&&MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    MCObjectHeap *other=MCObjectHeap_new(1u<<20);EntityItemFrame *foreign=EntityItemFrame_nativeAllocate(other,NULL,NULL,NULL,NULL);CHECK(foreign);
    h=MCObjectHeap_new(1u<<20);s=ItemStack_new(h,ItemStack_registryItem(1),1,0);CHECK(s);
    CHECK(!ItemStack_setItemFrame(s,foreign)&&s->itemFrame==NULL&&MCObjectHeap_failed(h)&&!MCObjectHeap_failed(other));
    MCObjectHeap_free(h);MCObjectHeap_free(other);
    for(int mode=0;mode<2;mode++) {
        h=MCObjectHeap_new(64u<<20);Witness *w=witness(h);f=recording_frame(h,w,random,ids,true);
        other=MCObjectHeap_new(1024);MCObject *foreignContext=MCObjectHeap_alloc(other,sizeof(MCObject),&witness_class);CHECK(foreignContext);
        w->nextContext=foreignContext;
        if(mode==0){w->mode=5;CHECK(EntityItemFrame_setItemRotation(f,5)==ENTITY_FRAME_FAILURE&&MCObjectHeap_failed(h));}
        else {w->mode=6;CHECK(EntityHanging_updateFacingWithBoundingBox(&f->hanging,&NativeHangingFacing_EAST)==ENTITY_FRAME_FAILURE&&MCObjectHeap_failed(h));}
        CHECK(!MCObjectHeap_failed(other));MCObjectHeap_free(h);MCObjectHeap_free(other);
    }
}
static void allocation_prefixes(NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    unsigned failed=0,passed=0;
    for(size_t budget=128;budget<=12000;budget+=127) {
        MCObjectHeap *h=MCObjectHeap_new(budget);EntityItemFrame *f=EntityItemFrame_nativeAllocate(h,NULL,NULL,NULL,NULL);
        EntityFrameResult r=f?EntityItemFrame_construct(f,NULL,NULL,NULL,random,ids):ENTITY_FRAME_FAILURE;
        if(r==ENTITY_FRAME_OK){++passed;CHECK(f->itemDropChance==1&&f->hanging.entity.dataWatcher&&f->hanging.entity.width==0.5f);}
        else {++failed;CHECK(MCObjectHeap_failed(h));if(f){CHECK(f->itemDropChance==0);CHECK(f->hanging.entity.width==0||f->hanging.entity.width==0.6f||f->hanging.entity.width==0.5f);}}
        MCObjectHeap_free(h);
    }
    CHECK(failed>0&&passed>0);
}
static void undersized_stack_guards(NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    for(int mode=0;mode<6;mode++) {
        MCObjectHeap *h=MCObjectHeap_new(1u<<20);
        ItemStack *source=ItemStack_new(h,ItemStack_registryItem(1),3,0);CHECK(source);
        ItemStack *small=(ItemStack *)MCObjectHeap_alloc(h,sizeof(MCObject),source->object.klass);
        CHECK(small&&ItemStack_isInstance((MCObject *)small)&&MCObjectHeap_objectSize((MCObject *)small)==sizeof(MCObject));
        EntityItemFrame *f=EntityItemFrame_nativeAllocate(h,NULL,NULL,NULL,NULL);CHECK(f);
        CHECK(EntityItemFrame_construct(f,NULL,NULL,NULL,random,ids)==ENTITY_FRAME_OK);
        ItemStack *out=source;
        if(mode==0)CHECK(!ItemStack_isOnItemFrame(small)&&MCObjectHeap_failed(h));
        else if(mode==1)CHECK(!ItemStack_setItemFrame(small,f)&&MCObjectHeap_failed(h));
        else if(mode==2)CHECK(ItemStack_getItemFrame(small)==NULL&&MCObjectHeap_failed(h));
        else if(mode==3) {
            size_t before=MCObjectHeap_liveObjects(h);
            CHECK(EntityItemFrame_setDisplayedItem(f,small)==ENTITY_FRAME_FAILURE&&MCObjectHeap_failed(h));
            CHECK(MCObjectHeap_liveObjects(h)==before);
            CHECK(WatchableObject_getObject(DataWatcher_nativeGetWatchedObject(f->hanging.entity.dataWatcher,8))==NULL);
        } else {
            /* Native corruption of the real watched reference is rejected before
               a live ItemStack escapes or its NBT body reads beyond the object. */
            CHECK(WatchableObject_setObject(DataWatcher_nativeGetWatchedObject(f->hanging.entity.dataWatcher,8),(MCObject *)small));
            if(mode==4)CHECK(EntityItemFrame_getDisplayedItem(f,&out)==ENTITY_FRAME_FAILURE&&out==source&&MCObjectHeap_failed(h));
            else {
                NBTTagCompound *tag=NBTTagCompound_new(h);CHECK(tag);
                CHECK(EntityItemFrame_writeEntityToNBT(f,tag)==ENTITY_FRAME_FAILURE&&MCObjectHeap_failed(h));
                CHECK(NBTBase_hasNoTags((NBTBase *)tag));
            }
        }
        MCObjectHeap_free(h);
    }
}
static void hanging_nbt_allocation_prefix(NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    MCObjectHeap *h=MCObjectHeap_new(1u<<20);
    EntityItemFrame *f=EntityItemFrame_nativeAllocate(h,NULL,NULL,NULL,NULL);CHECK(f);
    CHECK(EntityItemFrame_construct(f,NULL,NULL,NULL,random,ids)==ENTITY_FRAME_OK);
    BlockPos *old=f->hanging.hangingPosition;
    size_t before=MCObjectHeap_liveObjects(h);
    CHECK(EntityHanging_readEntityFromNBT(&f->hanging,NULL)==ENTITY_FRAME_EXCEPTION&&!MCObjectHeap_failed(h));
    /* NEW happens before argument evaluation; the allocation is native-prefix
       evidence, not an observation of unreachable Java objects. */
    CHECK(MCObjectHeap_liveObjects(h)==before+1&&f->hanging.hangingPosition==old);
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(1u<<20);f=EntityItemFrame_nativeAllocate(h,NULL,NULL,NULL,NULL);CHECK(f);
    CHECK(EntityItemFrame_construct(f,NULL,NULL,NULL,random,ids)==ENTITY_FRAME_OK);
    static const MCObjectClass filler={"fixture.nativeBudget",MCObjectHeap_plainClone,NULL,NULL};
    old=f->hanging.hangingPosition;before=MCObjectHeap_liveObjects(h);
    size_t remaining=(1u<<20)-MCObjectHeap_liveBytes(h);
    CHECK(remaining>=sizeof(MCObject)&&MCObjectHeap_alloc(h,remaining,&filler));
    CHECK(EntityHanging_readEntityFromNBT(&f->hanging,NULL)==ENTITY_FRAME_FAILURE&&MCObjectHeap_failed(h));
    CHECK(MCObjectHeap_liveObjects(h)==before+1&&f->hanging.hangingPosition==old);
    MCObjectHeap_free(h);
}
int main(void) {
    NativeJavaRandomRuntime *random=NativeJavaRandomRuntime_new(&clock_dependencies,NULL);
    NativeEntityIDRuntime *ids=NativeEntityIDRuntime_new(0);CHECK(random&&ids);
    undersized_stack_guards(random,ids);
    hanging_nbt_allocation_prefix(random,ids);
    constructor_defaults(random,ids);bounds_cases(random,ids);callback_order(random,ids);
    displayed_and_rotation(random,ids);nbt_and_lifetime(random,ids);
    healthy_and_native_failures(random,ids);allocation_prefixes(random,ids);
    CHECK(NativeJavaRandomRuntime_free(random));
    CHECK(NativeEntityIDRuntime_free(ids));
    printf("Source item frame: %u checks passed\n",checks);
    return 0;
}
