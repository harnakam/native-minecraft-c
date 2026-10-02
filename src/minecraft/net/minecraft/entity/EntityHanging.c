#include "entity/EntityHanging.h"
#include "entity/item/EntityItemFrame.h"
#include "nbt/NBTTagCompound.h"
#include <string.h>

const NativeHangingFacing NativeHangingFacing_DOWN={0,-1,0,-1,0,NATIVE_HANGING_AXIS_Y};
const NativeHangingFacing NativeHangingFacing_UP={1,-1,0,1,0,NATIVE_HANGING_AXIS_Y};
const NativeHangingFacing NativeHangingFacing_NORTH={2,2,0,0,-1,NATIVE_HANGING_AXIS_Z};
const NativeHangingFacing NativeHangingFacing_SOUTH={3,0,0,0,1,NATIVE_HANGING_AXIS_Z};
const NativeHangingFacing NativeHangingFacing_WEST={4,1,-1,0,0,NATIVE_HANGING_AXIS_X};
const NativeHangingFacing NativeHangingFacing_EAST={5,3,1,0,0,NATIVE_HANGING_AXIS_X};
bool NativeHangingFacing_isKnown(const NativeHangingFacing *f) {
    return f==&NativeHangingFacing_DOWN||f==&NativeHangingFacing_UP||
        f==&NativeHangingFacing_NORTH||f==&NativeHangingFacing_SOUTH||
        f==&NativeHangingFacing_WEST||f==&NativeHangingFacing_EAST;
}
bool NativeHangingFacing_getHorizontalIndex(const NativeHangingFacing *f,int32_t *out) {
    if(!NativeHangingFacing_isKnown(f)||!out)return false;
    *out=f->horizontalIndex;return true;
}
const NativeHangingFacing *NativeHangingFacing_getHorizontal(int32_t index) {
    static const NativeHangingFacing *const values[]={&NativeHangingFacing_SOUTH,
        &NativeHangingFacing_WEST,&NativeHangingFacing_NORTH,&NativeHangingFacing_EAST};
    int32_t remainder=index%4;return values[remainder<0?-remainder:remainder];
}
const NativeHangingFacing *NativeHangingFacing_rotateYCCW(const NativeHangingFacing *f) {
    if(f==&NativeHangingFacing_NORTH)return &NativeHangingFacing_WEST;
    if(f==&NativeHangingFacing_WEST)return &NativeHangingFacing_SOUTH;
    if(f==&NativeHangingFacing_SOUTH)return &NativeHangingFacing_EAST;
    if(f==&NativeHangingFacing_EAST)return &NativeHangingFacing_NORTH;
    return NULL;
}
static bool same(const MCObject *o,void *context){return o==context;}
static bool tracked(MCObjectHeap *h,MCObject *o) {
    return !o||(o->heap==h&&MCObjectHeap_findObject(h,o->klass,same,o)==o);
}
static EntityFrameResult fail(EntityHanging *h) {
    MCObjectHeap_fail(h?h->entity.object.heap:NULL);return ENTITY_FRAME_FAILURE;
}
bool EntityHanging_isInstance(const MCObject *o) {
    return EntityItemFrame_isInstance(o)&&MCObjectHeap_objectSize(o)>=sizeof(EntityHanging);
}
static const NativeJavaClassDescriptor *const parents[]={&NativeJavaClass_EntityClass};
/* Abstract parent is ancestry only, never the concrete getClass result. */
const NativeJavaClassDescriptor EntityHanging_Class={"net.minecraft.entity.EntityHanging",parents,1,NULL};
NativeJavaClass *EntityHanging_nativeClass(MCObjectHeap *h){return NativeJavaClass_literal(h,&EntityHanging_Class);}
void EntityHanging_traceFields(EntityHanging *h,MCObjectVisitor v,void *context) {
    Entity_traceFields(&h->entity,v,context);
    h->hangingPosition=(BlockPos *)v((MCObject *)h->hangingPosition,context);
    h->nativeContext=v(h->nativeContext,context);
}
static bool begin(EntityHanging *h,MCObjectRootScope *s) {
    if(!h||!tracked(h->entity.object.heap,(MCObject *)h)||!EntityHanging_isInstance((MCObject *)h)||
       MCObjectHeap_failed(h->entity.object.heap)||!MCObjectRootScope_begin(s,h->entity.object.heap)) {
        fail(h);return false;
    }
    if(tracked(h->entity.object.heap,h->nativeContext)&&MCObjectRootScope_pin(s,(MCObject *)h)&&
       MCObjectRootScope_pin(s,h->nativeContext))return true;
    fail(h);MCObjectRootScope_end(s);return false;
}
static EntityFrameResult end(EntityHanging *h,MCObjectRootScope *s,EntityFrameResult r) {
    MCObjectHeap *heap=h->entity.object.heap;
    if(!tracked(heap,h->nativeContext)||!tracked(heap,h->entity.entityContext)||MCObjectHeap_failed(heap))r=fail(h);
    if(r==ENTITY_FRAME_FAILURE)fail(h);
    MCObjectRootScope_end(s);return r;
}
static bool pin(EntityHanging *h,MCObjectRootScope *s,MCObject *o) {
    return tracked(h->entity.object.heap,o)&&MCObjectRootScope_pin(s,o);
}
static EntityFrameResult width(EntityHanging *h,MCObjectRootScope *s,bool height,int32_t *out) {
    const EntityHangingDependencies *d=h->nativeDependencies;
    MCObject *context=h->nativeContext;
    if(!pin(h,s,context))return fail(h);
    bool ok;
    if(height&&d&&d->getHeightPixels)ok=d->getHeightPixels(context,h,out);
    else if(!height&&d&&d->getWidthPixels)ok=d->getWidthPixels(context,h,out);
    else if(EntityItemFrame_isInstance((MCObject *)h)) {
        *out=height?EntityItemFrame_getHeightPixels((EntityItemFrame *)h):EntityItemFrame_getWidthPixels((EntityItemFrame *)h);ok=true;
    } else ok=false;
    if(!ok||!tracked(h->entity.object.heap,h->nativeContext)||MCObjectHeap_failed(h->entity.object.heap))return fail(h);
    return ENTITY_FRAME_OK;
}
static EntityFrameResult facing(EntityHanging *h,const NativeHangingFacing **out) {
    const NativeHangingFacing *f=h->facingDirection;
    if(!f)return ENTITY_FRAME_EXCEPTION;
    if(!NativeHangingFacing_isKnown(f))return fail(h);
    *out=f;return ENTITY_FRAME_OK;
}
/* Original private updateBoundingBox, including repeated virtual size reads.
   No Entity.setPosition call or additional width/height normalization is added. */
static EntityFrameResult bounding_box(EntityHanging *h,MCObjectRootScope *s) {
    if(!h->facingDirection)return ENTITY_FRAME_OK;
    BlockPos *p=h->hangingPosition;
    if(!p)return ENTITY_FRAME_EXCEPTION;
    if(!pin(h,s,(MCObject *)p)||!BlockPos_isInstance((MCObject *)p))return fail(h);
    int32_t px,py,pz;
    if(Vec3i_getX(&p->vec3i,&px)!=NATIVE_ARRAY_OK||Vec3i_getY(&p->vec3i,&py)!=NATIVE_ARRAY_OK||
       Vec3i_getZ(&p->vec3i,&pz)!=NATIVE_ARRAY_OK)return fail(h);
    double x=(double)px+0.5,y=(double)py+0.5,z=(double)pz+0.5;
    int32_t pixels;EntityFrameResult r=width(h,s,false,&pixels);if(r!=ENTITY_FRAME_OK)return r;
    double horizontal=pixels%32==0?0.5:0.0;
    r=width(h,s,true,&pixels);if(r!=ENTITY_FRAME_OK)return r;
    double vertical=pixels%32==0?0.5:0.0;
    const NativeHangingFacing *f=NULL;
    r=facing(h,&f);if(r!=ENTITY_FRAME_OK)return r;
    x=x-(double)f->offsetX*0.46875;
    r=facing(h,&f);if(r!=ENTITY_FRAME_OK)return r;
    z=z-(double)f->offsetZ*0.46875;
    y=y+vertical;
    r=facing(h,&f);if(r!=ENTITY_FRAME_OK)return r;
    const NativeHangingFacing *ccw=NativeHangingFacing_rotateYCCW(f);
    if(!ccw)return ENTITY_FRAME_EXCEPTION;
    x=x+horizontal*(double)ccw->offsetX;z=z+horizontal*(double)ccw->offsetZ;
    h->entity.posX=x;h->entity.posY=y;h->entity.posZ=z;MCObjectHeap_touch(h->entity.object.heap);
    r=width(h,s,false,&pixels);if(r!=ENTITY_FRAME_OK)return r;double dx=(double)pixels;
    r=width(h,s,true,&pixels);if(r!=ENTITY_FRAME_OK)return r;double dy=(double)pixels;
    r=width(h,s,false,&pixels);if(r!=ENTITY_FRAME_OK)return r;double dz=(double)pixels;
    r=facing(h,&f);if(r!=ENTITY_FRAME_OK)return r;
    if(f->axis==NATIVE_HANGING_AXIS_Z)dz=1.0;else dx=1.0;
    dx=dx/32.0;dy=dy/32.0;dz=dz/32.0;
    AxisAlignedBB *box=AxisAlignedBB_new(h->entity.object.heap,x-dx,y-dy,z-dz,x+dx,y+dy,z+dz);
    const EntityDependencies *d=h->entity.entityDependencies;MCObject *context=h->entity.entityContext;
    if(!box||!pin(h,s,context)||!d||!d->setEntityBoundingBox||
       !d->setEntityBoundingBox(context,&h->entity,box)||MCObjectHeap_failed(h->entity.object.heap))return fail(h);
    return ENTITY_FRAME_OK;
}
EntityFrameResult EntityHanging_construct(EntityHanging *h,World *world,const EntityDependencies *d,
    MCObject *context,NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    MCObjectRootScope s={0};if(!begin(h,&s))return ENTITY_FRAME_FAILURE;
    EntityFrameResult r=Entity_construct(&h->entity,(MCObject *)world,d,context,random,ids)&&
        Entity_setSize(&h->entity,0.5f,0.5f)?ENTITY_FRAME_OK:ENTITY_FRAME_FAILURE;
    return end(h,&s,r);
}
EntityFrameResult EntityHanging_constructPosition(EntityHanging *h,World *w,BlockPos *p,
    const EntityDependencies *d,MCObject *c,NativeJavaRandomRuntime *random,NativeEntityIDRuntime *ids) {
    MCObjectRootScope s={0};if(!begin(h,&s))return ENTITY_FRAME_FAILURE;
    EntityFrameResult r=ENTITY_FRAME_FAILURE;
    if(!pin(h,&s,(MCObject *)p)||(p&&!BlockPos_isInstance((MCObject *)p)))goto done;
    r=EntityHanging_construct(h,w,d,c,random,ids);
    if(r==ENTITY_FRAME_OK){h->hangingPosition=p;MCObjectHeap_touch(h->entity.object.heap);}
done:return end(h,&s,r);
}
bool EntityHanging_entityInit(EntityHanging *h) {
    /* Source intentionally empty abstract-parent override. */
    MCObjectRootScope s={0};if(!begin(h,&s))return false;
    return end(h,&s,ENTITY_FRAME_OK)==ENTITY_FRAME_OK;
}
EntityFrameResult EntityHanging_updateFacingWithBoundingBox(EntityHanging *h,const NativeHangingFacing *f) {
    MCObjectRootScope s={0};if(!begin(h,&s))return ENTITY_FRAME_FAILURE;
    EntityFrameResult r=ENTITY_FRAME_OK;
    if(!f){r=ENTITY_FRAME_EXCEPTION;goto done;}
    if(!NativeHangingFacing_isKnown(f)){r=ENTITY_FRAME_FAILURE;goto done;}
    if(f->axis==NATIVE_HANGING_AXIS_Y){r=ENTITY_FRAME_EXCEPTION;goto done;}
    h->facingDirection=f;
    /* Registered indices are 0..3: no additional integer overflow is possible. */
    h->entity.prevRotationYaw=h->entity.rotationYaw=(float)(f->horizontalIndex*90);
    MCObjectHeap_touch(h->entity.object.heap);r=bounding_box(h,&s);
done:return end(h,&s,r);
}
EntityFrameResult EntityHanging_setPosition(EntityHanging *h,double x,double y,double z) {
    MCObjectRootScope s={0};if(!begin(h,&s))return ENTITY_FRAME_FAILURE;
    h->entity.posX=x;h->entity.posY=y;h->entity.posZ=z;MCObjectHeap_touch(h->entity.object.heap);
    BlockPos *old=h->hangingPosition;EntityFrameResult r=ENTITY_FRAME_FAILURE;
    if(!pin(h,&s,(MCObject *)old)||(old&&!BlockPos_isInstance((MCObject *)old)))goto done;
    BlockPos *next=BlockPos_newDouble(h->entity.object.heap,x,y,z);if(!next)goto done;
    h->hangingPosition=next;MCObjectHeap_touch(h->entity.object.heap);
    bool equal;
    if(Vec3i_equals(&next->vec3i,(MCObject *)old,&equal)!=NATIVE_ARRAY_OK)goto done;
    if(!equal) {
        r=bounding_box(h,&s);if(r!=ENTITY_FRAME_OK)goto done;
        h->entity.isAirBorne=true;MCObjectHeap_touch(h->entity.object.heap);
    }
    r=ENTITY_FRAME_OK;
done:return end(h,&s,r);
}
BlockPos *EntityHanging_getHangingPosition(EntityHanging *h) {
    MCObjectRootScope s={0};if(!begin(h,&s))return NULL;BlockPos *p=h->hangingPosition;
    EntityFrameResult r=pin(h,&s,(MCObject *)p)&&(!p||BlockPos_isInstance((MCObject *)p))?ENTITY_FRAME_OK:ENTITY_FRAME_FAILURE;
    return end(h,&s,r)==ENTITY_FRAME_OK?p:NULL;
}
const NativeHangingFacing *EntityHanging_getHorizontalFacing(EntityHanging *h) {
    MCObjectRootScope s={0};if(!begin(h,&s))return NULL;const NativeHangingFacing *f=h->facingDirection;
    EntityFrameResult r=!f||NativeHangingFacing_isKnown(f)?ENTITY_FRAME_OK:ENTITY_FRAME_FAILURE;
    return end(h,&s,r)==ENTITY_FRAME_OK?f:NULL;
}
EntityFrameResult EntityHanging_writeEntityToNBT(EntityHanging *h,NBTTagCompound *tag) {
    MCObjectRootScope s={0};if(!begin(h,&s))return ENTITY_FRAME_FAILURE;
    EntityFrameResult r=ENTITY_FRAME_FAILURE;
    if(!pin(h,&s,(MCObject *)tag)||(tag&&!NBTTagCompound_isInstance((MCObject *)tag)))goto done;
    const NativeHangingFacing *f=NULL;r=facing(h,&f);if(r!=ENTITY_FRAME_OK)goto done;
    if(!tag){r=ENTITY_FRAME_EXCEPTION;goto done;}
    if(!NBTTagCompound_setByte_ascii(tag,"Facing",(int8_t)f->horizontalIndex)){r=ENTITY_FRAME_FAILURE;goto done;}
    static const char *const keys[]={"TileX","TileY","TileZ"};
    for(int i=0;i<3;i++) {
        BlockPos *p=EntityHanging_getHangingPosition(h);if(MCObjectHeap_failed(h->entity.object.heap)){r=ENTITY_FRAME_FAILURE;goto done;}
        if(!p){r=ENTITY_FRAME_EXCEPTION;goto done;}
        int32_t value;
        NativeArrayResult coordinate=i==0?Vec3i_getX(&p->vec3i,&value):i==1?Vec3i_getY(&p->vec3i,&value):Vec3i_getZ(&p->vec3i,&value);
        if(coordinate!=NATIVE_ARRAY_OK){r=coordinate==NATIVE_ARRAY_EXCEPTION?ENTITY_FRAME_EXCEPTION:ENTITY_FRAME_FAILURE;goto done;}
        if(!NBTTagCompound_setInteger_ascii(tag,keys[i],value)){r=ENTITY_FRAME_FAILURE;goto done;}
    }
    r=ENTITY_FRAME_OK;
done:return end(h,&s,r);
}
EntityFrameResult EntityHanging_readEntityFromNBT(EntityHanging *h,NBTTagCompound *tag) {
    MCObjectRootScope s={0};if(!begin(h,&s))return ENTITY_FRAME_FAILURE;
    EntityFrameResult r=ENTITY_FRAME_FAILURE;
    if(!pin(h,&s,(MCObject *)tag)||(tag&&!NBTTagCompound_isInstance((MCObject *)tag)))goto done;
    /* Source NEW precedes every constructor argument, including the first
       nullable tag invocation. The same Source BlockPos owner is initialized
       through its parent constructor only after the tag getters. */
    BlockPos *p=NativeBlockPos_allocate(h->entity.object.heap);
    if(!p||!pin(h,&s,(MCObject *)p))goto done;
    if(!tag){r=ENTITY_FRAME_EXCEPTION;goto done;}
    int32_t x=NBTTagCompound_getInteger_ascii(tag,"TileX");if(MCObjectHeap_failed(h->entity.object.heap))goto done;
    int32_t y=NBTTagCompound_getInteger_ascii(tag,"TileY");if(MCObjectHeap_failed(h->entity.object.heap))goto done;
    int32_t z=NBTTagCompound_getInteger_ascii(tag,"TileZ");
    if(MCObjectHeap_failed(h->entity.object.heap))goto done;
    if(!NativeBlockPos_constructCoordinates(p,x,y,z))goto done;
    h->hangingPosition=p;MCObjectHeap_touch(h->entity.object.heap);
    const NativeHangingFacing *f;
    if(NBTTagCompound_hasKeyType_ascii(tag,"Direction",99)) {
        f=NativeHangingFacing_getHorizontal(NBTTagCompound_getByte_ascii(tag,"Direction"));
        p=BlockPos_add(h->hangingPosition,f->offsetX,f->offsetY,f->offsetZ);if(!p)goto done;
        h->hangingPosition=p;MCObjectHeap_touch(h->entity.object.heap);
    } else if(NBTTagCompound_hasKeyType_ascii(tag,"Facing",99))f=NativeHangingFacing_getHorizontal(NBTTagCompound_getByte_ascii(tag,"Facing"));
    else f=NativeHangingFacing_getHorizontal(NBTTagCompound_getByte_ascii(tag,"Dir"));
    if(MCObjectHeap_failed(h->entity.object.heap))goto done;
    r=EntityHanging_updateFacingWithBoundingBox(h,f);
done:return end(h,&s,r);
}
