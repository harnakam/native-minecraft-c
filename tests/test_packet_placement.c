#include "network/play/client/C08PacketPlayerBlockPlacement.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"C08 check %u failed: %s line %d\n",checks,#x,__LINE__); exit(1); } } while(0)
static void handle(void *context,const C08PacketPlayerBlockPlacement *packet) {
    int *count=context; ++*count; CHECK(C08PacketPlayerBlockPlacement_getPlacedBlockDirection(packet)==255);
}
int main(void) {
    C08PacketPlayerBlockPlacement packet,decoded;
    C08PacketPlayerBlockPlacement_init(&packet); C08PacketPlayerBlockPlacement_init(&decoded);
    CHECK(C08PacketPlayerBlockPlacement_getPosition(&packet)==NULL);
    mc_buf blank; mc_buf_init(&blank);
    CHECK(!C08PacketPlayerBlockPlacement_writePacketData(&packet,&blank));
    CHECK(blank.failed && blank.len==0); mc_buf_free(&blank);
    mc_slot stack; mc_slot_init(&stack); CHECK(mc_slot_set(&stack,395,3,7));
    static const uint8_t nbt[]={10,0,0,8,0,1,'n',0,2,'o','k',0};
    mc_buf tag={(uint8_t *)nbt,sizeof nbt,sizeof nbt,0,false}; CHECK(mc_nbt_read(&tag,&stack.nbt));
    CHECK(C08PacketPlayerBlockPlacement_constructUseItem(&packet,&stack));
    CHECK(C08PacketPlayerBlockPlacement_getPosition(&packet)->x==-1 && packet.position.y==-1 && packet.position.z==-1);
    CHECK(packet.stack.nbt.data!=stack.nbt.data && mc_slot_equal(&packet.stack,&stack));
    mc_slot_free(&stack); CHECK(C08PacketPlayerBlockPlacement_getStack(&packet)->count==3);
    mc_buf wire; mc_buf_init(&wire); CHECK(C08PacketPlayerBlockPlacement_writePacketData(&packet,&wire));
    CHECK(wire.len==8+1+5+sizeof nbt+3 && wire.data[8]==255);
    CHECK(C08PacketPlayerBlockPlacement_readPacketData(&decoded,&wire)); CHECK(wire.pos==wire.len);
    CHECK(mc_slot_equal(&packet.stack,&decoded.stack));
    int calls=0; C08PacketPlayerBlockPlacement_processPacket(&decoded,&calls,handle); CHECK(calls==1);
    CHECK(C08PacketPlayerBlockPlacement_construct(&packet,&(C08BlockPos){-33554432,2047,33554431},259,NULL,-0.1f,15.9375f,INFINITY));
    CHECK(!C08PacketPlayerBlockPlacement_getStack(&packet));
    mc_buf_clear(&wire); CHECK(C08PacketPlayerBlockPlacement_writePacketData(&packet,&wire));
    CHECK(wire.data[8]==3 && wire.data[11]==255 && wire.data[12]==255 && wire.data[13]==255);
    CHECK(C08PacketPlayerBlockPlacement_readPacketData(&decoded,&wire));
    CHECK(decoded.position.x==-33554432 && decoded.position.y==2047 && decoded.position.z==33554431);
    CHECK(C08PacketPlayerBlockPlacement_getPlacedBlockOffsetX(&decoded)==15.9375f);
    CHECK(C08PacketPlayerBlockPlacement_getPlacedBlockOffsetY(&decoded)==15.9375f);
    CHECK(C08PacketPlayerBlockPlacement_getPlacedBlockOffsetZ(&decoded)==15.9375f);
    for (size_t length=0;length<wire.len;length++) {
        mc_buf truncated={wire.data,length,length,0,false};
        CHECK(!C08PacketPlayerBlockPlacement_readPacketData(&decoded,&truncated));
        CHECK(decoded.position.x==-33554432 && decoded.placedBlockDirection==3 && !decoded.stack.nbt.size);
    }
    CHECK(C08PacketPlayerBlockPlacement_construct(&packet,&(C08BlockPos){0,-2048,0},255,NULL,NAN,-INFINITY,-256.5f));
    mc_buf_clear(&wire); CHECK(C08PacketPlayerBlockPlacement_writePacketData(&packet,&wire));
    CHECK(wire.data[11]==0 && wire.data[12]==0 && wire.data[13]==248);
    CHECK(C08PacketPlayerBlockPlacement_construct(&packet,NULL,6,NULL,1,2,3));
    CHECK(C08PacketPlayerBlockPlacement_getPosition(&packet)==NULL);
    mc_buf_clear(&wire); CHECK(!C08PacketPlayerBlockPlacement_writePacketData(&packet,&wire)); CHECK(wire.len==0);
    C08PacketPlayerBlockPlacement_free(&packet); C08PacketPlayerBlockPlacement_free(&decoded); mc_buf_free(&wire);
    printf("C08 placement: %u checks passed\n",checks); return 0;
}
