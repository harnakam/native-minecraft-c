#ifndef C919_NATIVE_WALL_CLOCK_H
#define C919_NATIVE_WALL_CLOCK_H
#include "util/MCObjectHeap.h"
/* Actual UTC Unix milliseconds for the System.currentTimeMillis dependency.
   This is distinct from native monotonic scheduling and Source world ticks. */
bool NativeWallClock_currentTimeMillis(MCObject *context,int64_t *out);
#endif
