#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "util/NativeCalendar.h"
#include <time.h>
static const MCObjectClass klass={"native.HostCalendar",MCObjectHeap_plainClone,NULL,NULL};
bool NativeCalendar_isInstance(const MCObject *object){return object&&object->klass==&klass&&MCObjectHeap_objectSize(object)>=sizeof(NativeCalendar);}
NativeCalendar *NativeCalendar_getInstance(MCObjectHeap *heap) {
    struct timespec stamp;struct tm fields;
    if(timespec_get(&stamp,TIME_UTC)!=TIME_UTC){MCObjectHeap_fail(heap);return NULL;}
#ifdef _WIN32
    if(localtime_s(&fields,&stamp.tv_sec)!=0){MCObjectHeap_fail(heap);return NULL;}
#else
    if(!localtime_r(&stamp.tv_sec,&fields)){MCObjectHeap_fail(heap);return NULL;}
#endif
    if(stamp.tv_sec>INT64_MAX/1000||stamp.tv_sec<INT64_MIN/1000||stamp.tv_nsec<0||stamp.tv_nsec>=1000000000L||
       (int64_t)stamp.tv_sec*1000>INT64_MAX-stamp.tv_nsec/1000000){MCObjectHeap_fail(heap);return NULL;}
    NativeCalendar *calendar=(NativeCalendar *)MCObjectHeap_alloc(heap,sizeof(*calendar),&klass);
    if(calendar) {
        calendar->timeInMillis=(int64_t)stamp.tv_sec*1000+stamp.tv_nsec/1000000;
        calendar->year=fields.tm_year+1900;calendar->month=fields.tm_mon;calendar->day=fields.tm_mday;
        calendar->hour=fields.tm_hour;calendar->minute=fields.tm_min;calendar->second=fields.tm_sec;
        calendar->dayOfWeek=fields.tm_wday+1;calendar->dayOfYear=fields.tm_yday+1;calendar->daylightSaving=fields.tm_isdst>0;
    }
    return calendar;
}
