#include "nbt/NBTSizeTracker.h"
#include <string.h>
void NBTSizeTracker_init(NBTSizeTracker *t,int64_t max) { if (t) { t->max=max;t->read=0;t->infinite=false;t->failed=false; } }
void NBTSizeTracker_initInfinite(NBTSizeTracker *t) { NBTSizeTracker_init(t,0); if (t) t->infinite=true; }
bool NBTSizeTracker_read(NBTSizeTracker *t,int64_t bits) {
    if (!t || t->failed) return false;
    if (t->infinite) return true;
    uint64_t raw=(uint64_t)t->read+(uint64_t)(bits/8);
    memcpy(&t->read,&raw,sizeof(raw));
    if (t->read>t->max) t->failed=true;
    return !t->failed;
}
