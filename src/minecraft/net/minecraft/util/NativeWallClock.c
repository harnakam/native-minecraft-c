#include "util/NativeWallClock.h"
#include <limits.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif
bool NativeWallClock_currentTimeMillis(MCObject *context,int64_t *out) {
    bool ok=out!=NULL;
    int64_t result=0;
#ifdef _WIN32
    FILETIME time;
    if(ok) {
        GetSystemTimeAsFileTime(&time);
        uint64_t ticks=((uint64_t)time.dwHighDateTime<<32)|time.dwLowDateTime;
        const uint64_t epoch=UINT64_C(116444736000000000);
        if(ticks>=epoch)result=(int64_t)((ticks-epoch)/10000);
        else result=-(int64_t)((epoch-ticks)/10000);
    }
#else
    struct timespec time;
    if(ok)ok=timespec_get(&time,TIME_UTC)==TIME_UTC&&time.tv_nsec>=0&&time.tv_nsec<1000000000L;
    if(ok) {
        int64_t seconds=(int64_t)time.tv_sec;
        ok=seconds>=INT64_MIN/1000&&seconds<=INT64_MAX/1000;
        if(ok) {
            result=seconds*1000;
            int64_t fraction=time.tv_nsec/1000000L;
            ok=result<=INT64_MAX-fraction;
            if(ok)result+=fraction;
        }
    }
#endif
    if(!ok){MCObjectHeap_fail(context?context->heap:NULL);return false;}
    *out=result;return true;
}
