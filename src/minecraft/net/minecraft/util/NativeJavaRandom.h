#ifndef C919_NATIVE_JAVA_RANDOM_H
#define C919_NATIVE_JAVA_RANDOM_H
#include "util/MCObjectHeap.h"

/* Independently authored mathematical Java 8 Random API platform adapter.
   This is not a copied JDK class. Streams/serialization/subclass
   dispatch are not implemented here. The scalar state is also usable by the
   native process Math.random service outside gameplay transaction heaps. */
typedef struct {
    uint64_t seed48;
    bool haveNextNextGaussian;
    double nextNextGaussian;
} NativeJavaRandomState;
typedef struct NativeJavaRandom {
    MCObject object;
    NativeJavaRandomState state;
} NativeJavaRandom;

bool NativeJavaRandomState_setSeed(NativeJavaRandomState *,int64_t);
bool NativeJavaRandomState_nextBits(NativeJavaRandomState *,int32_t bits,int32_t *);
bool NativeJavaRandomState_nextInt(NativeJavaRandomState *,int32_t *);
bool NativeJavaRandomState_nextIntBound(NativeJavaRandomState *,int32_t bound,int32_t *);
bool NativeJavaRandomState_nextLong(NativeJavaRandomState *,int64_t *);
bool NativeJavaRandomState_nextBoolean(NativeJavaRandomState *,bool *);
bool NativeJavaRandomState_nextFloat(NativeJavaRandomState *,float *);
bool NativeJavaRandomState_nextDouble(NativeJavaRandomState *,double *);
bool NativeJavaRandomState_nextGaussian(NativeJavaRandomState *,double *);
bool NativeJavaRandomState_nextBytes(NativeJavaRandomState *,uint8_t *,size_t length);

NativeJavaRandom *NativeJavaRandom_new(MCObjectHeap *,int64_t seed);
bool NativeJavaRandom_isInstance(const MCObject *);
bool NativeJavaRandom_setSeed(NativeJavaRandom *,int64_t);
bool NativeJavaRandom_nextBits(NativeJavaRandom *,int32_t bits,int32_t *);
bool NativeJavaRandom_nextInt(NativeJavaRandom *,int32_t *);
bool NativeJavaRandom_nextIntBound(NativeJavaRandom *,int32_t bound,int32_t *);
bool NativeJavaRandom_nextLong(NativeJavaRandom *,int64_t *);
bool NativeJavaRandom_nextBoolean(NativeJavaRandom *,bool *);
bool NativeJavaRandom_nextFloat(NativeJavaRandom *,float *);
bool NativeJavaRandom_nextDouble(NativeJavaRandom *,double *);
bool NativeJavaRandom_nextGaussian(NativeJavaRandom *,double *);
bool NativeJavaRandom_nextBytes(NativeJavaRandom *,uint8_t *,size_t length);
/* Plain-state invalid arguments return false before any mutation. Managed
   failures additionally fail the heap, the native source-exception boundary.
   Output arguments remain unchanged on failure. Uncached Gaussian additionally
   requires the NativeStrictMath numerical environment (nearest-even, ordinary
   IEEE binary64, gradual underflow, no floating contraction/fast math). An
   unsupported environment fails before consuming draws; cached Gaussian only
   returns stored bits and consumes no draws. setSeed clears cache validity and
   preserves its stale double bits. Callers retain managed
   references as usual; operations allocate no objects or invoke callbacks.
   Managed wrappers obey the single-writer heap contract. Process concurrency
   belongs to the separate runtime service, not this state implementation.
   nextBytes takes a native buffer of at least length bytes; a NULL buffer is
   invalid even for length zero, matching a null Java array exception. */
#endif
