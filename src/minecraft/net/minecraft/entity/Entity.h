#ifndef C919_SOURCE_ENTITY_H
#define C919_SOURCE_ENTITY_H
#include "util/MCObjectHeap.h"

/* Original inherited Entity method. EntityItem has no override. This body is
   intentionally empty in Entity.java; it supplies no native success hook.
   The complete Entity state, constructors, physics and virtual methods remain
   separate dependencies. The receiver is the actual managed entity identity. */
void Entity_onDataWatcherUpdate(MCObject *entity, int32_t dataID);
#endif
