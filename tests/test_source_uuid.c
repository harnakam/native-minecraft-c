#include "util/NativeJavaUUID.h"
#include "util/MathHelper.h"
#include "nbt/NBTInternal.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const MCObjectClass otherClass={"fixture.uuid.other",MCObjectHeap_plainClone,NULL,NULL};
static void constructor(void) {
    MCObjectHeap *h=MCObjectHeap_new(4096); CHECK(h);
    MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,h));
    NativeJavaUUID *u=NativeJavaUUID_new(h,INT64_MIN,INT64_MAX); CHECK(u);
    CHECK(NativeJavaUUID_isInstance((MCObject*)u));
    CHECK(u->mostSignificantBits==INT64_MIN && u->leastSignificantBits==INT64_MAX);
    MCObject *undersized=MCObjectHeap_alloc(h,sizeof(MCObject),u->object.klass); CHECK(undersized);
    CHECK(!NativeJavaUUID_isInstance(undersized));
    NativeJavaUUID *second=NativeJavaUUID_new(h,INT64_MIN,INT64_MAX); CHECK(second && second!=u);
    CHECK(!NativeJavaUUID_isInstance(NULL));
    MCObject *other=MCObjectHeap_alloc(h,sizeof(MCObject),&otherClass); CHECK(other);
    CHECK(!NativeJavaUUID_isInstance(other));
    MCObjectRoot a={0},b={0}; CHECK(MCObjectRoot_init(&a,h,(MCObject*)u));
    CHECK(MCObjectRoot_init(&b,h,(MCObject*)u));
    MCObjectRootScope_end(&scope);
    MCObjectHeap *working=MCObjectHeap_clone(h); CHECK(working);
    MCObjectRoot wa={0},wb={0}; CHECK(MCObjectRoot_rebind(&wa,working,&a));
    CHECK(MCObjectRoot_rebind(&wb,working,&b));
    NativeJavaUUID *copy=(NativeJavaUUID*)MCObjectRoot_get(&wa);
    CHECK(copy && copy!=u && MCObjectRoot_get(&wb)==(MCObject*)copy);
    CHECK(copy->mostSignificantBits==INT64_MIN && copy->leastSignificantBits==INT64_MAX);
    CHECK(MCObjectHeap_collect(working)); CHECK(MCObjectHeap_liveObjects(working)==1);
    CHECK(MCObjectHeap_adopt(h,working));
    CHECK(MCObjectRoot_get(&a)==(MCObject*)copy && MCObjectRoot_get(&b)==(MCObject*)copy);
    MCObjectRoot_drop(&a); MCObjectRoot_drop(&b); MCObjectHeap_free(working);
    CHECK(MCObjectHeap_collect(h)); CHECK(MCObjectHeap_liveObjects(h)==0);
    MCObjectHeap_free(h);
    CHECK(!NativeJavaUUID_new(NULL,0,0));
    h=MCObjectHeap_new(sizeof(NativeJavaUUID)-1); CHECK(h);
    CHECK(!NativeJavaUUID_new(h,0,0)); CHECK(MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void accepted(const char *text,uint64_t most,uint64_t least) {
    MCObjectHeap *h=MCObjectHeap_new(8192); CHECK(h);
    NBTString *s=NBTString_fromUTF8(h,text); CHECK(s);
    size_t bytes=MCObjectHeap_liveBytes(h);
    NativeJavaUUID *u=NativeJavaUUID_fromString(h,s); CHECK(u);
    CHECK((uint64_t)u->mostSignificantBits==most && (uint64_t)u->leastSignificantBits==least);
    CHECK(!MCObjectHeap_failed(h));
    CHECK(MCObjectHeap_liveBytes(h)==bytes+sizeof(*u));
    NativeJavaUUID *v=NativeJavaUUID_fromString(h,s); CHECK(v && v!=u);
    CHECK((uint64_t)v->mostSignificantBits==most && (uint64_t)v->leastSignificantBits==least);
    MCObjectHeap_free(h);
}
static void rejected_units(const uint16_t *units,size_t length) {
    MCObjectHeap *h=MCObjectHeap_new(8192); CHECK(h);
    NBTString *s=NBTString_fromUTF16(h,units,length); CHECK(s);
    size_t bytes=MCObjectHeap_liveBytes(h);
    CHECK(!NativeJavaUUID_fromString(h,s)); CHECK(MCObjectHeap_failed(h));
    CHECK(MCObjectHeap_liveBytes(h)==bytes);
    CHECK(NBTString_length(s)==length);
    CHECK(!length || !memcmp(NBTString_units(s),units,length*sizeof(*units)));
    MCObjectHeap_free(h);
}
static void rejected(const char *text) {
    size_t n=strlen(text); uint16_t units[256]; CHECK(n<=256);
    for (size_t i=0;i<n;i++) units[i]=(unsigned char)text[i];
    rejected_units(units,n);
}
static void strings(void) {
    /* Numeric vectors were independently observed with the installed JDK 8.
       Components intentionally exceed conventional UUID field widths. */
    accepted("0-0-0-0-0",0,0);
    accepted("1-2-3-4-5",UINT64_C(0x0000000100020003),UINT64_C(0x0004000000000005));
    accepted("1-2-3-4-5---",UINT64_C(0x0000000100020003),UINT64_C(0x0004000000000005));
    accepted("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF",UINT64_MAX,UINT64_MAX);
    accepted("100000000-2-3-4-5",UINT64_C(0x0000000000020003),UINT64_C(0x0004000000000005));
    accepted("1-100000000-3-4-5",UINT64_C(0x0001000100000003),UINT64_C(0x0004000000000005));
    accepted("1-ffffffffffff-3-4-5",UINT64_C(0xffffffffffff0003),UINT64_C(0x0004000000000005));
    accepted("1-7fffffffffffffff-3-4-5",UINT64_C(0xffffffffffff0003),UINT64_C(0x0004000000000005));
    accepted("0000000000000000000001-2-3-4-5",UINT64_C(0x0000000100020003),UINT64_C(0x0004000000000005));
    accepted("\xef\xbc\x91-\xef\xbc\x92-\xef\xbc\x93-\xef\xbc\x94-\xef\xbc\x95",UINT64_C(0x0000000100020003),UINT64_C(0x0004000000000005));
    accepted("\xef\xbc\xa6-\xef\xbd\x86-\xef\xbc\xa1-\xef\xbd\x81-\xef\xbc\x99",UINT64_C(0x0000000f000f000a),UINT64_C(0x000a000000000009));
    static const uint16_t starts[]={0x0030,0x0660,0x06f0,0x07c0,0x0966,0x09e6,0x0a66,
        0x0ae6,0x0b66,0x0be6,0x0c66,0x0ce6,0x0d66,0x0e50,0x0ed0,0x0f20,0x1040,
        0x1090,0x17e0,0x1810,0x1946,0x19d0,0x1a80,0x1a90,0x1b50,0x1bb0,0x1c40,
        0x1c50,0xa620,0xa8d0,0xa900,0xa9d0,0xaa50,0xabf0,0xff10};
    for (size_t r=0;r<sizeof(starts)/sizeof(*starts);r++) for (unsigned d=0;d<10;d++) {
        uint16_t units[]={(uint16_t)(starts[r]+d),'-','0','-','0','-','0','-','0'};
        MCObjectHeap *h=MCObjectHeap_new(4096); CHECK(h);
        NBTString *s=NBTString_fromUTF16(h,units,9); CHECK(s);
        NativeJavaUUID *u=NativeJavaUUID_fromString(h,s); CHECK(u);
        CHECK((uint64_t)u->mostSignificantBits==(uint64_t)d<<32 && u->leastSignificantBits==0);
        MCObjectHeap_free(h);
    }
    static const char *bad[]={"","----","1-2-3-4","1-2-3-4-5-6","-1-2-3-4-5",
        "1--3-4-5","1-2-3-4-","+1-2-3-4-5","0x1-2-3-4-5","1-2-3-4-+5",
        "1-2-3-4-0X5"," 1-2-3-4-5","1-2-3-4-5 ","g-2-3-4-5",
        "8000000000000000-2-3-4-5","1-8000000000000000-3-4-5",
        "1-2-8000000000000000-4-5","1-2-3-8000000000000000-5",
        "1-2-3-4-8000000000000000","ffffffffffffffff-2-3-4-5"};
    for (size_t i=0;i<sizeof(bad)/sizeof(*bad);i++) rejected(bad[i]);
    uint16_t nul[]={'1',0,'-','2','-','3','-','4','-','5'};
    rejected_units(nul,sizeof(nul)/sizeof(*nul));
    uint16_t surrogate[]={0xd835,0xdfd9,'-','2','-','3','-','4','-','5'};
    rejected_units(surrogate,sizeof(surrogate)/sizeof(*surrogate));
    uint16_t unassigned[]={0xa9f1,'-','2','-','3','-','4','-','5'};
    rejected_units(unassigned,sizeof(unassigned)/sizeof(*unassigned));
}
static void failures(void) {
    MCObjectHeap *h=MCObjectHeap_new(4096),*foreign=MCObjectHeap_new(4096); CHECK(h && foreign);
    NBTString *s=NBTString_fromASCII(foreign,"1-2-3-4-5"); CHECK(s);
    CHECK(!NativeJavaUUID_fromString(h,s)); CHECK(MCObjectHeap_failed(h));
    CHECK(!MCObjectHeap_failed(foreign)); MCObjectHeap_free(h); MCObjectHeap_free(foreign);
    h=MCObjectHeap_new(4096); CHECK(h); CHECK(!NativeJavaUUID_fromString(h,NULL));
    CHECK(MCObjectHeap_failed(h)); MCObjectHeap_free(h);
    h=MCObjectHeap_new(4096); CHECK(h);
    MCObject *other=MCObjectHeap_alloc(h,sizeof(MCObject),&otherClass); CHECK(other);
    CHECK(!NativeJavaUUID_fromString(h,(NBTString*)other)); CHECK(MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(sizeof(NBTString)+18+sizeof(NativeJavaUUID)-1); CHECK(h);
    s=NBTString_fromASCII(h,"1-2-3-4-5"); CHECK(s);
    CHECK(!NativeJavaUUID_fromString(h,s)); CHECK(MCObjectHeap_failed(h));
    CHECK(NBTString_equalsASCII(s,"1-2-3-4-5")); MCObjectHeap_free(h);
    h=MCObjectHeap_new(4096); CHECK(h); s=NBTString_fromASCII(h,"1-2-3-4-5"); CHECK(s);
    s->length=SIZE_MAX;
    CHECK(!NativeJavaUUID_fromString(h,s)); CHECK(MCObjectHeap_failed(h)); MCObjectHeap_free(h);
}
static void random_uuid(void) {
    static const struct { int64_t seed; uint64_t most,least,after; } vectors[]={
        {0,UINT64_C(0xbb20b45fd4d94138),UINT64_C(0xbd93cb799b3970be),UINT64_C(0x9b3970be8d41)},
        {1,UINT64_C(0xbb1ad57319b84cd8),UINT64_C(0xa8fb0e6f684df992),UINT64_C(0x684df9922e30)},
        {-1,UINT64_C(0x44d96cb3708742c3),UINT64_C(0x832420168c4bff9e),UINT64_C(0x8c4bff9eadb6)},
        {INT64_MIN,UINT64_C(0xbb20b45fd4d94138),UINT64_C(0xbd93cb799b3970be),UINT64_C(0x9b3970be8d41)},
        {INT64_MAX,UINT64_C(0x44d96cb3708742c3),UINT64_C(0x832420168c4bff9e),UINT64_C(0x8c4bff9eadb6)}
    };
    for (size_t i=0;i<sizeof(vectors)/sizeof(*vectors);i++) {
        MCObjectHeap *h=MCObjectHeap_new(4096); CHECK(h);
        NativeJavaRandom *r=NativeJavaRandom_new(h,vectors[i].seed); CHECK(r);
        NativeJavaUUID *u=MathHelper_getRandomUuid(r); CHECK(u);
        CHECK((uint64_t)u->mostSignificantBits==vectors[i].most);
        CHECK((uint64_t)u->leastSignificantBits==vectors[i].least);
        CHECK(r->state.seed48==vectors[i].after);
        CHECK(u->object.heap==h && NativeJavaUUID_isInstance((MCObject*)u));
        MCObjectHeap_free(h);
    }
    MCObjectHeap *h=MCObjectHeap_new(4096); CHECK(h);
    NativeJavaRandom *r=NativeJavaRandom_new(h,0); CHECK(r);
    NativeJavaUUID *u=MathHelper_getRandomUuid(r); CHECK(u);
    NativeJavaUUID *second=MathHelper_getRandomUuid(r); CHECK(second && second!=u);
    CHECK((uint64_t)second->mostSignificantBits==UINT64_C(0xa32dc9f64f1d403a));
    CHECK((uint64_t)second->leastSignificantBits==UINT64_C(0x8ce970b71df42503));
    CHECK(r->state.seed48==UINT64_C(0x1df425034d55));
    MCObjectRoot randomRoot={0},uuidRoot={0},alias={0};
    CHECK(MCObjectRoot_init(&randomRoot,h,(MCObject*)r));
    CHECK(MCObjectRoot_init(&uuidRoot,h,(MCObject*)u));
    CHECK(MCObjectRoot_init(&alias,h,(MCObject*)r));
    MCObjectHeap *working=MCObjectHeap_clone(h); CHECK(working);
    MCObjectRoot wr={0},wa={0},wu={0}; CHECK(MCObjectRoot_rebind(&wr,working,&randomRoot));
    CHECK(MCObjectRoot_rebind(&wa,working,&alias)); CHECK(MCObjectRoot_rebind(&wu,working,&uuidRoot));
    NativeJavaRandom *clone=(NativeJavaRandom*)MCObjectRoot_get(&wr);
    CHECK(clone && clone!=r && MCObjectRoot_get(&wa)==(MCObject*)clone);
    CHECK(MCObjectHeap_collect(working)); CHECK(MCObjectHeap_liveObjects(working)==2);
    CHECK(MCObjectRoot_get(&wu)!=NULL);
    NativeJavaUUID *parentNext=MathHelper_getRandomUuid(r),*cloneNext=MathHelper_getRandomUuid(clone);
    CHECK(parentNext && cloneNext && parentNext!=cloneNext);
    CHECK(parentNext->mostSignificantBits==cloneNext->mostSignificantBits &&
          parentNext->leastSignificantBits==cloneNext->leastSignificantBits);
    CHECK((uint64_t)parentNext->mostSignificantBits==UINT64_C(0x98f8ba3dc812476d));
    CHECK((uint64_t)parentNext->leastSignificantBits==UINT64_C(0x954dcd2b40b5f04b));
    MCObjectHeap_free(working); MCObjectHeap_free(h);
    /* Source evaluates both draws before UUID allocation. Capacity failure is
       an exception boundary, not a rollback of the original Random calls. */
    h=MCObjectHeap_new(sizeof(NativeJavaRandom)); CHECK(h);
    r=NativeJavaRandom_new(h,0); CHECK(r);
    CHECK(!MathHelper_getRandomUuid(r)); CHECK(MCObjectHeap_failed(h));
    CHECK(r->state.seed48==UINT64_C(0x9b3970be8d41));
    CHECK(MCObjectHeap_liveObjects(h)==1); MCObjectHeap_free(h);
    h=MCObjectHeap_new(4096); CHECK(h); r=NativeJavaRandom_new(h,0); CHECK(r);
    uint64_t before=r->state.seed48; MCObjectHeap_fail(h);
    CHECK(!MathHelper_getRandomUuid(r)); CHECK(r->state.seed48==before); MCObjectHeap_free(h);
    h=MCObjectHeap_new(4096); CHECK(h);
    MCObject *other=MCObjectHeap_alloc(h,sizeof(MCObject),&otherClass); CHECK(other);
    CHECK(!MathHelper_getRandomUuid((NativeJavaRandom*)other));
    CHECK(MCObjectHeap_failed(h)); MCObjectHeap_free(h);
    CHECK(!MathHelper_getRandomUuid(NULL));
}
int main(void) {
    constructor(); strings(); failures(); random_uuid();
    printf("source uuid: %u checks passed\n",checks); return 0;
}
