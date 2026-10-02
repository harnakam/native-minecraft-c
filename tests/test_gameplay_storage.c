#include "world/WorldDataStorage.h"
#include "item/ItemMapData.h"
#include "world/storage/SaveDataMemoryStorage.h"
#include "util/MCGameplayStorage.h"
#include "util/MCGameplayPackets.h"
#include "inventory/ContainerWorkbench.h"
#include "item/crafting/RecipeBookCloning.h"
#include "nbt/NBTTagFloat.h"
#include "nbt/NBTTagDouble.h"
#include "nbt/NBTTagByteArray.h"
#include "nbt/NBTTagString.h"
#include "entity/EntityUUIDNBT.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define make_dir(path) _mkdir(path)
#define remove_dir(path) _rmdir(path)
#define task_pid() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define make_dir(path) mkdir(path,0700)
#define remove_dir(path) rmdir(path)
#define task_pid() getpid()
#endif
static unsigned checks;
#define CHECK(x) do {++checks;if (!(x)) {fprintf(stderr,"gameplay storage check %u at %d: %s\n",checks,__LINE__,#x);exit(1);}} while (0)
static int32_t map_next(const MCGameplayWorld *world) {
    int32_t value=-1;CHECK(World_nativeMapNextProjection(world,&value));
    CHECK(world->maps.next_id==0);return value;
}

typedef struct {MCObject object;unsigned watched,logged,dead,drops;ItemStack *dropped[16];unsigned positions,failPosition;unsigned watchedAtPosition[2];EntityItem *constructed;} Effects;
static ItemStack *watched(EntityItem *e) {return DataWatcher_getWatchableObjectItemStack(EntityItem_getDataWatcher(e),10);}
static void effects_trace(MCObject *o,MCObjectVisitor v,void *ctx) {Effects *e=(Effects *)o;for(unsigned i=0;i<e->drops;i++)e->dropped[i]=(ItemStack *)v((MCObject *)e->dropped[i],ctx);e->constructed=(EntityItem *)v((MCObject *)e->constructed,ctx);}
static const MCObjectClass effects_class={"fixture.storage.required-effects",MCObjectHeap_plainClone,effects_trace,NULL};
/* These are explicit inherited-Entity/world/effect fixtures. They construct
   actual source EntityItem objects and retain direct references, not mc_slot
   inventory mirrors or production socket/GUI/stat substitutes. */
static bool notified(MCObject *ctx,MCObject *owner,int32_t i) {CHECK(EntityItem_isInstance(owner)&&i==10&&ctx->heap==owner->heap);((Effects *)ctx)->watched++;MCObjectHeap_touch(ctx->heap);return true;}
static const DataWatcherDependencies watcherMethods={.onDataWatcherUpdate=notified};
static bool logged(MCObject *ctx,int32_t id) {(void)id;((Effects *)ctx)->logged++;MCObjectHeap_touch(ctx->heap);return true;}
static bool remote(MCObject *ctx,MCObject *world) {(void)ctx;return MCGameplayWorld_isRemote(world);}
static InventoryPlayer *inventory(MCObject *ctx,MCObject *p) {(void)ctx;return MCGameplayPlayer_inventory(p);}
static const NBTString *name(MCObject *ctx,MCObject *p) {(void)ctx;return EntityPlayer_getName((MCGameplayPlayer *)p);}
static MCObject *find(MCObject *ctx,MCObject *world,const NBTString *n) {(void)ctx;MCGameplayObjects *o=((MCGameplayWorld *)world)->owners;for(size_t i=0;i<MC_TRANSFER_MAX_PLAYERS;i++)if(o->players[i]&&NBTString_equals(((MCGameplayPlayer *)o->players[i])->gameProfile->name,n))return o->players[i];return NULL;}
static bool achieve(MCObject *ctx,MCObject *p,EntityItemAchievement a) {(void)ctx;(void)p;(void)a;CHECK(false);return false;}
static bool silent(MCObject *ctx,const EntityItem *e) {(void)ctx;(void)e;return false;}
static float next_float(MCObject *ctx,EntityItem *e) {(void)ctx;(void)e;CHECK(false);return 0;}
static bool sound(MCObject *ctx,MCObject *w,MCObject *p,const char *n,float v,float pitch) {(void)ctx;(void)w;(void)p;(void)n;(void)v;(void)pitch;CHECK(false);return false;}
static bool pickup(MCObject *ctx,MCObject *p,EntityItem *e,int32_t n) {(void)ctx;(void)p;(void)e;(void)n;CHECK(false);return false;}
static bool dead(MCObject *ctx,EntityItem *e) {((Effects *)ctx)->dead++;e->entity.isDead=true;MCObjectHeap_touch(ctx->heap);return true;}
static const EntityItemDependencies entity_dependencies={logged,remote,inventory,name,find,achieve,silent,next_float,sound,pickup,dead};
/* Source superclass dependencies used by the fixture: no alternate
   Entity state or guessed constructor body. Observer counters remain scoped
   to the subclass/dependency calls tested by this suite. */
static bool inherited_init(MCObject *c,Entity *e) {(void)c;return EntityItem_entityInit((EntityItem *)e);}
static bool inherited_position(MCObject *c,Entity *e,double x,double y,double z) {(void)c;return Entity_setPosition(e,x,y,z);}
static bool inherited_bounds(MCObject *c,Entity *e,AxisAlignedBB *b) {(void)c;return Entity_setEntityBoundingBox(e,b);}
static bool inherited_dimension(MCObject *c,MCObject *w,int32_t *out) {(void)c;CHECK(MCGameplayWorld_isInstance(w));*out=WorldProvider_getDimensionId(((World *)w)->provider);return true;}
static const EntityDependencies inherited_methods={.entityInit=inherited_init,.setPosition=inherited_position,.setEntityBoundingBox=inherited_bounds,.getDimensionId=inherited_dimension,.watcher=&watcherMethods};
static bool base_constructor(MCObject *ctx,EntityItem *e,MCObject *w) {
    CHECK(ctx->heap==w->heap&&e->health==0&&EntityItem_getDataWatcher(e)==NULL);
    MCGameplayWorld *world=(MCGameplayWorld *)w;
    NativeEntityIDRuntime *ids=NativeEntityIDRuntime_new(world->nextEntityId++);CHECK(ids);
    bool ok=Entity_construct(&e->entity,w,&inherited_methods,ctx,world->randomRuntime,ids);
    CHECK(NativeEntityIDRuntime_free(ids));if(!ok)return false;
    /* Deliberately deterministic fixture identities for save-order tests. */
    e->entity.rand=NativeJavaRandom_new(e->entity.object.heap,17);
    e->entity.entityUniqueID=NativeJavaUUID_new(e->entity.object.heap,37,e->entity.entityId);
    CHECK(e->entity.rand&&e->entity.entityUniqueID);((Effects *)ctx)->constructed=e;
    MCObjectHeap_touch(ctx->heap);return true;
}
static double math_random(MCObject *ctx) {MCObjectHeap_touch(ctx->heap);return 0.25;}
static bool size_entity(MCObject *ctx,EntityItem *e,float w,float h) {CHECK(ctx->heap==e->entity.object.heap);return Entity_setSize(&e->entity,w,h);}
static bool position_entity(MCObject *ctx,EntityItem *e,double x,double y,double z) {CHECK(ctx->heap==e->entity.object.heap);Effects *effects=(Effects *)ctx;if(effects->positions<2)effects->watchedAtPosition[effects->positions]=effects->watched;effects->positions++;if(effects->positions==effects->failPosition)return false;return Entity_setPosition(&e->entity,x,y,z);}
static const EntityItemConstructorDependencies constructors={base_constructor,math_random,size_entity,position_entity};
static NBTString *display(MCObject *world,const ItemStack *s) {(void)world;return NBTString_fromASCII(s->object.heap,"Book");}
static ItemStack *recipe(InventoryCrafting *g,MCObject *w) {return CraftingManager_findMatchingRecipe(((MCGameplayWorld *)w)->manager,g,w,display,w);}
static ItemStackArray *remaining(InventoryCrafting *g,MCObject *w) {return CraftingManager_func_180303_b(((MCGameplayWorld *)w)->manager,g,w);}
static bool crafted(ItemStack *s,MCObject *w,MCObject *p,int32_t n) {(void)s;(void)w;(void)p;(void)n;CHECK(false);return false;}
static bool crafting_achieve(MCObject *p,mc_crafting_achievement a) {(void)p;(void)a;CHECK(false);return false;}
static bool drop(MCObject *object,ItemStack *s,bool scatter) {
    MCGameplayPlayer *p=(MCGameplayPlayer *)object;Effects *effects=(Effects *)p->effects;CHECK(!scatter&&effects->drops<16);effects->dropped[effects->drops++]=s;
    EntityItem *e=EntityItem_new_stack(p->living.entity.object.heap,(MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj)),(MCObject *)effects,&entity_dependencies,&constructors,p->living.entity.posX,p->living.entity.posY,p->living.entity.posZ,s);CHECK(e&&watched(e)==s);
    MCGameplayObjects *o=((MCGameplayWorld *)(p->living.entity.worldObj))->owners;CHECK(o->itemCount<MC_GAMEPLAY_MAX_ITEMS);o->items[o->itemCount++]=(MCObject *)e;MCObjectHeap_touch(p->living.entity.object.heap);return true;
}
static bool false_item(const Item *i) {(void)i;return false;}
static int32_t armor(const Item *i) {int n=ItemStack_registryId(i);return n>=298&&n<=317?(n-298)%4:-1;}
static const mc_crafting_dispatch crafting={MCGameplayPlayer_inventory,MCGameplayPlayer_world,recipe,remaining,crafted,crafting_achieve,drop,false_item,false_item,false_item,false_item,armor,MCGameplayWorld_isRemote,MCGameplayWorld_isCraftingTable,MCGameplayPlayer_getDistanceSq};
static const char *uuids[]={"00000000-0000-3000-8000-000000000001","00000000-0000-3000-8000-000000000002"};
static MCGameplayPlayer *setup(MCGameplay *g,MCObjectRootScope *scope) {
    CHECK(MCGameplay_init(g,64*1024*1024));CHECK(MCObjectRootScope_begin(scope,g->heap));CraftingManager *m=CraftingManager_newEmpty(g->heap);CHECK(m);RecipeBookCloning *b=RecipeBookCloning_new(g->heap);CHECK(b&&CraftingManager_addRecipe(m,RecipeBookCloning_asRecipe(b)));
    MCGameplayWorld *w=MCGameplayWorld_new(g->heap,MCGameplay_get(g),NULL,m);CHECK(w&&MCGameplay_setWorld(g,(MCObject *)w));w->nextEntityId=100;
    for(unsigned i=0;i<2;i++){MCGameplayPlayer *p=MCGameplayPlayer_new(w,NBTString_fromASCII(g->heap,i?"Second":"First"),NULL,&crafting);CHECK(p&&MCGameplay_setPlayer(g,i,uuids[i],(MCObject *)p));p->gameProfile->id=NativeJavaUUID_fromString(g->heap,NBTString_fromASCII(g->heap,uuids[i]));CHECK(p->gameProfile->id&&p->living.entity.rand);p->living.entity.entityUniqueID=p->gameProfile->id;Effects *e=(Effects *)MCObjectHeap_alloc(g->heap,sizeof(*e),&effects_class);CHECK(e);p->effects=(MCObject *)e;CHECK(MCGameplayPackets_bind(p));}
    return (MCGameplayPlayer *)MCGameplay_get(g)->players[0];
}
static void finish(MCGameplay *g,MCObjectRootScope *scope) {CHECK(!MCObjectHeap_failed(g->heap));MCObjectRootScope_end(scope);CHECK(MCGameplay_free(g));}
static ItemStack *book(MCGameplayPlayer *p,int32_t count) {ItemStack *s=ItemStack_new(p->living.entity.object.heap,ItemStack_registryItem(387),count,7);CHECK(s);NBTTagCompound *tag=NBTTagCompound_new(p->living.entity.object.heap);CHECK(tag&&NBTTagCompound_setInteger_ascii(tag,"generation",0)&&NBTTagCompound_setString_ascii(tag,"title",NBTString_fromUTF8(p->living.entity.object.heap,"非公開の原本検証"))&&ItemStack_setTagCompound(s,tag));return s;}
static void encode_player(MCGameplayPlayer *p,mc_nbt *out) {CHECK(MCGameplayStorage_encodePlayer(((MCGameplayWorld *)(p->living.entity.worldObj))->owners,0,out,NULL)&&out->size&&out->size<=MC_NBT_MAX_BYTES);}
static void roundtrip_sources_foreign_and_names(void) {
    MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);MCGameplayPlayer *second=(MCGameplayPlayer *)MCGameplay_get(&g)->players[1];ItemStack *s=book(p,0);
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,0,s)&&InventoryPlayer_setInventorySlotContents(p->inventory,9,s)&&InventoryPlayer_setInventorySlotContents(p->inventory,36,s)&&InventoryPlayer_setItemStack(p->inventory,s)&&InventoryCrafting_setInventorySlotContents(((ContainerPlayer *)(p->inventoryContainer))->craftMatrix,0,s)&&InventoryPlayer_setItemStack(second->inventory,s));
    const uint16_t rootUnits[]={0x65e5,0,0xd800,0xdcff,0xdc00};p->savedRootName=NBTString_fromUTF16(g.heap,rootUnits,5);CHECK(p->savedRootName);CHECK(NBTTagCompound_setFloat_ascii(p->savedFields,"foreignFloat",-0.5f)&&NBTTagCompound_setDouble_ascii(p->savedFields,"foreignDouble",-0.0)&&NBTTagCompound_setLong_ascii(p->savedFields,"foreignLong",INT64_C(0x1234567812345678)));NBTTagCompound *nested=NBTTagCompound_new(g.heap);CHECK(nested&&NBTTagCompound_setString_ascii(nested,"text",p->savedRootName)&&NBTTagCompound_setTag_ascii(p->savedFields,"foreignCompound",(NBTBase *)nested));p->inventory->currentItem=INT32_MIN;
    mc_nbt data={0};encode_player(p,&data);CHECK(InventoryPlayer_getItemStack(p->inventory)==s&&s->stackSize==0&&InventoryPlayer_getItemStack(second->inventory)==s);
    MCGameplay copy={0};MCObjectRootScope copiedScope={0};MCGameplayPlayer *q=setup(&copy,&copiedScope);CHECK(MCGameplayStorage_loadPlayer(q,&data,&crafting));CHECK(NBTString_equals(q->savedRootName,p->savedRootName)&&q->inventory->currentItem==INT32_MIN);
    ItemStack *one=InventoryPlayer_getStackInSlot(q->inventory,0),*two=InventoryPlayer_getStackInSlot(q->inventory,9),*grid=InventoryCrafting_getStackInSlot(((ContainerPlayer *)(q->inventoryContainer))->craftMatrix,0),*cursor=InventoryPlayer_getItemStack(q->inventory);
    CHECK(one&&two&&grid&&cursor&&one->stackSize==0&&one!=two&&one!=grid&&one!=cursor&&one->stackTagCompound!=two->stackTagCompound&&one->itemDamage==7);
    CHECK(NBTTagCompound_getTagId_ascii(q->savedFields,"foreignFloat")==5&&NBTTagCompound_getTagId_ascii(q->savedFields,"foreignDouble")==6&&NBTTagCompound_getLong_ascii(q->savedFields,"foreignLong")==INT64_C(0x1234567812345678));CHECK(NBTString_equals(NBTTagCompound_getString_ascii(NBTTagCompound_getCompoundTag_ascii(q->savedFields,"foreignCompound"),"text"),p->savedRootName));
    cursor->stackSize=-1;mc_nbt again={0};encode_player(q,&again);CHECK(MCGameplayStorage_loadPlayer(q,&again,&crafting));CHECK(InventoryPlayer_getItemStack(q->inventory)&&InventoryPlayer_getItemStack(q->inventory)->stackSize==-1);mc_nbt_free(&again);mc_nbt_free(&data);finish(&copy,&copiedScope);finish(&g,&scope);
}
static void workbench_recovery_and_item_envelope(void) {
    MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);mc_crafting_position pos={1,2,3};ContainerWorkbench *b=ContainerWorkbench_new(p->inventory,(MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj)),&pos,&crafting);CHECK(b);b->container.windowId=17;p->openContainer=&b->container;ItemStack *shared=book(p,-1);CHECK(InventoryCrafting_setInventorySlotContents(b->craftMatrix,0,shared)&&InventoryCrafting_setInventorySlotContents(b->craftMatrix,8,shared)&&InventoryPlayer_setItemStack(p->inventory,shared));mc_nbt playerData={0};encode_player(p,&playerData);
    EntityItem *entity=EntityItem_new_stack(g.heap,(MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj)),p->effects,&entity_dependencies,&constructors,1.25,20.5,-2.0,shared);CHECK(entity);entity->entity.entityId=INT32_MIN;entity->health=128;entity->age=-6000;entity->delayBeforeCanPickup=-9;entity->entity.motionX=0.1;entity->entity.motionY=-0.2;entity->entity.motionZ=0.3;entity->entity.onGround=true;entity->savedFields=NBTTagCompound_new(g.heap);CHECK(entity->savedFields&&NBTTagCompound_setFloat_ascii(entity->savedFields,"foreignEntity",2.5f)&&EntityItem_setOwner(entity,p->gameProfile->name)&&EntityItem_setThrower(entity,p->gameProfile->name));CHECK(MCGameplay_addItem(&g,(MCObject *)entity));
    ((MCGameplayWorld *)(p->living.entity.worldObj))->savedItemFields=NBTTagCompound_new(g.heap);((MCGameplayWorld *)(p->living.entity.worldObj))->savedItemRootName=NBTString_fromUTF8(g.heap,"アイテム世界");CHECK(((MCGameplayWorld *)(p->living.entity.worldObj))->savedItemFields&&((MCGameplayWorld *)(p->living.entity.worldObj))->savedItemRootName&&NBTTagCompound_setDouble_ascii(((MCGameplayWorld *)(p->living.entity.worldObj))->savedItemFields,"foreignItems",-7.5));mc_nbt itemData={0};CHECK(MCGameplayStorage_encodeItems(MCGameplay_get(&g),&itemData,NULL));
    MCGameplay copy={0};MCObjectRootScope copiedScope={0};MCGameplayPlayer *q=setup(&copy,&copiedScope);CHECK(MCGameplayStorage_loadPlayer(q,&playerData,&crafting));CHECK(q->openContainer!=q->inventoryContainer&&ContainerWorkbench_isInstance((MCObject *)q->openContainer));ContainerWorkbench *recovered=(ContainerWorkbench *)q->openContainer;CHECK(!recovered->hasPosition&&InventoryCrafting_getStackInSlot(recovered->craftMatrix,0)->stackSize==-1);
    CHECK(MCGameplayStorage_loadItems(((MCGameplayWorld *)(q->living.entity.worldObj)),&itemData,q->effects,&entity_dependencies,&constructors));CHECK(((MCGameplayWorld *)(q->living.entity.worldObj))->owners->itemCount==1);EntityItem *e=(EntityItem *)((MCGameplayWorld *)(q->living.entity.worldObj))->owners->items[0];CHECK(e->entity.entityId==INT32_MIN&&e->health==128&&e->age==-6000&&e->delayBeforeCanPickup==-9&&watched(e)->stackSize==-1&&e->entity.posX==1.25&&e->entity.posY==20.5&&e->entity.posZ==-2&&e->entity.motionY==-0.2&&e->entity.onGround);CHECK(NBTTagCompound_getTagId_ascii(e->savedFields,"foreignEntity")==5&&NBTTagCompound_getFloat_ascii(e->savedFields,"foreignEntity")==2.5f&&NBTString_equals(e->owner,q->gameProfile->name));CHECK(NBTString_equals(((MCGameplayWorld *)(q->living.entity.worldObj))->savedItemRootName,((MCGameplayWorld *)(p->living.entity.worldObj))->savedItemRootName)&&NBTTagCompound_getDouble_ascii(((MCGameplayWorld *)(q->living.entity.worldObj))->savedItemFields,"foreignItems")==-7.5);
    CHECK(EntityPlayerMPWindows_closeContainer(q));Effects *effects=(Effects *)q->effects;CHECK(effects->drops==3&&q->openContainer==q->inventoryContainer&&!InventoryPlayer_getItemStack(q->inventory)&&!InventoryCrafting_getStackInSlot(recovered->craftMatrix,0)&&((MCGameplayWorld *)(q->living.entity.worldObj))->owners->itemCount==4);CHECK(effects->dropped[0]!=effects->dropped[1]&&effects->dropped[1]!=effects->dropped[2]);
    mc_nbt_free(&itemData);mc_nbt_free(&playerData);finish(&copy,&copiedScope);finish(&g,&scope);
}
static void maps_and_snapshot_metadata(void) {
    MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);mc_map_info map;mc_map_info_init(&map);map.id=1;map.metadata_known=true;map.center_x=128;map.center_z=-64;map.scale=2;map.colors[0]=17;CHECK(NativeItemMapData_setItemData((MCGameplayWorld *)p->living.entity.worldObj,&map));mc_map_info_free(&map);CHECK(World_nativeImportMapNextProjection((MCGameplayWorld *)p->living.entity.worldObj,2));
    mc_nbt data={0};CHECK(MCGameplayStorage_encodeMaps(MCGameplay_get(&g),&data,NULL));MCGameplay copy={0};MCObjectRootScope copiedScope={0};MCGameplayPlayer *q=setup(&copy,&copiedScope);CHECK(MCGameplayStorage_loadMaps(((MCGameplayWorld *)(q->living.entity.worldObj)),&data)&&((MCGameplayWorld *)(q->living.entity.worldObj))->maps.count==1&&((MCGameplayWorld *)(q->living.entity.worldObj))->maps.entries[0].colors[0]==17&&map_next((MCGameplayWorld *)q->living.entity.worldObj)==2);mc_nbt_free(&data);finish(&copy,&copiedScope);
    p->savedRootName=NBTString_fromUTF8(g.heap,"保存名");((MCGameplayWorld *)(p->living.entity.worldObj))->savedItemRootName=p->savedRootName;((MCGameplayWorld *)(p->living.entity.worldObj))->savedItemFields=p->savedFields;EntityItem *e=EntityItem_new_stack(g.heap,(MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj)),p->effects,&entity_dependencies,&constructors,0,20,0,book(p,0));CHECK(e&&MCGameplay_addItem(&g,(MCObject *)e));e->savedFields=p->savedFields;MCObjectRootScope_end(&scope);MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&g,&tx));CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));MCGameplayPlayer *cp=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];EntityItem *ce=(EntityItem *)MCGameplay_get(&tx.working)->items[0];CHECK(cp->savedRootName==((MCGameplayWorld *)(cp->living.entity.worldObj))->savedItemRootName&&cp->savedRootName!=p->savedRootName&&cp->savedFields==((MCGameplayWorld *)(cp->living.entity.worldObj))->savedItemFields&&cp->savedFields==ce->savedFields&&ce->savedFields!=e->savedFields);MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));CHECK(MCObjectRootScope_begin(&scope,g.heap));finish(&g,&scope);
}
static bool sink(const mc_buf *packet,void *ctx) {unsigned *count=ctx;CHECK(packet&&packet->len);++*count;return true;}
static void durable_group_and_packet_preflight(void) {
    MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);MCGameplayPlayer *second=(MCGameplayPlayer *)MCGameplay_get(&g)->players[1];ItemStack *s=book(p,1);CHECK(InventoryCrafting_setInventorySlotContents(((ContainerPlayer *)(p->inventoryContainer))->craftMatrix,0,s)&&InventoryPlayer_setItemStack(second->inventory,s));EntityItem *e=EntityItem_new_stack(g.heap,(MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj)),p->effects,&entity_dependencies,&constructors,0,20,0,s);CHECK(e&&MCGameplay_addItem(&g,(MCObject *)e));MCObjectRootScope_end(&scope);
    char base[128],directory[160],path[256],error[256];snprintf(base,sizeof(base),"test-gameplay-storage-%ld.c919",(long)task_pid());snprintf(directory,sizeof(directory),"%s.players",base);CHECK(make_dir(directory)==0);
    MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&g,&tx));CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));MCGameplayPlayer *a=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0],*b=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[1];EntityItem *ie=(EntityItem *)MCGameplay_get(&tx.working)->items[0];CHECK(InventoryCrafting_getStackInSlot(((ContainerPlayer *)(a->inventoryContainer))->craftMatrix,0)==watched(ie)&&InventoryPlayer_getItemStack(b->inventory)==watched(ie));watched(ie)->stackSize=0;CHECK(MCGameplayPackets_sendSetSlot(a,-1,-1,watched(ie)));MCObjectRootScope_end(&scope);
    CHECK(MCGameplay_commit(&tx,base,MCGameplayStorage_encoders(),NULL,error,sizeof(error))==MC_GAMEPLAY_COMMITTED);p=(MCGameplayPlayer *)MCGameplay_get(&g)->players[0];e=(EntityItem *)MCGameplay_get(&g)->items[0];CHECK(watched(e)==InventoryCrafting_getStackInSlot(((ContainerPlayer *)(p->inventoryContainer))->craftMatrix,0)&&watched(e)->stackSize==0);unsigned sent=0;CHECK(MCGameplayPackets_flush(&g,0,sink,&sent)==MC_GAMEPLAY_PACKETS_SENT&&sent==1);
    mc_nbt data={0};snprintf(path,sizeof(path),"%s.players/%s.dat",base,uuids[0]);CHECK(mc_nbt_load_gzip(&data,path,error,sizeof(error)));MCGameplay reload={0};MCObjectRootScope reloadScope={0};MCGameplayPlayer *q=setup(&reload,&reloadScope);CHECK(MCGameplayStorage_loadPlayer(q,&data,&crafting));CHECK(InventoryCrafting_getStackInSlot(((ContainerPlayer *)(q->inventoryContainer))->craftMatrix,0)&&InventoryCrafting_getStackInSlot(((ContainerPlayer *)(q->inventoryContainer))->craftMatrix,0)->stackSize==0);mc_nbt_free(&data);finish(&reload,&reloadScope);
    CHECK(MCGameplay_begin(&g,&tx));CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));a=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];CHECK(MCGameplayPackets_sendWindowItems(a,0,Container_getInventory(a->openContainer)));S30PacketWindowItems *bad=(S30PacketWindowItems *)MCGameplayPackets_packetAt(a,0,NULL);CHECK(bad);bad->itemStacks=NULL;MCObjectHeap_touch(tx.working.heap);MCObjectRootScope_end(&scope);CHECK(MCGameplay_commit(&tx,base,MCGameplayStorage_encoders(),NULL,error,sizeof(error))==MC_GAMEPLAY_NOT_COMMITTED&&!g.fatal);CHECK(MCGameplayPackets_count((MCGameplayPlayer *)MCGameplay_get(&g)->players[0])==0);
    for(unsigned i=0;i<2;i++){snprintf(path,sizeof(path),"%s.players/%s.dat",base,uuids[i]);CHECK(remove(path)==0);}snprintf(path,sizeof(path),"%s.items.dat",base);CHECK(remove(path)==0);snprintf(path,sizeof(path),"%s.maps.dat",base);CHECK(remove(path)==0);CHECK(remove_dir(directory)==0);CHECK(MCGameplay_free(&g));
}
static void snapshot(NBTTagCompound *root,mc_nbt *out) {mc_buf buffer={0};CHECK(NBTWire_encodeCompound(&buffer,root)&&mc_nbt_read(&buffer,out)&&buffer.pos==buffer.len);mc_buf_free(&buffer);}
static NBTTagCompound *storage_tag(MCObjectHeap *heap,const mc_nbt *data) {
    mc_buf in={data->data,data->size,data->size,0,false};NBTSizeTracker tracker;NBTSizeTracker_initInfinite(&tracker);NBTTagCompound *tag=NULL;
    CHECK(NBTWire_decodeCompound(heap,&in,&tracker,&tag)&&tag&&in.pos==in.len);return tag;
}
static void random_unchanged(const NativeJavaRandomState *before,const NativeJavaRandom *after) {
    CHECK(after&&before->seed48==after->state.seed48&&before->haveNextNextGaussian==after->state.haveNextNextGaussian);
    CHECK(!memcmp(&before->nextNextGaussian,&after->state.nextNextGaussian,sizeof before->nextNextGaussian));
}
/* Missing UUID writes, stale saved-field authority, profile restoration after
   the inherited read, and accidental RNG draws/persistence all break this test. */
static void uuid_authority_profile_order_and_random_nonpersistence(void) {
    MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);
    CHECK(NativeJavaRandom_setSeed(p->living.entity.rand,123));NativeJavaRandomState playerRandom=p->living.entity.rand->state;
    p->living.entity.entityUniqueID=NativeJavaUUID_new(g.heap,-1,-2);CHECK(p->living.entity.entityUniqueID);
    p->savedFields=NBTTagCompound_new(g.heap);CHECK(p->savedFields);
    CHECK(NBTTagCompound_setLong_ascii(p->savedFields,"UUIDMost",123)&&NBTTagCompound_setLong_ascii(p->savedFields,"UUIDLeast",456)&&
          NBTTagCompound_setString_ascii(p->savedFields,"UUID",NBTString_fromASCII(g.heap,"1-2-3-4-5")));
    CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,0,book(p,-1)));
    mc_nbt playerData={0};encode_player(p,&playerData);NBTTagCompound *playerTag=storage_tag(g.heap,&playerData);
    CHECK(NBTTagCompound_getTagId_ascii(playerTag,"UUIDMost")==4&&NBTTagCompound_getLong_ascii(playerTag,"UUIDMost")==-1&&
          NBTTagCompound_getLong_ascii(playerTag,"UUIDLeast")==-2);
    CHECK(!NBTTagCompound_hasKey_ascii(playerTag,"RandomSeed")&&!NBTTagCompound_hasKey_ascii(playerTag,"rand"));random_unchanged(&playerRandom,p->living.entity.rand);
    EntityItem *e=EntityItem_new_stack(g.heap,(MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj)),p->effects,&entity_dependencies,&constructors,1.25,20.5,-2,book(p,1));CHECK(e&&MCGameplay_addItem(&g,(MCObject *)e));
    e->entity.entityUniqueID=NativeJavaUUID_new(g.heap,INT64_MIN,INT64_MAX);e->savedFields=NBTTagCompound_new(g.heap);CHECK(e->entity.entityUniqueID&&e->savedFields&&
        NBTTagCompound_setLong_ascii(e->savedFields,"UUIDMost",3)&&NBTTagCompound_setLong_ascii(e->savedFields,"UUIDLeast",4));
    NativeJavaRandomState entityRandom=e->entity.rand->state;mc_nbt itemData={0};CHECK(MCGameplayStorage_encodeItems(MCGameplay_get(&g),&itemData,NULL));
    NBTTagCompound *itemsTag=storage_tag(g.heap,&itemData);NBTTagCompound *entry=NBTTagList_getCompoundTagAt(NBTTagCompound_getTagList_ascii(itemsTag,"Entities",10),0);
    CHECK(entry&&NBTTagCompound_getLong_ascii(entry,"UUIDMost")==INT64_MIN&&NBTTagCompound_getLong_ascii(entry,"UUIDLeast")==INT64_MAX);
    CHECK(!NBTTagCompound_hasKey_ascii(entry,"RandomSeed")&&!NBTTagCompound_hasKey_ascii(entry,"rand"));random_unchanged(&entityRandom,e->entity.rand);
    MCGameplay loaded={0};MCObjectRootScope loadedScope={0};MCGameplayPlayer *q=setup(&loaded,&loadedScope);NativeJavaRandomState readRandom=q->living.entity.rand->state;
    NativeJavaUUID *profile=q->gameProfile->id;CHECK(MCGameplayStorage_loadPlayer(q,&playerData,&crafting));
    CHECK(q->living.entity.entityUniqueID==profile&&q->living.entity.entityUniqueID->mostSignificantBits==12288&&q->living.entity.entityUniqueID->leastSignificantBits==INT64_MIN+1);
    CHECK(InventoryPlayer_getStackInSlot(q->inventory,0)->stackSize==-1);random_unchanged(&readRandom,q->living.entity.rand);
    CHECK(MCGameplayStorage_loadItems(((MCGameplayWorld *)(q->living.entity.worldObj)),&itemData,q->effects,&entity_dependencies,&constructors));EntityItem *restored=(EntityItem *)MCGameplay_get(&loaded)->items[0];
    CHECK(restored->entity.entityUniqueID->mostSignificantBits==INT64_MIN&&restored->entity.entityUniqueID->leastSignificantBits==INT64_MAX);
    random_unchanged(&entityRandom,restored->entity.rand);
    /* Legacy input uses the actual UUID adapter, then player profile reset;
       malformed legacy input stops before both reset and Inventory reads. */
    CHECK(NBTTagCompound_removeTag_ascii(playerTag,"UUIDMost")&&NBTTagCompound_removeTag_ascii(playerTag,"UUIDLeast"));mc_nbt legacy={0};snapshot(playerTag,&legacy);
    q->living.entity.entityUniqueID=NativeJavaUUID_new(loaded.heap,71,72);CHECK(q->living.entity.entityUniqueID&&MCGameplayStorage_loadPlayer(q,&legacy,&crafting)&&q->living.entity.entityUniqueID==profile);random_unchanged(&readRandom,q->living.entity.rand);mc_nbt_free(&legacy);
    CHECK(NBTTagCompound_removeTag_ascii(entry,"UUIDMost")&&NBTTagCompound_removeTag_ascii(entry,"UUIDLeast")&&
          NBTTagCompound_setString_ascii(entry,"UUID",NBTString_fromASCII(g.heap,"1-2-3-4-5---")));snapshot(itemsTag,&legacy);
    CHECK(MCGameplayStorage_loadItems(((MCGameplayWorld *)(q->living.entity.worldObj)),&legacy,q->effects,&entity_dependencies,&constructors));restored=(EntityItem *)MCGameplay_get(&loaded)->items[0];
    CHECK(restored->entity.entityUniqueID->mostSignificantBits==INT64_C(0x0000000100020003)&&restored->entity.entityUniqueID->leastSignificantBits==INT64_C(0x0004000000000005));random_unchanged(&entityRandom,restored->entity.rand);mc_nbt_free(&legacy);
    CHECK(NBTTagCompound_setString_ascii(playerTag,"UUID",NBTString_fromASCII(g.heap,"invalid")));snapshot(playerTag,&legacy);
    ItemStack *before=book(q,7);CHECK(InventoryPlayer_setInventorySlotContents(q->inventory,0,before));MCObjectRootScope_end(&loadedScope);
    MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&loaded,&tx)&&MCObjectRootScope_begin(&loadedScope,tx.working.heap));MCGameplayPlayer *bad=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
    NativeJavaUUID *oldUUID=bad->living.entity.entityUniqueID;ItemStack *oldInventory=InventoryPlayer_getStackInSlot(bad->inventory,0);
    CHECK(!MCGameplayStorage_loadPlayer(bad,&legacy,&crafting)&&MCObjectHeap_failed(tx.working.heap));
    CHECK(bad->living.entity.entityUniqueID==oldUUID&&InventoryPlayer_getStackInSlot(bad->inventory,0)==oldInventory);random_unchanged(&readRandom,bad->living.entity.rand);
    MCObjectRootScope_end(&loadedScope);CHECK(MCGameplay_abort(&tx)&&MCObjectRootScope_begin(&loadedScope,loaded.heap));
    CHECK(q->living.entity.entityUniqueID==profile&&InventoryPlayer_getStackInSlot(q->inventory,0)==before);mc_nbt_free(&legacy);
    mc_nbt_free(&itemData);mc_nbt_free(&playerData);finish(&loaded,&loadedScope);finish(&g,&scope);
}
static void malformed_item_uuid_preserves_prefix_and_abort_preserves_parent(void) {
    MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);
    EntityItem *original=EntityItem_new_stack(g.heap,(MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj)),p->effects,&entity_dependencies,&constructors,1.25,20.5,-2,book(p,1));CHECK(original&&MCGameplay_addItem(&g,(MCObject *)original));original->entity.motionX=0.5;original->age=37;original->entity.onGround=true;
    mc_nbt data={0};CHECK(MCGameplayStorage_encodeItems(MCGameplay_get(&g),&data,NULL));NBTTagCompound *root=storage_tag(g.heap,&data);
    NBTTagCompound *entry=NBTTagList_getCompoundTagAt(NBTTagCompound_getTagList_ascii(root,"Entities",10),0);CHECK(entry&&
        NBTTagCompound_removeTag_ascii(entry,"UUIDMost")&&NBTTagCompound_removeTag_ascii(entry,"UUIDLeast")&&
        NBTTagCompound_setString_ascii(entry,"UUID",NBTString_fromASCII(g.heap,"invalid")));snapshot(root,&data);
    MCObjectRootScope_end(&scope);MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&g,&tx)&&MCObjectRootScope_begin(&scope,tx.working.heap));
    MCGameplayPlayer *copy=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];Effects *effects=(Effects *)copy->effects;effects->positions=0;
    EntityItem *old=(EntityItem *)MCGameplay_get(&tx.working)->items[0];CHECK(!MCGameplayStorage_loadItems(((MCGameplayWorld *)(copy->living.entity.worldObj)),&data,copy->effects,&entity_dependencies,&constructors)&&MCObjectHeap_failed(tx.working.heap));
    EntityItem *partial=effects->constructed;CHECK(partial&&partial!=old&&partial->entity.posX==1.25&&partial->entity.posY==20.5&&partial->entity.posZ==-2&&partial->entity.motionX==0.5&&partial->entity.onGround);
    CHECK(partial->entity.entityUniqueID&&partial->entity.entityUniqueID->mostSignificantBits==37&&partial->age==0&&effects->positions==0);
    CHECK(MCGameplay_get(&tx.working)->items[0]==(MCObject *)old&&old->age==37);
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx)&&MCObjectRootScope_begin(&scope,g.heap));
    CHECK(MCGameplay_get(&g)->items[0]==(MCObject *)original&&original->age==37&&original->entity.motionX==0.5);mc_nbt_free(&data);finish(&g,&scope);
}
static void missing_profile_fails_after_base_uuid_before_inventory(void) {
    MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);ItemStack *old=book(p,7);CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,0,old));
    NBTTagCompound *root=NBTTagCompound_new(g.heap);CHECK(root&&NBTTagCompound_setLong_ascii(root,"UUIDMost",71)&&NBTTagCompound_setLong_ascii(root,"UUIDLeast",72));mc_nbt data={0};snapshot(root,&data);
    MCObjectRootScope_end(&scope);MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&g,&tx)&&MCObjectRootScope_begin(&scope,tx.working.heap));MCGameplayPlayer *copy=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];NativeJavaUUID *prior=copy->living.entity.entityUniqueID;ItemStack *inventory=InventoryPlayer_getStackInSlot(copy->inventory,0);copy->gameProfile=NULL;
    CHECK(!MCGameplayStorage_loadPlayer(copy,&data,&crafting)&&MCObjectHeap_failed(tx.working.heap));
    CHECK(copy->living.entity.entityUniqueID!=prior&&copy->living.entity.entityUniqueID->mostSignificantBits==71&&copy->living.entity.entityUniqueID->leastSignificantBits==72&&InventoryPlayer_getStackInSlot(copy->inventory,0)==inventory);
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx)&&MCObjectRootScope_begin(&scope,g.heap));CHECK(p->gameProfile->id&&p->living.entity.entityUniqueID==p->gameProfile->id&&InventoryPlayer_getStackInSlot(p->inventory,0)==old);mc_nbt_free(&data);finish(&g,&scope);
}
static NBTTagCompound *item_entry(MCGameplayPlayer *p,int32_t index,int32_t count) {NBTTagCompound *entry=NBTTagCompound_new(p->living.entity.object.heap);CHECK(entry&&NBTTagCompound_setByte_ascii(entry,"Slot",(int8_t)index)&&ItemStack_writeToNBT(book(p,count),entry));return entry;}
static void source_inventory_last_wins_and_partial_rollback(void) {
    MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);NBTTagCompound *root=NBTTagCompound_new(g.heap);NBTTagList *list=NBTTagList_new(g.heap);CHECK(root&&list);
    CHECK(NBTTagList_appendTag(list,(NBTBase *)item_entry(p,0,1))&&NBTTagList_appendTag(list,(NBTBase *)item_entry(p,0,-1))&&NBTTagList_appendTag(list,(NBTBase *)item_entry(p,200,17)));CHECK(NBTTagCompound_setTag_ascii(root,"Inventory",(NBTBase *)list)&&NBTTagCompound_setFloat_ascii(root,"SelectedItemSlot",-0.5f));mc_nbt data={0};snapshot(root,&data);
    CHECK(MCGameplayStorage_loadPlayer(p,&data,&crafting));CHECK(InventoryPlayer_getStackInSlot(p->inventory,0)&&InventoryPlayer_getStackInSlot(p->inventory,0)->stackSize==-1&&p->inventory->currentItem==-1);mc_nbt_free(&data);
    ItemStack *shared=book(p,5);CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,0,shared));MCGameplayPlayer *second=(MCGameplayPlayer *)MCGameplay_get(&g)->players[1];CHECK(InventoryPlayer_setItemStack(second->inventory,shared));
    root=NBTTagCompound_new(g.heap);list=NBTTagList_new(g.heap);NBTTagList *grid=NBTTagList_new(g.heap);CHECK(root&&list&&grid&&NBTTagList_appendTag(list,(NBTBase *)item_entry(p,0,0))&&NBTTagCompound_setTag_ascii(root,"Inventory",(NBTBase *)list));CHECK(NBTTagList_appendTag(grid,(NBTBase *)item_entry(p,1,0))&&NBTTagList_appendTag(grid,(NBTBase *)item_entry(p,1,-1))&&NBTTagCompound_setTag_ascii(root,"C919Crafting",(NBTBase *)grid));snapshot(root,&data);
    MCObjectRootScope_end(&scope);MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&g,&tx));CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));MCGameplayPlayer *copy=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];CHECK(!MCGameplayStorage_loadPlayer(copy,&data,&crafting)&&MCObjectHeap_failed(tx.working.heap));CHECK(InventoryPlayer_getStackInSlot(copy->inventory,0)->stackSize==0&&InventoryCrafting_getStackInSlot(((ContainerPlayer *)(copy->inventoryContainer))->craftMatrix,0)->stackSize==0);MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));CHECK(MCObjectRootScope_begin(&scope,g.heap));CHECK(InventoryPlayer_getStackInSlot(p->inventory,0)==shared&&InventoryPlayer_getItemStack(second->inventory)==shared&&shared->stackSize==5);mc_nbt_free(&data);
    root=NBTTagCompound_new(g.heap);CHECK(root&&NBTTagCompound_setInteger_ascii(root,"Inventory",7));snapshot(root,&data);CHECK(MCGameplayStorage_loadPlayer(p,&data,&crafting)&&!InventoryPlayer_getStackInSlot(p->inventory,0));mc_nbt_free(&data);finish(&g,&scope);
}
static void malformed_extensions_and_profile_dependency(void) {
    for(unsigned which=0;which<5;which++) {
        MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);NBTTagCompound *root=NBTTagCompound_new(g.heap);CHECK(root);
        if(which==0)CHECK(NBTTagCompound_setInteger_ascii(root,"C919Cursor",7));
        else if(which==1)CHECK(NBTTagCompound_setInteger_ascii(root,"C919Workbench",7));
        else if(which==2||which==3){NBTTagList *list=NBTTagList_new(g.heap);CHECK(list&&NBTTagList_appendTag(list,(NBTBase *)item_entry(p,which==2?0:5,1))&&NBTTagCompound_setTag_ascii(root,"C919Crafting",(NBTBase *)list));}
        else {NBTTagCompound *item=NBTTagCompound_new(g.heap),*tag=NBTTagCompound_new(g.heap);ItemStack *skull=ItemStack_new(g.heap,ItemStack_registryItem(397),1,3);CHECK(item&&tag&&skull&&NBTTagCompound_setString_ascii(tag,"SkullOwner",NBTString_fromASCII(g.heap,"ExplicitProfileDependency"))&&ItemStack_setTagCompound(skull,tag)&&ItemStack_writeToNBT(skull,item)&&NBTTagCompound_setTag_ascii(root,"C919Cursor",(NBTBase *)item));}
        mc_nbt data={0};snapshot(root,&data);CHECK(!MCGameplayStorage_loadPlayer(p,&data,&crafting)&&MCObjectHeap_failed(g.heap));mc_nbt_free(&data);MCObjectRootScope_end(&scope);CHECK(!MCObjectHeap_hasBorrowers(g.heap)&&MCGameplay_free(&g));
    }
    MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);uint8_t malformed[]={10,0,0,9,0,1,'x',10,255,255,255,255,0};mc_nbt fake={malformed,sizeof(malformed)};CHECK(!MCGameplayStorage_loadPlayer(p,&fake,&crafting)&&MCObjectHeap_failed(g.heap));MCObjectRootScope_end(&scope);CHECK(MCGameplay_free(&g));
}
static void storage_byte_cap_and_persistence_tracker(void) {
    MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);NBTTagList *strings=NBTTagList_new(g.heap);CHECK(strings);char *text=malloc(60001);CHECK(text);memset(text,'x',60000);text[60000]=0;
    for(unsigned i=0;i<20;i++){NBTTagString *s=NBTTagString_new(g.heap,NBTString_fromASCII(g.heap,text));CHECK(s&&NBTTagList_appendTag(strings,(NBTBase *)s));}free(text);CHECK(NBTTagCompound_setTag_ascii(p->savedFields,"largeForeignStrings",(NBTBase *)strings));mc_nbt data={0};encode_player(p,&data);CHECK(data.size>1200000&&data.size<MC_NBT_MAX_BYTES);
    MCGameplay copy={0};MCObjectRootScope copyScope={0};MCGameplayPlayer *q=setup(&copy,&copyScope);CHECK(MCGameplayStorage_loadPlayer(q,&data,&crafting));NBTTagList *loaded=NBTTagCompound_getTagList_ascii(q->savedFields,"largeForeignStrings",8);CHECK(NBTTagList_tagCount(loaded)==20&&NBTString_length(NBTTagString_getString((NBTTagString *)NBTTagList_get(loaded,19)))==60000);finish(&copy,&copyScope);mc_nbt_free(&data);
    CHECK(NBTTagCompound_removeTag_ascii(p->savedFields,"largeForeignStrings"));ItemStack *s=book(p,1);int8_t *bytes=calloc(1100000,1);CHECK(bytes);NBTByteArrayStorage *array=NBTByteArrayStorage_new(g.heap,bytes,1100000);free(bytes);NBTTagByteArray *tag=NBTTagByteArray_new(g.heap,array);CHECK(tag&&NBTTagCompound_setTag_ascii(s->stackTagCompound,"large",(NBTBase *)tag));CHECK(InventoryPlayer_setInventorySlotContents(p->inventory,0,s)&&InventoryPlayer_setItemStack(p->inventory,s));
    NBTTagCompound *marker=NBTTagCompound_new(g.heap);CHECK(marker&&NBTTagCompound_setInteger_ascii(marker,"unchanged",17));snapshot(marker,&data);mc_nbt previous={0};CHECK(mc_nbt_copy(&previous,&data));CHECK(!MCGameplayStorage_encodePlayer(MCGameplay_get(&g),0,&data,NULL)&&mc_nbt_equal(&data,&previous)&&MCObjectHeap_failed(g.heap));mc_nbt_free(&previous);mc_nbt_free(&data);MCObjectRootScope_end(&scope);CHECK(MCGameplay_free(&g));
}
static void item_list_boundaries_and_invalid_adoption(void) {
    for(unsigned which=0;which<4;which++) {
        MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);NBTTagCompound *root=NBTTagCompound_new(g.heap);NBTTagList *list=NBTTagList_new(g.heap);CHECK(root&&list&&NBTTagCompound_setInteger_ascii(root,"Version",1)&&NBTTagCompound_setTag_ascii(root,"Entities",(NBTBase *)list));
        if(which){NBTTagCompound *entry=NBTTagCompound_new(g.heap);NBTTagList *v=NBTTagList_new(g.heap);CHECK(entry&&v);for(int i=0;i<3;i++)CHECK(NBTTagList_appendTag(v,(NBTBase *)NBTTagDouble_new(g.heap,i==1?20:0)));CHECK(NBTTagCompound_setString_ascii(entry,"id",NBTString_fromASCII(g.heap,which==3?"UnsupportedEntity":"Item"))&&NBTTagCompound_setInteger_ascii(entry,"C919EntityId",7)&&NBTTagCompound_setTag_ascii(entry,"Pos",(NBTBase *)v)&&NBTTagCompound_setTag_ascii(entry,"Motion",(NBTBase *)v)&&NBTTagCompound_setTag_ascii(entry,"Item",(NBTBase *)item_entry(p,0,0)));unsigned count=which==2?MC_GAMEPLAY_MAX_ITEMS+1:2;for(unsigned i=0;i<count;i++)CHECK(NBTTagList_appendTag(list,(NBTBase *)entry));}
        mc_nbt data={0};snapshot(root,&data);Effects *effects=(Effects *)p->effects;unsigned watchedBefore=effects->watched;
        if(which==0){CHECK(MCGameplayStorage_loadItems(((MCGameplayWorld *)(p->living.entity.worldObj)),&data,p->effects,&entity_dependencies,&constructors)&&MCGameplay_get(&g)->itemCount==0&&effects->watched==watchedBefore);mc_nbt_free(&data);finish(&g,&scope);}
        else {CHECK(!MCGameplayStorage_loadItems(((MCGameplayWorld *)(p->living.entity.worldObj)),&data,p->effects,&entity_dependencies,&constructors)&&MCGameplay_get(&g)->itemCount==0&&MCObjectHeap_failed(g.heap));if(which>1)CHECK(effects->watched==watchedBefore);mc_nbt_free(&data);MCObjectRootScope_end(&scope);CHECK(MCGameplay_free(&g));}
    }
}
static void native_position_restore_and_failure(void) {
    for(unsigned which=0;which<3;which++) {
        MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);ItemStack *shared=book(p,1);CHECK(InventoryPlayer_setItemStack(p->inventory,shared));EntityItem *original=EntityItem_new_stack(g.heap,(MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj)),p->effects,&entity_dependencies,&constructors,1.25,20.5,-2.0,shared);CHECK(original&&MCGameplay_addItem(&g,(MCObject *)original));original->entity.motionX=10;original->entity.motionY=-10.000000000000002;original->entity.motionZ=1e308;original->age=37;mc_nbt data={0};CHECK(MCGameplayStorage_encodeItems(MCGameplay_get(&g),&data,NULL));
        MCObjectRootScope_end(&scope);MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&g,&tx)&&MCObjectRootScope_begin(&scope,tx.working.heap));MCGameplayPlayer *copy=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];Effects *effects=(Effects *)copy->effects;effects->positions=effects->watched=0;effects->failPosition=which;EntityItem *old=(EntityItem *)MCGameplay_get(&tx.working)->items[0];bool ok=MCGameplayStorage_loadItems(((MCGameplayWorld *)(copy->living.entity.worldObj)),&data,copy->effects,&entity_dependencies,&constructors);
        if(which==0){CHECK(ok&&!MCObjectHeap_failed(tx.working.heap));EntityItem *loaded=(EntityItem *)MCGameplay_get(&tx.working)->items[0];CHECK(loaded!=old&&loaded->entity.posX==1.25&&loaded->entity.posY==20.5&&loaded->entity.posZ==-2&&loaded->age==37);CHECK(loaded->entity.motionX==10&&loaded->entity.motionY==0&&loaded->entity.motionZ==0);CHECK(effects->positions==2&&effects->watchedAtPosition[0]==1&&effects->watchedAtPosition[1]==2);}
        else {CHECK(!ok&&MCObjectHeap_failed(tx.working.heap)&&MCGameplay_get(&tx.working)->items[0]==(MCObject *)old);CHECK(effects->positions==which&&effects->watched==which);}
        MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx)&&MCObjectRootScope_begin(&scope,g.heap));CHECK(MCGameplay_get(&g)->items[0]==(MCObject *)original&&watched(original)==InventoryPlayer_getItemStack(p->inventory)&&original->entity.motionY==-10.000000000000002&&original->age==37);mc_nbt_free(&data);finish(&g,&scope);
    }
}
static void source_constructor_ids_survive_native_load(void) {
    MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);
    MCGameplayWorld *w=(MCGameplayWorld *)p->living.entity.worldObj;
    EntityItem *original=EntityItem_new_stack(g.heap,(MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj)),p->effects,
        &entity_dependencies,&constructors,1.25,20.5,-2.0,book(p,1));
    CHECK(original&&MCGameplay_addItem(&g,(MCObject *)original));
    original->entity.entityId=INT32_MIN;original->age=37;MCObjectHeap_touch(g.heap);
    CHECK(MCGameplay_reindexWorld(MCGameplay_get(&g)));
    /* A real Source entity outside the native registered-item array survives
       native bulk-load replacement in its existing list position and Entry. */
    EntityItem *unrelated=EntityItem_new_stack(g.heap,(MCObject *)w,p->effects,
        &entity_dependencies,&constructors,2,20,2,book(p,0));CHECK(unrelated);
    Entity_setEntityId(&unrelated->entity,INT32_MAX);
    CHECK(NativeReferenceList_add(w->loadedEntityList,(MCObject *)unrelated)&&
          IntHashMap_addKey(w->entitiesById,INT32_MAX,(MCObject *)unrelated));
    IntHashMapEntry *unrelatedEntry=IntHashMap_lookupEntry(w->entitiesById,INT32_MAX);
    CHECK(unrelatedEntry&&IntHashMap_lookup(w->entitiesById,INT32_MIN)==(MCObject *)original);
    mc_nbt data={0};CHECK(MCGameplayStorage_encodeItems(MCGameplay_get(&g),&data,NULL));
    int32_t expected=((MCGameplayWorld *)(p->living.entity.worldObj))->nextEntityId;
    CHECK(MCGameplayStorage_loadItemsWithSourceIDs(((MCGameplayWorld *)(p->living.entity.worldObj)),&data,p->effects,&entity_dependencies,&constructors));
    EntityItem *loaded=(EntityItem *)MCGameplay_get(&g)->items[0];
    CHECK(loaded!=original&&loaded->entity.entityId==expected&&loaded->entity.entityId!=INT32_MIN);
    CHECK(loaded->age==37&&loaded->entity.posY==20.5&&watched(loaded)->stackSize==1);
    CHECK(((MCGameplayWorld *)(p->living.entity.worldObj))->nextEntityId==expected+1);
    CHECK(MCGameplay_validateWorldIndexes(MCGameplay_get(&g))&&
          !IntHashMap_lookup(w->entitiesById,INT32_MIN)&&IntHashMap_lookup(w->entitiesById,expected)==(MCObject *)loaded);
    CHECK(NativeReferenceList_size(w->loadedEntityList)==4&&
          NativeReferenceList_get(w->loadedEntityList,2)==(MCObject *)unrelated&&
          NativeReferenceList_get(w->loadedEntityList,3)==(MCObject *)loaded&&
          IntHashMap_lookupEntry(w->entitiesById,INT32_MAX)==unrelatedEntry);
    EntityItem *previous=loaded;
    CHECK(MCGameplayStorage_loadItems(((MCGameplayWorld *)(p->living.entity.worldObj)),&data,p->effects,&entity_dependencies,&constructors));
    loaded=(EntityItem *)MCGameplay_get(&g)->items[0];CHECK(loaded->entity.entityId==INT32_MIN);
    CHECK(loaded!=previous&&MCGameplay_validateWorldIndexes(MCGameplay_get(&g))&&
          !IntHashMap_lookup(w->entitiesById,expected)&&IntHashMap_lookup(w->entitiesById,INT32_MIN)==(MCObject *)loaded);
    CHECK(NativeReferenceList_size(w->loadedEntityList)==4&&
          NativeReferenceList_get(w->loadedEntityList,2)==(MCObject *)unrelated&&
          NativeReferenceList_get(w->loadedEntityList,3)==(MCObject *)loaded&&
          IntHashMap_lookupEntry(w->entitiesById,INT32_MAX)==unrelatedEntry);
    mc_nbt_free(&data);finish(&g,&scope);
}
static void map_provider_namespaces_and_journal(void) {
    MCGameplay g={0};MCObjectRootScope scope={0};MCGameplayPlayer *p=setup(&g,&scope);
    MCGameplayWorld *w=(MCGameplayWorld *)p->living.entity.worldObj;
    MapStorage *base=w->mapStorage;int32_t id=-1;
    NBTString *map=NBTString_fromASCII(g.heap,"map"),*other=NBTString_fromUTF8(g.heap,"別の保存ID");
    CHECK(map&&other&&World_getUniqueDataId(w,map,&id)&&id==0);
    CHECK(World_getUniqueDataId(w,other,&id)&&id==0);
    w->isRemote=true;CHECK(World_getUniqueDataId(w,map,&id)&&id==1&&map_next(w)==2);
    CHECK(MapStorage_nativeImportExactShort(base,other,INT16_MAX));
    CHECK(World_getUniqueDataId(w,other,&id)&&id==INT16_MIN);
    SaveDataMemoryStorage *memory=SaveDataMemoryStorage_nativeNewCounterProvider(g.heap);
    CHECK(memory);w->mapStorage=&memory->base;w->isRemote=false;
    CHECK(World_getUniqueDataId(w,map,&id)&&id==0&&World_getUniqueDataId(w,other,&id)&&id==0);
    CHECK(MapStorage_nativeIdCountSize(w->mapStorage)==0&&map_next(w)==0);
    w->mapStorage=base;w->isRemote=false;MCObjectHeap_touch(g.heap);
    mc_nbt data={0};CHECK(MCGameplayStorage_encodeMaps(MCGameplay_get(&g),&data,NULL));
    CHECK(map_next(w)==2&&MapStorage_nativeIdCountSize(base)==2);
    MCGameplay copy={0};MCObjectRootScope copiedScope={0};MCGameplayPlayer *q=setup(&copy,&copiedScope);
    MCGameplayWorld *cw=(MCGameplayWorld *)q->living.entity.worldObj;
    CHECK(MCGameplayStorage_loadMaps(cw,&data)&&map_next(cw)==2);
    NBTString *copiedOther=NBTString_fromUTF8(copy.heap,"別の保存ID");
    CHECK(copiedOther&&World_getUniqueDataId(cw,copiedOther,&id)&&id==INT16_MIN+1);
    CHECK(World_getUniqueDataId(cw,NBTString_fromASCII(copy.heap,"map"),&id)&&id==2);
    CHECK(map_next(w)==2);finish(&copy,&copiedScope);
    NBTTagCompound *root=storage_tag(g.heap,&data);CHECK(root);
    CHECK(NBTTagCompound_removeTag_ascii(root,"C919MapIdCounts"));
    mc_nbt legacy={0};snapshot(root,&legacy);
    q=setup(&copy,&copiedScope);cw=(MCGameplayWorld *)q->living.entity.worldObj;
    CHECK(MCGameplayStorage_loadMaps(cw,&legacy)&&map_next(cw)==2&&MapStorage_nativeIdCountSize(cw->mapStorage)==1);
    finish(&copy,&copiedScope);mc_nbt_free(&legacy);
    /* Native load failures retain source prefixes in a disposable graph;
       the authoritative parent's namespace counters are never adopted. */
    MCObjectRootScope_end(&scope);
    for(unsigned which=0;which<2;which++) {
        MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&g,&tx));
        CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
        MCGameplayWorld *tw=(MCGameplayWorld *)MCGameplay_get(&tx.working)->world;
        NBTTagCompound *bad=storage_tag(tx.working.heap,&data);CHECK(bad);
        if(which==0)CHECK(NBTTagCompound_setInteger_ascii(bad,"C919MapIdCounts",7));
        else CHECK(NBTTagCompound_setInteger_ascii(bad,"NextId",31));
        mc_nbt broken={0};snapshot(bad,&broken);
        CHECK(!MCGameplayStorage_loadMaps(tw,&broken)&&MCObjectHeap_failed(tx.working.heap));
        mc_nbt_free(&broken);MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));
        CHECK(MCObjectRootScope_begin(&scope,g.heap));
        CHECK(map_next((MCGameplayWorld *)MCGameplay_get(&g)->world)==2);
        MCObjectRootScope_end(&scope);
    }
    mc_nbt_free(&data);CHECK(MCGameplay_free(&g));
}
int main(void) {map_provider_namespaces_and_journal();source_constructor_ids_survive_native_load();uuid_authority_profile_order_and_random_nonpersistence();malformed_item_uuid_preserves_prefix_and_abort_preserves_parent();missing_profile_fails_after_base_uuid_before_inventory();roundtrip_sources_foreign_and_names();workbench_recovery_and_item_envelope();maps_and_snapshot_metadata();durable_group_and_packet_preflight();source_inventory_last_wins_and_partial_rollback();malformed_extensions_and_profile_dependency();storage_byte_cap_and_persistence_tracker();item_list_boundaries_and_invalid_adoption();native_position_restore_and_failure();printf("gameplay storage: %u checks passed\n",checks);return 0;}
