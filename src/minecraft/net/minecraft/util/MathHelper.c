#include "util/MathHelper.h"
#include <limits.h>
#include <math.h>
#include <stdatomic.h>
#include <stdint.h>

static float sin_table[65536];
static atomic_uint initialized;
static void initialize(void) {
    unsigned expected=0;
    if (atomic_compare_exchange_strong_explicit(&initialized,&expected,1,memory_order_acq_rel,memory_order_acquire)) {
        for (uint32_t i=0;i<65536;i++) {
            volatile double angle=(double)i*3.141592653589793;
            angle=angle*2.0; angle=angle/65536.0;
            sin_table[i]=(float)sin(angle);
        }
        atomic_store_explicit(&initialized,2,memory_order_release);
    } else while (atomic_load_explicit(&initialized,memory_order_acquire)!=2) { }
}
static int32_t java_int(float value) {
    if (isnan(value)) return 0;
    if (value>=2147483648.0f) return INT32_MAX;
    if (value<=-2147483648.0f) return INT32_MIN;
    return (int32_t)value;
}
float MathHelper_sin(float value) {
    initialize(); volatile float scaled=value*10430.378f;
    return sin_table[(uint32_t)java_int(scaled)&65535];
}
float MathHelper_cos(float value) {
    initialize(); volatile float scaled=value*10430.378f;
    scaled=scaled+16384.0f;
    return sin_table[(uint32_t)java_int(scaled)&65535];
}
double MathHelper_clamp_double(double value,double minimum,double maximum) {
    return value<minimum ? minimum : (value>maximum ? maximum : value);
}
float MathHelper_clamp_float(float value,float minimum,float maximum) {
    return value<minimum ? minimum : (value>maximum ? maximum : value);
}

NativeJavaUUID *MathHelper_getRandomUuid(NativeJavaRandom *random) {
    if (!random) return NULL;
    MCObjectHeap *heap=random->object.heap;
    if (!NativeJavaRandom_isInstance((const MCObject*)random)) {
        MCObjectHeap_fail(heap); return NULL;
    }
    MCObjectRootScope scope={0};
    if (!MCObjectRootScope_begin(&scope,heap) || !MCObjectRootScope_pin(&scope,(MCObject*)random)) {
        MCObjectHeap_fail(heap); MCObjectRootScope_end(&scope); return NULL;
    }
    NativeJavaUUID *uuid=NULL; int64_t first,second;
    if (!NativeJavaRandom_nextLong(random,&first)) goto done;
    uint64_t i=((uint64_t)first&UINT64_C(0xffffffffffff0fff))|UINT64_C(0x4000);
    if (!NativeJavaRandom_nextLong(random,&second)) goto done;
    uint64_t j=((uint64_t)second&UINT64_C(0x3fffffffffffffff))|UINT64_C(0x8000000000000000);
    /* Unsigned masks preserve Java long bits without signed C overflow or
       implementation-defined narrowing into the UUID platform adapter. */
    int64_t most=i<=INT64_MAX ? (int64_t)i : -1-(int64_t)(UINT64_MAX-i);
    int64_t least=j<=INT64_MAX ? (int64_t)j : -1-(int64_t)(UINT64_MAX-j);
    uuid=NativeJavaUUID_new(heap,most,least);
done:
    MCObjectRootScope_end(&scope); return uuid;
}
