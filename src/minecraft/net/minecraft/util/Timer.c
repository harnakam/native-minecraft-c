#include "util/Timer.h"
#include "util/MathHelper.h"
#include <limits.h>
#include <math.h>
#include <string.h>

static void trace(MCObject *o,MCObjectVisitor visitor,void *context) {
    Timer *timer=(Timer *)o; timer->context=visitor(timer->context,context);
}
static const MCObjectClass klass={"Timer",MCObjectHeap_plainClone,trace,NULL};
bool Timer_isInstance(const MCObject *o) {
    return o && o->klass==&klass && MCObjectHeap_objectSize(o)>=sizeof(Timer);
}
static bool complete(const TimerDependencies *d) { return d && d->getSystemTime && d->nanoTime; }
static int64_t signed_bits(uint64_t value) { int64_t result; memcpy(&result,&value,sizeof result); return result; }
static int32_t java_int(float value) {
    if (isnan(value)) return 0;
    if (value>=2147483648.0f) return INT32_MAX;
    if (value<=-2147483648.0f) return INT32_MIN;
    return (int32_t)value;
}
static bool clock_result(MCObjectHeap *h,bool ok) {
    if (!ok) MCObjectHeap_fail(h);
    return ok && !MCObjectHeap_failed(h);
}
Timer *Timer_new(MCObjectHeap *h,float tps,const TimerDependencies *d,MCObject *context) {
    MCObjectRootScope scope={0};
    if (!complete(d) || !MCObjectRootScope_begin(&scope,h)) { MCObjectHeap_fail(h); return NULL; }
    bool ok=MCObjectRootScope_pin(&scope,context);
    Timer *t=ok ? (Timer *)MCObjectHeap_alloc(h,sizeof *t,&klass) : NULL;
    if (t) {
        t->context=context; t->dependencies=d;
        t->timerSpeed=1.0f; t->timeSyncAdjustment=1.0;
        t->ticksPerSecond=tps;
        int64_t value=0;
        ok=clock_result(h,d->getSystemTime(context,&value));
        if (ok) {
            t->lastSyncSysClock=value;
            ok=clock_result(h,d->nanoTime(context,&value));
            if (ok) t->lastSyncHRClock=value/1000000;
        }
    } else ok=false;
    if (!ok) MCObjectHeap_fail(h);
    MCObjectRootScope_end(&scope); return ok ? t : NULL;
}
bool Timer_updateTimer(Timer *t) {
    MCObjectHeap *h=t ? t->object.heap : NULL;
    MCObjectRootScope scope={0};
    if (!Timer_isInstance((MCObject *)t) || !complete(t->dependencies) ||
        !MCObjectRootScope_begin(&scope,h)) { MCObjectHeap_fail(h); return false; }
    const TimerDependencies *d=t->dependencies;
    bool ok=MCObjectRootScope_pin(&scope,t->context);
    int64_t i=0,nano=0;
    if (ok) ok=clock_result(h,d->getSystemTime(t->context,&i));
    if (!ok) { MCObjectRootScope_end(&scope); return false; }
    int64_t j=signed_bits((uint64_t)i-(uint64_t)t->lastSyncSysClock);
    ok=MCObjectRootScope_pin(&scope,t->context);
    if (ok) ok=clock_result(h,d->nanoTime(t->context,&nano));
    if (!ok) { MCObjectRootScope_end(&scope); return false; }
    int64_t k=nano/1000000;
    double d0=(double)k/1000.0;
    if (j<=1000 && j>=0) {
        t->counter=signed_bits((uint64_t)t->counter+(uint64_t)j);
        if (t->counter>1000) {
            int64_t l=signed_bits((uint64_t)k-(uint64_t)t->lastSyncHRClock);
            double d1=(double)t->counter/(double)l;
            /* Separate rounded operations preserve the original JVM expression
               even on hosts whose optimizer would contract multiply/add. */
            volatile double difference=d1-t->timeSyncAdjustment;
            volatile double correction=difference*0.20000000298023224;
            t->timeSyncAdjustment=t->timeSyncAdjustment+correction;
            t->lastSyncHRClock=k;
            t->counter=0;
        }
        if (t->counter<0) t->lastSyncHRClock=k;
    } else t->lastHRTime=d0;
    t->lastSyncSysClock=i;
    volatile double difference=d0-t->lastHRTime;
    double d2=difference*t->timeSyncAdjustment;
    t->lastHRTime=d0;
    d2=MathHelper_clamp_double(d2,0.0,1.0);
    volatile double scaled=d2*(double)t->timerSpeed;
    scaled=scaled*(double)t->ticksPerSecond;
    t->elapsedPartialTicks=(float)((double)t->elapsedPartialTicks+scaled);
    t->elapsedTicks=java_int(t->elapsedPartialTicks);
    t->elapsedPartialTicks=t->elapsedPartialTicks-(float)t->elapsedTicks;
    if (t->elapsedTicks>10) t->elapsedTicks=10;
    t->renderPartialTicks=t->elapsedPartialTicks;
    MCObjectRootScope_end(&scope); return !MCObjectHeap_failed(h);
}
