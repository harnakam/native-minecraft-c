#include "util/MCGameplayStorage.h"
#include "util/MCGameplayPackets.h"
#include "inventory/ContainerWorkbench.h"
#include "inventory/inventory_dispatch.h"
#include "nbt/NBTTagString.h"
#include "nbt/NBTTagDouble.h"
#include "entity/item/NativeItemMotion.h"
#include <limits.h>
#include <math.h>

static bool fail(MCObjectHeap *heap) {MCObjectHeap_fail(heap);return false;}
static bool same(MCObjectHeap *heap,const MCObject *o) {return !o||o->heap==heap||fail(heap);}
static MCGameplayWorld *world(const MCGameplayObjects *o) {
    MCObjectHeap *heap=o?o->object.heap:NULL;
    if (!o||!o->world||!same(heap,o->world)||!MCGameplayWorld_isInstance(o->world)) {fail(heap);return NULL;}
    MCGameplayWorld *w=(MCGameplayWorld *)o->world;
    if (w->owners!=o||o->itemCount>MC_GAMEPLAY_MAX_ITEMS) {fail(heap);return NULL;}
    return w;
}
static MCGameplayPlayer *player(const MCGameplayObjects *o,size_t index) {
    MCObjectHeap *heap=o?o->object.heap:NULL;MCGameplayWorld *w=world(o);
    if (!w||index>=MC_TRANSFER_MAX_PLAYERS||!o->players[index]||!same(heap,o->players[index])||
        !MCGameplayPlayer_isInstance(o->players[index])) {fail(heap);return NULL;}
    MCGameplayPlayer *p=(MCGameplayPlayer *)o->players[index];
    if (p->worldObj!=w||!p->inventory||!same(heap,(MCObject *)p->inventory)||
        !p->inventoryContainer||!same(heap,(MCObject *)p->inventoryContainer)||
        !p->openContainer||!same(heap,(MCObject *)p->openContainer)) {fail(heap);return NULL;}
    return p;
}
static NBTTagCompound *copy_fields(MCObjectHeap *heap,NBTTagCompound *saved) {
    if (saved&&(!same(heap,(MCObject *)saved)||NBTBase_getId((NBTBase *)saved)!=10)) {fail(heap);return NULL;}
    return saved?(NBTTagCompound *)NBTBase_copy(heap,(NBTBase *)saved):NBTTagCompound_new(heap);
}
/* Root names are source String values, including NUL and lone UTF-16
   surrogates. Reuse the translated modified-UTF string payload methods rather
   than converting through lossy native UTF-8 filenames or C strings. */
static bool decode_named(MCObjectHeap *heap,const mc_nbt *data,NBTTagCompound **tag,NBTString **name) {
    if (!data||!data->data||!data->size||data->size>MC_NBT_MAX_BYTES||data->data[0]!=10||
        !mc_nbt_validate(data->data,data->size))return fail(heap);
    /* Persistent-file reads use the source unlimited tracker. The native
       immutable-file cap and bounded heap enforce this adapter's limits. */
    mc_buf in={data->data,data->size,data->size,1,false};NBTSizeTracker tracker;NBTSizeTracker_initInfinite(&tracker);
    NBTTagString *named=NBTTagString_new(heap,NBTString_literalASCII(heap,""));
    NBTTagCompound *root=NBTTagCompound_new(heap);
    if (!named||!root||!NBTBase_read((NBTBase *)named,&in,0,&tracker)||
        !NBTBase_read((NBTBase *)root,&in,0,&tracker)||in.failed||in.pos!=in.len)return fail(heap);
    *name=NBTTagString_getString(named);*tag=root;return !MCObjectHeap_failed(heap);
}
static bool encode_named(MCObjectHeap *heap,NBTTagCompound *root,NBTString *name,mc_nbt *output) {
    if (!output||!root||!same(heap,(MCObject *)root)||!same(heap,(MCObject *)name))return fail(heap);
    if (!name)name=NBTString_literalASCII(heap,"");
    NBTTagString *named=name?NBTTagString_new(heap,name):NULL;mc_buf out={0};mc_put_u8(&out,10);
    bool ok=named&&NBTBase_write((NBTBase *)named,&out)&&NBTBase_write((NBTBase *)root,&out)&&
        !out.failed&&out.len<=MC_NBT_MAX_BYTES;
    mc_nbt next={0};if (ok) {out.pos=0;ok=mc_nbt_read(&out,&next)&&out.pos==out.len;}
    mc_buf_free(&out);
    if (!ok) {mc_nbt_free(&next);return fail(heap);}
    mc_nbt_free(output);*output=next;return !MCObjectHeap_failed(heap);
}
static const char *const player_fields[]={"Inventory","SelectedItemSlot","C919Crafting","C919Cursor","C919Workbench"};
static bool strip_player(NBTTagCompound *root) {
    for(size_t i=0;i<sizeof(player_fields)/sizeof(player_fields[0]);i++)
        if (!NBTTagCompound_removeTag_ascii(root,player_fields[i]))return false;
    return true;
}
static NBTTagList *grid_list(InventoryCrafting *grid,int32_t expected) {
    MCObjectHeap *heap=grid?grid->object.heap:NULL;
    if (!grid||InventoryCrafting_getSizeInventory(grid)!=expected) {fail(heap);return NULL;}
    NBTTagList *list=NBTTagList_new(heap);if (!list)return NULL;
    for(int32_t i=0;i<expected;i++) {
        ItemStack *stack=InventoryCrafting_getStackInSlot(grid,i);
        if (!same(heap,(MCObject *)stack))return NULL;
        if (stack) {
            NBTTagCompound *entry=NBTTagCompound_new(heap);
            if (!entry||!NBTTagCompound_setByte_ascii(entry,"Slot",(int8_t)(i+1))||
                !ItemStack_writeToNBT(stack,entry)||!NBTTagList_appendTag(list,(NBTBase *)entry))return NULL;
        }
    }
    return list;
}
bool MCGameplayStorage_encodePlayer(const MCGameplayObjects *objects,size_t index,mc_nbt *output,void *context) {
    (void)context;MCObjectHeap *heap=objects?objects->object.heap:NULL;MCObjectRootScope scope={0};
    if (!MCObjectRootScope_begin(&scope,heap))return false;
    MCGameplayPlayer *p=player(objects,index);bool ok=p&&MCGameplayPackets_validate(p);
    NBTTagCompound *root=ok?copy_fields(heap,p->savedFields):NULL;
    NBTTagList *inventory=root?NBTTagList_new(heap):NULL;
    ok=root&&inventory&&InventoryPlayer_writeToNBT(p->inventory,inventory)&&strip_player(root)&&
        NBTTagCompound_setTag_ascii(root,"Inventory",(NBTBase *)inventory)&&
        NBTTagCompound_setInteger_ascii(root,"SelectedItemSlot",p->inventory->currentItem);
    NBTTagList *grid=ok?grid_list(p->inventoryContainer->craftMatrix,4):NULL;
    if (ok)ok=grid&&NBTTagCompound_setTag_ascii(root,"C919Crafting",(NBTBase *)grid);
    NBTTagCompound *cursor=ok?NBTTagCompound_new(heap):NULL;
    ItemStack *held=ok?InventoryPlayer_getItemStack(p->inventory):NULL;
    if (ok)ok=cursor&&same(heap,(MCObject *)held)&&(!held||ItemStack_writeToNBT(held,cursor))&&
        NBTTagCompound_setTag_ascii(root,"C919Cursor",(NBTBase *)cursor);
    if (ok&&p->openContainer!=&p->inventoryContainer->container) {
        if (!ContainerWorkbench_isInstance((MCObject *)p->openContainer))ok=fail(heap);
        else {
            ContainerWorkbench *bench=(ContainerWorkbench *)p->openContainer;
            /* Recovery-only inputs must be source-closed before commitment. */
            if (!bench->hasPosition)ok=fail(heap);
            else {grid=grid_list(bench->craftMatrix,9);ok=grid&&NBTTagCompound_setTag_ascii(root,"C919Workbench",(NBTBase *)grid);}
        }
    }
    if (ok)ok=encode_named(heap,root,p->savedRootName,output);
    if (!ok)fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap);
}
static bool vector_write(NBTTagCompound *root,const char *name,double x,double y,double z) {
    MCObjectHeap *heap=((MCObject *)root)->heap;double values[]={x,y,z};NBTTagList *list=NBTTagList_new(heap);if (!list)return false;
    for(unsigned i=0;i<3;i++) {
        if (!isfinite(values[i]))return fail(heap);
        NBTTagDouble *value=NBTTagDouble_new(heap,values[i]);if (!value||!NBTTagList_appendTag(list,(NBTBase *)value))return false;
    }
    return NBTTagCompound_setTag_ascii(root,name,(NBTBase *)list);
}
static bool entity_write(EntityItem *e,NBTTagList *list) {
    MCObjectHeap *heap=e->object.heap;NBTTagCompound *root=copy_fields(heap,e->savedFields);
    if (!NativeItemMotion_positionSupported(e->posX,e->posY,e->posZ))return fail(heap);
    if (!root||!NBTTagCompound_setString_ascii(root,"id",NBTString_literalASCII(heap,"Item"))||
        !NBTTagCompound_setInteger_ascii(root,"C919EntityId",e->entityId)||
        !vector_write(root,"Pos",e->posX,e->posY,e->posZ)||
        !vector_write(root,"Motion",e->motionX,e->motionY,e->motionZ)||
        !NBTTagCompound_setBoolean_ascii(root,"OnGround",e->onGround))return false;
    /* Unknown envelope fields survive, while absent source owner/thrower do
       not resurrect stale values from a previous file. */
    if (!NBTTagCompound_removeTag_ascii(root,"Owner")||!NBTTagCompound_removeTag_ascii(root,"Thrower")||
        !NBTTagCompound_removeTag_ascii(root,"Item")||!EntityItem_writeEntityToNBT(e,root))return false;
    return NBTTagList_appendTag(list,(NBTBase *)root);
}
bool MCGameplayStorage_encodeItems(const MCGameplayObjects *objects,mc_nbt *output,void *context) {
    (void)context;MCObjectHeap *heap=objects?objects->object.heap:NULL;MCObjectRootScope scope={0};
    if (!MCObjectRootScope_begin(&scope,heap))return false;
    MCGameplayWorld *w=world(objects);NBTTagCompound *root=w?copy_fields(heap,w->savedItemFields):NULL;
    NBTTagList *list=root?NBTTagList_new(heap):NULL;bool ok=root&&list;
    for(size_t i=0;i<(ok?objects->itemCount:0);i++) {
        MCObject *object=objects->items[i];
        if (!object||!same(heap,object)||!EntityItem_isInstance(object)) {ok=fail(heap);break;}
        EntityItem *e=(EntityItem *)object;if (e->worldObj!=(MCObject *)w) {ok=fail(heap);break;}
        if (!e->isDead) {
            for(size_t j=0;j<i;j++)if (!((EntityItem *)objects->items[j])->isDead&&((EntityItem *)objects->items[j])->entityId==e->entityId) {ok=fail(heap);break;}
            if (!ok||!entity_write(e,list)) {ok=false;break;}
        }
    }
    if (ok)ok=NBTTagCompound_setInteger_ascii(root,"Version",1)&&NBTTagCompound_setTag_ascii(root,"Entities",(NBTBase *)list)&&encode_named(heap,root,w->savedItemRootName,output);
    if (!ok)fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap);
}
bool MCGameplayStorage_encodeMaps(const MCGameplayObjects *objects,mc_nbt *output,void *context) {
    (void)context;MCObjectHeap *heap=objects?objects->object.heap:NULL;MCObjectRootScope scope={0};
    if (!MCObjectRootScope_begin(&scope,heap))return false;
    MCGameplayWorld *w=world(objects);bool ok=w&&output&&mc_maps_encode(&w->maps,output);
    if (!ok)fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap);
}
static bool load_grid(NBTTagCompound *root,const char *name,InventoryCrafting *grid,int32_t count) {
    MCObjectHeap *heap=((MCObject *)root)->heap;InventoryCrafting_clear(grid);
    if (!NBTTagCompound_hasKey_ascii(root,name))return !MCObjectHeap_failed(heap);
    if (!NBTTagCompound_hasKeyType_ascii(root,name,9))return fail(heap);
    NBTTagList *list=NBTTagCompound_getTagList_ascii(root,name,10);
    NBTBase *raw=NBTTagCompound_getTag_ascii(root,name);
    if (list!=(NBTTagList *)raw||NBTTagList_tagCount(list)>count)return fail(heap);
    bool used[9]={false};
    for(int32_t i=0;i<NBTTagList_tagCount(list);i++) {
        NBTTagCompound *entry=NBTTagList_getCompoundTagAt(list,i);
        if (!entry||!NBTTagCompound_hasKeyType_ascii(entry,"Slot",1))return fail(heap);
        int32_t index=(uint8_t)NBTTagCompound_getByte_ascii(entry,"Slot")-1;
        if (index<0||index>=count||used[index])return fail(heap);
        used[index]=true;
        ItemStackNBTResult status;ItemStack *stack=ItemStack_loadItemStackFromNBT(heap,entry,&status);
        if (status!=ITEMSTACK_NBT_OK||!InventoryCrafting_setInventorySlotContents(grid,index,stack))return fail(heap);
    }
    return !MCObjectHeap_failed(heap);
}
bool MCGameplayStorage_loadPlayer(MCGameplayPlayer *p,const mc_nbt *input,const mc_crafting_dispatch *d) {
    MCObjectHeap *heap=p?p->object.heap:NULL;MCObjectRootScope scope={0};
    if (!p||!MCGameplayPlayer_isInstance((MCObject *)p)||!p->inventory||!p->inventoryContainer||!p->worldObj||
        !same(heap,(MCObject *)p->inventory)||!same(heap,(MCObject *)p->inventoryContainer)||
        !same(heap,(MCObject *)p->worldObj))return fail(heap);
    if (!MCObjectRootScope_begin(&scope,heap))return false;
    NBTTagCompound *root=NULL;NBTString *name=NULL;bool ok=decode_named(heap,input,&root,&name);
    NBTTagList *inventory=ok?NBTTagCompound_getTagList_ascii(root,"Inventory",10):NULL;
    if (ok)ok=inventory&&InventoryPlayer_readFromNBT(p->inventory,inventory)==ITEMSTACK_NBT_OK;
    if (ok) {p->inventory->currentItem=NBTTagCompound_getInteger_ascii(root,"SelectedItemSlot");MCObjectHeap_touch(heap);}
    if (ok)ok=load_grid(root,"C919Crafting",p->inventoryContainer->craftMatrix,4)&&
        ContainerPlayer_onCraftMatrixChanged(p->inventoryContainer,mc_IInventory_crafting(p->inventoryContainer->craftMatrix));
    if (ok) {
        ItemStack *cursor=NULL;
        if (NBTTagCompound_hasKey_ascii(root,"C919Cursor")) {
            if (!NBTTagCompound_hasKeyType_ascii(root,"C919Cursor",10))ok=fail(heap);
            else {ItemStackNBTResult status;cursor=ItemStack_loadItemStackFromNBT(heap,NBTTagCompound_getCompoundTag_ascii(root,"C919Cursor"),&status);ok=status==ITEMSTACK_NBT_OK;}
        }
        if (ok)ok=InventoryPlayer_setItemStack(p->inventory,cursor);
    }
    if (ok) {
        p->openContainer=&p->inventoryContainer->container;MCObjectHeap_touch(heap);
        if (NBTTagCompound_hasKey_ascii(root,"C919Workbench")) {
            ContainerWorkbench *bench=ContainerWorkbench_new(p->inventory,(MCObject *)p->worldObj,NULL,d);
            ok=bench&&load_grid(root,"C919Workbench",bench->craftMatrix,9);
            if (ok) {p->openContainer=&bench->container;MCObjectHeap_touch(heap);}
        }
    }
    if (ok) {ok=strip_player(root);if (ok) {p->savedFields=root;p->savedRootName=name;MCObjectHeap_touch(heap);}}
    if (!ok)fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap);
}
static bool vector_read(NBTTagCompound *root,const char *name,double out[3]) {
    if (!NBTTagCompound_hasKeyType_ascii(root,name,9))return false;
    NBTTagList *list=NBTTagCompound_getTagList_ascii(root,name,6);
    if ((NBTBase *)list!=NBTTagCompound_getTag_ascii(root,name)||NBTTagList_tagCount(list)!=3||NBTTagList_getTagType(list)!=6)return false;
    for(int32_t i=0;i<3;i++) {out[i]=NBTTagList_getDoubleAt(list,i);if (!isfinite(out[i]))return false;}
    return true;
}
bool MCGameplayStorage_loadItems(MCGameplayWorld *w,const mc_nbt *input,MCObject *context,
    const EntityItemDependencies *d,const EntityItemConstructorDependencies *constructors) {
    MCObjectHeap *heap=w?w->object.heap:NULL;MCObjectRootScope scope={0};
    if (!w||!MCGameplayWorld_isInstance((MCObject *)w)||!w->owners||!same(heap,(MCObject *)w->owners)||!same(heap,context))return fail(heap);
    if (!MCObjectRootScope_begin(&scope,heap))return false;
    NBTTagCompound *root=NULL;NBTString *name=NULL;bool ok=decode_named(heap,input,&root,&name);
    if (ok)ok=NBTTagCompound_hasKeyType_ascii(root,"Version",3)&&NBTTagCompound_getInteger_ascii(root,"Version")==1&&NBTTagCompound_hasKeyType_ascii(root,"Entities",9);
    NBTTagList *list=ok?NBTTagCompound_getTagList_ascii(root,"Entities",10):NULL;
    if (ok)ok=list&&(NBTBase *)list==NBTTagCompound_getTag_ascii(root,"Entities")&&NBTTagList_tagCount(list)<=(int32_t)MC_GAMEPLAY_MAX_ITEMS;
    EntityItem *loaded[MC_GAMEPLAY_MAX_ITEMS];size_t count=0;
    for(int32_t i=0;i<(ok?NBTTagList_tagCount(list):0);i++) {
        NBTTagCompound *tag=NBTTagList_getCompoundTagAt(list,i);double pos[3],motion[3];
        ok=tag&&NBTTagCompound_hasKeyType_ascii(tag,"id",8)&&NBTString_equalsASCII(NBTTagCompound_getString_ascii(tag,"id"),"Item")&&
            NBTTagCompound_hasKeyType_ascii(tag,"C919EntityId",3)&&vector_read(tag,"Pos",pos)&&vector_read(tag,"Motion",motion);
        if (ok)ok=NativeItemMotion_positionSupported(pos[0],pos[1],pos[2]);
        if (!ok)break;
        EntityItem *e=EntityItem_new_world(heap,(MCObject *)w,context,d,constructors);if (!e) {ok=false;break;}
        /* Native restoration of the inherited envelope follows the source
           Entity read order for these fields. The complete Entity NBT method
           (UUID/rotation/fire/commands/etc.) remains an explicit dependency. */
        e->entityId=NBTTagCompound_getInteger_ascii(tag,"C919EntityId");e->posX=pos[0];e->posY=pos[1];e->posZ=pos[2];e->motionX=fabs(motion[0])>10?0:motion[0];e->motionY=fabs(motion[1])>10?0:motion[1];e->motionZ=fabs(motion[2])>10?0:motion[2];e->onGround=NBTTagCompound_getBoolean_ascii(tag,"OnGround");MCObjectHeap_touch(heap);
        for(size_t j=0;j<count;j++)if (loaded[j]->entityId==e->entityId) {ok=false;break;}
        if (!ok||!constructors->setPosition(context,e,pos[0],pos[1],pos[2])||MCObjectHeap_failed(heap)||
            EntityItem_readEntityFromNBT(e,tag)!=ITEMSTACK_NBT_OK||
            !constructors->setPosition(context,e,e->posX,e->posY,e->posZ)||MCObjectHeap_failed(heap)) {ok=false;break;}
        e->savedFields=tag;MCObjectHeap_touch(heap);loaded[count++]=e;
        if (e->entityId>=w->nextEntityId&&w->nextEntityId>0) {w->nextEntityId=e->entityId==INT32_MAX?0:e->entityId+1;MCObjectHeap_touch(heap);}
    }
    if (ok) {
        for(size_t i=0;i<MC_GAMEPLAY_MAX_ITEMS;i++)w->owners->items[i]=i<count?(MCObject *)loaded[i]:NULL;
        w->owners->itemCount=count;ok=NBTTagCompound_removeTag_ascii(root,"Version")&&NBTTagCompound_removeTag_ascii(root,"Entities");
        if (ok) {w->savedItemFields=root;w->savedItemRootName=name;MCObjectHeap_touch(heap);}
    }
    if (!ok)fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap);
}
bool MCGameplayStorage_loadMaps(MCGameplayWorld *w,const mc_nbt *input) {
    MCObjectHeap *heap=w?w->object.heap:NULL;MCObjectRootScope scope={0};
    if (!w||!MCGameplayWorld_isInstance((MCObject *)w))return fail(heap);
    if (!MCObjectRootScope_begin(&scope,heap))return false;
    bool ok=input&&mc_maps_decode(input,&w->maps);if (ok)MCObjectHeap_touch(heap);else fail(heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(heap);
}
static const MCGameplayEncoders encoders={MCGameplayStorage_encodePlayer,MCGameplayStorage_encodeItems,MCGameplayStorage_encodeMaps};
const MCGameplayEncoders *MCGameplayStorage_encoders(void) {return &encoders;}
