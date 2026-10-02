#include "util/NativeJavaClass.h"
#include "client/entity/EntityPlayerSP.h"
#include "entity/player/EntityPlayerMP.h"
#include "entity/item/EntityItem.h"
#include "entity/item/EntityItemFrame.h"
#include "block/material/MapColor.h"
#include "util/MCGameplayPlayer.h"
#include "util/BlockPosMutableBlockPos.h"
#include <stdlib.h>

static const MCObjectClass klass={"native.java.lang.Class",MCObjectHeap_plainClone,NULL,NULL};
static bool fail(MCObjectHeap *h){MCObjectHeap_fail(h);return false;}
static bool same_reference(const MCObject *object,void *expected){return object==expected;}
static bool tracked(const MCObject *object){
    return object&&object->heap&&object->klass&&
        MCObjectHeap_findObject(object->heap,object->klass,same_reference,(void *)object)==object;
}
bool NativeJavaClass_isInstance(const MCObject *o){
    return tracked(o)&&o->klass==&klass&&MCObjectHeap_objectSize(o)>=sizeof(NativeJavaClass);
}
typedef struct {const NativeJavaClassDescriptor **items;size_t size,capacity;} Descriptors;
static bool append(Descriptors *a,const NativeJavaClassDescriptor *d){
    for(size_t i=0;i<a->size;++i)if(a->items[i]==d)return true;
    if(a->size==a->capacity){
        size_t capacity=a->capacity?a->capacity*2:16;
        if(capacity<a->capacity||capacity>SIZE_MAX/sizeof(*a->items))return false;
        const NativeJavaClassDescriptor **items=realloc(a->items,capacity*sizeof(*items));
        if(!items)return false;
        a->items=items;a->capacity=capacity;
    }
    a->items[a->size++]=d;return true;
}
/* Descriptor memory is immutable and has process lifetime, like MCObjectClass.
   Traverse iteratively so interface diamonds and long class chains do not
   recurse through the C stack. The visited set also terminates bad cycles. */
static bool hierarchy(MCObjectHeap *h,const NativeJavaClassDescriptor *d,
                      const NativeJavaClassDescriptor *target,bool *found){
    Descriptors pending={0};bool ok=append(&pending,d),match=false;
    for(size_t i=0;ok&&i<pending.size;++i){
        const NativeJavaClassDescriptor *next=pending.items[i];
        if(!next||!next->name||(next->supertypeCount&&!next->supertypes)){
            ok=false;break;
        }
        if(next==target)match=true;
        for(size_t j=0;ok&&j<next->supertypeCount;++j)ok=append(&pending,next->supertypes[j]);
    }
    free(pending.items);
    if(!ok)return fail(h);
    if(found)*found=match;
    return true;
}
static bool same_descriptor(const MCObject *o,void *context){
    return NativeJavaClass_isInstance(o)&&((const NativeJavaClass *)o)->descriptor==context;
}
NativeJavaClass *NativeJavaClass_literal(MCObjectHeap *h,const NativeJavaClassDescriptor *d){
    if(!h||MCObjectHeap_failed(h)||!hierarchy(h,d,NULL,NULL))return NULL;
    NativeJavaClass *c=(NativeJavaClass *)MCObjectHeap_findObject(h,&klass,same_descriptor,(void *)d);
    if(c)return c;
    c=(NativeJavaClass *)MCObjectHeap_alloc(h,sizeof(*c),&klass);
    if(!c)return NULL;
    c->descriptor=d;
    MCObjectRoot root={0};
    if(!MCObjectRoot_init(&root,h,(MCObject *)c)){fail(h);return NULL;}
    return c;
}
static bool source_sp(const MCObject *o){return EntityPlayerSP_isInstance(o);}
static bool source_mp(const MCObject *o){return EntityPlayerMP_isInstance(o);}
static bool source_item(const MCObject *o){return EntityItem_isInstance(o);}
static bool native_player(const MCObject *o){
    return MCGameplayPlayer_isInstance(o)&&!EntityPlayerSP_isInstance(o)&&!EntityPlayerMP_isInstance(o);
}
const NativeJavaClassDescriptor NativeJavaClass_ObjectClass={"java.lang.Object",NULL,0,NULL};
static const NativeJavaClassDescriptor *const entityParents[]={&NativeJavaClass_ObjectClass};
const NativeJavaClassDescriptor NativeJavaClass_EntityClass={"net.minecraft.entity.Entity",entityParents,1,NULL};
static const NativeJavaClassDescriptor *const livingParents[]={&NativeJavaClass_EntityClass};
static const NativeJavaClassDescriptor livingDescriptor={"net.minecraft.entity.EntityLivingBase",livingParents,1,NULL};
static const NativeJavaClassDescriptor *const playerParents[]={&livingDescriptor};
static const NativeJavaClassDescriptor playerDescriptor={"net.minecraft.entity.player.EntityPlayer",playerParents,1,NULL};
static const NativeJavaClassDescriptor *const acpParents[]={&playerDescriptor};
static const NativeJavaClassDescriptor acpDescriptor={"net.minecraft.client.entity.AbstractClientPlayer",acpParents,1,NULL};
static const NativeJavaClassDescriptor *const spParents[]={&acpDescriptor};
static const NativeJavaClassDescriptor spDescriptor={"net.minecraft.client.entity.EntityPlayerSP",spParents,1,source_sp};
static const NativeJavaClassDescriptor *const mpParents[]={&playerDescriptor};
static const NativeJavaClassDescriptor mpDescriptor={"net.minecraft.entity.player.EntityPlayerMP",mpParents,1,source_mp};
static const NativeJavaClassDescriptor *const itemParents[]={&NativeJavaClass_EntityClass};
static const NativeJavaClassDescriptor itemDescriptor={"net.minecraft.entity.item.EntityItem",itemParents,1,source_item};
static const NativeJavaClassDescriptor *const nativePlayerParents[]={&playerDescriptor};
static const NativeJavaClassDescriptor nativePlayerDescriptor={"C919.native.GameplayPlayer",nativePlayerParents,1,native_player};
NativeJavaClass *NativeJavaClass_Object(MCObjectHeap *h){return NativeJavaClass_literal(h,&NativeJavaClass_ObjectClass);}
NativeJavaClass *NativeJavaClass_Entity(MCObjectHeap *h){return NativeJavaClass_literal(h,&NativeJavaClass_EntityClass);}
static bool matches(const MCObject *o,void *context){
    if(!NativeJavaClass_isInstance(o))return false;
    const NativeJavaClassDescriptor *d=((const NativeJavaClass *)o)->descriptor;
    return d&&d->matchesRuntimeClass&&d->matchesRuntimeClass(context);
}
static const NativeJavaClassDescriptor *runtime_descriptor(MCObjectHeap *h,MCObject *o){
    if(!tracked(o)||o->heap!=h||MCObjectHeap_objectSize(o)<sizeof(MCObject)||MCObjectHeap_failed(h)){fail(h);return NULL;}
    NativeJavaClass *c=(NativeJavaClass *)MCObjectHeap_findObject(h,&klass,matches,o);
    if(c)return c->descriptor;
    const NativeJavaClassDescriptor *const builtins[]={&spDescriptor,&mpDescriptor,&itemDescriptor,&nativePlayerDescriptor,&EntityItemFrame_Class,&MapColor_Class,&Vec3i_Class,&BlockPos_Class,&BlockPosMutableBlockPos_Class};
    for(size_t i=0;i<sizeof(builtins)/sizeof(*builtins);++i)
        if(builtins[i]->matchesRuntimeClass(o))return builtins[i];
    fail(h);return NULL;
}
NativeJavaClass *NativeJavaClass_getClass(MCObjectHeap *h,MCObject *o){
    const NativeJavaClassDescriptor *d=runtime_descriptor(h,o);
    return d?NativeJavaClass_literal(h,d):NULL;
}
bool NativeJavaClass_isAssignableFrom(NativeJavaClass *self,NativeJavaClass *other,bool *out){
    MCObjectHeap *h=self?self->object.heap:NULL;
    if(!NativeJavaClass_isInstance((MCObject *)self)||!NativeJavaClass_isInstance((MCObject *)other)||
       other->object.heap!=h||!out||MCObjectHeap_failed(h))return fail(h);
    bool found=false;
    if(!hierarchy(h,self->descriptor,NULL,NULL)||!hierarchy(h,other->descriptor,self->descriptor,&found))return false;
    *out=found;return true;
}
bool NativeJavaClass_isInstanceOf(NativeJavaClass *self,MCObject *o,bool *out){
    MCObjectHeap *h=self?self->object.heap:NULL;
    if(!NativeJavaClass_isInstance((MCObject *)self)||!out||MCObjectHeap_failed(h))return fail(h);
    if(!o){*out=false;return true;}
    /* Runtime type checks do not manufacture a managed Class or root. The
       explicit getClass API still returns its canonical per-heap literal. */
    const NativeJavaClassDescriptor *actual=runtime_descriptor(h,o);
    if(!actual||!hierarchy(h,self->descriptor,NULL,NULL))return false;
    return hierarchy(h,actual,self->descriptor,out);
}
