#include "inventory/InventoryEnderChest.h"
#include "nbt/NBTTagCompound.h"
#include "nbt/NBTTagList.h"
#include "nbt/NBTInternal.h"
static void trace(MCObject *o,MCObjectVisitor v,void *ctx){InventoryEnderChest *e=(InventoryEnderChest *)o;InventoryBasic_traceFields(&e->basic,v,ctx);e->associatedChest=v(e->associatedChest,ctx);}
const MCObjectClass c919_inventoryenderchest_class={"InventoryEnderChest",MCObjectHeap_plainClone,trace,NULL};
bool InventoryEnderChest_isInstance(const MCObject *o){return o&&o->klass==&c919_inventoryenderchest_class&&MCObjectHeap_objectSize(o)>=sizeof(InventoryEnderChest);}
static bool valid(InventoryEnderChest *e){if(!InventoryEnderChest_isInstance((MCObject *)e)){MCObjectHeap_fail(e?e->basic.object.heap:NULL);return false;}return !MCObjectHeap_failed(e->basic.object.heap);}
static bool effect(InventoryEnderChest *e,bool ok){if(!ok)MCObjectHeap_fail(e->basic.object.heap);return ok&&!MCObjectHeap_failed(e->basic.object.heap);}
static bool begin(InventoryEnderChest *e,MCObjectRootScope *s){if(!valid(e)||!MCObjectRootScope_begin(s,e->basic.object.heap))return false;if(MCObjectRootScope_pin(s,(MCObject *)e))return true;MCObjectRootScope_end(s);return false;}
InventoryEnderChest *InventoryEnderChest_new(MCObjectHeap *heap,const InventoryBasicDependencies *deps,const InventoryEnderChestDependencies *chest,MCObject *ctx) {
    InventoryEnderChest *e=(InventoryEnderChest *)MCObjectHeap_alloc(heap,sizeof *e,&c919_inventoryenderchest_class);if(!e)return NULL;
    MCObjectRootScope scope={0};if(!begin(e,&scope))return NULL;
    NBTString *name=NBTString_literalASCII(heap,"container.enderchest");e->chestDependencies=chest;
    bool ok=name&&InventoryBasic_construct(&e->basic,name,false,27,deps,ctx);MCObjectRootScope_end(&scope);return ok?e:NULL;
}
bool InventoryEnderChest_setChestTileEntity(InventoryEnderChest *e,MCObject *chest){if(!valid(e)||!effect(e,!chest||chest->heap==e->basic.object.heap))return false;e->associatedChest=chest;MCObjectHeap_touch(e->basic.object.heap);return true;}
ItemStackNBTResult InventoryEnderChest_loadInventoryFromNBT(InventoryEnderChest *e,NBTTagList *list){
    MCObjectRootScope scope={0};if(!begin(e,&scope))return ITEMSTACK_NBT_FAILURE;ItemStackNBTResult status=ITEMSTACK_NBT_FAILURE;
    /* Source clears first, before dereferencing the input list. */
    for(int32_t i=0;i<InventoryBasic_getSizeInventory(&e->basic);i++)if(!InventoryBasic_setInventorySlotContents(&e->basic,i,NULL))goto done;
    if(!list||MCObjectHeap_objectSize((MCObject *)list)<sizeof *list||NBTBase_getId((NBTBase *)list)!=9||((MCObject *)list)->heap!=e->basic.object.heap||!MCObjectRootScope_pin(&scope,(MCObject *)list))goto done;
    for(int32_t k=0;k<NBTTagList_tagCount(list);k++){
        NBTTagCompound *n=NBTTagList_getCompoundTagAt(list,k);if(!n)goto done;
        int32_t i=(uint8_t)NBTTagCompound_getByte_ascii(n,"Slot");if(MCObjectHeap_failed(e->basic.object.heap))goto done;
        if(i>=0&&i<InventoryBasic_getSizeInventory(&e->basic)){
            ItemStackNBTResult result;ItemStack *s=ItemStack_loadItemStackFromNBT(e->basic.object.heap,n,&result);
            if(result!=ITEMSTACK_NBT_OK){status=result;goto done;}
            if(!InventoryBasic_setInventorySlotContents(&e->basic,i,s))goto done;
        }
    }
    status=ITEMSTACK_NBT_OK;
done:if(status!=ITEMSTACK_NBT_OK)MCObjectHeap_fail(e->basic.object.heap);MCObjectRootScope_end(&scope);return status;
}
NBTTagList *InventoryEnderChest_saveInventoryToNBT(InventoryEnderChest *e){
    MCObjectRootScope scope={0};if(!begin(e,&scope))return NULL;NBTTagList *list=NBTTagList_new(e->basic.object.heap);bool ok=list!=NULL;
    for(int32_t i=0;ok&&i<InventoryBasic_getSizeInventory(&e->basic);i++){
        ItemStack *s=InventoryBasic_getStackInSlot(&e->basic,i);if(MCObjectHeap_failed(e->basic.object.heap)){ok=false;break;}
        if(s){NBTTagCompound *n=NBTTagCompound_new(e->basic.object.heap);uint8_t byte=(uint8_t)i;int8_t slot=byte<=127?(int8_t)byte:(int8_t)(-1-(int8_t)(255-byte));ok=n&&NBTTagCompound_setByte_ascii(n,"Slot",slot)&&ItemStack_writeToNBT(s,n)&&NBTTagList_appendTag(list,(NBTBase *)n);}
    }
    if(!ok)MCObjectHeap_fail(e->basic.object.heap);
    MCObjectRootScope_end(&scope);return ok?list:NULL;
}
static bool inputs(InventoryEnderChest *e,MCObjectRootScope *s,MCObject *player){return effect(e,(!player||player->heap==e->basic.object.heap)&&(!e->associatedChest||e->associatedChest->heap==e->basic.object.heap)&&(!e->basic.context||e->basic.context->heap==e->basic.object.heap))&&MCObjectRootScope_pin(s,player);}
bool InventoryEnderChest_isUseableByPlayer(InventoryEnderChest *e,MCObject *player){
    MCObjectRootScope scope={0};if(!begin(e,&scope))return false;bool out=false;
    if(!inputs(e,&scope,player))goto done;
    if(e->associatedChest){bool allowed;const InventoryEnderChestDependencies *d=e->chestDependencies;if(!d||!d->canBeUsed){MCObjectHeap_fail(e->basic.object.heap);goto done;}if(!effect(e,d->canBeUsed(e->basic.context,e->associatedChest,player,&allowed)))goto done;if(!allowed)goto done;}
    out=InventoryBasic_isUseableByPlayer(&e->basic,player);
done:MCObjectRootScope_end(&scope);return out&&!MCObjectHeap_failed(e->basic.object.heap);
}
bool InventoryEnderChest_openInventory(InventoryEnderChest *e,MCObject *player){
    MCObjectRootScope scope={0};if(!begin(e,&scope))return false;bool ok=false;
    if(!inputs(e,&scope,player))goto done;
    if(e->associatedChest){const InventoryEnderChestDependencies *d=e->chestDependencies;if(!d||!d->openChest||!effect(e,d->openChest(e->basic.context,e->associatedChest)))goto done;}
    InventoryBasic_openInventory(&e->basic,player);ok=true;
done:if(!ok)MCObjectHeap_fail(e->basic.object.heap);MCObjectRootScope_end(&scope);return ok;
}
bool InventoryEnderChest_closeInventory(InventoryEnderChest *e,MCObject *player){
    MCObjectRootScope scope={0};if(!begin(e,&scope))return false;bool ok=false;
    if(!inputs(e,&scope,player))goto done;
    if(e->associatedChest){const InventoryEnderChestDependencies *d=e->chestDependencies;if(!d||!d->closeChest||!effect(e,d->closeChest(e->basic.context,e->associatedChest)))goto done;}
    InventoryBasic_closeInventory(&e->basic,player);e->associatedChest=NULL;MCObjectHeap_touch(e->basic.object.heap);ok=true;
done:if(!ok)MCObjectHeap_fail(e->basic.object.heap);MCObjectRootScope_end(&scope);return ok;
}
IInventory InventoryEnderChest_asIInventory(InventoryEnderChest *e){return InventoryBasic_asIInventory((InventoryBasic *)e);}
