#include "util/MCGameplayPlayer.h"
#include "entity/Entity.h"
#include "item/crafting/RecipeBookCloning.h"
#include "stats/StatBase.h"
#include "stats/StatFileWriter.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
static bool any_object(const MCObject *object,void *context) {(void)object;(void)context;return true;}
#define CHECK(x) do { ++checks;if (!(x)) {fprintf(stderr,"gameplay owner check %u at %d: %s\n",checks,__LINE__,#x);exit(1);} } while (0)
typedef struct {MCObject object;unsigned crafts,drops,achievements;ItemStack *last;} FixtureEffects;
static void effects_trace(MCObject *o,MCObjectVisitor v,void *c) {FixtureEffects *f=(FixtureEffects *)o;f->last=(ItemStack *)v((MCObject *)f->last,c);}
static const MCObjectClass effects_class={"fixture.owner.effect-record",MCObjectHeap_plainClone,effects_trace,NULL};
static unsigned constructor_callbacks;
static NBTString *display(MCObject *world,const ItemStack *stack) {(void)world;return NBTString_fromASCII(stack->object.heap,"Written Book");}
/* Fixtures use the real source manager and BookCloning recipe. Effect callbacks
   record source calls; they are not production stats, sound, world or network. */
static ItemStack *recipe(InventoryCrafting *grid,MCObject *object) {
    MCGameplayWorld *world=(MCGameplayWorld *)object;++constructor_callbacks;
    CHECK(!MCObjectHeap_collect(world->object.heap));
    return CraftingManager_findMatchingRecipe(world->manager,grid,object,display,object);
}
static ItemStackArray *remaining(InventoryCrafting *grid,MCObject *object) {return CraftingManager_func_180303_b(((MCGameplayWorld *)object)->manager,grid,object);}
static bool crafted(ItemStack *stack,MCObject *world,MCObject *object,int32_t count) {
    MCGameplayPlayer *p=(MCGameplayPlayer *)object;FixtureEffects *f=(FixtureEffects *)p->effects;CHECK(world==(MCObject *)((MCGameplayWorld *)(p->living.entity.worldObj))&&count==1&&f);
    ++f->crafts;f->last=stack;MCObjectHeap_touch(object->heap);return true;
}
static bool achievement(MCObject *object,mc_crafting_achievement value) {(void)value;FixtureEffects *f=(FixtureEffects *)((MCGameplayPlayer *)object)->effects;CHECK(f);++f->achievements;return true;}
static bool drop(MCObject *object,ItemStack *stack,bool scatter) {(void)scatter;FixtureEffects *f=(FixtureEffects *)((MCGameplayPlayer *)object)->effects;CHECK(f);++f->drops;f->last=stack;return true;}
static int id(const Item *item) {return ItemStack_registryId(item);}
static bool pickaxe(const Item *item) {int n=id(item);return n==257||n==270||n==274||n==278||n==285;}
static bool hoe(const Item *item) {return id(item)>=290&&id(item)<=294;}
static bool sword(const Item *item) {int n=id(item);return n==267||n==268||n==272||n==276||n==283;}
static bool wood_pickaxe(const Item *item) {return id(item)==270;}
static int32_t armor(const Item *item) {int n=id(item);return n>=298&&n<=317?(n-298)%4:-1;}
static const mc_crafting_dispatch dependencies={MCGameplayPlayer_inventory,MCGameplayPlayer_world,recipe,remaining,crafted,achievement,drop,pickaxe,hoe,sword,wood_pickaxe,armor,MCGameplayWorld_isRemote,MCGameplayWorld_isCraftingTable,MCGameplayPlayer_getDistanceSq};
static MCGameplayWorld *world_new(MCGameplay *game,const mc_world *terrain) {
    CraftingManager *manager=CraftingManager_newEmpty(game->heap);CHECK(manager);RecipeBookCloning *book=RecipeBookCloning_new(game->heap);CHECK(book);CHECK(CraftingManager_addRecipe(manager,RecipeBookCloning_asRecipe(book)));
    MCGameplayWorld *w=MCGameplayWorld_new(game->heap,MCGameplay_get(game),terrain,manager);CHECK(w);CHECK(MCGameplay_setWorld(game,(MCObject *)w));return w;
}
static FixtureEffects *effects(MCGameplayPlayer *p) {FixtureEffects *f=(FixtureEffects *)MCObjectHeap_alloc(p->living.entity.object.heap,sizeof(*f),&effects_class);CHECK(f);p->effects=(MCObject *)f;return f;}
static ItemStack *stack(MCObjectHeap *h,int id,int32_t count,int32_t damage) {ItemStack *s=ItemStack_new(h,ItemStack_registryItem(id),count,damage);CHECK(s);return s;}
static void constructor_container_world_flag(void) {
    for(unsigned remote=0;remote<2;remote++) {
        MCGameplay game={0};CHECK(MCGameplay_init(&game,32*1024*1024));
        MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,game.heap));
        MCGameplayWorld *world=world_new(&game,NULL);world->isRemote=remote!=0;
        MCGameplayPlayer *player=MCGameplayPlayer_new(world,NBTString_fromASCII(game.heap,"WorldFlag"),NULL,&dependencies);CHECK(player);
        CHECK(Entity_isInstance((MCObject *)player));
        CHECK(((ContainerPlayer *)(player->inventoryContainer))->isLocalWorld==!world->isRemote);
        CHECK(MCGameplay_setPlayer(&game,0,"11111111-1111-1111-1111-111111111111",(MCObject *)player));
        MCObjectRootScope_end(&scope);MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&game,&tx));
        CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
        MCGameplayPlayer *copy=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
        CHECK(((MCGameplayWorld *)(copy->living.entity.worldObj))->isRemote==(remote!=0)&&((ContainerPlayer *)(copy->inventoryContainer))->isLocalWorld==!((MCGameplayWorld *)(copy->living.entity.worldObj))->isRemote);
        CHECK(((ContainerPlayer *)(copy->inventoryContainer))!=((ContainerPlayer *)(player->inventoryContainer))&&((ContainerPlayer *)(copy->inventoryContainer))->thePlayer==(MCObject *)copy);
        MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));CHECK(MCGameplay_free(&game));
    }
}
static void construction_and_source_crafting(void) {
    mc_world terrain;mc_world_init(&terrain,7);CHECK(mc_world_set(&terrain,1,2,3,(uint16_t)(58u<<4)));
    MCGameplay game={0};CHECK(MCGameplay_init(&game,32*1024*1024));MCGameplayWorld *world=world_new(&game,&terrain);
    NBTString *name=NBTString_fromUTF8(game.heap,"所有者");CHECK(name);unsigned before=constructor_callbacks;
    /* Intentionally no outer scope: the native constructor must protect its
       borrowed actor/inventory/grid through the actual source recipe callback. */
    MCGameplayPlayer *player=MCGameplayPlayer_new(world,name,NULL,&dependencies);CHECK(player);CHECK(constructor_callbacks==before+1);
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,game.heap));CHECK(MCGameplay_setPlayer(&game,0,"11111111-1111-1111-1111-111111111111",(MCObject *)player));FixtureEffects *f=effects(player);
    CHECK(((MCGameplayWorld *)(player->living.entity.worldObj))==world&&player->gameProfile->name==name&&player->stats==NULL&&player->savedFields);
    CHECK(player->inventory->player==(MCObject *)player&&player->openContainer==player->inventoryContainer);
    CHECK(ContainerList_size(player->openContainer->inventorySlots)==45&&((ContainerPlayer *)(player->inventoryContainer))->thePlayer==(MCObject *)player);
    CHECK(MCGameplayPlayer_inventory((MCObject *)player)==player->inventory&&MCGameplayPlayer_world((MCObject *)player)==(MCObject *)world);
    CHECK(!MCGameplayPlayer_isCreativeMode((MCObject *)player)&&!MCGameplayWorld_isRemote((MCObject *)world));player->capabilities->isCreativeMode=true;world->isRemote=true;CHECK(MCGameplayPlayer_isCreativeMode((MCObject *)player)&&MCGameplayWorld_isRemote((MCObject *)world));player->capabilities->isCreativeMode=false;world->isRemote=false;
    player->living.entity.posX=1;player->living.entity.posY=2;player->living.entity.posZ=3;CHECK(MCGameplayPlayer_getDistanceSq((MCObject *)player,4,6,3)==25);CHECK(isnan(MCGameplayPlayer_getDistanceSq((MCObject *)player,NAN,0,0)));
    CHECK(MCGameplayWorld_isCraftingTable((MCObject *)world,1,2,3)&&!MCGameplayWorld_isCraftingTable((MCObject *)world,1,1,3));
    ItemStack *source=stack(game.heap,387,2,7);NBTTagCompound *tag=NBTTagCompound_new(game.heap);CHECK(tag);CHECK(NBTTagCompound_setInteger_ascii(tag,"generation",0));CHECK(ItemStack_setTagCompound(source,tag));
    CHECK(InventoryCrafting_setInventorySlotContents(((ContainerPlayer *)(player->inventoryContainer))->craftMatrix,0,stack(game.heap,386,1,0)));CHECK(InventoryCrafting_setInventorySlotContents(((ContainerPlayer *)(player->inventoryContainer))->craftMatrix,1,source));
    CHECK(Container_slotClick(player->openContainer,0,0,0,player->inventory));CHECK(InventoryCrafting_getStackInSlot(((ContainerPlayer *)(player->inventoryContainer))->craftMatrix,1)==source&&source->stackSize==0);
    CHECK(player->inventory->mainInventory->items[0]&&player->inventory->mainInventory->items[0]!=source&&player->inventory->mainInventory->items[0]->stackSize==1);
    CHECK(f->crafts==1&&f->drops==0&&f->achievements==0&&InventoryPlayer_getItemStack(player->inventory)==f->last);
    CHECK(!MCObjectHeap_failed(game.heap));MCObjectRootScope_end(&scope);CHECK(MCObjectHeap_collect(game.heap));CHECK(MCGameplay_free(&game));CHECK(mc_world_get(&terrain,1,2,3)==(58u<<4));mc_world_free(&terrain);
}
static void map_body(mc_nbt *out,int32_t value) {
    mc_buf b;mc_buf_init(&b);mc_put_u8(&b,10);mc_put_i16(&b,0);mc_put_u8(&b,3);mc_put_i16(&b,7);mc_put_bytes(&b,"Foreign",7);mc_put_i32(&b,value);mc_put_u8(&b,0);
    CHECK(mc_nbt_read(&b,out)&&b.pos==b.len);mc_buf_free(&b);
}
static void snapshots_and_maps(void) {
    mc_world terrain;mc_world_init(&terrain,9);MCGameplay game={0};CHECK(MCGameplay_init(&game,32*1024*1024));MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,game.heap));MCGameplayWorld *world=world_new(&game,&terrain);
    StatFileWriter *stats=StatFileWriter_new(game.heap);CHECK(stats);
    MCGameplayPlayer *a=MCGameplayPlayer_new(world,NBTString_fromASCII(game.heap,"A"),stats,&dependencies),*b=MCGameplayPlayer_new(world,NBTString_fromASCII(game.heap,"B"),stats,&dependencies);CHECK(a&&b);
    CHECK(MCGameplay_setPlayer(&game,0,"11111111-1111-1111-1111-111111111111",(MCObject *)a));CHECK(MCGameplay_setPlayer(&game,1,"22222222-2222-2222-2222-222222222222",(MCObject *)b));effects(a);b->effects=a->effects;a->handler=b->handler=(MCObject *)a->savedFields;
    ItemStack *shared=stack(game.heap,1,0,7);CHECK(InventoryPlayer_setItemStack(a->inventory,shared));CHECK(InventoryPlayer_setInventorySlotContents(b->inventory,0,shared));CHECK(InventoryCrafting_setInventorySlotContents(((ContainerPlayer *)(a->inventoryContainer))->craftMatrix,3,shared));
    StatBase *stat=StatBase_newIdentity(game.heap,NBTString_fromASCII(game.heap,"stat.craft.test"),STAT_BASE_KIND_CRAFTING);CHECK(stat);world->craftStats[1]=world->craftStats[2]=stat;CHECK(StatFileWriter_increaseStat(stats,(MCObject *)a,stat,17));
    mc_map_info map={0};map.id=7;map.metadata_known=true;map.colors[0]=42;map_body(&map.original_nbt,19);map_body(&map.original_entry_nbt,23);CHECK(mc_maps_add(&world->maps,&map));mc_map_info_free(&map);map_body(&world->maps.original_nbt,29);
    mc_MapInfo *tracking=mc_MapData_getMapInfo(&world->maps.entries[0],5);CHECK(tracking);tracking->packet_counter=17;
    world->isRemote=true;world->worldInfo->spawnX=-55;world->worldInfo->spawnZ=91;world->provider->dimensionId=-1;CHECK(NativeJavaRandom_setSeed(world->rand,-1));world->nextEntityId=123;
    a->living.entity.posX=1.5;a->living.entity.posY=20;a->living.entity.posZ=-8;a->living.entity.rotationYaw=33;a->living.entity.rotationPitch=-15;a->spectator=true;CHECK(Entity_setSilent(&a->living.entity,true));a->isChangingQuantityOnly=true;
    MCObjectRootScope_end(&scope);MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&game,&tx));CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));MCGameplayObjects *owners=MCGameplay_get(&tx.working);MCGameplayWorld *copy=(MCGameplayWorld *)owners->world;MCGameplayPlayer *ca=(MCGameplayPlayer *)owners->players[0],*cb=(MCGameplayPlayer *)owners->players[1];
    CHECK(copy!=world&&copy->owners==owners&&copy->terrain==&terrain&&copy->manager!=world->manager);CHECK(((MCGameplayWorld *)(ca->living.entity.worldObj))==copy&&((MCGameplayWorld *)(cb->living.entity.worldObj))==copy);
    CHECK(ca->inventory->player==(MCObject *)ca&&((ContainerPlayer *)(ca->inventoryContainer))->thePlayer==(MCObject *)ca&&ca->openContainer==ca->inventoryContainer);
    CHECK(ca->effects==cb->effects&&ca->effects!=a->effects&&ca->handler==cb->handler&&ca->handler==(MCObject *)ca->savedFields);
    CHECK(InventoryPlayer_getItemStack(ca->inventory)==cb->inventory->mainInventory->items[0]&&InventoryPlayer_getItemStack(ca->inventory)==InventoryCrafting_getStackInSlot(((ContainerPlayer *)(ca->inventoryContainer))->craftMatrix,3));
    CHECK(InventoryPlayer_getItemStack(ca->inventory)!=shared&&InventoryPlayer_getItemStack(ca->inventory)->stackSize==0);
    CHECK(copy->craftStats[1]==copy->craftStats[2]&&copy->craftStats[1]!=stat&&copy->craftStats[0]==NULL);
    CHECK(ca->stats==cb->stats&&ca->stats!=stats&&StatFileWriter_readStat(ca->stats,copy->craftStats[1])==17);
    CHECK(StatFileWriter_increaseStat(ca->stats,(MCObject *)ca,copy->craftStats[1],6));CHECK(StatFileWriter_readStat(stats,stat)==17&&StatFileWriter_readStat(ca->stats,copy->craftStats[1])==23);
    CHECK(copy->maps.entries!=world->maps.entries&&copy->maps.entries[0].original_nbt.data!=world->maps.entries[0].original_nbt.data&&copy->maps.original_nbt.data!=world->maps.original_nbt.data);
    CHECK(copy->maps.entries[0].original_entry_nbt.data!=world->maps.entries[0].original_entry_nbt.data&&copy->maps.entries[0].tracking!=world->maps.entries[0].tracking);
    CHECK(mc_MapData_getMapInfo(&copy->maps.entries[0],5)->packet_counter==17);CHECK(copy->isRemote&&copy->worldInfo->spawnX==-55&&copy->worldInfo->spawnZ==91&&copy->provider->dimensionId==-1&&copy->nextEntityId==123);
    CHECK(copy->rand!=world->rand&&copy->rand->state.seed48==world->rand->state.seed48&&copy->randomRuntime==world->randomRuntime);
    CHECK(ca->living.entity.rand!=a->living.entity.rand&&cb->living.entity.rand!=b->living.entity.rand&&ca->living.entity.rand!=cb->living.entity.rand&&ca->living.entity.entityUniqueID!=a->living.entity.entityUniqueID);
    CHECK(ca->living.entity.entityUniqueID->mostSignificantBits==a->living.entity.entityUniqueID->mostSignificantBits&&ca->living.entity.entityUniqueID->leastSignificantBits==a->living.entity.entityUniqueID->leastSignificantBits);
    CHECK(ca->living.randomUnused1==a->living.randomUnused1&&ca->living.randomUnused2==a->living.randomUnused2&&ca->living.rotationYawHead==a->living.rotationYawHead);
    CHECK(ca->living.entity.posX==1.5&&ca->living.entity.posY==20&&ca->living.entity.posZ==-8&&ca->living.entity.rotationYaw==33&&ca->living.entity.rotationPitch==-15&&ca->spectator&&Entity_isSilent(&ca->living.entity)&&ca->isChangingQuantityOnly);
    copy->maps.entries[0].colors[0]=99;mc_MapData_getMapInfo(&copy->maps.entries[0],5)->packet_counter=18;InventoryPlayer_getItemStack(ca->inventory)->stackSize=-1;
    CHECK(world->maps.entries[0].colors[0]==42&&mc_MapData_getMapInfo(&world->maps.entries[0],5)->packet_counter==17&&shared->stackSize==0);
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));CHECK(mc_maps_find_const(&world->maps,7)->colors[0]==42);CHECK(MCObjectHeap_collect(game.heap));CHECK(MCGameplay_free(&game));mc_world_free(&terrain);
}
static void invalid_dependencies_and_clone_failure(void) {
    MCGameplay game={0};CHECK(MCGameplay_init(&game,32*1024*1024));MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,game.heap));MCGameplayWorld *world=world_new(&game,NULL);MCObjectRootScope_end(&scope);
    /* Clone failure must destroy only copied native buffers and preserve every
       original world/map buffer and managed child. Shape corruption is private. */
    map_body(&world->maps.original_nbt,31);uint8_t *original=world->maps.original_nbt.data;
    world->maps.capacity=1;MCObjectHeap *failed=MCObjectHeap_clone(game.heap);CHECK(!failed&&!MCObjectHeap_failed(game.heap));world->maps.capacity=0;CHECK(world->maps.original_nbt.data==original);CHECK(world->manager&&world->owners==MCGameplay_get(&game));
    /* A later managed-edge failure occurs after the native map clone succeeds.
       Its destructor must still preserve original buffers and foreign stats. */
    MCObjectHeap *other=MCObjectHeap_new(1024*1024);CHECK(other);StatBase *foreign_stat=StatBase_newIdentity(other,NBTString_fromASCII(other,"Foreign"),STAT_BASE_KIND_CRAFTING);CHECK(foreign_stat);world->craftStats[0]=foreign_stat;
    failed=MCObjectHeap_clone(game.heap);CHECK(!failed&&!MCObjectHeap_failed(game.heap)&&!MCObjectHeap_failed(other));CHECK(world->maps.original_nbt.data==original&&StatBase_getKind(foreign_stat)==STAT_BASE_KIND_CRAFTING);world->craftStats[0]=NULL;CHECK(MCGameplay_free(&game));MCObjectHeap_free(other);
    for (unsigned missing=0;missing<3;missing++) {
        CHECK(MCGameplay_init(&game,32*1024*1024));world=world_new(&game,NULL);
        if (missing==0) CHECK(!MCGameplayWorld_isCraftingTable((MCObject *)world,0,0,0));
        else {mc_crafting_dispatch d=dependencies;if(missing==1)d.findMatchingRecipe=NULL;else d.drop=NULL;CHECK(!MCGameplayPlayer_new(world,NBTString_fromASCII(game.heap,"Name"),NULL,&d));}
        CHECK(MCObjectHeap_failed(game.heap));CHECK(MCGameplay_free(&game));
    }
    MCObjectHeap *foreign=MCObjectHeap_new(1024*1024);CHECK(foreign);
    CHECK(MCGameplay_init(&game,32*1024*1024));world=world_new(&game,NULL);NBTString *wrong=NBTString_fromASCII(foreign,"Foreign");CHECK(wrong);CHECK(!MCGameplayPlayer_new(world,wrong,NULL,&dependencies));CHECK(MCObjectHeap_failed(game.heap)&&!MCObjectHeap_failed(foreign));CHECK(MCGameplay_free(&game));MCObjectHeap_free(foreign);
}
static void bounded_constructor_failures(void) {
    unsigned failed_world=0,failed_after_world=0,successful=0;
    MCGameplay measured={0};CHECK(MCGameplay_init(&measured,32*1024*1024));
    CraftingManager *measured_manager=CraftingManager_newEmpty(measured.heap);CHECK(measured_manager);
    MCGameplayWorld *measured_world=MCGameplayWorld_new(measured.heap,MCGameplay_get(&measured),NULL,measured_manager);CHECK(measured_world);
    CHECK(MCGameplay_setWorld(&measured,(MCObject *)measured_world));
    NBTString *measured_name=NBTString_fromASCII(measured.heap,"Bounded");CHECK(measured_name);
    size_t world_bytes=MCObjectHeap_liveBytes(measured.heap);
    MCGameplayPlayer *measured_player=MCGameplayPlayer_new(measured_world,measured_name,NULL,&dependencies);CHECK(measured_player);
    size_t complete_bytes=MCObjectHeap_liveBytes(measured.heap);
    CHECK(complete_bytes>world_bytes && world_bytes>8192 && complete_bytes<=SIZE_MAX-8192);
    CHECK(MCGameplay_free(&measured));
    /* Exercise allocation failure at successive source constructor stages.
       Derive the interval from this actual World/player graph, including the
       Source 32768-element light array. No fake allocation hook is needed. */
    for (size_t budget=world_bytes-8192;budget<=complete_bytes+8192;budget+=128) {
        MCGameplay game={0};CHECK(MCGameplay_init(&game,budget));
        CraftingManager *manager=CraftingManager_newEmpty(game.heap);
        MCGameplayWorld *world=manager ? MCGameplayWorld_new(game.heap,MCGameplay_get(&game),NULL,manager) : NULL;
        if (world && MCGameplay_setWorld(&game,(MCObject *)world)) {NBTString *name=NBTString_fromASCII(game.heap,"Bounded");
            if (name) {MCGameplayPlayer *p=MCGameplayPlayer_new(world,name,NULL,&dependencies);
                if (p) {++successful;CHECK(p->openContainer==p->inventoryContainer&&!MCObjectHeap_failed(game.heap));
                    const MCObjectClass *player_class=p->living.entity.object.klass;
                    CHECK(MCObjectHeap_collect(game.heap));
                    CHECK(!MCObjectHeap_findObject(game.heap,player_class,any_object,NULL));
                    CHECK(MCGameplay_get(&game)->world==(MCObject *)world && !MCObjectHeap_failed(game.heap));
                }
                else {++failed_after_world;CHECK(MCObjectHeap_failed(game.heap));}
            } else {++failed_world;CHECK(MCObjectHeap_failed(game.heap));}
        } else {++failed_world;CHECK(MCObjectHeap_failed(game.heap));}
        CHECK(!MCObjectHeap_hasBorrowers(game.heap));CHECK(MCGameplay_free(&game));
        CHECK(!game.heap && !game.root.heap && !game.root.id);
    }
    CHECK(failed_world>20&&failed_after_world>20&&successful>20);
}
int main(void) {constructor_container_world_flag();construction_and_source_crafting();snapshots_and_maps();invalid_dependencies_and_clone_failure();bounded_constructor_failures();printf("gameplay owners: %u checks passed\n",checks);return 0;}
