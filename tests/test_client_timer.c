/* Actual native frame/input/item paths with an owned deterministic clock. */
#define main c919_embedded_main
#include "../src/minecraft/net/minecraft/client/client.c"
#undef main
#include "client/native_timer_clock.h"
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif
static unsigned checks;
#define REQUIRE(x) do { ++checks; if (!(x)) { fprintf(stderr,"client timer %u line%d: %s\n",checks,__LINE__,#x); exit(1); } } while (0)
typedef struct { MCObject object; int64_t system,nano; unsigned systemCalls,nanoCalls; bool failNano; } Clock;
static const MCObjectClass clock_class={"ClientTimerClockFixture",MCObjectHeap_plainClone,NULL,NULL};
static bool system_clock(MCObject *o,int64_t *out) { Clock *c=(Clock *)o; ++c->systemCalls; *out=c->system; return true; }
static bool nano_clock(MCObject *o,int64_t *out) { Clock *c=(Clock *)o; ++c->nanoCalls; if (c->failNano) return false; *out=c->nano; return true; }
static const TimerDependencies clocks={system_clock,nano_clock};
static Clock *clock_of(mc_client *c) { return (Clock *)mc_client_graph_bindings(&c->gameplay)->timer->context; }
typedef struct { float tps,render,speed,partial; double hr,adjustment; int32_t ticks; int64_t system,syncHR,counter; } State;
static State timer_state(const Timer *t) { return (State){t->ticksPerSecond,t->renderPartialTicks,t->timerSpeed,t->elapsedPartialTicks,t->lastHRTime,t->timeSyncAdjustment,t->elapsedTicks,t->lastSyncSysClock,t->lastSyncHRClock,t->counter}; }
static bool same_state(State a,State b) {
    return a.tps==b.tps && a.render==b.render && a.speed==b.speed && a.partial==b.partial && a.hr==b.hr &&
        a.adjustment==b.adjustment && a.ticks==b.ticks && a.system==b.system && a.syncHR==b.syncHR && a.counter==b.counter;
}
static mc_client *client(mc_conn *peer) {
    char error[160]; mc_socket listener=mc_net_listen("127.0.0.1",0,error,sizeof error); REQUIRE(listener!=MC_INVALID_SOCKET);
    struct sockaddr_in address;
#ifdef _WIN32
    int length=sizeof address;
#else
    socklen_t length=sizeof address;
#endif
    REQUIRE(getsockname(listener,(struct sockaddr *)&address,&length)==0);
    mc_socket writer=mc_net_connect("127.0.0.1",ntohs(address.sin_port),error,sizeof error); REQUIRE(writer!=MC_INVALID_SOCKET);
    mc_socket reader=mc_net_accept(listener); REQUIRE(reader!=MC_INVALID_SOCKET); mc_socket_close(listener);
    mc_client *c=calloc(1,sizeof *c); REQUIRE(c); mc_conn_init(&c->connection,writer); mc_conn_init(peer,reader);
    mc_world_init(&c->world,0); REQUIRE(mc_world_set(&c->world,8,0,8,16)); REQUIRE(mc_client_graph_init(&c->gameplay,&c->world,"TimerRuntime"));
    c->state=1; c->joined=true; c->positioned=true; c->inventory_ready=true; c->gamemode=1;
    c->x=8.5; c->y=6; c->z=8.5; c->last_receive_ms=mc_time_ms(); c->last_move_ms=mc_time_ms();
    MCObjectRootScope scope={0}; REQUIRE(MCObjectRootScope_begin(&scope,c->gameplay.heap)); MCClientBindings *b=mc_client_graph_bindings(&c->gameplay);
    Clock *clock=(Clock *)MCObjectHeap_alloc(c->gameplay.heap,sizeof *clock,&clock_class); REQUIRE(clock);
    clock->system=100; clock->nano=INT64_C(2000000000); b->timer=Timer_new(c->gameplay.heap,20,&clocks,(MCObject *)clock); REQUIRE(b->timer);
    REQUIRE(clock->systemCalls==1 && clock->nanoCalls==1 && b->timer->lastHRTime==0);
    ItemStack *stack=ItemStack_new(c->gameplay.heap,ItemStack_registryItem(1),7,0); REQUIRE(stack); stack->animationsToGo=100;
    b->player->inventory->mainInventory->items[0]=stack; MCObjectRootScope_end(&scope); return c;
}
static void destroy(mc_client *c,mc_conn *peer) { mc_conn_close(&c->connection); mc_conn_close(peer); REQUIRE(MCGameplay_free(&c->gameplay)); mc_world_free(&c->world); free(c); }
static void advance(mc_client *c,int64_t system,int64_t nano,float speed) {
    MCObjectRootScope scope={0}; REQUIRE(MCObjectRootScope_begin(&scope,c->gameplay.heap)); Clock *clock=clock_of(c);
    clock->system=system; clock->nano=nano; mc_client_graph_bindings(&c->gameplay)->timer->timerSpeed=speed; MCObjectRootScope_end(&scope);
}
static void collect_packets(mc_client *c,mc_conn *peer,unsigned counts[64]) {
    memset(counts,0,64*sizeof *counts);
    for (unsigned i=0;i<30;i++) {
        REQUIRE(mc_conn_poll(&c->connection) && mc_conn_poll(peer));
        for (;;) { mc_buf packet; mc_buf_init(&packet); int ready=mc_conn_next(peer,&packet); REQUIRE(ready>=0);
            if (!ready) { mc_buf_free(&packet); break; }
            int32_t id=mc_get_varint(&packet); REQUIRE(!packet.failed && id>=0 && id<64); ++counts[id]; mc_buf_free(&packet);
        }
        mc_sleep_ms(1);
    }
}
static void capped_and_discarded(void) {
    mc_conn peer; mc_client *c=client(&peer); MCObjectRootScope scope={0}; REQUIRE(MCObjectRootScope_begin(&scope,c->gameplay.heap));
    REQUIRE(mc_client_graph_spawn_item(&c->gameplay,123,8.5,10,8.5,0,0,0)); EntityItem *item=mc_client_graph_item(&c->gameplay,123); REQUIRE(item);
    REQUIRE(EntityItem_setEntityItemStack(item,ItemStack_new(c->gameplay.heap,ItemStack_registryItem(1),1,0)));
    c->items[0]=(mc_client_item){true,true,123,0,0,0}; MCObjectRootScope_end(&scope); advance(c,1100,INT64_C(3000000000),1);
    int32_t ticks=-99; REQUIRE(update_frame_timer(c,&ticks)); REQUIRE(ticks==10 && c->partial_ticks==0);
    mc_input input={0}; input.select_slot=-1; input.creative_pick=-1; input.paused=true; update_player(c,&input,0,ticks); REQUIRE(!c->failed);
    REQUIRE(MCObjectRootScope_begin(&scope,c->gameplay.heap)); REQUIRE(mc_client_graph_player(&c->gameplay)->inventory->mainInventory->items[0]->animationsToGo==90);
    REQUIRE(mc_client_graph_item(&c->gameplay,123)->age==10); MCObjectRootScope_end(&scope);
    REQUIRE(update_frame_timer(c,&ticks)); REQUIRE(ticks==0); update_player(c,&input,0,ticks);
    REQUIRE(MCObjectRootScope_begin(&scope,c->gameplay.heap)); REQUIRE(mc_client_graph_player(&c->gameplay)->inventory->mainInventory->items[0]->animationsToGo==90);
    REQUIRE(mc_client_graph_item(&c->gameplay,123)->age==10); REQUIRE(clock_of(c)->systemCalls==3 && clock_of(c)->nanoCalls==3); MCObjectRootScope_end(&scope);
    advance(c,1200,INT64_C(3100000000),-1); REQUIRE(update_frame_timer(c,&ticks)); REQUIRE(ticks==-2); update_player(c,&input,0,ticks); REQUIRE(!c->failed);
    REQUIRE(MCObjectRootScope_begin(&scope,c->gameplay.heap)); REQUIRE(mc_client_graph_player(&c->gameplay)->inventory->mainInventory->items[0]->animationsToGo==90);
    REQUIRE(mc_client_graph_item(&c->gameplay,123)->age==10); MCObjectRootScope_end(&scope); destroy(c,&peer);
}
static void one_shot(int32_t expected) {
    mc_conn peer; mc_client *c=client(&peer); int32_t ticks;
    REQUIRE(update_frame_timer(c,&ticks)); REQUIRE(ticks==10); /* Source first update, not runtime priming. */
    int64_t delta=expected==10 ? 1000 : expected ? 100 : 0; advance(c,100+delta,INT64_C(2000000000)+delta*1000000,expected<0 ? -1 : 1);
    REQUIRE(update_frame_timer(c,&ticks)); REQUIRE(ticks==expected);
    mc_input input={0}; input.creative_pick=-1; input.select_slot=3; input.drop_item=true; input.chat_submit=true; snprintf(input.chat,sizeof input.chat,"one frame");
    update_player(c,&input,0,ticks); REQUIRE(!c->failed); unsigned counts[64]; collect_packets(c,&peer,counts);
    REQUIRE(counts[7]==1 && counts[9]==1 && counts[1]==1 && counts[14]==0);
    MCObjectRootScope scope={0}; REQUIRE(MCObjectRootScope_begin(&scope,c->gameplay.heap)); InventoryPlayer *inventory=mc_client_graph_player(&c->gameplay)->inventory;
    REQUIRE(inventory->currentItem==3 && inventory->mainInventory->items[0]->stackSize==7);
    REQUIRE(inventory->mainInventory->items[0]->animationsToGo==100-(expected>0 ? expected : 0)); MCObjectRootScope_end(&scope);
    open_inventory(c); REQUIRE(!c->failed && c->inventory_open); input=(mc_input){0}; input.creative_pick=-1; input.select_slot=-1; input.inventory_click=true; input.inventory_slot=9;
    update_player(c,&input,0,ticks); REQUIRE(!c->failed); collect_packets(c,&peer,counts); REQUIRE(counts[14]==1 && counts[7]==0 && counts[1]==0 && counts[9]==0); destroy(c,&peer);
}
static void adoption_and_lifetime(void) {
    mc_conn peer; mc_client *c=client(&peer); advance(c,1100,INT64_C(3000000000),1); int32_t ticks; REQUIRE(update_frame_timer(c,&ticks));
    REQUIRE(ticks==10 && !MCObjectHeap_hasBorrowers(c->gameplay.heap)); MCObjectRootScope scope={0}; REQUIRE(MCObjectRootScope_begin(&scope,c->gameplay.heap));
    Timer *timer=mc_client_graph_bindings(&c->gameplay)->timer; int32_t identity=MCObjectHeap_identityHashCode((MCObject *)timer); State state=timer_state(timer); MCObjectRootScope_end(&scope);
    mc_buf packet; start_packet(&packet,0x2f); mc_put_u8(&packet,0); mc_put_i16(&packet,36); mc_put_i16(&packet,1); mc_put_u8(&packet,8); mc_put_i16(&packet,0); mc_put_u8(&packet,0);
    REQUIRE(mc_conn_send(&peer,&packet)); mc_buf_free(&packet);
    /* The actual loopback socket is asynchronous; observe the Source frame,
       rather than assuming send() has synchronously delivered it. */
    for (unsigned i=0;i<1000 && !c->inventory_packets && !c->failed;i++) { REQUIRE(mc_conn_poll(&peer)); poll_network(c); if (!c->inventory_packets) mc_sleep_ms(1); }
    REQUIRE(!c->failed && c->inventory_packets==1);
    REQUIRE(MCObjectRootScope_begin(&scope,c->gameplay.heap)); timer=mc_client_graph_bindings(&c->gameplay)->timer;
    REQUIRE(identity==MCObjectHeap_identityHashCode((MCObject *)timer) && same_state(state,timer_state(timer))); REQUIRE(clock_of(c)->systemCalls==2 && clock_of(c)->nanoCalls==2);
    REQUIRE(mc_client_graph_player(&c->gameplay)->inventory->mainInventory->items[0]->animationsToGo==5); MCObjectRootScope_end(&scope);
    MCGameplayTransaction tx={0}; REQUIRE(MCGameplay_begin(&c->gameplay,&tx)); REQUIRE(MCObjectRootScope_begin(&scope,tx.working.heap));
    mc_client_graph_bindings(&tx.working)->timer->timerSpeed=7; MCObjectRootScope_end(&scope); REQUIRE(MCGameplay_abort(&tx)); REQUIRE(MCObjectHeap_collect(c->gameplay.heap));
    start_packet(&packet,7); mc_put_i32(&packet,0); mc_put_u8(&packet,0); mc_put_u8(&packet,1); mc_put_string(&packet,"default"); handle_packet(c,&packet); REQUIRE(!packet.failed && !c->failed); mc_buf_free(&packet);
    REQUIRE(MCObjectRootScope_begin(&scope,c->gameplay.heap)); timer=mc_client_graph_bindings(&c->gameplay)->timer;
    REQUIRE(identity==MCObjectHeap_identityHashCode((MCObject *)timer) && same_state(state,timer_state(timer))); REQUIRE(clock_of(c)->systemCalls==2 && clock_of(c)->nanoCalls==2); MCObjectRootScope_end(&scope);
    REQUIRE(update_frame_timer(c,&ticks)); REQUIRE(ticks==0); destroy(c,&peer);
}
static void failed_clock(void) {
    mc_conn peer; mc_client *c=client(&peer); MCObjectRootScope scope={0}; REQUIRE(MCObjectRootScope_begin(&scope,c->gameplay.heap)); Clock *clock=clock_of(c); clock->failNano=true;
    Timer *timer=mc_client_graph_bindings(&c->gameplay)->timer; int32_t ticks=-19; c->partial_ticks=0.25f; REQUIRE(!update_frame_timer(c,&ticks)); REQUIRE(c->failed && MCObjectHeap_failed(c->gameplay.heap));
    REQUIRE(!strcmp(c->status,"Source client timer update failed") && ticks==-19 && c->partial_ticks==0.25f); REQUIRE(clock->systemCalls==2 && clock->nanoCalls==2 && timer->lastHRTime==0 && timer->lastSyncSysClock==100);
    MCObjectRootScope_end(&scope); destroy(c,&peer);
}
int main(void) {
    REQUIRE(mc_net_init()); const TimerDependencies *real=mc_client_timer_clocks(); int64_t sys0,sys1,nano0,nano1;
    REQUIRE(real->getSystemTime(NULL,&sys0) && real->nanoTime(NULL,&nano0)); REQUIRE(real->getSystemTime(NULL,&sys1) && real->nanoTime(NULL,&nano1));
    REQUIRE(sys1>=sys0 && nano1>=nano0 && !real->getSystemTime(NULL,NULL) && !real->nanoTime(NULL,NULL));
    capped_and_discarded(); one_shot(0); one_shot(2); one_shot(10); one_shot(-2); adoption_and_lifetime(); failed_clock(); mc_net_shutdown();
    printf("client source Timer runtime: %u checks GREEN\n",checks); return 0;
}
