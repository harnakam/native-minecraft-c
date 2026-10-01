#ifndef C919_SOURCE_WORLD_BORDER_H
#define C919_SOURCE_WORLD_BORDER_H
#include "util/NativeReferenceList.h"
#include "entity/Entity.h"

typedef struct WorldBorder WorldBorder;
/* Immutable ordinal facts of the original enum. Generated enum/JDK methods and
   arbitrary Java getter subclasses are not supplied by this native adapter. */
typedef enum {
    WORLD_BORDER_GROWING=0,
    WORLD_BORDER_SHRINKING=1,
    WORLD_BORDER_STATIONARY=2
} WorldBorderStatus;
typedef struct {
    bool (*currentTimeMillis)(MCObject *context,int64_t *out);
    bool (*onSizeChanged)(MCObject *context,MCObject *listener,WorldBorder *,double size);
    bool (*onTransitionStarted)(MCObject *context,MCObject *listener,WorldBorder *,
                                double oldSize,double newSize,int64_t time);
    bool (*onCenterChanged)(MCObject *context,MCObject *listener,WorldBorder *,double x,double z);
} WorldBorderDependencies;
struct WorldBorder {
    MCObject object;
    NativeReferenceList *listeners;
    double centerX,centerZ,startDiameter,endDiameter;
    int64_t endTime,startTime;
    int32_t worldSize;
    double damageAmount,damageBuffer;
    int32_t warningTime,warningDistance;
    /* Required clock/listener interfaces are reached lazily. Context is traced;
       immutable methods must outlive the graph. No fake default clock/effect. */
    const WorldBorderDependencies *dependencies;
    MCObject *dependencyContext;
};
WorldBorder *WorldBorder_new(MCObjectHeap *,const WorldBorderDependencies *,MCObject *context);
bool WorldBorder_isInstance(const MCObject *);
WorldBorderStatus WorldBorder_getStatus(WorldBorder *);
double WorldBorder_getCenterX(WorldBorder *);
double WorldBorder_getCenterZ(WorldBorder *);
bool WorldBorder_getDiameter(WorldBorder *,double *out);
bool WorldBorder_minX(WorldBorder *,double *out);
bool WorldBorder_minZ(WorldBorder *,double *out);
bool WorldBorder_maxX(WorldBorder *,double *out);
bool WorldBorder_maxZ(WorldBorder *,double *out);
bool WorldBorder_getClosestDistance(WorldBorder *,double x,double z,double *out);
bool WorldBorder_getClosestDistanceEntity(WorldBorder *,Entity *,double *out);
bool WorldBorder_setTransition(WorldBorder *,double newSize);
bool WorldBorder_setTransitionTimed(WorldBorder *,double oldSize,double newSize,int64_t time);
NativeReferenceList *WorldBorder_getListeners(WorldBorder *);
bool WorldBorder_addListener(WorldBorder *,MCObject *listener);
bool WorldBorder_setCenter(WorldBorder *,double x,double z);
bool WorldBorder_setSize(WorldBorder *,int32_t size);
int32_t WorldBorder_getSize(WorldBorder *);
/* Remaining contains/damage/warning/time/resize methods are not declared until
   ported. The above bodies plus all original instance fields/constructor are
   supplied; this does not claim the whole WorldBorder or JDK collections. */
#endif
