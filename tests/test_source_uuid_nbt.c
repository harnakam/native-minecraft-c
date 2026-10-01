#include "entity/EntityUUIDNBT.h"
#include "nbt/NBTTagString.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr,"UUID NBT line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct {
    MCObject object;
    NativeJavaUUID *uuid, *second;
    unsigned gets, sets;
    bool replaceOnSecond, nullOnSecond, failSet, scopeObserved;
} Owner;
static void trace(MCObject *o, MCObjectVisitor v, void *context) {
    Owner *p=(Owner *)o;
    p->uuid=(NativeJavaUUID *)v((MCObject *)p->uuid,context);
    p->second=(NativeJavaUUID *)v((MCObject *)p->second,context);
}
static const MCObjectClass ownerClass={"fixture.UUID.virtual-owner",MCObjectHeap_plainClone,trace,NULL};
static NativeJavaUUID *get(MCObject *o) {
    Owner *p=(Owner *)o; ++p->gets;
    p->scopeObserved=MCObjectHeap_hasBorrowers(o->heap);
    CHECK(!MCObjectHeap_collect(o->heap)&&MCObjectHeap_clone(o->heap)==NULL&&!MCObjectHeap_failed(o->heap));
    if (p->gets==2&&p->nullOnSecond) return NULL;
    if (p->gets==2&&p->replaceOnSecond) p->uuid=p->second;
    MCObjectHeap_touch(o->heap); return p->uuid;
}
static bool set(MCObject *o, NativeJavaUUID *uuid) {
    Owner *p=(Owner *)o; ++p->sets;
    p->scopeObserved=MCObjectHeap_hasBorrowers(o->heap);
    if (p->failSet) return false;
    p->uuid=uuid; MCObjectHeap_touch(o->heap); return true;
}
static const EntityUUIDNBTDispatch dispatch={get,set};
static Owner *owner(MCObjectHeap *h) {
    Owner *p=(Owner *)MCObjectHeap_alloc(h,sizeof *p,&ownerClass);
    CHECK(p); p->uuid=NativeJavaUUID_new(h,17,-23); CHECK(p->uuid); return p;
}
static void write_reads_each_receiver_and_overwrites_longs(void) {
    MCObjectHeap *h=MCObjectHeap_new(1024*1024); CHECK(h);
    Owner *p=owner(h); NBTTagCompound *tag=NBTTagCompound_new(h); CHECK(tag);
    CHECK(NBTTagCompound_setInteger_ascii(tag,"UUIDMost",9)&&
          NBTTagCompound_setString_ascii(tag,"UUIDLeast",NBTString_fromASCII(h,"stale"))&&
          NBTTagCompound_setString_ascii(tag,"UUID",NBTString_fromASCII(h,"1-2-3-4-5")));
    p->second=NativeJavaUUID_new(h,INT64_MIN,INT64_MAX); CHECK(p->second); p->replaceOnSecond=true;
    CHECK(Entity_writeUUIDToNBTSegment((MCObject *)p,tag,&dispatch));
    CHECK(p->gets==2&&p->scopeObserved&&p->uuid==p->second);
    CHECK(NBTTagCompound_getTagId_ascii(tag,"UUIDMost")==4&&NBTTagCompound_getLong_ascii(tag,"UUIDMost")==17);
    CHECK(NBTTagCompound_getTagId_ascii(tag,"UUIDLeast")==4&&NBTTagCompound_getLong_ascii(tag,"UUIDLeast")==INT64_MAX);
    CHECK(NBTString_equalsASCII(NBTTagCompound_getString_ascii(tag,"UUID"),"1-2-3-4-5"));
    CHECK(!NBTTagCompound_hasKey_ascii(tag,"RandomSeed")&&!NBTTagCompound_hasKey_ascii(tag,"rand"));
    CHECK(!MCObjectHeap_hasBorrowers(h)&&!MCObjectHeap_failed(h)); MCObjectHeap_free(h);
}
static void read_long_priority_and_legacy_fallback(void) {
    const struct { const char *text; int64_t most,least; } legacy[]={
        {"00000000-0000-0000-0000-000000000000",0,0},
        {"ffffffff-ffff-ffff-ffff-ffffffffffff",-1,-1},
        {"1-2-3-4-5",INT64_C(0x0000000100020003),INT64_C(0x0004000000000005)},
        {"1-2-3-4-5---",INT64_C(0x0000000100020003),INT64_C(0x0004000000000005)}
    };
    for (size_t i=0;i<sizeof legacy/sizeof *legacy;i++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024); CHECK(h); Owner *p=owner(h);
        NativeJavaUUID *old=p->uuid; NBTTagCompound *tag=NBTTagCompound_new(h); CHECK(tag);
        CHECK(NBTTagCompound_setString_ascii(tag,"UUID",NBTString_fromASCII(h,legacy[i].text))&&
              NBTTagCompound_setLong_ascii(tag,"UUIDMost",999)&&NBTTagCompound_setInteger_ascii(tag,"UUIDLeast",7));
        CHECK(Entity_readUUIDFromNBTSegment((MCObject *)p,tag,&dispatch));
        CHECK(p->uuid!=old&&p->uuid->mostSignificantBits==legacy[i].most&&p->uuid->leastSignificantBits==legacy[i].least);
        CHECK(p->gets==0&&p->sets==1&&p->scopeObserved);
        CHECK(NBTTagCompound_setLong_ascii(tag,"UUIDMost",INT64_MIN)&&
              NBTTagCompound_setLong_ascii(tag,"UUIDLeast",INT64_MAX)&&
              NBTTagCompound_setString_ascii(tag,"UUID",NBTString_fromASCII(h,"invalid")));
        CHECK(Entity_readUUIDFromNBTSegment((MCObject *)p,tag,&dispatch));
        CHECK(p->uuid->mostSignificantBits==INT64_MIN&&p->uuid->leastSignificantBits==INT64_MAX&&p->sets==2);
        CHECK(!MCObjectHeap_failed(h)&&!MCObjectHeap_hasBorrowers(h)); MCObjectHeap_free(h);
    }
}
static void missing_wrong_tags_retain_the_exact_reference(void) {
    for (unsigned which=0;which<5;which++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024); CHECK(h); Owner *p=owner(h);
        NativeJavaUUID *old=p->uuid; NBTTagCompound *tag=NBTTagCompound_new(h); CHECK(tag);
        if (which==1) CHECK(NBTTagCompound_setLong_ascii(tag,"UUIDMost",1));
        if (which==2) CHECK(NBTTagCompound_setLong_ascii(tag,"UUIDLeast",2));
        if (which==3) CHECK(NBTTagCompound_setInteger_ascii(tag,"UUIDMost",1)&&NBTTagCompound_setInteger_ascii(tag,"UUIDLeast",2));
        if (which==4) CHECK(NBTTagCompound_setInteger_ascii(tag,"UUID",3));
        CHECK(Entity_readUUIDFromNBTSegment((MCObject *)p,tag,&dispatch));
        CHECK(p->uuid==old&&p->sets==0&&p->gets==0&&!MCObjectHeap_failed(h)&&!MCObjectHeap_hasBorrowers(h));
        MCObjectHeap_free(h);
    }
}
static void failure_preserves_completed_source_changes(void) {
    for (unsigned which=0;which<3;which++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024); CHECK(h); Owner *p=owner(h);
        NativeJavaUUID *old=p->uuid; NBTTagCompound *tag=NBTTagCompound_new(h); CHECK(tag);
        CHECK(NBTTagCompound_setLong_ascii(tag,"UUIDLeast",-999)&&NBTTagCompound_setInteger_ascii(tag,"earlier",31));
        if (which==0) {
            p->nullOnSecond=true;
            CHECK(!Entity_writeUUIDToNBTSegment((MCObject *)p,tag,&dispatch));
            CHECK(NBTTagCompound_getLong_ascii(tag,"UUIDMost")==17&&NBTTagCompound_getLong_ascii(tag,"UUIDLeast")==-999);
            CHECK(p->gets==2&&p->uuid==old);
        } else {
            CHECK(NBTTagCompound_setString_ascii(tag,"UUID",NBTString_fromASCII(h,"not-a-uuid")));
            if (which==2) {
                CHECK(NBTTagCompound_setLong_ascii(tag,"UUIDMost",11)&&NBTTagCompound_setLong_ascii(tag,"UUIDLeast",12));
                p->failSet=true;
            }
            CHECK(!Entity_readUUIDFromNBTSegment((MCObject *)p,tag,&dispatch));
            CHECK(p->uuid==old&&p->sets==(which==2?1u:0u));
        }
        CHECK(NBTTagCompound_getInteger_ascii(tag,"earlier")==31&&MCObjectHeap_failed(h)&&!MCObjectHeap_hasBorrowers(h));
        MCObjectHeap_free(h);
    }
}
static void invalid_native_edges_fail_without_synthesizing_uuid(void) {
    for (unsigned which=0;which<4;which++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024),*foreign=MCObjectHeap_new(1024*1024); CHECK(h&&foreign);
        Owner *p=owner(h); NBTTagCompound *tag=NBTTagCompound_new(which==2?foreign:h); CHECK(tag);
        EntityUUIDNBTDispatch bad=dispatch;
        if (which==0) p->uuid=NULL;
        if (which==1) p->uuid=NativeJavaUUID_new(foreign,1,2);
        if (which==3) bad.getUniqueID=NULL;
        CHECK(!Entity_writeUUIDToNBTSegment((MCObject *)p,tag,&bad)&&MCObjectHeap_failed(h));
        CHECK(!NBTTagCompound_hasKey_ascii(tag,"UUIDMost")&&!MCObjectHeap_hasBorrowers(h));
        MCObjectHeap_free(h); MCObjectHeap_free(foreign);
    }
}
int main(void) {
    write_reads_each_receiver_and_overwrites_longs(); read_long_priority_and_legacy_fallback();
    missing_wrong_tags_retain_the_exact_reference(); failure_preserves_completed_source_changes();
    invalid_native_edges_fail_without_synthesizing_uuid();
    printf("source UUID NBT: %u checks passed\n",checks); return 0;
}
