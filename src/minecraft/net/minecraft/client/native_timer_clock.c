#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "client/native_timer_clock.h"
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#ifdef _WIN32
/* floor(remainder * 1e9 / divisor), without an overflowing multiplication.
   QueryPerformanceFrequency is positive signed64, remainder < divisor. */
static uint64_t fraction_ns(uint64_t remainder,uint64_t divisor) {
    uint64_t quotient=0,rest=0;
    for (int bit=29;bit>=0;--bit) {
        quotient*=2;
        if (rest>=divisor-rest) { rest-=divisor-rest; ++quotient; }
        else rest*=2;
        if ((UINT64_C(1000000000)>>bit)&1u) {
            if (rest>=divisor-remainder) { rest-=divisor-remainder; ++quotient; }
            else rest+=remainder;
        }
    }
    return quotient;
}
#else
static bool monotonic(struct timespec *time) {
    return clock_gettime(CLOCK_MONOTONIC,time)==0 && time->tv_sec>=0 &&
        time->tv_nsec>=0 && time->tv_nsec<1000000000L;
}
#endif
static bool system_time(MCObject *context,int64_t *value) {
    (void)context; if (!value) return false;
#ifdef _WIN32
    uint64_t ms=(uint64_t)GetTickCount64(); if (ms>INT64_MAX) return false;
    *value=(int64_t)ms;
#else
    struct timespec time; if (!monotonic(&time)) return false;
    uint64_t seconds=(uint64_t)time.tv_sec,ms=(uint64_t)time.tv_nsec/1000000u;
    if (seconds>((uint64_t)INT64_MAX-ms)/1000u) return false;
    *value=(int64_t)(seconds*1000u+ms);
#endif
    return true;
}
static bool nano_time(MCObject *context,int64_t *value) {
    (void)context; if (!value) return false;
#ifdef _WIN32
    LARGE_INTEGER counter,frequency;
    if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart<=0 ||
        !QueryPerformanceCounter(&counter) || counter.QuadPart<0) return false;
    uint64_t count=(uint64_t)counter.QuadPart,divisor=(uint64_t)frequency.QuadPart;
    uint64_t seconds=count/divisor,ns=fraction_ns(count%divisor,divisor);
    if (seconds>((uint64_t)INT64_MAX-ns)/UINT64_C(1000000000)) return false;
    *value=(int64_t)(seconds*UINT64_C(1000000000)+ns);
#else
    struct timespec time; if (!monotonic(&time)) return false;
    uint64_t seconds=(uint64_t)time.tv_sec,ns=(uint64_t)time.tv_nsec;
    if (seconds>((uint64_t)INT64_MAX-ns)/UINT64_C(1000000000)) return false;
    *value=(int64_t)(seconds*UINT64_C(1000000000)+ns);
#endif
    return true;
}
const TimerDependencies *mc_client_timer_clocks(void) {
    static const TimerDependencies dependencies={system_time,nano_time}; return &dependencies;
}
