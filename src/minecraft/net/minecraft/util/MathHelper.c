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
