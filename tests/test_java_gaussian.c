#include "util/NativeJavaRandom.h"
#include "util/NativeStrictMath.h"
#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__SSE2__)
#include <xmmintrin.h>
#endif

static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static uint64_t bits(double d){uint64_t u;memcpy(&u,&d,sizeof u);return u;}
static double number(uint64_t u){double d;memcpy(&d,&u,sizeof d);return d;}
static bool same_state(const NativeJavaRandomState *a,const NativeJavaRandomState *b){
    return a->seed48==b->seed48 && a->haveNextNextGaussian==b->haveNextNextGaussian && bits(a->nextNextGaussian)==bits(b->nextNextGaussian);
}
/* Independent Java8 API observations. The first two log inputs distinguish
   the target fdlibm contract from both host libm and correctly-rounded MPFR. */
static void numerical_contract(void){
    static const uint64_t cases[][3]={
        {UINT64_C(0x3fe4822e71f49b3a),UINT64_C(0xbfdc792ab3c11f2e),UINT64_C(0x3fe99e296bf8cf70)},
        {UINT64_C(0x3fedb499e1c327ce),UINT64_C(0xbfb30c2dfb227f68),UINT64_C(0x3feed4d6895a18df)},
        {UINT64_C(0x0000000000000001),UINT64_C(0xc0874385446d71c3),UINT64_C(0x1e60000000000000)},
        {UINT64_C(0x0010000000000000),UINT64_C(0xc086232bdd7abcd2),UINT64_C(0x2000000000000000)},
        {UINT64_C(0x3fefffffffffffff),UINT64_C(0xbca0000000000000),UINT64_C(0x3fefffffffffffff)},
        {UINT64_C(0x3ff0000000000000),UINT64_C(0),UINT64_C(0x3ff0000000000000)},
        {UINT64_C(0x3ff0000000000001),UINT64_C(0x3cafffffffffffff),UINT64_C(0x3ff0000000000000)},
        {UINT64_C(0x4000000000000000),UINT64_C(0x3fe62e42fefa39ef),UINT64_C(0x3ff6a09e667f3bcd)},
        {UINT64_C(0x7fefffffffffffff),UINT64_C(0x40862e42fefa39ef),UINT64_C(0x5fefffffffffffff)}
    };
    CHECK(NativeStrictMath_isSupported());
    for(size_t i=0;i<sizeof cases/sizeof *cases;i++){
        double d=number(cases[i][0]);
        CHECK(bits(NativeStrictMath_log(d))==cases[i][1]);
        CHECK(bits(NativeStrictMath_sqrt(d))==cases[i][2]);
    }
    CHECK(bits(NativeStrictMath_sqrt(-0.0))==UINT64_C(0x8000000000000000));
    CHECK(bits(NativeStrictMath_sqrt(INFINITY))==UINT64_C(0x7ff0000000000000));
    CHECK(isnan(NativeStrictMath_sqrt(-1.0)));
    CHECK(bits(NativeStrictMath_log(0.0))==UINT64_C(0xfff0000000000000));
    CHECK(bits(NativeStrictMath_log(-0.0))==UINT64_C(0xfff0000000000000));
    CHECK(bits(NativeStrictMath_log(INFINITY))==UINT64_C(0x7ff0000000000000));
    CHECK(isnan(NativeStrictMath_log(-1.0)));
}
static void cached_and_rejected_pairs(void){
    static const struct {uint64_t seed,output,cache,after;} cases[]={
        {UINT64_C(0x0005deece66d),UINT64_C(0x3fe9ae59d1d6f861),UINT64_C(0xbfecd9772eb2e0c8),UINT64_C(0x9b3970be8d41)},
        {UINT64_C(0x0005deece66c),UINT64_C(0x3ff8fc3c669aa4c1),UINT64_C(0xbfe3763b5ee2e541),UINT64_C(0x684df9922e30)},
        {UINT64_C(0xfffa21131992),UINT64_C(0x3ffc90b7b3790a3a),UINT64_C(0xbfed740e27d7f93c),UINT64_C(0x6637c50ce01a)},
        {UINT64_C(0),UINT64_C(0xbfcf6e880764bd18),UINT64_C(0xbfe7a951ad374034),UINT64_C(0x7cba449ae648)},
        {UINT64_C(0xabc831749039),UINT64_C(0xbfc756074392fd1f),UINT64_C(0x4004a67363c67400),UINT64_C(0x154446d387cd)},
        {UINT64_C(0x55508bb9b338),UINT64_C(0xbfd8f4a6d1dcbac2),UINT64_C(0xbfbb77e3bd666aec),UINT64_C(0x860d7a3b7bbc)},
        {UINT64_C(0xaaaf74464cc7),UINT64_C(0xbfa76ff6377dc7b4),UINT64_C(0x3fd470f31ff1812d),UINT64_C(0x79609d1d64ef)},
        {UINT64_C(0x30458e8c96ed),UINT64_C(0xbff3cc451096cca6),UINT64_C(0x3ff2aa69d34f1524),UINT64_C(0xe19e32d8c5c1)}
    };
    for(size_t i=0;i<sizeof cases/sizeof *cases;i++){
        NativeJavaRandomState s={cases[i].seed,false,0.0};double d=123;
        CHECK(NativeJavaRandomState_nextGaussian(&s,&d));CHECK(bits(d)==cases[i].output);
        CHECK(s.seed48==cases[i].after);CHECK(s.haveNextNextGaussian);CHECK(bits(s.nextNextGaussian)==cases[i].cache);
        CHECK(NativeJavaRandomState_nextGaussian(&s,&d));CHECK(bits(d)==cases[i].cache);
        CHECK(s.seed48==cases[i].after);CHECK(!s.haveNextNextGaussian);CHECK(bits(s.nextNextGaussian)==cases[i].cache);
    }
    NativeJavaRandomState s={0};double d;int32_t n;
    CHECK(NativeJavaRandomState_setSeed(&s,0));CHECK(NativeJavaRandomState_nextGaussian(&s,&d));
    CHECK(NativeJavaRandomState_nextDouble(&s,&d));CHECK(bits(d)==UINT64_C(0x3fe465b93a78ef81));
    CHECK(NativeJavaRandomState_nextIntBound(&s,1073741825,&n));CHECK(n==251269761);
    CHECK(NativeJavaRandomState_nextGaussian(&s,&d));CHECK(bits(d)==UINT64_C(0xbfecd9772eb2e0c8));
    CHECK(s.seed48==UINT64_C(0x1df425034d55));
    CHECK(NativeJavaRandomState_nextGaussian(&s,&d));CHECK(bits(d)==UINT64_C(0x3fef81a273668e4a));
    uint64_t stale=bits(s.nextNextGaussian);
    CHECK(NativeJavaRandomState_setSeed(&s,0));CHECK(!s.haveNextNextGaussian);CHECK(bits(s.nextNextGaussian)==stale);
    CHECK(NativeJavaRandomState_nextGaussian(&s,&d));CHECK(bits(d)==UINT64_C(0x3fe9ae59d1d6f861));
}
static void invalid_and_environment(void){
    NativeJavaRandomState s={UINT64_C(0x5deece66d),false,-0.0},before=s;double d=42;
    CHECK(!NativeJavaRandomState_nextGaussian(NULL,&d));CHECK(d==42);
    CHECK(!NativeJavaRandomState_nextGaussian(&s,NULL));CHECK(same_state(&s,&before));
    s.haveNextNextGaussian=true;before=s;
    CHECK(!NativeJavaRandomState_nextGaussian(&s,NULL));CHECK(same_state(&s,&before));
    s.haveNextNextGaussian=false;before=s;
    int old_round=fegetround();CHECK(old_round==FE_TONEAREST);CHECK(fesetround(FE_UPWARD)==0);
    bool supported=NativeStrictMath_isSupported();bool accepted=NativeJavaRandomState_nextGaussian(&s,&d);
    bool unchanged=same_state(&s,&before);bool output_unchanged=d==42;
    /* Cached returns only move stored bits; the numerical dependency is unused. */
    s.haveNextNextGaussian=true;s.nextNextGaussian=number(UINT64_C(0x7ff8123456789abc));
    bool cached=NativeJavaRandomState_nextGaussian(&s,&d);uint64_t cached_bits=bits(d);
    CHECK(fesetround(old_round)==0);CHECK(!supported);CHECK(!accepted);CHECK(unchanged);CHECK(output_unchanged);
    CHECK(cached);CHECK(cached_bits==UINT64_C(0x7ff8123456789abc));CHECK(!s.haveNextNextGaussian);CHECK(s.seed48==before.seed48);
#if defined(__SSE2__)
    unsigned original=_mm_getcsr();
    static const unsigned unsupported[]={0x8000u,0x0040u};
    for(size_t i=0;i<sizeof unsupported/sizeof *unsupported;i++){
        s=before;d=42;
        _mm_setcsr((original&~0x8040u)|unsupported[i]);
        supported=NativeStrictMath_isSupported();accepted=NativeJavaRandomState_nextGaussian(&s,&d);
        _mm_setcsr(original);
        CHECK(!supported);CHECK(!accepted);CHECK(d==42);CHECK(same_state(&s,&before));
    }
#endif
}
static void managed_aliases_and_adoption(void){
    MCObjectHeap *heap=MCObjectHeap_new(65536);CHECK(heap);
    NativeJavaRandom *r=NativeJavaRandom_new(heap,0);CHECK(r);MCObjectRoot a={0},b={0};
    CHECK(MCObjectRoot_init(&a,heap,(MCObject *)r));CHECK(MCObjectRoot_init(&b,heap,(MCObject *)r));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,heap));CHECK(MCObjectRootScope_pin(&scope,(MCObject *)r));
    double d;CHECK(NativeJavaRandom_nextGaussian(r,&d));CHECK(bits(d)==UINT64_C(0x3fe9ae59d1d6f861));
    MCObjectRootScope_end(&scope);NativeJavaRandomState before=r->state;
    MCObjectHeap *working=MCObjectHeap_clone(heap);CHECK(working);MCObjectRoot wa={0},wb={0};
    CHECK(MCObjectRoot_rebind(&wa,working,&a));CHECK(MCObjectRoot_rebind(&wb,working,&b));
    NativeJavaRandom *copy=(NativeJavaRandom *)MCObjectRoot_get(&wa);CHECK(copy);CHECK(copy==(NativeJavaRandom *)MCObjectRoot_get(&wb));CHECK(copy!=r);
    CHECK(same_state(&copy->state,&before));CHECK(NativeJavaRandom_nextGaussian(copy,&d));CHECK(bits(d)==UINT64_C(0xbfecd9772eb2e0c8));
    CHECK(!copy->state.haveNextNextGaussian);CHECK(copy->state.seed48==before.seed48);CHECK(same_state(&r->state,&before));
    CHECK(MCObjectHeap_adopt(heap,working));MCObjectHeap_free(working);
    r=(NativeJavaRandom *)MCObjectRoot_get(&a);CHECK(r);CHECK((MCObject *)r==MCObjectRoot_get(&b));
    CHECK(!r->state.haveNextNextGaussian);CHECK(MCObjectHeap_collect(heap));CHECK(MCObjectHeap_liveObjects(heap)==1);
    before=r->state;working=MCObjectHeap_clone(heap);CHECK(working);wa=(MCObjectRoot){0};CHECK(MCObjectRoot_rebind(&wa,working,&a));
    copy=(NativeJavaRandom *)MCObjectRoot_get(&wa);CHECK(copy);CHECK(!NativeJavaRandom_nextGaussian(copy,NULL));CHECK(MCObjectHeap_failed(working));
    CHECK(same_state(&copy->state,&before));CHECK(!MCObjectHeap_canAdopt(heap,working));CHECK(same_state(&r->state,&before));MCObjectHeap_free(working);
    CHECK(NativeJavaRandom_nextGaussian(r,&d));CHECK(!MCObjectHeap_failed(heap));
    /* A cached managed draw still mutates validity and must invalidate an old
       snapshot even though the seed and allocated object count do not change. */
    before=r->state;CHECK(before.haveNextNextGaussian);working=MCObjectHeap_clone(heap);CHECK(working);
    CHECK(NativeJavaRandom_nextGaussian(r,&d));CHECK(!r->state.haveNextNextGaussian);CHECK(r->state.seed48==before.seed48);
    CHECK(bits(d)==bits(before.nextNextGaussian));CHECK(!MCObjectHeap_canAdopt(heap,working));MCObjectHeap_free(working);
    MCObjectRoot_drop(&a);MCObjectRoot_drop(&b);CHECK(MCObjectHeap_collect(heap));CHECK(MCObjectHeap_liveObjects(heap)==0);MCObjectHeap_free(heap);
}
static void managed_failures(void){
    double d=99;CHECK(!NativeJavaRandom_nextGaussian(NULL,&d));CHECK(d==99);
    MCObjectHeap *heap=MCObjectHeap_new(65536);CHECK(heap);NativeJavaRandom *r=NativeJavaRandom_new(heap,0);CHECK(r);
    NativeJavaRandomState before=r->state;MCObjectHeap_fail(heap);
    CHECK(!NativeJavaRandom_nextGaussian(r,&d));CHECK(d==99);CHECK(same_state(&r->state,&before));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(65536);CHECK(heap);r=NativeJavaRandom_new(heap,0);CHECK(r);
    MCObject *tiny=MCObjectHeap_alloc(heap,sizeof(MCObject),r->object.klass);CHECK(tiny);
    CHECK(!NativeJavaRandom_nextGaussian((NativeJavaRandom *)tiny,&d));CHECK(d==99);CHECK(MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
    heap=MCObjectHeap_new(65536);CHECK(heap);r=NativeJavaRandom_new(heap,0);CHECK(r);before=r->state;
    int old_round=fegetround();CHECK(fesetround(FE_DOWNWARD)==0);bool accepted=NativeJavaRandom_nextGaussian(r,&d);CHECK(fesetround(old_round)==0);
    CHECK(!accepted);CHECK(d==99);CHECK(same_state(&r->state,&before));CHECK(MCObjectHeap_failed(heap));MCObjectHeap_free(heap);
}
int main(void){
    numerical_contract();cached_and_rejected_pairs();invalid_and_environment();managed_aliases_and_adoption();managed_failures();
    printf("Java Gaussian API: %u checks\n",checks);return 0;
}
