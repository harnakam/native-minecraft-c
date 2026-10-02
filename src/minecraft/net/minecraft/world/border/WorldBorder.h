#ifndef C919_SOURCE_WORLD_BORDER_H
#define C919_SOURCE_WORLD_BORDER_H
#include "util/NativeReferenceList.h"
#include "entity/Entity.h"
#include "util/BlockPos.h"

typedef struct WorldBorder WorldBorder;
typedef struct {
    double (*getCenterX)(WorldBorder *);
    double (*getCenterZ)(WorldBorder *);
    bool (*minX)(WorldBorder *,double *);
    bool (*minZ)(WorldBorder *,double *);
    bool (*maxX)(WorldBorder *,double *);
    bool (*maxZ)(WorldBorder *,double *);
    bool (*containsBlockPos)(WorldBorder *,BlockPos *,bool *);
} WorldBorderOverrides;
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
/* Reached Vec3i virtual getter boundary for contains(BlockPos). This is
   separate from clocks/listeners and does not claim arbitrary subclasses.
   NULL uses the translated Vec3i getter; a supplied table must
   contain each getter when that getter is actually reached. */
typedef struct {
    bool (*getX)(MCObject *,BlockPos *,int32_t *);
    bool (*getZ)(MCObject *,BlockPos *,int32_t *);
} WorldBorderPositionDependencies;
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
    const WorldBorderOverrides *overrides;
    const WorldBorderPositionDependencies *positionDependencies;
    MCObject *positionContext;
};
WorldBorder *WorldBorder_nativeAllocate(MCObjectHeap *,const WorldBorderDependencies *,MCObject *);
bool WorldBorder_construct(WorldBorder *);
WorldBorder *WorldBorder_new(MCObjectHeap *,const WorldBorderDependencies *,MCObject *context);
bool WorldBorder_isInstance(const MCObject *);
void WorldBorder_traceFields(WorldBorder *,MCObjectVisitor,void *);
WorldBorderStatus WorldBorder_getStatus(WorldBorder *);
double WorldBorder_getCenterX(WorldBorder *);
double WorldBorder_getCenterZ(WorldBorder *);
double WorldBorder_getCenterXBase(WorldBorder *);
double WorldBorder_getCenterZBase(WorldBorder *);
/* Boolean output separates an original false result from native failure;
   output is preserved on a reached dependency/receiver failure. */
bool WorldBorder_containsBlockPos(WorldBorder *,BlockPos *,bool *out);
bool WorldBorder_containsBlockPosBase(WorldBorder *,BlockPos *,bool *out);
bool WorldBorder_getDiameter(WorldBorder *,double *out);
bool WorldBorder_minX(WorldBorder *,double *out);
bool WorldBorder_minZ(WorldBorder *,double *out);
bool WorldBorder_maxX(WorldBorder *,double *out);
bool WorldBorder_maxZ(WorldBorder *,double *out);
bool WorldBorder_minXBase(WorldBorder *,double *out);
bool WorldBorder_minZBase(WorldBorder *,double *out);
bool WorldBorder_maxXBase(WorldBorder *,double *out);
bool WorldBorder_maxZBase(WorldBorder *,double *out);
bool WorldBorder_getClosestDistance(WorldBorder *,double x,double z,double *out);
bool WorldBorder_getClosestDistanceEntity(WorldBorder *,Entity *,double *out);
bool WorldBorder_setTransition(WorldBorder *,double newSize);
bool WorldBorder_setTransitionTimed(WorldBorder *,double oldSize,double newSize,int64_t time);
NativeReferenceList *WorldBorder_getListeners(WorldBorder *);
bool WorldBorder_addListener(WorldBorder *,MCObject *listener);
bool WorldBorder_setCenter(WorldBorder *,double x,double z);
bool WorldBorder_setSize(WorldBorder *,int32_t size);
int32_t WorldBorder_getSize(WorldBorder *);
/* Remaining contains overloads/damage/warning/time/resize methods are not declared until
   ported. The above bodies plus all original instance fields/constructor are
   supplied; this does not claim the whole WorldBorder or JDK collections. */
#endif
