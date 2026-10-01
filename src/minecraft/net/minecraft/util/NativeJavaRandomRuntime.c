#include "util/NativeJavaRandomRuntime.h"
#include "client/native_timer_clock.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

struct NativeJavaRandomRuntime {
    atomic_flag lock;
    NativeJavaRandomRuntimeDependencies dependencies;
    void *context;
    uint64_t seedUniquifier;
    bool mathInitialized;
    NativeJavaRandomState mathState;
};
static _Thread_local unsigned clockDepth;
static bool platform_nano(void *context,int64_t *value) {
    (void)context;
    /* Reuse the checked native clocks, not network timing or a guessed seed. */
    return mc_client_timer_clocks()->nanoTime(NULL,value);
}
static NativeJavaRandomRuntime processRuntime={
    .lock=ATOMIC_FLAG_INIT,.dependencies={platform_nano},
    .seedUniquifier=UINT64_C(8682522807148012)
};
static bool lock_runtime(NativeJavaRandomRuntime *runtime) {
    if (!runtime||clockDepth||!runtime->dependencies.nanoTime) return false;
    while (atomic_flag_test_and_set_explicit(&runtime->lock,memory_order_acquire)) {}
    return true;
}
static void unlock_runtime(NativeJavaRandomRuntime *runtime) {
    atomic_flag_clear_explicit(&runtime->lock,memory_order_release);
}
/* Caller already owns the service lock. Advance-before-clock is observable
   even when the native clock fails. Java signed64 multiplication/XOR wrap. */
static bool next_seed_locked(NativeJavaRandomRuntime *runtime,int64_t *value) {
    runtime->seedUniquifier*=UINT64_C(181783497276652981);
    int64_t nano=0;
    ++clockDepth;
    bool ok=runtime->dependencies.nanoTime(runtime->context,&nano);
    --clockDepth;
    if (!ok) return false;
    uint64_t bits=runtime->seedUniquifier^(uint64_t)nano;
    memcpy(value,&bits,sizeof bits);
    return true;
}
NativeJavaRandomRuntime *NativeJavaRandomRuntime_new(const NativeJavaRandomRuntimeDependencies *dependencies,void *context) {
    if (!dependencies||!dependencies->nanoTime) return NULL;
    NativeJavaRandomRuntime *runtime=malloc(sizeof *runtime);
    if (runtime) *runtime=(NativeJavaRandomRuntime){.lock=ATOMIC_FLAG_INIT,
        .dependencies=*dependencies,.context=context,.seedUniquifier=UINT64_C(8682522807148012)};
    return runtime;
}
bool NativeJavaRandomRuntime_free(NativeJavaRandomRuntime *runtime) {
    if (runtime==&processRuntime||clockDepth) return false;
    free(runtime); return true;
}
NativeJavaRandomRuntime *NativeJavaRandomRuntime_process(void) { return &processRuntime; }
bool NativeJavaRandomRuntime_nextSeed(NativeJavaRandomRuntime *runtime,int64_t *value) {
    if (!value||!lock_runtime(runtime)) return false;
    int64_t seed=0; bool ok=next_seed_locked(runtime,&seed);
    unlock_runtime(runtime);
    if (ok) *value=seed;
    return ok;
}
NativeJavaRandom *NativeJavaRandomRuntime_newRandom(NativeJavaRandomRuntime *runtime,MCObjectHeap *heap) {
    if (!heap||!runtime||clockDepth) {MCObjectHeap_fail(heap);return NULL;}
    MCObjectRootScope scope={0};
    if (!MCObjectRootScope_begin(&scope,heap)) return NULL;
    /* The native Random identity is allocated before its no-argument seed
       segment, as with Java object allocation followed by construction. An
       allocation failure therefore never consumes external process entropy. */
    NativeJavaRandom *random=NativeJavaRandom_new(heap,0);
    int64_t seed;
    bool ok=random&&NativeJavaRandomRuntime_nextSeed(runtime,&seed)&&NativeJavaRandom_setSeed(random,seed);
    if (!ok) MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);
    return ok?random:NULL;
}
bool NativeJavaRandomRuntime_mathRandom(NativeJavaRandomRuntime *runtime,double *value) {
    if (!value||!lock_runtime(runtime)) return false;
    bool ok=true;
    if (!runtime->mathInitialized) {
        int64_t seed;
        ok=next_seed_locked(runtime,&seed)&&NativeJavaRandomState_setSeed(&runtime->mathState,seed);
        if (ok) runtime->mathInitialized=true;
    }
    double result=0;
    if (ok) ok=NativeJavaRandomState_nextDouble(&runtime->mathState,&result);
    unlock_runtime(runtime);
    if (ok) *value=result;
    return ok;
}
