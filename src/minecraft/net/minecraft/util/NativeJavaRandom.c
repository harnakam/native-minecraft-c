#include "util/NativeJavaRandom.h"
#include "util/NativeStrictMath.h"
#include <float.h>
#include <limits.h>

_Static_assert(FLT_RADIX==2 && FLT_MANT_DIG==24 && DBL_MANT_DIG==53,
               "Java scalar random values require binary32 and binary64");
static const uint64_t multiplier=UINT64_C(0x5deece66d),mask=UINT64_C(0xffffffffffff);
static int32_t signed32(uint32_t v) { return v<=INT32_MAX ? (int32_t)v : -1-(int32_t)(UINT32_MAX-v); }
static int64_t signed64(uint64_t v) { return v<=INT64_MAX ? (int64_t)v : -1-(int64_t)(UINT64_MAX-v); }
bool NativeJavaRandomState_setSeed(NativeJavaRandomState *s,int64_t seed) {
    if (!s) return false;
    s->seed48=((uint64_t)seed^multiplier)&mask;
    s->haveNextNextGaussian=false; return true;
}
bool NativeJavaRandomState_nextBits(NativeJavaRandomState *s,int32_t bits,int32_t *out) {
    if (!s || !out || bits<1 || bits>32) return false;
    s->seed48=(s->seed48*multiplier+UINT64_C(11))&mask;
    *out=signed32((uint32_t)(s->seed48>>(48-bits))); return true;
}
bool NativeJavaRandomState_nextInt(NativeJavaRandomState *s,int32_t *out) {
    return NativeJavaRandomState_nextBits(s,32,out);
}
bool NativeJavaRandomState_nextIntBound(NativeJavaRandomState *s,int32_t bound,int32_t *out) {
    if (!s || !out || bound<=0) return false;
    int32_t bits=0;
    if (((uint32_t)bound&((uint32_t)bound-1))==0) {
        (void)NativeJavaRandomState_nextBits(s,31,&bits);
        *out=(int32_t)(((uint64_t)(uint32_t)bound*(uint32_t)bits)>>31); return true;
    }
    for (;;) {
        (void)NativeJavaRandomState_nextBits(s,31,&bits);
        int32_t value=bits%bound;
        /* Java's overflowed signed32 rejection expression is tested by its
           sign bit; the unsigned sum has defined C arithmetic. */
        uint32_t acceptance=(uint32_t)bits-(uint32_t)value+((uint32_t)bound-1);
        if (!(acceptance&UINT32_C(0x80000000))) { *out=value; return true; }
    }
}
bool NativeJavaRandomState_nextLong(NativeJavaRandomState *s,int64_t *out) {
    if (!s || !out) return false;
    int32_t high=0,low=0;
    (void)NativeJavaRandomState_nextBits(s,32,&high);
    (void)NativeJavaRandomState_nextBits(s,32,&low);
    /* The low word is a signed addition, not an unsigned concatenation. */
    *out=signed64(((uint64_t)(uint32_t)high<<32)+(uint64_t)(int64_t)low); return true;
}
bool NativeJavaRandomState_nextBoolean(NativeJavaRandomState *s,bool *out) {
    if (!s || !out) return false;
    int32_t bits=0; (void)NativeJavaRandomState_nextBits(s,1,&bits); *out=bits!=0; return true;
}
bool NativeJavaRandomState_nextFloat(NativeJavaRandomState *s,float *out) {
    if (!s || !out) return false;
    int32_t bits=0; (void)NativeJavaRandomState_nextBits(s,24,&bits); *out=(float)bits*0x1p-24f; return true;
}
bool NativeJavaRandomState_nextDouble(NativeJavaRandomState *s,double *out) {
    if (!s || !out) return false;
    int32_t high=0,low=0;
    (void)NativeJavaRandomState_nextBits(s,26,&high);
    (void)NativeJavaRandomState_nextBits(s,27,&low);
    uint64_t bits=((uint64_t)(uint32_t)high<<27)+(uint32_t)low;
    *out=(double)bits*0x1p-53; return true;
}
bool NativeJavaRandomState_nextBytes(NativeJavaRandomState *s,uint8_t *out,size_t length) {
    if (!s || !out) return false;
    size_t index=0;
    while (index<length) {
        int32_t word=0; (void)NativeJavaRandomState_nextInt(s,&word);
        uint32_t bits=(uint32_t)word;
        for (unsigned byte=0;byte<4 && index<length;byte++,index++,bits>>=8) out[index]=(uint8_t)bits;
    }
    return true;
}
bool NativeJavaRandomState_nextGaussian(NativeJavaRandomState *s,double *out) {
    if (!s || !out) return false;
    if (s->haveNextNextGaussian) {
        s->haveNextNextGaussian=false;
        *out=s->nextNextGaussian;
        return true;
    }
    /* The platform numerical boundary fails before consuming this stream.
       The cache-only path above needs no arithmetic or environment support. */
    if (!NativeStrictMath_isSupported()) return false;
    double horizontal,vertical,radius;
    for (;;) {
        double uniform;
        if (!NativeJavaRandomState_nextDouble(s,&uniform)) return false;
        horizontal=2.0*uniform-1.0;
        if (!NativeJavaRandomState_nextDouble(s,&uniform)) return false;
        vertical=2.0*uniform-1.0;
        radius=horizontal*horizontal+vertical*vertical;
        if (radius>=1.0 || radius==0.0) continue;
        break;
    }
    double factor=NativeStrictMath_sqrt(-2.0*NativeStrictMath_log(radius)/radius);
    s->nextNextGaussian=vertical*factor;
    s->haveNextNextGaussian=true;
    *out=horizontal*factor;
    return true;
}
static const MCObjectClass klass={"C919.native.JavaRandomAdapter",MCObjectHeap_plainClone,NULL,NULL};
bool NativeJavaRandom_isInstance(const MCObject *o) {
    return o && o->klass==&klass && MCObjectHeap_objectSize(o)>=sizeof(NativeJavaRandom);
}
NativeJavaRandom *NativeJavaRandom_new(MCObjectHeap *heap,int64_t seed) {
    NativeJavaRandom *r=(NativeJavaRandom *)MCObjectHeap_alloc(heap,sizeof *r,&klass);
    if (r) { r->state=(NativeJavaRandomState){0}; (void)NativeJavaRandomState_setSeed(&r->state,seed); }
    return r;
}
static bool valid(NativeJavaRandom *r,const void *out) {
    MCObjectHeap *heap=r ? r->object.heap : NULL;
    if (!out || !NativeJavaRandom_isInstance((MCObject *)r) || MCObjectHeap_failed(heap)) {
        MCObjectHeap_fail(heap); return false;
    }
    return true;
}
static bool finish(NativeJavaRandom *r,bool ok) {
    if (!ok) MCObjectHeap_fail(r->object.heap);
    else MCObjectHeap_touch(r->object.heap);
    return ok && !MCObjectHeap_failed(r->object.heap);
}
bool NativeJavaRandom_setSeed(NativeJavaRandom *r,int64_t seed) {
    return valid(r,r) && finish(r,NativeJavaRandomState_setSeed(&r->state,seed));
}
#define SCALAR_WRAPPER(name,type) \
bool NativeJavaRandom_##name(NativeJavaRandom *r,type *out) { \
    if (!valid(r,out)) return false; \
    type value; \
    if (!finish(r,NativeJavaRandomState_##name(&r->state,&value))) return false; \
    *out=value; return true; \
}
SCALAR_WRAPPER(nextInt,int32_t)
SCALAR_WRAPPER(nextLong,int64_t)
SCALAR_WRAPPER(nextBoolean,bool)
SCALAR_WRAPPER(nextFloat,float)
SCALAR_WRAPPER(nextDouble,double)
SCALAR_WRAPPER(nextGaussian,double)
#undef SCALAR_WRAPPER
bool NativeJavaRandom_nextBits(NativeJavaRandom *r,int32_t bits,int32_t *out) {
    if (!valid(r,out)) return false;
    int32_t value;
    if (!finish(r,NativeJavaRandomState_nextBits(&r->state,bits,&value))) return false;
    *out=value; return true;
}
bool NativeJavaRandom_nextIntBound(NativeJavaRandom *r,int32_t bound,int32_t *out) {
    if (!valid(r,out)) return false;
    int32_t value;
    if (!finish(r,NativeJavaRandomState_nextIntBound(&r->state,bound,&value))) return false;
    *out=value; return true;
}
bool NativeJavaRandom_nextBytes(NativeJavaRandom *r,uint8_t *out,size_t length) {
    return valid(r,out) && finish(r,NativeJavaRandomState_nextBytes(&r->state,out,length));
}
