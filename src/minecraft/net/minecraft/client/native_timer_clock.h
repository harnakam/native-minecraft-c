#ifndef C919_CLIENT_NATIVE_TIMER_CLOCK_H
#define C919_CLIENT_NATIVE_TIMER_CLOCK_H
#include "util/Timer.h"

/* Checked native mappings for LWJGL system milliseconds and System.nanoTime.
   These are real monotonic platform clocks, not translated JDK/LWJGL classes.
   Reads remain distinct and keep the platform clock origin; no warm-up offset.
   The immutable table needs no managed context or platform resource lifetime. */
const TimerDependencies *mc_client_timer_clocks(void);
#endif
