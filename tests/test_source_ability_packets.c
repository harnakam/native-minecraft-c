#include "network/play/client/C13PacketPlayerAbilities.h"
#include "network/play/server/S39PacketPlayerAbilities.h"
#include "network/GameplayPacketRouter.h"
#include "client/native_runtime.h"
#include "server/native_gameplay.h"
#include "util/MCGameplayPackets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"ability packet check %u line%d: %s\n",checks,__LINE__,#x);exit(1);}} while(0)
static float from_bits(uint32_t x) {float f;memcpy(&f,&x,4);return f;}
static uint32_t bits(float f) {uint32_t x;memcpy(&x,&f,4);return x;}
static void wire_bodies(void) {
    static const uint32_t speeds[]={0,0x80000000,0x00000001,0x3d4ccccd,0x3dcccccd,0x7f800000,0xff800000,0x7fc12345,0xbf800000};
    MCObjectHeap *heap=MCObjectHeap_new(8*1024*1024);CHECK(heap);
    for (unsigned flags=0;flags<256;flags++) {
        mc_buf wire;mc_buf_init(&wire);mc_put_u8(&wire,(uint8_t)flags);
        uint32_t fly=speeds[flags%9],walk=speeds[(flags+4)%9];
        mc_put_f32(&wire,from_bits(fly));mc_put_f32(&wire,from_bits(walk));
        PacketBuffer io;CHECK(PacketBuffer_init(&io,heap,&wire));
        C13PacketPlayerAbilities *c=C13PacketPlayerAbilities_new_empty(heap);CHECK(c);
        CHECK(C13PacketPlayerAbilities_readPacketData(c,&io)&&wire.pos==9);
        CHECK(c->invulnerable==((flags&1)!=0)&&c->flying==((flags&2)!=0)&&c->allowFlying==((flags&4)!=0)&&c->creativeMode==((flags&8)!=0));
        CHECK(bits(c->flySpeed)==fly&&bits(c->walkSpeed)==walk);
        wire.pos=0;S39PacketPlayerAbilities *s=S39PacketPlayerAbilities_new_empty(heap);CHECK(s);
        CHECK(S39PacketPlayerAbilities_readPacketData(s,&io)&&wire.pos==9);
        CHECK(s->invulnerable==c->invulnerable&&s->flying==c->flying&&s->allowFlying==c->allowFlying&&s->creativeMode==c->creativeMode);
        CHECK(bits(S39PacketPlayerAbilities_getFlySpeed(s))==fly&&bits(S39PacketPlayerAbilities_getWalkSpeed(s))==walk);
        mc_buf encoded;mc_buf_init(&encoded);PacketBuffer out;CHECK(PacketBuffer_init(&out,heap,&encoded));
        CHECK(C13PacketPlayerAbilities_writePacketData(c,&out));CHECK(encoded.len==9&&encoded.data[0]==(flags&15)&&memcmp(encoded.data+1,wire.data+1,8)==0);
        mc_buf_free(&encoded);CHECK(PacketBuffer_init(&out,heap,&encoded));
        CHECK(S39PacketPlayerAbilities_writePacketData(s,&out));CHECK(encoded.len==9&&encoded.data[0]==(flags&15)&&memcmp(encoded.data+1,wire.data+1,8)==0);
        mc_buf_free(&encoded);mc_buf_free(&wire);
    }
    PlayerCapabilities *cap=PlayerCapabilities_new(heap);CHECK(cap);
    cap->isFlying=true;cap->allowEdit=false;cap->flySpeed=from_bits(0x80000000);cap->walkSpeed=from_bits(0x7fc12345);
    C13PacketPlayerAbilities *c=C13PacketPlayerAbilities_new(heap,cap);S39PacketPlayerAbilities *s=S39PacketPlayerAbilities_new(heap,cap);CHECK(c&&s);
    cap->isFlying=false;cap->flySpeed=1;
    CHECK(c->flying&&s->flying&&bits(c->flySpeed)==0x80000000&&bits(s->walkSpeed)==0x7fc12345);
    CHECK(!MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
}
static void truncated_prefixes(void) {
    const uint8_t wire[]={5,0x80,0,0,0,0x7f,0xc1,0x23,0x45};
    for (size_t n=0;n<9;n++) for (unsigned side=0;side<2;side++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024);CHECK(h);
        mc_buf b={(uint8_t *)wire,n,n,0,false};PacketBuffer io;CHECK(PacketBuffer_init(&io,h,&b));
        if (side==0) {
            C13PacketPlayerAbilities *p=C13PacketPlayerAbilities_new_empty(h);CHECK(p);
            p->flying=p->creativeMode=true;p->flySpeed=2;p->walkSpeed=3;
            CHECK(!C13PacketPlayerAbilities_readPacketData(p,&io)&&b.failed);
            CHECK(p->invulnerable==(n>=1)&&p->allowFlying==(n>=1)&&p->flying==(n==0)&&p->creativeMode==(n==0));
            CHECK(bits(p->flySpeed)==(n>=5?0x80000000:0x40000000)&&p->walkSpeed==3);
        } else {
            S39PacketPlayerAbilities *p=S39PacketPlayerAbilities_new_empty(h);CHECK(p);
            p->flying=p->creativeMode=true;p->flySpeed=2;p->walkSpeed=3;
            CHECK(!S39PacketPlayerAbilities_readPacketData(p,&io)&&b.failed);
            CHECK(p->invulnerable==(n>=1)&&p->allowFlying==(n>=1)&&p->flying==(n==0)&&p->creativeMode==(n==0));
            CHECK(bits(p->flySpeed)==(n>=5?0x80000000:0x40000000)&&p->walkSpeed==3);
        }
        CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
    }
}
static void live_owner_and_router(void) {
    mc_world terrain;mc_world_init(&terrain,0);MCGameplay game={0};CHECK(mc_client_graph_init(&game,&terrain,"Abilities"));
    MCGameplayPlayer *player=mc_client_graph_player(&game);CHECK(player);
    CHECK(player->capabilities&&PlayerCapabilities_isInstance((MCObject *)player->capabilities));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,game.heap));
    MCClientBindings *binding=mc_client_graph_bindings(&game);CHECK(binding);
    CHECK(PlayerControllerMP_setGameType(binding->controller,&WorldSettingsGameType_CREATIVE));
    CHECK(player->capabilities->isCreativeMode&&player->capabilities->allowFlying&&!player->capabilities->isFlying&&player->capabilities->allowEdit);
    MCObjectRootScope_end(&scope);
    MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&game,&tx));
    uint8_t wire[]={2,0x80,0,0,0,0x7f,0xc1,0x23,0x45};mc_buf in={wire,9,9,0,false};
    CHECK(GameplayPacketRouter_client(&tx.working,0,0x39,&in)==MC_GAMEPLAY_PACKET_APPLIED);
    player=mc_client_graph_player(&tx.working);binding=mc_client_graph_bindings(&tx.working);
    CHECK(player->capabilities->isFlying&&!player->capabilities->allowFlying&&!player->capabilities->isCreativeMode&&!player->capabilities->disableDamage&&player->capabilities->allowEdit);
    CHECK(bits(player->capabilities->flySpeed)==0x80000000&&bits(player->capabilities->walkSpeed)==0x7fc12345);
    CHECK(binding->controller->currentGameType==&WorldSettingsGameType_CREATIVE);
    char error[128];CHECK(MCGameplay_acceptClientFrame(&tx,MCGameplayClientPackets_validateFrame,NULL,error,sizeof error));
    player=mc_client_graph_player(&game);CHECK(!MCGameplayPlayer_isCreativeMode((MCObject *)player));
    CHECK(MCObjectHeap_collect(game.heap));player=mc_client_graph_player(&game);CHECK(player->capabilities->isFlying);
    CHECK(MCGameplay_free(&game));mc_world_free(&terrain);
}
static MCGameplayPlayer *replace_controller_type(MCObject *context,MCObject *mc) {
    CHECK(context==mc);MCClientBindings *binding=(MCClientBindings *)context;
    binding->controller->currentGameType=&WorldSettingsGameType_SURVIVAL;
    return binding->player;
}
static void controller_receiver_order(void) {
    mc_world terrain;mc_world_init(&terrain,0);MCGameplay g={0};CHECK(mc_client_graph_init(&g,&terrain,"ReceiverOrder"));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,g.heap));
    MCClientBindings *b=mc_client_graph_bindings(&g);
    const PlayerControllerMPActionsDependencies *original=b->controller->actionsDependencies;
    PlayerControllerMPActionsDependencies changed=*original;changed.getPlayer=replace_controller_type;
    b->controller->actionsDependencies=&changed;
    CHECK(PlayerControllerMP_setGameType(b->controller,&WorldSettingsGameType_CREATIVE));
    CHECK(b->controller->currentGameType==&WorldSettingsGameType_SURVIVAL);
    CHECK(b->player->capabilities->isCreativeMode&&b->player->capabilities->allowFlying&&b->player->capabilities->disableDamage);
    b->controller->actionsDependencies=original;
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_free(&g));mc_world_free(&terrain);
}
static void server_authority(void) {
    mc_world terrain;mc_world_init(&terrain,0);MCGameplay g={0};CHECK(mc_server_graph_init(&g,&terrain,0,0,0));
    MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&g,&tx));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
    CHECK(mc_server_graph_add_player_auto(&tx.working,0,"00000000-0000-0000-0000-000000000001","AbilityServer",0,20,0,true));
    MCGameplayPlayer *p=mc_server_graph_player(&tx.working,0);CHECK(p);
    PlayerCapabilities *caps=p->capabilities;CHECK(caps&&caps->isCreativeMode&&!caps->isFlying);
    for (unsigned allow=0;allow<2;allow++) for (unsigned flags=0;flags<256;flags++) {
        caps->allowFlying=allow!=0;caps->isFlying=false;caps->disableDamage=true;caps->isCreativeMode=true;caps->allowEdit=false;caps->flySpeed=0.25f;caps->walkSpeed=-2;
        uint8_t wire[]={(uint8_t)flags,0x7f,0xc1,0x23,0x45,0xff,0x80,0,0};mc_buf in={wire,9,9,0,false};
        CHECK(GameplayPacketRouter_server(&tx.working,0,0x13,&in)==MC_GAMEPLAY_PACKET_APPLIED);
        CHECK(caps->isFlying==(allow!=0&&(flags&2)!=0));
        CHECK(caps->allowFlying==(allow!=0)&&caps->disableDamage&&caps->isCreativeMode&&!caps->allowEdit&&caps->flySpeed==0.25f&&caps->walkSpeed==-2);
    }
    S39PacketPlayerAbilities *snapshot=S39PacketPlayerAbilities_new(tx.working.heap,caps);CHECK(snapshot);
    CHECK(MCGameplayPackets_sendAbilities(p,snapshot));MCGameplayPacketKind kind;
    CHECK(MCGameplayPackets_packetAt(p,0,&kind)==(MCObject *)snapshot&&kind==MC_GAMEPLAY_PACKET_ABILITIES);
    mc_buf encoded;mc_buf_init(&encoded);CHECK(MCGameplayPackets_encodeAt(p,0,&encoded));
    CHECK(encoded.len==10&&encoded.data[0]==0x39&&encoded.data[1]==15);mc_buf_free(&encoded);
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));CHECK(MCGameplay_free(&g));mc_world_free(&terrain);
}
static MCGameplayPlayer *foreignPlayer;
static MCPacketThreadResult foreign_after_thread(MCObject *context,NetHandlerPlayServer *handler,MCObject *packet) {
    CHECK(context&&packet&&packet->heap==handler->object.heap);
    handler->playerEntity=foreignPlayer;return MC_PACKET_THREAD_EXECUTE;
}
static void foreign_player_after_thread_is_rejected(void) {
    mc_world terrain;mc_world_init(&terrain,0);MCGameplay server={0},external={0};
    CHECK(mc_server_graph_init(&server,&terrain,0,0,0));CHECK(mc_client_graph_init(&external,&terrain,"ForeignAbility"));
    foreignPlayer=mc_client_graph_player(&external);foreignPlayer->capabilities->allowFlying=true;
    MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&server,&tx));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
    CHECK(mc_server_graph_add_player_auto(&tx.working,0,"00000000-0000-0000-0000-000000000001","OwnerGuard",0,20,0,true));
    NetHandlerPlayServer *handler=(NetHandlerPlayServer *)mc_server_graph_player(&tx.working,0)->handler;
    NetHandlerPlayServerDependencies replacement=*handler->dependencies;replacement.checkThreadAndEnqueue=foreign_after_thread;handler->dependencies=&replacement;
    C13PacketPlayerAbilities *packet=C13PacketPlayerAbilities_new_empty(tx.working.heap);CHECK(packet);packet->flying=true;
    CHECK(!NetHandlerPlayServer_processPlayerAbilities(handler,packet)&&MCObjectHeap_failed(tx.working.heap));
    CHECK(!foreignPlayer->capabilities->isFlying&&!MCObjectHeap_failed(external.heap));
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));
    CHECK(MCGameplay_free(&server)&&MCGameplay_free(&external));foreignPlayer=NULL;mc_world_free(&terrain);
}
static void malformed_transactions(void) {
    mc_world terrain;mc_world_init(&terrain,0);MCGameplay g={0};CHECK(mc_client_graph_init(&g,&terrain,"RejectAbility"));
    PlayerCapabilities *before=mc_client_graph_player(&g)->capabilities;
    for (size_t length=0;length<=10;length++) {
        if (length==9) continue;
        MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&g,&tx));
        uint8_t bytes[10]={15,0x3e,0x80,0,0,0x3f,0x40,0,0,0};mc_buf in={bytes,length,length,0,false};
        CHECK(GameplayPacketRouter_client(&tx.working,0,0x39,&in)==MC_GAMEPLAY_PACKET_REJECTED);
        CHECK(MCGameplay_abort(&tx));CHECK(mc_client_graph_player(&g)->capabilities==before);
        CHECK(!before->isFlying&&!before->isCreativeMode&&before->allowEdit&&bits(before->flySpeed)==0x3d4ccccd);
    }
    MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&g,&tx));
    mc_client_graph_player(&tx.working)->capabilities=NULL;
    uint8_t bytes[9]={15};mc_buf in={bytes,9,9,0,false};
    CHECK(GameplayPacketRouter_client(&tx.working,0,0x39,&in)==MC_GAMEPLAY_PACKET_FAILED&&MCObjectHeap_failed(tx.working.heap));
    CHECK(MCGameplay_abort(&tx)&&!MCObjectHeap_failed(g.heap));CHECK(mc_client_graph_player(&g)->capabilities==before);
    CHECK(MCGameplay_free(&g));mc_world_free(&terrain);
}
int main(void) {wire_bodies();truncated_prefixes();live_owner_and_router();controller_receiver_order();server_authority();foreign_player_after_thread_is_rejected();malformed_transactions();printf("source ability packets: %u checks\n",checks);return 0;}
