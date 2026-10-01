#ifndef C919_NATIVE_NBT_SIZE_TRACKER_H
#define C919_NATIVE_NBT_SIZE_TRACKER_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { int64_t max,read; bool infinite,failed; } NBTSizeTracker;
void NBTSizeTracker_init(NBTSizeTracker *tracker,int64_t max);
void NBTSizeTracker_initInfinite(NBTSizeTracker *tracker);
bool NBTSizeTracker_read(NBTSizeTracker *tracker,int64_t bits);
#endif
