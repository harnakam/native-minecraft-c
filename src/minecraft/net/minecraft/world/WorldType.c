#include "world/WorldType.h"
#include "util/NativeJavaString.h"
static bool fail(MCObjectHeap *h){MCObjectHeap_fail(h);return false;}
static void trace_type(MCObject *o,MCObjectVisitor v,void *c){WorldType *t=(WorldType *)o;t->worldType=(NBTString *)v((MCObject *)t->worldType,c);}
static void trace_array(MCObject *o,MCObjectVisitor v,void *c){WorldTypeArray *a=(WorldTypeArray *)o;for(size_t i=0;i<16;i++)a->items[i]=(WorldType *)v((MCObject *)a->items[i],c);}
static void trace_statics(MCObject *o,MCObjectVisitor v,void *c) {
    WorldTypeStatics *s=(WorldTypeStatics *)o;s->worldTypes=(WorldTypeArray *)v((MCObject *)s->worldTypes,c);
#define VISIT(field) s->field=(WorldType *)v((MCObject *)s->field,c)
    VISIT(DEFAULT);VISIT(FLAT);VISIT(LARGE_BIOMES);VISIT(AMPLIFIED);VISIT(CUSTOMIZED);VISIT(DEBUG_WORLD);VISIT(DEFAULT_1_1);
#undef VISIT
}
static const MCObjectClass typeClass={"net.minecraft.world.WorldType",MCObjectHeap_plainClone,trace_type,NULL};
static const MCObjectClass arrayClass={"native.WorldType[16]",MCObjectHeap_plainClone,trace_array,NULL};
static const MCObjectClass staticsClass={"native.WorldType.Statics",MCObjectHeap_plainClone,trace_statics,NULL};
bool WorldType_isInstance(const MCObject *o){return o&&o->klass==&typeClass&&MCObjectHeap_objectSize(o)>=sizeof(WorldType);}
static bool valid(WorldType *t){return WorldType_isInstance((MCObject *)t)&&!MCObjectHeap_failed(t->object.heap)?true:fail(t?t->object.heap:NULL);}
static bool any(const MCObject *o,void *c){(void)o;(void)c;return true;}
static bool construct(WorldType *t,WorldTypeStatics *s,int32_t id,NBTString *name,int32_t version) {
    MCObjectHeap *h=t->object.heap;
    if(name&&(!NBTString_isInstance((MCObject *)name)||((MCObject *)name)->heap!=h))return fail(h);
    t->worldType=name;t->generatorVersion=version;t->canBeCreated=true;t->worldTypeId=id;MCObjectHeap_touch(h);
    if(id<0||id>=16)return fail(h);
    s->worldTypes->items[id]=t;MCObjectHeap_touch(h);return true;
}
WorldTypeStatics *WorldType_getStatics(MCObjectHeap *h) {
    if(!h||MCObjectHeap_failed(h))return NULL;
    WorldTypeStatics *s=(WorldTypeStatics *)MCObjectHeap_findObject(h,&staticsClass,any,NULL);
    if(s)return s;
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    s=(WorldTypeStatics *)MCObjectHeap_alloc(h,sizeof *s,&staticsClass);
    MCObjectRoot root={0};
    if(s&&!MCObjectRoot_init(&root,h,(MCObject *)s))s=NULL;
    if(s)s->worldTypes=(WorldTypeArray *)MCObjectHeap_alloc(h,sizeof(WorldTypeArray),&arrayClass);
    static const int32_t ids[7]={0,1,2,3,4,5,8};
    static const char *const names[7]={"default","flat","largeBiomes","amplified","customized","debug_all_block_states","default_1_1"};
    for(size_t i=0;s&&s->worldTypes&&i<7&&!MCObjectHeap_failed(h);i++) {
        NBTString *name=NBTString_literalASCII(h,names[i]);
        WorldType *t=name?(WorldType *)MCObjectHeap_alloc(h,sizeof *t,&typeClass):NULL;
        if(!t||!construct(t,s,ids[i],name,i?0:1))break;
        if(i==0)t->isWorldTypeVersioned=true;
        if(i==3)t->hasNotificationData=true;
        if(i==6)t->canBeCreated=false;
        switch(i){case 0:s->DEFAULT=t;break;case 1:s->FLAT=t;break;case 2:s->LARGE_BIOMES=t;break;case 3:s->AMPLIFIED=t;break;case 4:s->CUSTOMIZED=t;break;case 5:s->DEBUG_WORLD=t;break;default:s->DEFAULT_1_1=t;break;}
        MCObjectHeap_touch(h);
    }
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(h)?NULL:s;
}
WorldType *WorldType_nativeAllocate(MCObjectHeap *h){return WorldType_getStatics(h)?(WorldType *)MCObjectHeap_alloc(h,sizeof(WorldType),&typeClass):NULL;}
bool WorldType_construct(WorldType *t,int32_t id,NBTString *name,int32_t version) {
    if(!valid(t))return false;
    MCObjectRootScope scope={0};MCObjectHeap *h=t->object.heap;
    if(!MCObjectRootScope_begin(&scope,h)||!MCObjectRootScope_pin(&scope,(MCObject *)t)){MCObjectRootScope_end(&scope);return fail(h);}
    WorldTypeStatics *s=WorldType_getStatics(h);bool ok=s&&construct(t,s,id,name,version);
    MCObjectRootScope_end(&scope);return ok;
}
NBTString *WorldType_getWorldTypeName(WorldType *t){return valid(t)?t->worldType:NULL;}
NBTString *WorldType_getTranslateName(WorldType *t){return valid(t)?NativeJavaString_concat(t->object.heap,"generator.",t->worldType,""):NULL;}
NBTString *WorldType_getTranslatedInfo(WorldType *t) {
    if(!valid(t))return NULL;
    MCObjectHeap *h=t->object.heap;MCObjectRootScope scope={0};
    if(!MCObjectRootScope_begin(&scope,h))return NULL;
    NBTString *name=WorldType_getTranslateName(t);
    NBTString *result=name?NativeJavaString_concat(h,"",name,".info"):NULL;
    MCObjectRootScope_end(&scope);return result;
}
WorldType *WorldType_parseWorldType(MCObjectHeap *h,NBTString *name) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    WorldTypeStatics *s=WorldType_getStatics(h);WorldType *result=NULL;
    if(name&&(!NBTString_isInstance((MCObject *)name)||((MCObject *)name)->heap!=h)){fail(h);goto done;}
    for(size_t i=0;s&&i<16;i++) {
        WorldType *t=s->worldTypes->items[i];if(!t)continue;
        if(!WorldType_isInstance((MCObject *)t)||t->object.heap!=h){fail(h);break;}
        bool equal;if(!NativeJavaString_equalsIgnoreCase(t->worldType,name,&equal))break;
        if(equal){result=t;break;}
    }
done:
    MCObjectRootScope_end(&scope);return result;
}
int32_t WorldType_getGeneratorVersion(WorldType *t){return valid(t)?t->generatorVersion:0;}
WorldType *WorldType_getWorldTypeForGeneratorVersion(WorldType *t,int32_t version) {
    if(!valid(t))return NULL;
    WorldTypeStatics *s=WorldType_getStatics(t->object.heap);
    return s&&t==s->DEFAULT&&version==0?s->DEFAULT_1_1:t;
}
bool WorldType_getCanBeCreated(WorldType *t){return valid(t)&&t->canBeCreated;}
bool WorldType_isVersioned(WorldType *t){return valid(t)&&t->isWorldTypeVersioned;}
int32_t WorldType_getWorldTypeID(WorldType *t){return valid(t)?t->worldTypeId:0;}
bool WorldType_showWorldInfoNotice(WorldType *t){return valid(t)&&t->hasNotificationData;}
