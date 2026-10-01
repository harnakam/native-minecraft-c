#include "entity/player/EntityPlayer.h"
#include "network/PacketBuffer.h"
#include "nbt/NBTInternal.h"
#include "util/MCGameplayPlayer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(condition) do { ++checks;if(!(condition)) {fprintf(stderr,"Player profile check %u line %d: %s\n",checks,__LINE__,#condition);exit(1);} } while(0)
static void identity_and_offline(void) {
    MCObjectHeap *heap=MCObjectHeap_new(8*1024*1024);CHECK(heap);
    NativeJavaUUID *fixed=NativeJavaUUID_new(heap,INT64_C(0x0123456789abcdef),-INT64_C(0x0123456789abcdf0));CHECK(fixed);
    NBTString *name=NBTString_fromASCII(heap,"Notch");CHECK(name);
    NativeGameProfile *profile=NativeGameProfile_new(heap,fixed,name);CHECK(profile);
    CHECK(EntityPlayer_getUUID(profile)==fixed);
    NativeGameProfile *offline=NativeGameProfile_new(heap,NULL,name);CHECK(offline);
    NativeJavaUUID *uuid=EntityPlayer_getUUID(offline);CHECK(uuid);
    CHECK((uint64_t)uuid->mostSignificantBits==UINT64_C(0xb50ad385829d3141));
    CHECK((uint64_t)uuid->leastSignificantBits==UINT64_C(0xa2167e7d7539ba7f));
    CHECK(uuid!=EntityPlayer_getUUID(offline));
    CHECK(NativeGameProfile_getName(profile)==name&&NativeGameProfile_getId(profile)==fixed);
    CHECK(!MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
}
/* Java concatenation treats null as four units, while the retained legacy
   CString API has its original empty-name contract. NUL is hashed, not a terminator. */
static void null_and_utf16_names(void) {
    MCObjectHeap *heap=MCObjectHeap_new(16*1024*1024);CHECK(heap);
    NativeJavaUUID *nullName=EntityPlayer_getOfflineUUID(heap,NULL);CHECK(nullName);
    NBTString *literal=NBTString_fromASCII(heap,"null");CHECK(literal);
    NativeJavaUUID *literalName=EntityPlayer_getOfflineUUID(heap,literal);CHECK(literalName);
    CHECK(nullName!=literalName&&nullName->mostSignificantBits==literalName->mostSignificantBits&&
        nullName->leastSignificantBits==literalName->leastSignificantBits);
    uint8_t legacy[16];mc_offline_uuid(NULL,legacy);
    NativeJavaUUID *empty=EntityPlayer_getOfflineUUID(heap,NBTString_fromASCII(heap,""));CHECK(empty);
    uint64_t most=0,least=0;for(unsigned i=0;i<8;i++)most=(most<<8)|legacy[i];
    for(unsigned i=8;i<16;i++)least=(least<<8)|legacy[i];
    CHECK((uint64_t)empty->mostSignificantBits==most&&(uint64_t)empty->leastSignificantBits==least);
    CHECK(empty->mostSignificantBits!=nullName->mostSignificantBits);
    NativeGameProfile *changed=NativeGameProfile_new(heap,NULL,literal);CHECK(changed);
    changed->name=NULL;MCObjectHeap_touch(heap);
    NativeJavaUUID *changedId=EntityPlayer_getUUID(changed);CHECK(changedId);
    CHECK(changedId->mostSignificantBits==nullName->mostSignificantBits&&changedId->leastSignificantBits==nullName->leastSignificantBits);
    uint16_t units[]={'a',0,'b',0xd800,'x',0xdc00,0xd83d,0xde00,0x65e5};
    NBTString *name=NBTString_fromUTF16(heap,units,sizeof(units)/sizeof(*units));CHECK(name);
    static const uint8_t expected[]={'a',0,'b','?','x','?',0xf0,0x9f,0x98,0x80,0xe6,0x97,0xa5};
    NBTByteArrayStorage *bytes=PacketBuffer_nativeEncodeUTF8(heap,name);CHECK(bytes);
    CHECK(NBTByteArrayStorage_length(bytes)==(int32_t)sizeof(expected));
    CHECK(!memcmp(NBTByteArrayStorage_data(bytes),expected,sizeof(expected)));
    NativeJavaUUID *embedded=EntityPlayer_getOfflineUUID(heap,name),*shortName=EntityPlayer_getOfflineUUID(heap,NBTString_fromASCII(heap,"a"));CHECK(embedded&&shortName);
    CHECK(embedded->mostSignificantBits!=shortName->mostSignificantBits);
    mc_buf buffer;mc_buf_init(&buffer);PacketBuffer view;CHECK(PacketBuffer_init(&view,heap,&buffer));
    CHECK(PacketBuffer_writeString(&view,name));CHECK(mc_get_varint(&buffer)==(int32_t)sizeof(expected));
    CHECK(buffer.len-buffer.pos==sizeof(expected)&&!memcmp(buffer.data+buffer.pos,expected,sizeof(expected)));
    mc_buf_free(&buffer);CHECK(!MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
}
/* These exact target JDK8 blank chars differ from modern Unicode tables and
   from trim/ASCII-space approximations. The profile keeps the original refs. */
static void profile_nullable_and_blank(void) {
    static const uint16_t blank[]={9,10,11,12,13,0x1c,0x1d,0x1e,0x1f,0x20,0x1680,0x180e,
        0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2008,0x2009,0x200a,0x2028,0x2029,0x205f,0x3000};
    for(size_t i=0;i<sizeof(blank)/sizeof(*blank);i++) {
        MCObjectHeap *heap=MCObjectHeap_new(4096);CHECK(heap);
        NBTString *name=NBTString_fromUTF16(heap,&blank[i],1);CHECK(name);
        CHECK(!NativeGameProfile_new(heap,NULL,name)&&MCObjectHeap_failed(heap));
        CHECK(!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    }
    MCObjectHeap *heap=MCObjectHeap_new(16384);CHECK(heap);
    NativeJavaUUID *fixed=NativeJavaUUID_new(heap,5,9);CHECK(fixed);
    NativeGameProfile *nullable=NativeGameProfile_new(heap,fixed,NULL);CHECK(nullable);
    CHECK(NativeGameProfile_getName(nullable)==NULL&&!MCObjectHeap_failed(heap));
    CHECK(EntityPlayer_getUUID(nullable)==fixed);
    static const uint16_t nonblank[]={0,0x00a0,0x2007,0x202f,0xd800};
    for(size_t i=0;i<sizeof(nonblank)/sizeof(*nonblank);i++) {
        NBTString *name=NBTString_fromUTF16(heap,&nonblank[i],1);CHECK(name);
        NativeGameProfile *profile=NativeGameProfile_new(heap,NULL,name);CHECK(profile);
        CHECK(NativeGameProfile_getName(profile)==name&&NativeGameProfile_getId(profile)==NULL);
    }
    NativeGameProfile *empty=NativeGameProfile_new(heap,fixed,NBTString_fromASCII(heap,""));CHECK(empty);
    CHECK(EntityPlayer_getUUID(empty)==fixed&&!MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(4096);CHECK(heap);CHECK(!NativeGameProfile_new(heap,NULL,NULL));
    CHECK(MCObjectHeap_failed(heap)&&!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
}
/* The shared dependency must not acquire PacketBuffer's 32767-byte or native
   wire 2MiB limit: these names are only input to the source UUID operation. */
static void unbounded_charset_and_packet_limit(void) {
    MCObjectHeap *heap=MCObjectHeap_new(32*1024*1024);CHECK(heap);size_t length=700000;
    uint16_t *units=(uint16_t *)malloc(length*sizeof(*units));CHECK(units);
    for(size_t i=0;i<length;i++)units[i]=0x0800;
    NBTString *name=NBTString_fromUTF16(heap,units,length);free(units);CHECK(name);
    NBTByteArrayStorage *bytes=PacketBuffer_nativeEncodeUTF8(heap,name);CHECK(bytes);
    CHECK(NBTByteArrayStorage_length(bytes)==2100000);
    const uint8_t *data=(const uint8_t *)NBTByteArrayStorage_data(bytes);
    CHECK(data[0]==0xe0&&data[1]==0xa0&&data[2]==0x80&&data[2099999]==0x80);
    CHECK(EntityPlayer_getOfflineUUID(heap,name));CHECK(!MCObjectHeap_failed(heap));
    mc_buf buffer;mc_buf_init(&buffer);PacketBuffer view;CHECK(PacketBuffer_init(&view,heap,&buffer));
    CHECK(!PacketBuffer_writeString(&view,name)&&buffer.failed&&buffer.len==0);
    CHECK(!MCObjectHeap_failed(heap));mc_buf_free(&buffer);MCObjectHeap_free(heap);
}
static void raw_name_uuid(void) {
    static const uint8_t empty[]={0xd4,0x1d,0x8c,0xd9,0x8f,0x00,0x32,0x04,0xa9,0x80,0x09,0x98,0xec,0xf8,0x42,0x7e};
    uint8_t uuid[16];CHECK(mc_name_uuid_from_bytes(NULL,0,uuid));CHECK(!memcmp(uuid,empty,16));
    memset(uuid,0xa5,16);CHECK(!mc_name_uuid_from_bytes(NULL,1,uuid));
    for(size_t i=0;i<16;i++)CHECK(uuid[i]==0xa5);
    CHECK(!mc_name_uuid_from_bytes((const uint8_t *)"x",1,NULL));
    uint8_t overlap[16]={'a',0,'b'},separate[16];CHECK(mc_name_uuid_from_bytes(overlap,3,separate));
    CHECK(mc_name_uuid_from_bytes(overlap,3,overlap));CHECK(!memcmp(overlap,separate,16));
    CHECK((overlap[6]&0xf0)==0x30&&(overlap[8]&0xc0)==0x80);
}
/* Two owners, UUID and name roots retain aliases across transaction clone and
   adoption; the source supplied-ID path does not allocate a UUID occurrence. */
static void graph_aliases_and_abort(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    NativeJavaUUID *id=NativeJavaUUID_new(heap,13,17);NBTString *name=NBTString_fromASCII(heap,"Alice");CHECK(id&&name);
    NativeGameProfile *a=NativeGameProfile_new(heap,id,name),*b=NativeGameProfile_new(heap,id,name);CHECK(a&&b);
    MCObjectRoot ar={0},br={0},ir={0},nr={0};CHECK(MCObjectRoot_init(&ar,heap,(MCObject *)a));
    CHECK(MCObjectRoot_init(&br,heap,(MCObject *)b));CHECK(MCObjectRoot_init(&ir,heap,(MCObject *)id));CHECK(MCObjectRoot_init(&nr,heap,(MCObject *)name));
    CHECK(MCObjectHeap_collect(heap));MCObjectHeap *working=MCObjectHeap_clone(heap);CHECK(working);
    MCObjectRoot wa={0},wb={0},wi={0},wn={0};CHECK(MCObjectRoot_rebind(&wa,working,&ar));CHECK(MCObjectRoot_rebind(&wb,working,&br));
    CHECK(MCObjectRoot_rebind(&wi,working,&ir));CHECK(MCObjectRoot_rebind(&wn,working,&nr));
    NativeGameProfile *cloned=(NativeGameProfile *)MCObjectRoot_get(&wa);NativeJavaUUID *copyId=(NativeJavaUUID *)MCObjectRoot_get(&wi);
    CHECK(cloned!=a&&cloned->id==copyId&&cloned->name==(NBTString *)MCObjectRoot_get(&wn));
    CHECK(((NativeGameProfile *)MCObjectRoot_get(&wb))->id==copyId);
    size_t before=MCObjectHeap_liveObjects(working);CHECK(EntityPlayer_getUUID(cloned)==copyId);CHECK(MCObjectHeap_liveObjects(working)==before);
    CHECK(MCObjectHeap_adopt(heap,working));MCObjectHeap_free(working);
    a=(NativeGameProfile *)MCObjectRoot_get(&ar);CHECK(a!=cloned||a->object.heap==heap);
    CHECK(a->id==(NativeJavaUUID *)MCObjectRoot_get(&ir)&&a->name==(NBTString *)MCObjectRoot_get(&nr));
    CHECK(MCObjectHeap_collect(heap));
    working=MCObjectHeap_clone(heap);CHECK(working);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,working));
    CHECK(!MCObjectHeap_adopt(heap,working));MCObjectRootScope_end(&scope);MCObjectHeap_free(working);
    CHECK(a->id==(NativeJavaUUID *)MCObjectRoot_get(&ir)&&NBTString_equalsASCII(a->name,"Alice"));
    CHECK(!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
}
/* Exercise the real canonical Player class descriptor and sole profile owner,
   including a NULL name that is distinct from a missing profile receiver. */
static void player_name_and_profile_owner(void) {
    MCObjectHeap *heap=MCObjectHeap_new(1024*1024);CHECK(heap);
    MCGameplayPlayer *player=MCGameplayPlayer_nativeAllocate(heap);CHECK(player);
    NBTString *name=NBTString_fromASCII(heap,"Alice");NativeJavaUUID *id=NativeJavaUUID_new(heap,1,2);CHECK(name&&id);
    NativeGameProfile *profile=NativeGameProfile_new(heap,id,name);CHECK(profile);
    player->gameProfile=profile;MCObjectHeap_touch(heap);CHECK(EntityPlayer_getName(player)==name);
    MCObjectRoot pr={0},nr={0},ir={0};CHECK(MCObjectRoot_init(&pr,heap,(MCObject *)player));
    CHECK(MCObjectRoot_init(&nr,heap,(MCObject *)name));CHECK(MCObjectRoot_init(&ir,heap,(MCObject *)id));
    CHECK(MCObjectHeap_collect(heap));CHECK(EntityPlayer_getName(player)==name);
    MCObjectHeap *working=MCObjectHeap_clone(heap);CHECK(working);
    MCObjectRoot wp={0},wn={0},wi={0};CHECK(MCObjectRoot_rebind(&wp,working,&pr));
    CHECK(MCObjectRoot_rebind(&wn,working,&nr));CHECK(MCObjectRoot_rebind(&wi,working,&ir));
    MCGameplayPlayer *copy=(MCGameplayPlayer *)MCObjectRoot_get(&wp);
    CHECK(copy!=player&&copy->gameProfile!=profile);
    CHECK(EntityPlayer_getName(copy)==(NBTString *)MCObjectRoot_get(&wn));
    CHECK(EntityPlayer_getUUID(copy->gameProfile)==(NativeJavaUUID *)MCObjectRoot_get(&wi));
    CHECK(MCObjectHeap_adopt(heap,working));MCObjectHeap_free(working);
    player=(MCGameplayPlayer *)MCObjectRoot_get(&pr);
    CHECK(EntityPlayer_getName(player)==(NBTString *)MCObjectRoot_get(&nr));
    player->gameProfile->name=NULL;MCObjectHeap_touch(heap);
    CHECK(EntityPlayer_getName(player)==NULL&&!MCObjectHeap_failed(heap));
    player->gameProfile=NULL;MCObjectHeap_touch(heap);
    CHECK(EntityPlayer_getName(player)==NULL&&MCObjectHeap_failed(heap));
    CHECK(!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(8192);MCObjectHeap *foreign=MCObjectHeap_new(8192);CHECK(heap&&foreign);
    player=MCGameplayPlayer_nativeAllocate(heap);CHECK(player);
    id=NativeJavaUUID_new(foreign,1,2);profile=NativeGameProfile_new(foreign,id,NULL);CHECK(profile);
    player->gameProfile=profile;MCObjectHeap_touch(heap);
    CHECK(!EntityPlayer_getName(player)&&MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(foreign));
    MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
    heap=MCObjectHeap_new(8192);CHECK(heap);name=NBTString_fromASCII(heap,"wrong receiver");CHECK(name);
    CHECK(!EntityPlayer_getName((MCGameplayPlayer *)name)&&MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
}
/* Native structural failures never convert missing dependencies or foreign
   refs into an offline identity, and failed allocation scopes always close. */
static void failures_and_budgets(void) {
    for(size_t budget=0;budget<1024;budget+=13) {
        MCObjectHeap *heap=MCObjectHeap_new(budget);CHECK(heap);NBTString *name=NBTString_fromASCII(heap,"Alice");
        NativeJavaUUID *id=EntityPlayer_getOfflineUUID(heap,name);
        if(id) {CHECK(!MCObjectHeap_failed(heap));} else {CHECK(MCObjectHeap_failed(heap));}
        CHECK(!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    }
    for(unsigned mode=0;mode<5;mode++) {
        MCObjectHeap *heap=MCObjectHeap_new(8192),*foreign=MCObjectHeap_new(8192);CHECK(heap&&foreign);
        NBTString *name=NBTString_fromASCII(foreign,"Alice");NativeJavaUUID *id=NativeJavaUUID_new(foreign,1,2);CHECK(name&&id);
        if(mode==0)CHECK(!NativeGameProfile_new(heap,id,NULL));
        if(mode==1)CHECK(!NativeGameProfile_new(heap,NULL,name));
        if(mode==2)CHECK(!EntityPlayer_getOfflineUUID(heap,name));
        if(mode==3)CHECK(!PacketBuffer_nativeEncodeUTF8(heap,name));
        if(mode==4){NBTString *local=NBTString_fromASCII(heap,"Alice");NativeGameProfile *profile=NativeGameProfile_new(heap,NULL,local);CHECK(profile);profile->id=id;MCObjectHeap_touch(heap);CHECK(!EntityPlayer_getUUID(profile));}
        CHECK(MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(foreign));CHECK(!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
    }
    MCObjectHeap *heap=MCObjectHeap_new(8192),*foreign=MCObjectHeap_new(8192);CHECK(heap&&foreign);
    NativeJavaUUID *id=NativeJavaUUID_new(heap,5,9);NativeGameProfile *profile=NativeGameProfile_new(heap,id,NULL);CHECK(profile);
    profile->name=NBTString_fromASCII(foreign,"must not be evaluated");MCObjectHeap_touch(heap);
    CHECK(EntityPlayer_getUUID(profile)==id&&!MCObjectHeap_failed(heap));
    CHECK(!NativeGameProfile_getName(profile)&&MCObjectHeap_failed(heap)&&!MCObjectHeap_failed(foreign));
    MCObjectHeap_free(heap);MCObjectHeap_free(foreign);
    for(unsigned mode=0;mode<3;mode++) {
        MCObjectHeap *heap=MCObjectHeap_new(8192);CHECK(heap);NBTString *name=NBTString_fromASCII(heap,"Alice");CHECK(name);
        name->length=SIZE_MAX;MCObjectHeap_touch(heap);
        if(mode==0)CHECK(!NativeGameProfile_new(heap,NULL,name));
        if(mode==1)CHECK(!EntityPlayer_getOfflineUUID(heap,name));
        if(mode==2)CHECK(!PacketBuffer_nativeEncodeUTF8(heap,name));
        CHECK(MCObjectHeap_failed(heap)&&!MCObjectHeap_hasBorrowers(heap));MCObjectHeap_free(heap);
    }
}
int main(void) {
    identity_and_offline();null_and_utf16_names();profile_nullable_and_blank();unbounded_charset_and_packet_limit();
    raw_name_uuid();graph_aliases_and_abort();player_name_and_profile_owner();failures_and_budgets();
    printf("Source Player profile: %u checks passed\n",checks);return 0;
}
