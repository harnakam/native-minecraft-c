#ifndef C919_NATIVE_JAVA_RANDOM_RUNTIME_H
#define C919_NATIVE_JAVA_RANDOM_RUNTIME_H
#include "util/NativeJavaRandom.h"

/* Native process service for the Java no-argument seed and Math.random API.
   It is outside the gameplay heap: neither snapshots nor aborts restore its
   seed uniquifier or shared lazy Math stream. This is not a JDK class port.
   The immutable clock dependency is a checked platform System.nanoTime view.
   Its native context must outlive the service. Re-entry from that callback
   into any RNG service fails promptly instead of attempting another lock. */
typedef struct NativeJavaRandomRuntime NativeJavaRandomRuntime;
typedef struct {
    bool (*nanoTime)(void *context,int64_t *value);
} NativeJavaRandomRuntimeDependencies;
NativeJavaRandomRuntime *NativeJavaRandomRuntime_new(const NativeJavaRandomRuntimeDependencies *,void *context);
/* Call after all users/graphs/threads are gone. The process singleton is never
   freed; passing it returns false. Independent services are native test/host
   execution contexts, not separate Random instances within one Java process. */
bool NativeJavaRandomRuntime_free(NativeJavaRandomRuntime *);
NativeJavaRandomRuntime *NativeJavaRandomRuntime_process(void);
bool NativeJavaRandomRuntime_nextSeed(NativeJavaRandomRuntime *,int64_t *value);
NativeJavaRandom *NativeJavaRandomRuntime_newRandom(NativeJavaRandomRuntime *,MCObjectHeap *);
bool NativeJavaRandomRuntime_mathRandom(NativeJavaRandomRuntime *,double *value);
#endif
