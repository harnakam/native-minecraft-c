#include "item/ItemMapData.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static int32_t bits(uint32_t n) {int32_t out;memcpy(&out,&n,sizeof(out));return out;}
static int32_t java_floor(double n) {int32_t i=isnan(n)?0:n>=INT32_MAX?INT32_MAX:n<=INT32_MIN?INT32_MIN:(int32_t)n;return n<(double)i?bits((uint32_t)i-1):i;}
/* Native MapData field adapter for the original calculateMapCenter method.
   Its integer multiplication/addition wrap, including extreme spawn values. */
void ItemMapData_calculateMapCenter(mc_map_info *map,double x,double z,int32_t scale) {
    int32_t size=bits(128u<<(uint32_t)(scale&31));
    int32_t a=java_floor((x+64.0)/(double)size),b=java_floor((z+64.0)/(double)size);
    map->center_x=bits((uint32_t)a*(uint32_t)size+(uint32_t)(size/2)-64u);
    map->center_z=bits((uint32_t)b*(uint32_t)size+(uint32_t)(size/2)-64u);
}
int32_t ItemMapData_getUniqueDataId(MCGameplayWorld *world) {
    MCObjectHeap *h=world?world->object.heap:NULL;
    if(!MCGameplayWorld_isInstance((MCObject *)world)){MCObjectHeap_fail(h);return 0;}
    if(world->maps.next_id<0||world->maps.next_id>UINT16_MAX){MCObjectHeap_fail(h);return 0;}
    uint16_t value=(uint16_t)world->maps.next_id;int16_t source;memcpy(&source,&value,sizeof(source));
    world->maps.next_id=(int32_t)((value+1u)&UINT16_MAX);MCObjectHeap_touch(h);return source;
}
mc_map_info *ItemMapData_nativeSetItemData(MCGameplayWorld *world,const mc_map_info *source) {
    MCObjectHeap *h=world?world->object.heap:NULL;
    if(!MCGameplayWorld_isInstance((MCObject *)world)){MCObjectHeap_fail(h);return NULL;}
    mc_maps *maps=&world->maps;mc_map_info copy={0};
    if(maps->count>maps->capacity||maps->capacity>MC_MAX_MAPS||(!maps->capacity?maps->entries!=NULL:maps->entries==NULL)||maps->next_id<0||maps->next_id>UINT16_MAX){MCObjectHeap_fail(h);return NULL;}
    if(!source||!mc_map_info_copy(&copy,source)){MCObjectHeap_fail(h);return NULL;}
    mc_map_info *old=mc_maps_find(&world->maps,copy.id);
    if(old){mc_map_info_free(old);*old=copy;MCObjectHeap_touch(h);return old;}
    if(maps->count>=MC_MAX_MAPS){mc_map_info_free(&copy);MCObjectHeap_fail(h);return NULL;}
    if(maps->count==maps->capacity) {
        size_t capacity=maps->capacity?maps->capacity*2:4;if(capacity>MC_MAX_MAPS)capacity=MC_MAX_MAPS;
        mc_map_info *entries=realloc(maps->entries,capacity*sizeof(*entries));
        if(!entries){mc_map_info_free(&copy);MCObjectHeap_fail(h);return NULL;}
        maps->entries=entries;maps->capacity=capacity;
    }
    maps->entries[maps->count++]=copy;MCObjectHeap_touch(h);return &maps->entries[maps->count-1];
}
mc_map_info *ItemMap_getMapData(ItemStack *stack,MCGameplayWorld *world) {
    MCObjectHeap *h=stack?stack->object.heap:world?world->object.heap:NULL;
    if(!stack||!MCGameplayWorld_isInstance((MCObject *)world)||world->object.heap!=h){MCObjectHeap_fail(h);return NULL;}
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)stack)&&MCObjectRootScope_pin(&scope,(MCObject *)world);
    mc_map_info *map=ok?mc_maps_find(&world->maps,ItemStack_getMetadata(stack)):NULL;
    if(ok&&!map&&!world->remote) {
        /* ItemMap source order: allocate ID, mutate the exact stack, construct
           MapData, scale/center/dimension, markDirty, then setItemData. */
        int32_t id=ItemMapData_getUniqueDataId(world);
        if(!MCObjectHeap_failed(h)) {
            ItemStack_setItemDamage(stack,id);
            mc_map_info created={0};created.id=ItemStack_getMetadata(stack);created.scale=3;
            ItemMapData_calculateMapCenter(&created,(double)world->spawnX,(double)world->spawnZ,created.scale);
            uint8_t dimension=(uint8_t)world->dimension;memcpy(&created.dimension,&dimension,sizeof(dimension));
            created.metadata_known=true;created.dirty=true;MCObjectHeap_touch(h);
            map=ItemMapData_nativeSetItemData(world,&created);
        }
    }
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(h)?NULL:map;
}
