#include "util/NativeJavaRandom.h"
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static uint32_t float_bits(float v) { uint32_t bits; memcpy(&bits,&v,sizeof bits); return bits; }
static uint64_t double_bits(double v) { uint64_t bits; memcpy(&bits,&v,sizeof bits); return bits; }
static bool same_state(const NativeJavaRandomState *a,const NativeJavaRandomState *b) {
    return a->seed48==b->seed48 && a->haveNextNextGaussian==b->haveNextNextGaussian &&
           double_bits(a->nextNextGaussian)==double_bits(b->nextNextGaussian);
}
static void scalar(void) {
    NativeJavaRandomState s={0}; int32_t i; int64_t l; float f; double d; bool b;
    CHECK(NativeJavaRandomState_setSeed(&s,0));
    CHECK(NativeJavaRandomState_nextInt(&s,&i)); CHECK(i==-1155484576);
    CHECK(s.seed48==UINT64_C(0xbb20b4600a74));
    CHECK(NativeJavaRandomState_setSeed(&s,0));
    CHECK(NativeJavaRandomState_nextLong(&s,&l)); CHECK(l==INT64_C(-4962768465676381896));
    CHECK(s.seed48==UINT64_C(0xd4d95138ab6f));
    CHECK(NativeJavaRandomState_setSeed(&s,0)); CHECK(NativeJavaRandomState_nextFloat(&s,&f));
    CHECK(float_bits(f)==UINT32_C(0x3f3b20b4));
    CHECK(NativeJavaRandomState_setSeed(&s,0)); CHECK(NativeJavaRandomState_nextDouble(&s,&d));
    CHECK(double_bits(d)==UINT64_C(0x3fe764168ea6ca89));
    CHECK(NativeJavaRandomState_setSeed(&s,0)); CHECK(NativeJavaRandomState_nextBoolean(&s,&b)); CHECK(b);
    s.seed48=UINT64_C(0x69871a3ba8b1);
    CHECK(NativeJavaRandomState_nextIntBound(&s,1073741825,&i)); CHECK(i==287633608);
    CHECK(s.seed48==UINT64_C(0x2249e191d4ec)); /* Four rejected candidates precede this result. */
    s.haveNextNextGaussian=true; s.nextNextGaussian=-0.0;
    CHECK(NativeJavaRandomState_setSeed(&s,INT64_MIN));
    CHECK(s.seed48==UINT64_C(0x5deece66d)); CHECK(!s.haveNextNextGaussian);
    CHECK(double_bits(s.nextNextGaussian)==UINT64_C(0x8000000000000000));
    CHECK(NativeJavaRandomState_setSeed(&s,-1)); CHECK(s.seed48==UINT64_C(0xfffa21131992));
    NativeJavaRandomState copy=s; i=12345; l=56789;
    CHECK(!NativeJavaRandomState_nextIntBound(&s,0,&i)); CHECK(i==12345); CHECK(same_state(&s,&copy));
    CHECK(!NativeJavaRandomState_nextIntBound(&s,INT32_MIN,&i)); CHECK(i==12345);
    CHECK(!NativeJavaRandomState_nextBits(&s,0,&i)); CHECK(!NativeJavaRandomState_nextBits(&s,33,&i));
    CHECK(!NativeJavaRandomState_nextLong(&s,NULL)); CHECK(!NativeJavaRandomState_nextBytes(&s,NULL,0));
    CHECK(!NativeJavaRandomState_nextLong(NULL,&l)); CHECK(l==56789); CHECK(same_state(&s,&copy));
    uint8_t bytes[9]; memset(bytes,0xa5,sizeof bytes);
    CHECK(NativeJavaRandomState_setSeed(&s,0)); CHECK(NativeJavaRandomState_nextBytes(&s,bytes,7));
    static const uint8_t expected[]={0x60,0xb4,0x20,0xbb,0x38,0x51,0xd9};
    CHECK(!memcmp(bytes,expected,sizeof expected)); CHECK(bytes[7]==0xa5&&bytes[8]==0xa5);
    CHECK(s.seed48==UINT64_C(0xd4d95138ab6f));
    copy=s; CHECK(NativeJavaRandomState_nextBytes(&s,bytes,0)); CHECK(same_state(&s,&copy));
    for (int32_t bits=1;bits<=32;bits++) {
        CHECK(NativeJavaRandomState_setSeed(&s,0)); CHECK(NativeJavaRandomState_nextBits(&s,bits,&i));
        uint32_t expectedBits=UINT32_C(0xbb20b460)>>(32-bits); CHECK((uint32_t)i==expectedBits);
    }
    static const int32_t bounds[]={1,2,3,7,255,256,257,65536,1073741824,1073741825,INT32_MAX};
    for (size_t j=0;j<sizeof bounds/sizeof *bounds;j++) {
        CHECK(NativeJavaRandomState_setSeed(&s,(int64_t)j-5));
        for (unsigned n=0;n<200;n++) { CHECK(NativeJavaRandomState_nextIntBound(&s,bounds[j],&i)); CHECK(i>=0&&i<bounds[j]); }
    }
}
static void managed(void) {
    MCObjectHeap *heap=MCObjectHeap_new(65536); CHECK(heap);
    NativeJavaRandom *random=NativeJavaRandom_new(heap,0); CHECK(random);
    CHECK(random->state.seed48==UINT64_C(0x5deece66d));
    CHECK(!random->state.haveNextNextGaussian);
    CHECK(random->state.nextNextGaussian==0.0);
    CHECK(NativeJavaRandom_isInstance((MCObject *)random));
    MCObjectRoot root={0}; CHECK(MCObjectRoot_init(&root,heap,(MCObject *)random));
    int32_t i; int64_t l; float f; double d; bool b; uint8_t bytes[5];
    CHECK(NativeJavaRandom_nextInt(random,&i)); CHECK(i==-1155484576);
    CHECK(NativeJavaRandom_setSeed(random,0)); CHECK(NativeJavaRandom_nextLong(random,&l)); CHECK(l==INT64_C(-4962768465676381896));
    CHECK(NativeJavaRandom_setSeed(random,0)); CHECK(NativeJavaRandom_nextBoolean(random,&b)); CHECK(b);
    CHECK(NativeJavaRandom_setSeed(random,0)); CHECK(NativeJavaRandom_nextFloat(random,&f)); CHECK(float_bits(f)==UINT32_C(0x3f3b20b4));
    CHECK(NativeJavaRandom_setSeed(random,0)); CHECK(NativeJavaRandom_nextDouble(random,&d)); CHECK(double_bits(d)==UINT64_C(0x3fe764168ea6ca89));
    CHECK(NativeJavaRandom_setSeed(random,0)); CHECK(NativeJavaRandom_nextBits(random,7,&i)); CHECK(i==93);
    CHECK(NativeJavaRandom_nextBytes(random,bytes,sizeof bytes)); CHECK(NativeJavaRandom_nextIntBound(random,123,&i)); CHECK(i>=0&&i<123);
    NativeJavaRandomState before=random->state;
    MCObjectHeap *working=MCObjectHeap_clone(heap); CHECK(working); MCObjectRoot copyRoot={0}; CHECK(MCObjectRoot_rebind(&copyRoot,working,&root));
    NativeJavaRandom *copy=(NativeJavaRandom *)MCObjectRoot_get(&copyRoot); CHECK(copy&&copy!=random);
    CHECK(same_state(&copy->state,&before)); CHECK(MCObjectHeap_identityHashCode((MCObject *)copy)==MCObjectHeap_identityHashCode((MCObject *)random));
    CHECK(NativeJavaRandom_nextInt(copy,&i)); CHECK(same_state(&random->state,&before));
    CHECK(MCObjectHeap_adopt(heap,working)); MCObjectHeap_free(working);
    random=(NativeJavaRandom *)MCObjectRoot_get(&root); CHECK(random); CHECK(MCObjectHeap_collect(heap));
    CHECK(MCObjectHeap_liveObjects(heap)==1); CHECK(NativeJavaRandom_nextInt(random,&i));
    before=random->state; i=991; CHECK(!NativeJavaRandom_nextIntBound(random,-1,&i));
    CHECK(i==991); CHECK(same_state(&random->state,&before)); CHECK(MCObjectHeap_failed(heap));
    CHECK(!NativeJavaRandom_nextInt(random,&i)); CHECK(i==991);
    MCObjectRoot_drop(&root);
    MCObjectHeap_free(heap);
}
int main(void) {
    scalar(); managed();
    printf("Java Random scalar API: %u checks\n",checks); return 0;
}
