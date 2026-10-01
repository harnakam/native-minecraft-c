#include "world/WorldDataStorage.h"
#include <limits.h>
#include <string.h>
static bool valid(const MCGameplayWorld *world) {
    MCObjectHeap *heap=world?world->object.heap:NULL;
    if(!MCGameplayWorld_isInstance((MCObject *)world)||!MapStorage_isInstance((MCObject *)world->mapStorage)||
       world->mapStorage->object.heap!=heap||MCObjectHeap_failed(heap)) {
        MCObjectHeap_fail(heap);return false;
    }
    return true;
}
bool World_getUniqueDataId(MCGameplayWorld *world,NBTString *key,int32_t *output) {
    if(!valid(world)||!output) {MCObjectHeap_fail(world?world->object.heap:NULL);return false;}
    return MapStorage_getUniqueDataId(world->mapStorage,key,output);
}
bool World_nativeMapNextProjection(const MCGameplayWorld *world,int32_t *output) {
    return valid(world)&&MapStorage_nativeGetMapNextProjection(world->mapStorage,output);
}
bool World_nativeImportMapNextProjection(MCGameplayWorld *world,int32_t next) {
    if(!valid(world)||next<0||next>UINT16_MAX) {MCObjectHeap_fail(world?world->object.heap:NULL);return false;}
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,world->object.heap))return false;
    NBTString *key=NBTString_fromASCII(world->object.heap,"map");
    uint16_t bits=(uint16_t)((uint32_t)next-1u);int16_t last;memcpy(&last,&bits,sizeof(last));
    bool ok=key&&MapStorage_nativeImportExactShort(world->mapStorage,key,last);
    MCObjectRootScope_end(&scope);return ok;
}
