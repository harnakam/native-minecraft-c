#include "util/NativeJavaRandomRuntime.h"
#include "util/MCGameplayPlayer.h"
#include "client/native_runtime.h"
#include "server/native_gameplay.h"
#include "util/MCGameplayPackets.h"
#include "item/item.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) {fprintf(stderr,"native RNG check %u line %d: %s\n",checks,__LINE__,#x);exit(1);} } while (0)
static uint64_t long_bits(int64_t value) {uint64_t bits;memcpy(&bits,&value,sizeof bits);return bits;}
static uint64_t double_bits(double value) {uint64_t bits;memcpy(&bits,&value,sizeof bits);return bits;}
static uint32_t float_bits(float value) {uint32_t bits;memcpy(&bits,&value,sizeof bits);return bits;}
typedef struct {
    NativeJavaRandomRuntime *runtime;
    int64_t next;
    unsigned calls;
    bool fail,reenter,reentryRejected;
} Clock;
static bool nano(void *context,int64_t *value) {
    Clock *clock=context;++clock->calls;
    if (clock->reenter) {
        int64_t seed=77;double number=88;
        clock->reentryRejected=!NativeJavaRandomRuntime_nextSeed(clock->runtime,&seed)&&seed==77&&
            !NativeJavaRandomRuntime_mathRandom(clock->runtime,&number)&&number==88;
    }
    if (clock->fail) return false;
    *value=clock->next;
    uint64_t next=(uint64_t)clock->next+1u;memcpy(&clock->next,&next,sizeof next);return true;
}
static const NativeJavaRandomRuntimeDependencies clock_dependencies={nano};
static NativeJavaRandomRuntime *runtime(Clock *clock,int64_t initial) {
    *clock=(Clock){.next=initial};
    clock->runtime=NativeJavaRandomRuntime_new(&clock_dependencies,clock);CHECK(clock->runtime);
    return clock->runtime;
}
/* Numeric facts observed with the installed Java8 seedUniquifier method,
   java.util.Random and actual 1.8.9 MathHelper UUID method. Java/JAR sources
   and the independently authored observer remain private. */
static void seed_order_and_failures(void) {
    static const int64_t nanos[]={0,1,-1,INT64_MIN,INT64_MAX,123456789};
    static const uint64_t golden[]={UINT64_C(0x6f1d6f662f21e3dc),UINT64_C(0xd273da0fe1fa868d),
        UINT64_C(0xcdc2991b39028303),UINT64_C(0x82c61ddf67f26a2c),UINT64_C(0x84fb06c60be972e3),UINT64_C(0xd5fd118de2a1bdd9)};
    Clock clock;NativeJavaRandomRuntime *service=runtime(&clock,0);
    for (unsigned i=0;i<sizeof nanos/sizeof *nanos;i++) {
        clock.next=nanos[i];int64_t seed=7;
        CHECK(NativeJavaRandomRuntime_nextSeed(service,&seed));CHECK(long_bits(seed)==golden[i]);
    }
    CHECK(clock.calls==6);CHECK(NativeJavaRandomRuntime_free(service));
    service=runtime(&clock,0);clock.fail=true;int64_t seed=99;
    CHECK(!NativeJavaRandomRuntime_nextSeed(service,&seed)&&seed==99&&clock.calls==1);
    clock.fail=false;clock.next=1;
    CHECK(NativeJavaRandomRuntime_nextSeed(service,&seed)&&long_bits(seed)==golden[1]);
    CHECK(!NativeJavaRandomRuntime_nextSeed(service,NULL)&&clock.calls==2);
    CHECK(NativeJavaRandomRuntime_free(service));
    service=runtime(&clock,0);clock.reenter=true;
    CHECK(NativeJavaRandomRuntime_nextSeed(service,&seed)&&long_bits(seed)==golden[0]);
    CHECK(clock.reentryRejected&&clock.calls==1);CHECK(NativeJavaRandomRuntime_free(service));
    CHECK(!NativeJavaRandomRuntime_new(NULL,NULL));NativeJavaRandomRuntimeDependencies missing={0};
    CHECK(!NativeJavaRandomRuntime_new(&missing,NULL));
    CHECK(!NativeJavaRandomRuntime_free(NativeJavaRandomRuntime_process()));
    service=runtime(&clock,0);MCObjectHeap *tiny=MCObjectHeap_new(1);CHECK(tiny);
    CHECK(!NativeJavaRandomRuntime_newRandom(service,tiny)&&MCObjectHeap_failed(tiny)&&clock.calls==0);
    MCObjectHeap_free(tiny);CHECK(NativeJavaRandomRuntime_free(service));
    service=runtime(&clock,0);MCObjectHeap *heap=MCObjectHeap_new(4096);CHECK(heap);clock.fail=true;
    CHECK(!NativeJavaRandomRuntime_newRandom(service,heap)&&MCObjectHeap_failed(heap)&&clock.calls==1);
    MCObjectHeap_free(heap);clock.fail=false;clock.next=1;
    CHECK(NativeJavaRandomRuntime_nextSeed(service,&seed)&&long_bits(seed)==golden[1]);
    CHECK(NativeJavaRandomRuntime_free(service));
}
static NBTString *registry_name(MCObject *context,const ItemStack *stack) {
    return NBTString_fromUTF8(context->heap,mc_item_name((int16_t)ItemStack_registryId(stack->item)));
}
static bool accept_fixture(MCGameplayObjects *objects,void *context) {
    (void)context;return objects&&objects->world&&objects->players[0];
}
static void constructor_results_and_graph_lifetime(void) {
    Clock clock;NativeJavaRandomRuntime *service=runtime(&clock,100);
    MCGameplay game={0};CHECK(MCGameplay_init(&game,32u*1024u*1024u));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,game.heap));
    CraftingManager *manager=CraftingManager_newEmpty(game.heap);CHECK(manager);
    MCGameplayWorld *world=MCGameplayWorld_newWithRandomRuntime(game.heap,MCGameplay_get(&game),NULL,manager,service);
    CHECK(world&&MCGameplay_setWorld(&game,(MCObject *)world));world->remote=true;
    world->itemDisplayName=registry_name;world->itemDisplayContext=(MCObject *)world;
    CHECK(clock.calls==2&&world->updateLCG==INT32_C(0x0a053de4)&&world->ambientTickCountdown==3286);
    CHECK(world->rand->state.seed48==UINT64_C(0x7df268edb03f));
    MCGameplayPlayer *player=MCGameplayPlayer_new(world,NBTString_fromASCII(game.heap,"Golden"),NULL,mc_client_graph_crafting());
    CHECK(player&&MCGameplay_setPlayer(&game,0,"11111111-1111-4111-8111-111111111111",(MCObject *)player));
    CHECK(clock.calls==4&&player->living.entity.rand!=world->rand&&player->gameProfile->id==NULL);
    CHECK(player->living.entity.rand->state.seed48==UINT64_C(0x5ae608e5c06b));
    CHECK(long_bits(player->living.entity.entityUniqueID->mostSignificantBits)==UINT64_C(0x53712c15f7913dec));
    CHECK(long_bits(player->living.entity.entityUniqueID->leastSignificantBits)==UINT64_C(0x912449edcf80b63d));
    CHECK(float_bits(player->living.randomUnused1)==UINT32_C(0x3c350bc9));
    CHECK(float_bits(player->living.randomUnused2)==UINT32_C(0x44692b2e));
    CHECK(float_bits(player->living.rotationYawHead)==UINT32_C(0x40c452aa)&&player->living.entity.rotationYaw==0&&player->living.entity.rotationPitch==0);
    EntityItem *entity=EntityItem_new_position(game.heap,(MCObject *)world,(MCObject *)world,
        mc_server_graph_item_dependencies(),mc_server_graph_item_constructors(),1,2,3);
    CHECK(entity&&MCGameplay_addItem(&game,(MCObject *)entity));
    CHECK(clock.calls==5&&entity->entity.rand!=player->living.entity.rand&&entity->entity.rand!=world->rand);
    CHECK(entity->entity.rand->state.seed48==UINT64_C(0x0af0437030ad));
    CHECK(long_bits(entity->entity.entityUniqueID->mostSignificantBits)==UINT64_C(0x0c72b1e46af14a72));
    CHECK(long_bits(entity->entity.entityUniqueID->leastSignificantBits)==UINT64_C(0xaccc15550af04370));
    CHECK(float_bits(entity->hoverStart)==UINT32_C(0x3ec2e24b)&&float_bits(entity->entity.rotationYaw)==UINT32_C(0x41eb8bd0));
    CHECK(double_bits(entity->entity.motionX)==UINT64_C(0xbfb0a2dc80000000)&&double_bits(entity->entity.motionZ)==UINT64_C(0xbfb3446000000000));
    CHECK(entity->entity.posX==1&&entity->entity.posY==2&&entity->entity.posZ==3&&entity->entity.dataWatcher&&entity->health==5);
    CHECK(world->rand->state.seed48==UINT64_C(0x7df268edb03f));
    /* Deliberate Java-reference aliases across owners must remain aliases,
       while the external process service is never cloned or rolled back. */
    player->living.entity.rand=entity->entity.rand;player->gameProfile->id=entity->entity.entityUniqueID;
    player->living.entity.entityUniqueID=player->gameProfile->id;MCObjectHeap_touch(game.heap);
    uint64_t parentSeed=player->living.entity.rand->state.seed48;
    MCObjectRootScope_end(&scope);MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&game,&tx));
    CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
    MCGameplayWorld *copy=(MCGameplayWorld *)MCGameplay_get(&tx.working)->world;
    MCGameplayPlayer *cp=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];
    EntityItem *ce=(EntityItem *)MCGameplay_get(&tx.working)->items[0];
    CHECK(copy->randomRuntime==service&&copy->rand!=world->rand);
    CHECK(cp->living.entity.rand==ce->entity.rand&&cp->living.entity.rand!=player->living.entity.rand&&cp->living.entity.entityUniqueID==ce->entity.entityUniqueID&&cp->gameProfile->id==cp->living.entity.entityUniqueID);
    float f;CHECK(NativeJavaRandom_nextFloat(cp->living.entity.rand,&f));CHECK(player->living.entity.rand->state.seed48==parentSeed);
    double value;CHECK(NativeJavaRandomRuntime_mathRandom(copy->randomRuntime,&value));CHECK(double_bits(value)==UINT64_C(0x3fe9f5e320818288));
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));
    CHECK(NativeJavaRandomRuntime_mathRandom(service,&value)&&double_bits(value)==UINT64_C(0x3fe343036e230209));
    CHECK(player->living.entity.rand->state.seed48==parentSeed&&clock.calls==5);
    CHECK(MCGameplay_begin(&game,&tx));CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
    cp=(MCGameplayPlayer *)MCGameplay_get(&tx.working)->players[0];CHECK(NativeJavaRandom_nextFloat(cp->living.entity.rand,&f));
    uint64_t adoptedSeed=cp->living.entity.rand->state.seed48;MCObjectRootScope_end(&scope);char error[160];
    CHECK(MCGameplay_acceptClientFrame(&tx,accept_fixture,NULL,error,sizeof error));CHECK(MCObjectHeap_collect(game.heap));
    player=(MCGameplayPlayer *)MCGameplay_get(&game)->players[0];entity=(EntityItem *)MCGameplay_get(&game)->items[0];
    CHECK(player->living.entity.rand==entity->entity.rand&&player->living.entity.rand->state.seed48==adoptedSeed);
    CHECK(player->living.entity.entityUniqueID==player->gameProfile->id&&player->living.entity.entityUniqueID==entity->entity.entityUniqueID);
    CHECK(((MCGameplayWorld *)(player->living.entity.worldObj))->randomRuntime==service&&clock.calls==5);
    CHECK(MCGameplay_free(&game));CHECK(NativeJavaRandomRuntime_free(service));
}
static void actual_server_drop_and_client_spawn(void) {
    mc_world terrain;mc_world_init(&terrain,919);MCGameplay game={0};
    /* Real loaded air chunks close the SourceMP top-solid/collision dependency. */
    for(int cz=-1;cz<=1;cz++)for(int cx=-1;cx<=1;cx++)CHECK(mc_world_chunk(&terrain,cx,cz,true));
    CHECK(mc_server_graph_init(&game,&terrain,0,0,919));MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&game,&tx));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
    Clock clock;NativeJavaRandomRuntime *service=runtime(&clock,100);
    MCGameplayWorld *world=mc_server_graph_world(&tx.working);world->randomRuntime=service;
    CHECK(mc_server_graph_add_player(&tx.working,0,"11111111-1111-4111-8111-111111111111","RngRuntime",1,8,20,8,false));
    MCGameplayPlayer *player=mc_server_graph_player(&tx.working,0);
    CHECK(player->living.entity.entityUniqueID==player->gameProfile->id&&player->gameProfile->id);
    CHECK(clock.calls==2&&player->living.entity.rotationYaw==0&&player->living.entity.rotationPitch==0);
    CHECK(NativeJavaRandom_setSeed(world->rand,919)&&NativeJavaRandom_setSeed(player->living.entity.rand,12345));
    uint64_t before=world->rand->state.seed48;
    ItemStack *stack=ItemStack_new(tx.working.heap,ItemStack_registryItem(1),2,0);
    CHECK(stack&&InventoryPlayer_setInventorySlotContents(player->inventory,0,stack));
    CHECK(mc_server_graph_drop(player,false));CHECK(world->rand->state.seed48==before);
    CHECK(player->living.entity.rand->state.seed48==UINT64_C(0xeac807783498)&&clock.calls==3);
    MCGameplayObjects *owners=MCGameplay_get(&tx.working);CHECK(owners->itemCount==1);
    EntityItem *entity=(EntityItem *)owners->items[0];CHECK(entity->entity.rand!=world->rand&&entity->entity.rand!=player->living.entity.rand&&entity->entity.entityUniqueID);
    CHECK(EntityItem_getEntityItem(entity)->stackSize==1&&stack->stackSize==1);
    uint64_t playerBefore=player->living.entity.rand->state.seed48;
    float value=mc_server_graph_item_dependencies()->nextFloat((MCObject *)world,entity);
    CHECK(value>=0&&value<1&&world->rand->state.seed48==before&&player->living.entity.rand->state.seed48==playerBefore);
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));CHECK(MCGameplay_free(&game));CHECK(NativeJavaRandomRuntime_free(service));
    CHECK(mc_client_graph_init(&game,&terrain,"RngClient"));CHECK(MCObjectRootScope_begin(&scope,game.heap));
    service=runtime(&clock,100);world=mc_client_graph_world(&game);world->randomRuntime=service;
    player=mc_client_graph_player(&game);CHECK(player->gameProfile->id&&player->living.entity.entityUniqueID==player->gameProfile->id);
    before=world->rand->state.seed48;playerBefore=player->living.entity.rand->state.seed48;
    CHECK(mc_client_graph_spawn_item(&game,123,8,20,8,0.1,0.2,0.3));entity=mc_client_graph_item(&game,123);
    CHECK(entity&&entity->entity.rand&&entity->entity.entityUniqueID&&clock.calls==2);
    CHECK(entity->hoverStart>=0&&entity->hoverStart<6.283186f&&entity->entity.motionX==0.1&&entity->entity.motionY==0.2&&entity->entity.motionZ==0.3);
    CHECK(entity->entity.entityId==123&&DataWatcher_getWatchableObjectItemStack(entity->entity.dataWatcher,10)==NULL);
    CHECK(world->rand->state.seed48==before&&player->living.entity.rand->state.seed48==playerBefore);
    CHECK(mc_client_graph_spawn_item_packet(&game,124,8,20,8,-128,-1,0,9,10,11));
    entity=mc_client_graph_item(&game,124);CHECK(entity&&clock.calls==3);
    CHECK(float_bits(entity->entity.rotationPitch)==UINT32_C(0xc3340000)&&float_bits(entity->entity.rotationYaw)==UINT32_C(0xbfb40000));
    CHECK(entity->entity.motionX>=-0.10000001&&entity->entity.motionX<0.10000001&&entity->entity.motionZ>=-0.10000001&&entity->entity.motionZ<0.10000001);
    CHECK(entity->entity.motionY==0.20000000298023224&&entity->entity.motionX!=9&&entity->entity.motionZ!=11);
    CHECK(mc_client_graph_spawn_item_packet(&game,125,8,20,8,127,INT32_MIN,-1,9,10,11));
    entity=mc_client_graph_item(&game,125);CHECK(entity&&clock.calls==4);
    CHECK(float_bits(entity->entity.rotationPitch)==UINT32_C(0x43329800)&&entity->entity.rotationYaw==0);
    CHECK(entity->entity.motionY==0.20000000298023224&&entity->entity.motionX!=9&&entity->entity.motionZ!=11);
    CHECK(mc_client_graph_spawn_item_packet(&game,126,8,20,8,-1,127,1,0.4,-0.5,0.6));
    entity=mc_client_graph_item(&game,126);CHECK(entity&&clock.calls==5);
    CHECK(float_bits(entity->entity.rotationPitch)==UINT32_C(0xbfb40000)&&float_bits(entity->entity.rotationYaw)==UINT32_C(0x43329800));
    CHECK(entity->entity.motionX==0.4&&entity->entity.motionY==-0.5&&entity->entity.motionZ==0.6);
    CHECK(world->rand->state.seed48==before&&player->living.entity.rand->state.seed48==playerBefore);
    MCObjectRootScope_end(&scope);CHECK(MCObjectHeap_collect(game.heap));
    CHECK(mc_client_graph_item(&game,123)->entity.rand&&mc_client_graph_item(&game,123)->entity.entityUniqueID);
    CHECK(MCGameplay_free(&game));CHECK(NativeJavaRandomRuntime_free(service));mc_world_free(&terrain);
}
static void source_pickup_silent_byte(void) {
    static const int8_t values[]={0,1,2,4,-1};
    mc_world terrain;mc_world_init(&terrain,919);MCGameplay game={0};
    /* Real loaded air chunks close the SourceMP top-solid/collision dependency. */
    for(int cz=-1;cz<=1;cz++)for(int cx=-1;cx<=1;cx++)CHECK(mc_world_chunk(&terrain,cx,cz,true));
    CHECK(mc_server_graph_init(&game,&terrain,0,0,919));
    for(size_t i=0;i<sizeof values/sizeof *values;i++) {
        MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&game,&tx));
        MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
        CHECK(mc_server_graph_add_player(&tx.working,0,"11111111-1111-4111-8111-111111111111","SilentPickup",1,8,20,8,false));
        MCGameplayPlayer *player=mc_server_graph_player(&tx.working,0);
        MCGameplayWorld *world=((MCGameplayWorld *)(player->living.entity.worldObj));
        ItemStack *stack=ItemStack_new(tx.working.heap,ItemStack_registryItem(1),1,0);
        EntityItem *entity=EntityItem_new_stack(tx.working.heap,(MCObject *)world,(MCObject *)world,
            mc_server_graph_item_dependencies(),mc_server_graph_item_constructors(),8,20,8,stack);
        CHECK(entity&&NativeJavaRandom_setSeed(entity->entity.rand,12345));
        CHECK(DataWatcher_updateObject(entity->entity.dataWatcher,4,DataWatcher_boxByte(tx.working.heap,values[i])));
        int32_t before=MCGameplayPackets_count(player);
        uint64_t worldState=world->rand->state.seed48,playerState=player->living.entity.rand->state.seed48;
        CHECK(EntityItem_onCollideWithPlayer(entity,(MCObject *)player));
        CHECK(stack->stackSize==0&&entity->entity.isDead&&player->inventory->mainInventory->items[0]->stackSize==1);
        CHECK(world->rand->state.seed48==worldState&&player->living.entity.rand->state.seed48==playerState);
        bool isSilent=values[i]==1;
        /* Actual java.util.Random(12345): seed after zero or two nextFloat
           calls. Only watcher byte exactly one suppresses the pop sound. */
        CHECK(entity->entity.rand->state.seed48==(isSilent?UINT64_C(0x5deecd654):UINT64_C(0x8361b331172e)));
        CHECK(MCGameplayPackets_count(player)==before+(isSilent?2:3));
        for(int32_t n=0;n<(isSilent?2:3);n++) {
            MCGameplayPacketKind kind;
            CHECK(MCGameplayPackets_packetAt(player,before+n,&kind));
            CHECK(kind==(n==0&&!isSilent?MC_GAMEPLAY_PACKET_SOUND:
                n==(isSilent?0:1)?MC_GAMEPLAY_PACKET_COLLECT:MC_GAMEPLAY_PACKET_DESTROY));
            if(kind==MC_GAMEPLAY_PACKET_SOUND) {
                mc_buf out;mc_buf_init(&out);CHECK(MCGameplayPackets_encodeAt(player,before+n,&out));
                char name[32];CHECK(mc_get_varint(&out)==0x29&&mc_get_string(&out,name,sizeof name)&&!strcmp(name,"random.pop"));
                CHECK(mc_get_i32(&out)==64&&mc_get_i32(&out)==160&&mc_get_i32(&out)==64);
                CHECK(float_bits(mc_get_f32(&out))==UINT32_C(0x3e4ccccd)&&mc_get_u8(&out)==112);
                CHECK(!out.failed&&out.pos==out.len);mc_buf_free(&out);
            }
        }
        MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));
    }
    CHECK(MCGameplay_free(&game));CHECK(mc_client_graph_init(&game,&terrain,"SilentRemote"));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,game.heap));
    CHECK(mc_client_graph_spawn_item(&game,42,8,20,8,0,0,0));
    EntityItem *entity=mc_client_graph_item(&game,42);MCGameplayPlayer *player=mc_client_graph_player(&game);
    uint64_t before=entity->entity.rand->state.seed48;
    for(size_t i=0;i<sizeof values/sizeof *values;i++) {
        CHECK(DataWatcher_updateObject(entity->entity.dataWatcher,4,DataWatcher_boxByte(game.heap,values[i])));
        CHECK(entity->dependencies->isSilent(entity->dependencyContext,entity)==(values[i]==1));
        /* The actual Source remote branch returns before stack/audio/pickup
           dependencies; do not manufacture successful local pickup effects. */
        CHECK(EntityItem_onCollideWithPlayer(entity,(MCObject *)player));
        CHECK(!entity->entity.isDead&&entity->entity.rand->state.seed48==before&&player->inventory->mainInventory->items[0]==NULL);
    }
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_free(&game));mc_world_free(&terrain);
}

enum {THREADS=4,DRAWS=1000};
typedef struct {atomic_uint calls;} ThreadClock;
static bool threaded_nano(void *context,int64_t *value) {ThreadClock *clock=context;atomic_fetch_add(&clock->calls,1);*value=0;return true;}
typedef struct {NativeJavaRandomRuntime *service;uint64_t seeds[DRAWS];double numbers[DRAWS];bool ok;} Worker;
static void work(Worker *worker) {
    worker->ok=true;
    for(unsigned i=0;i<DRAWS;i++) {
        int64_t seed;
        if (!NativeJavaRandomRuntime_nextSeed(worker->service,&seed)||!NativeJavaRandomRuntime_mathRandom(worker->service,&worker->numbers[i])) {worker->ok=false;return;}
        worker->seeds[i]=long_bits(seed);
    }
}
#ifdef _WIN32
static DWORD WINAPI start_worker(LPVOID value) {work(value);return 0;}
#else
static void *start_worker(void *value) {work(value);return NULL;}
#endif
static int compare_u64(const void *a,const void *b) {uint64_t x=*(const uint64_t *)a,y=*(const uint64_t *)b;return (x>y)-(x<y);}
static int compare_double(const void *a,const void *b) {double x=*(const double *)a,y=*(const double *)b;return (x>y)-(x<y);}
static void concurrent_process_stream(void) {
    ThreadClock clock;atomic_init(&clock.calls,0);static const NativeJavaRandomRuntimeDependencies dependencies={threaded_nano};
    NativeJavaRandomRuntime *service=NativeJavaRandomRuntime_new(&dependencies,&clock);CHECK(service);
    Worker *workers=calloc(THREADS,sizeof *workers);CHECK(workers);
#ifdef _WIN32
    HANDLE threads[THREADS];
    for(unsigned i=0;i<THREADS;i++){workers[i].service=service;threads[i]=CreateThread(NULL,0,start_worker,&workers[i],0,NULL);CHECK(threads[i]);}
    for(unsigned i=0;i<THREADS;i++){CHECK(WaitForSingleObject(threads[i],10000)==WAIT_OBJECT_0);CHECK(CloseHandle(threads[i]));}
#else
    pthread_t threads[THREADS];
    for(unsigned i=0;i<THREADS;i++){workers[i].service=service;CHECK(pthread_create(&threads[i],NULL,start_worker,&workers[i])==0);}
    for(unsigned i=0;i<THREADS;i++)CHECK(pthread_join(threads[i],NULL)==0);
#endif
    CHECK(atomic_load(&clock.calls)==THREADS*DRAWS+1u);
    uint64_t *seeds=malloc(sizeof *seeds*THREADS*DRAWS);double *numbers=malloc(sizeof *numbers*THREADS*DRAWS);
    uint64_t *expected=malloc(sizeof *expected*(THREADS*DRAWS+1u));double *expectedNumbers=malloc(sizeof *expectedNumbers*THREADS*DRAWS);
    CHECK(seeds&&numbers&&expected&&expectedNumbers);
    for(unsigned i=0;i<THREADS;i++){CHECK(workers[i].ok);memcpy(seeds+i*DRAWS,workers[i].seeds,sizeof workers[i].seeds);memcpy(numbers+i*DRAWS,workers[i].numbers,sizeof workers[i].numbers);}
    uint64_t uniquifier=UINT64_C(8682522807148012);
    for(unsigned i=0;i<THREADS*DRAWS+1u;i++){uniquifier*=UINT64_C(181783497276652981);expected[i]=uniquifier;}
    qsort(seeds,THREADS*DRAWS,sizeof *seeds,compare_u64);qsort(expected,THREADS*DRAWS+1u,sizeof *expected,compare_u64);
    unsigned j=0,missing=0;uint64_t mathSeed=0;
    for(unsigned i=0;i<THREADS*DRAWS+1u;i++){if(j<THREADS*DRAWS&&expected[i]==seeds[j])j++;else{missing++;mathSeed=expected[i];}}
    CHECK(j==THREADS*DRAWS&&missing==1);
    NativeJavaRandomState state={0};int64_t signedSeed;memcpy(&signedSeed,&mathSeed,sizeof mathSeed);CHECK(NativeJavaRandomState_setSeed(&state,signedSeed));
    for(unsigned i=0;i<THREADS*DRAWS;i++)CHECK(NativeJavaRandomState_nextDouble(&state,&expectedNumbers[i]));
    qsort(numbers,THREADS*DRAWS,sizeof *numbers,compare_double);qsort(expectedNumbers,THREADS*DRAWS,sizeof *expectedNumbers,compare_double);
    for(unsigned i=0;i<THREADS*DRAWS;i++)CHECK(double_bits(numbers[i])==double_bits(expectedNumbers[i]));
    free(expectedNumbers);free(expected);free(numbers);free(seeds);free(workers);CHECK(NativeJavaRandomRuntime_free(service));
}
int main(void) {
    seed_order_and_failures();constructor_results_and_graph_lifetime();actual_server_drop_and_client_spawn();source_pickup_silent_byte();concurrent_process_stream();
    printf("native random runtime: %u checks passed\n",checks);return 0;
}
