#ifndef C919_NATIVE_CALENDAR_H
#define C919_NATIVE_CALENDAR_H
#include "util/MCObjectHeap.h"
/* Checked host Calendar.getInstance creation boundary. It captures the real
   wall clock and host local calendar fields; JDK Calendar subclasses, locale,
   mutation, serialization and virtual methods are separate dependencies. */
typedef struct {MCObject object;int64_t timeInMillis;int32_t year,month,day,hour,minute,second,dayOfWeek,dayOfYear;bool daylightSaving;} NativeCalendar;
NativeCalendar *NativeCalendar_getInstance(MCObjectHeap *);
bool NativeCalendar_isInstance(const MCObject *);
#endif
