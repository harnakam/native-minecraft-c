#include "util/NativeEntityIDRuntime.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
struct NativeEntityIDRuntime {_Atomic uint32_t next;};
static NativeEntityIDRuntime process={.next=ATOMIC_VAR_INIT(0)};
NativeEntityIDRuntime *NativeEntityIDRuntime_process(void) {return &process;}
NativeEntityIDRuntime *NativeEntityIDRuntime_new(int32_t initial) {
    NativeEntityIDRuntime *runtime=malloc(sizeof *runtime);
    if(runtime)atomic_init(&runtime->next,(uint32_t)initial);
    return runtime;
}
bool NativeEntityIDRuntime_free(NativeEntityIDRuntime *runtime) {
    if(runtime==&process)return false;
    free(runtime);return true;
}
bool NativeEntityIDRuntime_next(NativeEntityIDRuntime *runtime,int32_t *out) {
    if(!runtime||!out)return false;
    uint32_t bits=atomic_fetch_add_explicit(&runtime->next,1,memory_order_relaxed);
    memcpy(out,&bits,sizeof bits);return true;
}
