#include "world/EnumSkyBlock.h"
static void trace_statics(MCObject *object,MCObjectVisitor visitor,void *context){
    EnumSkyBlockStatics *s=(EnumSkyBlockStatics *)object;
    s->SKY=(EnumSkyBlock *)visitor((MCObject *)s->SKY,context);
    s->BLOCK=(EnumSkyBlock *)visitor((MCObject *)s->BLOCK,context);
}
static const MCObjectClass enumClass={"net.minecraft.world.EnumSkyBlock",MCObjectHeap_plainClone,NULL,NULL};
static const MCObjectClass staticsClass={"native.EnumSkyBlock.Statics",MCObjectHeap_plainClone,trace_statics,NULL};
bool EnumSkyBlock_isInstance(const MCObject *object){
    return object&&object->klass==&enumClass&&MCObjectHeap_objectSize(object)>=sizeof(EnumSkyBlock);
}
EnumSkyBlock *EnumSkyBlock_nativeAllocate(MCObjectHeap *heap){
    return (EnumSkyBlock *)MCObjectHeap_alloc(heap,sizeof(EnumSkyBlock),&enumClass);
}
bool EnumSkyBlock_construct(EnumSkyBlock *value,int32_t light){
    MCObjectHeap *heap=value?value->object.heap:NULL;
    if(!EnumSkyBlock_isInstance((MCObject *)value)||MCObjectHeap_failed(heap)){MCObjectHeap_fail(heap);return false;}
    value->defaultLightValue=light;MCObjectHeap_touch(heap);return true;
}
static bool any(const MCObject *object,void *context){(void)object;(void)context;return true;}
EnumSkyBlockStatics *EnumSkyBlock_getStatics(MCObjectHeap *heap){
    if(!heap||MCObjectHeap_failed(heap))return NULL;
    EnumSkyBlockStatics *s=(EnumSkyBlockStatics *)MCObjectHeap_findObject(heap,&staticsClass,any,NULL);
    if(s)return s;
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    s=(EnumSkyBlockStatics *)MCObjectHeap_alloc(heap,sizeof(*s),&staticsClass);
    MCObjectRoot staticRoot={0};bool ok=s&&MCObjectRoot_init(&staticRoot,heap,(MCObject *)s);
    if(ok){
        EnumSkyBlock *sky=EnumSkyBlock_nativeAllocate(heap);
        ok=sky&&EnumSkyBlock_construct(sky,15);
        if(ok){s->SKY=sky;MCObjectHeap_touch(heap);}
    }
    if(ok){
        EnumSkyBlock *block=EnumSkyBlock_nativeAllocate(heap);
        ok=block&&EnumSkyBlock_construct(block,0);
        if(ok){s->BLOCK=block;MCObjectHeap_touch(heap);}
    }
    if(!ok)MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap)?s:NULL;
}
