#include "entity/DataWatcher.h"
#include "server/management/ItemInWorldManager.h"
#include "util/MathHelper.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks;if(!(x)) {fprintf(stderr,"Source item manager %u line%d: %s\n",checks,__LINE__,#x);exit(1);} } while(0)
static double from_bits(uint64_t bits) {double value;memcpy(&value,&bits,sizeof value);return value;}
static void floor_contract(void) {
    static const struct {uint64_t bits;int32_t expected;} vectors[]={
        {UINT64_C(0x0000000000000000),0},{UINT64_C(0x8000000000000000),0},
        {UINT64_C(0x0000000000000001),0},{UINT64_C(0x8000000000000001),-1},
        {UINT64_C(0x0010000000000000),0},{UINT64_C(0x8010000000000000),-1},
        {UINT64_C(0x3fdfffffffffffff),0},{UINT64_C(0xbfdfffffffffffff),-1},
        {UINT64_C(0x3fe0000000000000),0},{UINT64_C(0xbfe0000000000000),-1},
        {UINT64_C(0x3fefffffffffffff),0},{UINT64_C(0xbfefffffffffffff),-1},
        {UINT64_C(0x3ff0000000000000),1},{UINT64_C(0xbff0000000000000),-1},
        {UINT64_C(0x3ff0000000000001),1},{UINT64_C(0xbff0000000000001),-2},
        {UINT64_C(0x41dfffffffbfffff),2147483646},{UINT64_C(0x41dfffffffc00000),INT32_MAX},
        {UINT64_C(0x41dfffffffc00001),INT32_MAX},{UINT64_C(0x41e0000000000000),INT32_MAX},
        {UINT64_C(0xc1dfffffffbfffff),-2147483647},{UINT64_C(0xc1dfffffffc00000),-2147483647},
        {UINT64_C(0xc1dfffffffc00001),INT32_MIN},{UINT64_C(0xc1dfffffffffffff),INT32_MIN},
        {UINT64_C(0xc1e0000000000000),INT32_MIN},{UINT64_C(0xc1e0000000000001),INT32_MAX},
        {UINT64_C(0x7fefffffffffffff),INT32_MAX},{UINT64_C(0xffefffffffffffff),INT32_MAX},
        {UINT64_C(0x7ff0000000000000),INT32_MAX},{UINT64_C(0xfff0000000000000),INT32_MAX},
        {UINT64_C(0x7ff8000000000000),0},{UINT64_C(0xfff8000000000000),0},
        {UINT64_C(0x7ff0000000000001),0},{UINT64_C(0xfff0000000000001),0}
    };
    for(size_t i=0;i<sizeof vectors/sizeof *vectors;i++)CHECK(MathHelper_floor_double(from_bits(vectors[i].bits))==vectors[i].expected);
    for(int32_t i=-4096;i<=4096;i++) {
        CHECK(MathHelper_floor_double((double)i)==i);
        CHECK(MathHelper_floor_double((double)i+0.25)==i);
        CHECK(MathHelper_floor_double((double)i-0.25)==i-1);
    }
}
static void position_contract(void) {
    MCObjectHeap *heap=MCObjectHeap_new(65536);CHECK(heap);
    BlockPos *p=DataWatcher_blockPos(heap,1,2,3);CHECK(p);
    CHECK(BlockPos_isInstance((MCObject *)p)&&BlockPos_add(p,0,0,0)==p);
    BlockPos *other=BlockPos_add(p,3,-7,8);CHECK(other&&other!=p);
    CHECK(other->vec3i.x==4&&other->vec3i.y==-5&&other->vec3i.z==11&&p->vec3i.x==1&&p->vec3i.y==2&&p->vec3i.z==3);
    static const struct {int32_t before,delta,after;} wrapped[]={
        {INT32_MAX,1,INT32_MIN},{INT32_MIN,-1,INT32_MAX},{INT32_MIN,INT32_MIN,0},
        {INT32_MAX,INT32_MAX,-2},{-1,INT32_MIN,INT32_MAX},{INT32_MIN,INT32_MAX,-1},
        {0,INT32_MIN,INT32_MIN},{0,INT32_MAX,INT32_MAX}
    };
    for(size_t i=0;i<sizeof wrapped/sizeof *wrapped;i++) {
        p=DataWatcher_blockPos(heap,wrapped[i].before,wrapped[i].before,wrapped[i].before);CHECK(p);
        other=BlockPos_add(p,wrapped[i].delta,wrapped[i].delta,wrapped[i].delta);CHECK(other&&other!=p);
        CHECK(other->vec3i.x==wrapped[i].after&&other->vec3i.y==wrapped[i].after&&other->vec3i.z==wrapped[i].after);
    }
    BlockPos *origin=NativeBlockPos_origin(heap);CHECK(origin&&origin->vec3i.x==0&&origin->vec3i.y==0&&origin->vec3i.z==0);
    size_t before=MCObjectHeap_liveObjects(heap);
    for(unsigned i=0;i<1000;i++)CHECK(NativeBlockPos_origin(heap)==origin);
    CHECK(MCObjectHeap_liveObjects(heap)==before&&MCObjectHeap_collect(heap));
    CHECK(MCObjectHeap_liveObjects(heap)==2&&NativeBlockPos_origin(heap)==origin);
    CHECK(!MCObjectHeap_failed(heap)&&!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
}
static void manager_contract(void) {
    MCObjectHeap *heap=MCObjectHeap_new(65536);CHECK(heap);
    ItemInWorldManager *self=ItemInWorldManager_new(heap,NULL);CHECK(self);
    CHECK(ItemInWorldManager_getGameType(self)==&WorldSettingsGameType_NOT_SET);
    CHECK(self->field_180240_f==self->field_180241_i&&self->durabilityRemainingOnBlock==-1&&!self->theWorld);
    CHECK(!self->thisPlayerMP&&!self->isDestroyingBlock&&!self->initialDamage&&!self->curblockDamage&&
        !self->receivedFinishDiggingPacket&&!self->initialBlockDamage);
    CHECK(!ItemInWorldManager_isCreative(self)&&!ItemInWorldManager_survivalOrAdventure(self));
    const WorldSettingsGameType *modes[]={&WorldSettingsGameType_NOT_SET,&WorldSettingsGameType_SURVIVAL,
        &WorldSettingsGameType_CREATIVE,&WorldSettingsGameType_ADVENTURE,&WorldSettingsGameType_SPECTATOR};
    for(unsigned i=0;i<5;i++) {
        self->gameType=modes[i];CHECK(ItemInWorldManager_getGameType(self)==modes[i]);
        CHECK(ItemInWorldManager_isCreative(self)==(i==2));
        CHECK(ItemInWorldManager_survivalOrAdventure(self)==(i==1||i==3));
    }
    MCObjectHeap_free(heap);
}
typedef struct {MCObject object;MCObject *peer;} Reference;
static void reference_trace(MCObject *object,MCObjectVisitor visit,void *context) {
    Reference *reference=(Reference *)object;reference->peer=visit(reference->peer,context);
}
/* Required unported World/EntityPlayerMP identities for a constructor which
   stores references only. These fixtures provide no fake World/player methods. */
static const MCObjectClass worldClass={"fixture.ItemManager.World",MCObjectHeap_plainClone,reference_trace,NULL};
static const MCObjectClass playerClass={"fixture.ItemManager.EntityPlayerMP",MCObjectHeap_plainClone,reference_trace,NULL};
static const MCObjectClass paddingClass={"fixture.ItemManager.capacity",MCObjectHeap_plainClone,NULL,NULL};
static Reference *reference(MCObjectHeap *heap,const MCObjectClass *type) {
    Reference *result=(Reference *)MCObjectHeap_alloc(heap,sizeof(Reference),type);CHECK(result);return result;
}
static void fields_and_lifetime(void) {
    MCObjectHeap *heap=MCObjectHeap_new(65536);CHECK(heap);
    Reference *world=reference(heap,&worldClass),*player=reference(heap,&playerClass);
    ItemInWorldManager *self=ItemInWorldManager_nativeAllocate(heap);CHECK(self);
    BlockPos *old=DataWatcher_blockPos(heap,1,2,3);CHECK(old);
    self->theWorld=(MCObject *)world;self->thisPlayerMP=(EntityPlayerMP *)player;
    self->gameType=&WorldSettingsGameType_CREATIVE;self->field_180240_f=old;self->field_180241_i=old;
    self->isDestroyingBlock=true;self->initialDamage=17;self->curblockDamage=-6;
    self->receivedFinishDiggingPacket=true;self->initialBlockDamage=42;self->durabilityRemainingOnBlock=9;
    CHECK(ItemInWorldManager_construct(self,NULL));
    CHECK(!self->theWorld&&self->thisPlayerMP==(EntityPlayerMP *)player&&self->isDestroyingBlock&&
        self->initialDamage==17&&self->curblockDamage==-6&&self->receivedFinishDiggingPacket&&self->initialBlockDamage==42);
    CHECK(self->gameType==&WorldSettingsGameType_NOT_SET&&self->field_180240_f!=old&&
        self->field_180240_f==self->field_180241_i&&self->durabilityRemainingOnBlock==-1);
    CHECK(ItemInWorldManager_construct(self,(MCObject *)world));world->peer=(MCObject *)self;player->peer=(MCObject *)self;
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,heap,(MCObject *)self));
    CHECK(MCObjectHeap_collect(heap)&&self->theWorld==(MCObject *)world&&self->thisPlayerMP==(EntityPlayerMP *)player);
    MCObjectHeap *copy=MCObjectHeap_clone(heap);CHECK(copy);MCObjectRoot branch={0};CHECK(MCObjectRoot_rebind(&branch,copy,&root));
    ItemInWorldManager *other=(ItemInWorldManager *)MCObjectRoot_get(&branch);CHECK(other&&other!=self);
    CHECK(other->theWorld!=self->theWorld&&other->thisPlayerMP!=self->thisPlayerMP&&other->field_180240_f!=self->field_180240_f);
    CHECK(((Reference *)other->theWorld)->peer==(MCObject *)other&&((Reference *)other->thisPlayerMP)->peer==(MCObject *)other);
    CHECK(other->field_180240_f==other->field_180241_i&&NativeBlockPos_origin(copy)==other->field_180240_f);
    CHECK(other->gameType==&WorldSettingsGameType_NOT_SET&&other->durabilityRemainingOnBlock==-1);
    CHECK(MCObjectHeap_collect(copy)&&MCObjectHeap_adopt(heap,copy));MCObjectHeap_free(copy);
    self=(ItemInWorldManager *)MCObjectRoot_get(&root);
    CHECK(self==other&&self->object.heap==heap&&self->theWorld->heap==heap);
    CHECK(self->field_180240_f==self->field_180241_i&&NativeBlockPos_origin(heap)==self->field_180240_f);
    CHECK(((Reference *)self->theWorld)->peer==(MCObject *)self&&((Reference *)self->thisPlayerMP)->peer==(MCObject *)self);
    CHECK(MCObjectHeap_collect(heap)&&!MCObjectHeap_failed(heap));
    MCObjectRoot_drop(&root);CHECK(MCObjectHeap_collect(heap)&&MCObjectHeap_liveObjects(heap)==2);
    MCObjectHeap_free(heap);
}
static void malformed_native_boundaries(void) {
    for(unsigned mode=0;mode<4;mode++) {
        MCObjectHeap *heap=MCObjectHeap_new(65536),*foreign=MCObjectHeap_new(65536);CHECK(heap&&foreign);
        ItemInWorldManager *self=ItemInWorldManager_nativeAllocate(heap);CHECK(self);
        Reference *world=reference(foreign,&worldClass);
        if(mode==0) {
            CHECK(!ItemInWorldManager_construct(self,(MCObject *)world));CHECK(!self->gameType&&!self->theWorld);
        } else if(mode==1) {
            MCObject *shortObject=MCObjectHeap_alloc(heap,sizeof(MCObject),self->object.klass);CHECK(shortObject);
            CHECK(!ItemInWorldManager_isInstance(shortObject)&&!ItemInWorldManager_construct((ItemInWorldManager *)shortObject,NULL));
        } else if(mode==2) {
            Reference *wrong=reference(heap,&worldClass);CHECK(!ItemInWorldManager_isInstance((MCObject *)wrong));
            CHECK(!ItemInWorldManager_getGameType((ItemInWorldManager *)wrong));
        } else {
            BlockPos *valid=DataWatcher_blockPos(heap,0,0,0);CHECK(valid);
            MCObject *shortObject=MCObjectHeap_alloc(heap,sizeof(MCObject),valid->vec3i.object.klass);CHECK(shortObject);
            CHECK(!BlockPos_isInstance(shortObject)&&!BlockPos_add((BlockPos *)shortObject,0,0,0));
        }
        CHECK(MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(foreign)&&!MCObjectHeap_hasBorrowers(heap));
        MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
    }
    for(unsigned method=0;method<2;method++) {
        MCObjectHeap *heap=MCObjectHeap_new(65536);CHECK(heap);ItemInWorldManager *self=ItemInWorldManager_nativeAllocate(heap);CHECK(self);
        CHECK(!ItemInWorldManager_getGameType(self)&&!MCObjectHeap_failed(heap));
        CHECK(!(method?ItemInWorldManager_isCreative(self):ItemInWorldManager_survivalOrAdventure(self)));
        CHECK(MCObjectHeap_failed(heap)&&!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap=MCObjectHeap_new(65536);CHECK(heap);BlockPos *origin=NativeBlockPos_origin(heap);CHECK(origin);
    origin->vec3i.x=1; /* Immutable Source coordinate contract violation. */
    CHECK(!NativeBlockPos_origin(heap)&&MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
    CHECK(!ItemInWorldManager_new(NULL,NULL)&&!ItemInWorldManager_getGameType(NULL)&&!BlockPos_add(NULL,0,0,0));
}
static void capacity_and_failure_prefix(void) {
    const size_t budget=65536;
    for(size_t remaining=0;remaining<=96;remaining+=8) {
        MCObjectHeap *heap=MCObjectHeap_new(budget);CHECK(heap);
        Reference *world=reference(heap,&worldClass);
        ItemInWorldManager *self=ItemInWorldManager_nativeAllocate(heap);CHECK(self);
        self->theWorld=(MCObject *)world;self->gameType=&WorldSettingsGameType_CREATIVE;
        self->durabilityRemainingOnBlock=7;
        CHECK(MCObjectHeap_alloc(heap,budget-MCObjectHeap_liveBytes(heap)-remaining,&paddingClass));
        bool result=ItemInWorldManager_construct(self,NULL);
        CHECK(self->gameType==&WorldSettingsGameType_NOT_SET);
        if(result)CHECK(!self->theWorld&&self->field_180240_f==self->field_180241_i&&self->durabilityRemainingOnBlock==-1);
        else CHECK(self->theWorld==(MCObject *)world&&!self->field_180240_f&&!self->field_180241_i&&self->durabilityRemainingOnBlock==7);
        CHECK(MCObjectHeap_failed(heap)==!result&&!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap=MCObjectHeap_new(budget);CHECK(heap);BlockPos *p=DataWatcher_blockPos(heap,1,2,3);CHECK(p);
    CHECK(MCObjectHeap_alloc(heap,budget-MCObjectHeap_liveBytes(heap),&paddingClass));
    CHECK(BlockPos_add(p,0,0,0)==p&&!MCObjectHeap_failed(heap));
    CHECK(!BlockPos_add(p,1,0,0)&&MCObjectHeap_failed(heap)&&p->vec3i.x==1&&p->vec3i.y==2&&p->vec3i.z==3);
    CHECK(!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(budget);CHECK(heap);CHECK(NativeBlockPos_origin(heap));
    ItemInWorldManager *self=ItemInWorldManager_nativeAllocate(heap);CHECK(self);
    CHECK(MCObjectHeap_alloc(heap,budget-MCObjectHeap_liveBytes(heap),&paddingClass));
    CHECK(ItemInWorldManager_construct(self,NULL)&&self->durabilityRemainingOnBlock==-1&&!MCObjectHeap_failed(heap));
    MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(sizeof(MCObject));CHECK(heap);CHECK(!ItemInWorldManager_new(heap,NULL));
    CHECK(MCObjectHeap_failed(heap)&&!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
}
int main(int argc,char **argv) {
    if(argc<2||!strcmp(argv[1],"floor"))floor_contract();
    if(argc<2||!strcmp(argv[1],"position"))position_contract();
    if(argc<2||!strcmp(argv[1],"manager"))manager_contract();
    if(argc<2){fields_and_lifetime();malformed_native_boundaries();capacity_and_failure_prefix();}
    printf("Source item manager: %u checks passed\n",checks);return 0;
}
