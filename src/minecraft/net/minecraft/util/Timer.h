#ifndef C919_SOURCE_TIMER_H
#define C919_SOURCE_TIMER_H
#include "util/MCObjectHeap.h"

/* Native bindings for the two original clock calls. The immutable table must
   outlive its Timers; context is a managed reference, or NULL for stateless
   platform clocks. false models a failing call, before its assignment. */
typedef struct {
    bool (*getSystemTime)(MCObject *context, int64_t *value);
    bool (*nanoTime)(MCObject *context, int64_t *value);
} TimerDependencies;
typedef struct Timer {
    MCObject object;
    float ticksPerSecond;
    double lastHRTime;
    int32_t elapsedTicks;
    float renderPartialTicks, timerSpeed, elapsedPartialTicks;
    int64_t lastSyncSysClock, lastSyncHRClock, counter;
    double timeSyncAdjustment;
    const TimerDependencies *dependencies;
    MCObject *context;
} Timer;

/* Translated constructor and updateTimer; allocation/lifetime/clock dispatch
   are native environment boundaries, not a Minecraft.runGameLoop port. */
Timer *Timer_new(MCObjectHeap *heap, float ticksPerSecond,
                 const TimerDependencies *dependencies, MCObject *context);
bool Timer_updateTimer(Timer *timer);
bool Timer_isInstance(const MCObject *object);
#endif
