#include "item/ItemEmptyMap.h"
#include "item/ItemMapData.h"
#include <string.h>

static bool fail(MCObjectHeap *heap) {MCObjectHeap_fail(heap);return false;}

ItemStack *ItemEmptyMap_onItemRightClick(const Item *self,ItemStack *input,
    MCGameplayWorld *world,MCGameplayPlayer *player,
    const ItemEmptyMapDependencies *dependencies,MCObject *context) {
    MCObjectHeap *heap=input?input->object.heap:world?world->object.heap:NULL;
    if(!self||!ItemStack_isInstance((MCObject *)input)||
        !MCGameplayWorld_isInstance((MCObject *)world)||
        !MCGameplayPlayer_isInstance((MCObject *)player)||world->object.heap!=heap||
        player->living.entity.object.heap!=heap||(context&&context->heap!=heap)) {
        fail(heap);return NULL;
    }
    MCObjectRootScope scope={0};
    if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    bool ok=MCObjectRootScope_pin(&scope,(MCObject *)input)&&
        MCObjectRootScope_pin(&scope,(MCObject *)world)&&
        MCObjectRootScope_pin(&scope,(MCObject *)player)&&
        MCObjectRootScope_pin(&scope,context);
    ItemStack *filled=NULL,*result=NULL;
    if(ok) {
        int32_t id=ItemMapData_getUniqueDataId(world);
        if(!MCObjectHeap_failed(heap))filled=ItemStack_new(heap,ItemStack_registryItem(358),1,id);
        if(filled) {
            /* MapData's source constructor initializes these fields to zero.
               Store that object before configuring its fields, unlike getMapData. */
            mc_map_info created={0};created.id=ItemStack_getMetadata(filled);
            mc_map_info *stored=ItemMapData_nativeSetItemData(world,&created);
            if(stored) {
                stored->scale=0;
                ItemMapData_calculateMapCenter(stored,player->living.entity.posX,player->living.entity.posZ,stored->scale);
                WorldProvider *provider=world->provider;
                if(!WorldProvider_isInstance((MCObject *)provider)||provider->object.heap!=heap){fail(heap);goto done;}
                uint8_t dimension=(uint8_t)WorldProvider_getDimensionId(provider);
                if(MCObjectHeap_failed(heap))goto done;
                memcpy(&stored->dimension,&dimension,sizeof(dimension));
                stored->metadata_known=true;stored->dirty=true;MCObjectHeap_touch(heap);
                uint32_t count=(uint32_t)input->stackSize-1u;
                memcpy(&input->stackSize,&count,sizeof(count));MCObjectHeap_touch(heap);
                if(input->stackSize<=0)result=filled;
                else {
                    ItemStack *copy=ItemStack_copy(heap,filled);
                    if(copy) {
                        if(!player->inventory||player->inventory->object.heap!=heap)fail(heap);
                        bool inserted=!MCObjectHeap_failed(heap)&&InventoryPlayer_addItemStackToInventory(player->inventory,copy);
                        ok=!MCObjectHeap_failed(heap);
                        if(ok&&!inserted) {
                            EntityItem *ignored=NULL;
                            ok=dependencies&&dependencies->dropPlayerItemWithRandomChoice&&
                                dependencies->dropPlayerItemWithRandomChoice(context,player,filled,false,&ignored);
                            if(ok&&ignored&&(!EntityItem_isInstance((MCObject *)ignored)||ignored->entity.object.heap!=heap))ok=false;
                            if(!ok)fail(heap);
                        }
                        if(ok&&!MCObjectHeap_failed(heap)) {
                            if(!dependencies||!dependencies->objectUseStat)fail(heap);
                            else {
                                StatBase *stat=dependencies->objectUseStat(context,self);
                                if(!MCObjectHeap_failed(heap)) {
                                    if(stat&&(stat->object.heap!=heap||StatBase_getKind(stat)==STAT_BASE_KIND_UNKNOWN))fail(heap);
                                    else if(!dependencies->triggerAchievement||
                                        !dependencies->triggerAchievement(context,player,stat))fail(heap);
                                    if(!MCObjectHeap_failed(heap))result=input;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
done:
    MCObjectRootScope_end(&scope);
    return MCObjectHeap_failed(heap)?NULL:result;
}
