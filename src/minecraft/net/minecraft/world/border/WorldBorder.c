#include "world/border/WorldBorder.h"
#include "world/WorldProviderHell.h"
#include <math.h>
#include <string.h>

void WorldBorder_traceFields(WorldBorder *border,MCObjectVisitor visitor,void *context) {
    MCObject *object=(MCObject *)border;
    if(MCObjectHeap_objectSize(object)<sizeof(*border)){MCObjectHeap_fail(object->heap);return;}
    border->listeners=(NativeReferenceList *)visitor((MCObject *)border->listeners,context);
    border->dependencyContext=visitor(border->dependencyContext,context);
    border->positionContext=visitor(border->positionContext,context);
}
static void trace(MCObject *object,MCObjectVisitor visitor,void *context) { WorldBorder_traceFields((WorldBorder *)object,visitor,context); }
static const MCObjectClass borderClass={"net.minecraft.world.border.WorldBorder",MCObjectHeap_plainClone,trace,NULL};
bool WorldBorder_isInstance(const MCObject *object) {
    return object&&((object->klass==&borderClass&&MCObjectHeap_objectSize(object)>=sizeof(WorldBorder))||WorldProviderHellBorder_isInstance(object));
}
static bool failed(WorldBorder *border) {MCObjectHeap_fail(border?border->object.heap:NULL);return false;}
static bool valid(WorldBorder *border) {
    return WorldBorder_isInstance((MCObject *)border)&&!MCObjectHeap_failed(border->object.heap)?true:failed(border);
}
static bool begin(WorldBorder *border,MCObjectRootScope *scope) {
    if(!valid(border)||!MCObjectRootScope_begin(scope,border->object.heap))return false;
    if(MCObjectRootScope_pin(scope,(MCObject *)border))return true;
    MCObjectRootScope_end(scope);return false;
}
static bool end(WorldBorder *border,MCObjectRootScope *scope,bool ok) {
    ok=ok&&!MCObjectHeap_failed(border->object.heap);if(!ok)failed(border);
    MCObjectRootScope_end(scope);return ok;
}
WorldBorder *WorldBorder_nativeAllocate(MCObjectHeap *heap,const WorldBorderDependencies *deps,MCObject *context) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    WorldBorder *border=NULL;
    if(!MCObjectRootScope_pin(&scope,context))goto done;
    border=(WorldBorder *)MCObjectHeap_alloc(heap,sizeof(*border),&borderClass);
    if(!border)goto done;
    border->dependencies=deps;border->dependencyContext=context;
done:
    MCObjectRootScope_end(&scope);return border;
}
bool WorldBorder_construct(WorldBorder *border) {
    MCObjectRootScope scope={0};if(!begin(border,&scope))return false;
    MCObjectHeap *heap=border->object.heap;
    NativeReferenceList *listeners=NativeReferenceList_new(heap);
    if(!listeners)return end(border,&scope,false);
    border->listeners=listeners;
    border->centerX=0; border->centerZ=0; border->startDiameter=6.0E7;
    border->endDiameter=border->startDiameter;
    border->worldSize=29999984;border->damageAmount=0.2;border->damageBuffer=5;
    border->warningTime=15;border->warningDistance=5;MCObjectHeap_touch(heap);
    return end(border,&scope,true);
}
WorldBorder *WorldBorder_new(MCObjectHeap *heap,const WorldBorderDependencies *deps,MCObject *context) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,heap))return NULL;
    WorldBorder *border=WorldBorder_nativeAllocate(heap,deps,context);
    if(!border||!MCObjectRootScope_pin(&scope,(MCObject *)border)||!WorldBorder_construct(border))border=NULL;
    MCObjectRootScope_end(&scope);return border;
}
WorldBorderStatus WorldBorder_getStatus(WorldBorder *border) {
    if(!valid(border))return WORLD_BORDER_STATIONARY;
    return border->endDiameter<border->startDiameter?WORLD_BORDER_SHRINKING:
        border->endDiameter>border->startDiameter?WORLD_BORDER_GROWING:WORLD_BORDER_STATIONARY;
}
double WorldBorder_getCenterXBase(WorldBorder *border) {return valid(border)?border->centerX:NAN;}
double WorldBorder_getCenterZBase(WorldBorder *border) {return valid(border)?border->centerZ:NAN;}
double WorldBorder_getCenterX(WorldBorder *border) {
    if(!valid(border))return NAN;
    if(!border->overrides||!border->overrides->getCenterX)return border->centerX;
    MCObjectRootScope scope={0};if(!begin(border,&scope))return NAN;
    double value=border->overrides->getCenterX(border);
    if(!end(border,&scope,!MCObjectHeap_failed(border->object.heap)))return NAN;
    return value;
}
double WorldBorder_getCenterZ(WorldBorder *border) {
    if(!valid(border))return NAN;
    if(!border->overrides||!border->overrides->getCenterZ)return border->centerZ;
    MCObjectRootScope scope={0};if(!begin(border,&scope))return NAN;
    double value=border->overrides->getCenterZ(border);
    if(!end(border,&scope,!MCObjectHeap_failed(border->object.heap)))return NAN;
    return value;
}
static bool clock_now(WorldBorder *border,MCObjectRootScope *scope,int64_t *out) {
    const WorldBorderDependencies *deps=border->dependencies;
    MCObject *context=border->dependencyContext;
    return deps&&deps->currentTimeMillis&&MCObjectRootScope_pin(scope,context)&&
        deps->currentTimeMillis(context,out)&&!MCObjectHeap_failed(border->object.heap);
}
static int64_t sub64(int64_t a,int64_t b) {
    uint64_t bits=(uint64_t)a-(uint64_t)b;int64_t result;memcpy(&result,&bits,sizeof result);return result;
}
static int64_t add64(int64_t a,int64_t b) {
    uint64_t bits=(uint64_t)a+(uint64_t)b;int64_t result;memcpy(&result,&bits,sizeof result);return result;
}
bool WorldBorder_getDiameter(WorldBorder *border,double *out) {
    MCObjectRootScope scope={0};if(!begin(border,&scope))return false;
    bool ok=false;if(!out)goto done;
    if(WorldBorder_getStatus(border)!=WORLD_BORDER_STATIONARY) {
        int64_t now;if(!clock_now(border,&scope,&now))goto done;
        /* The clock is evaluated before both live time fields. Java performs
           both casts and division in float, then widens that result to double. */
        volatile float elapsed=(float)sub64(now,border->startTime);
        volatile float duration=(float)sub64(border->endTime,border->startTime);
        volatile float fraction=elapsed/duration;
        double delta=(double)fraction;
        /* The supplied decompiler renders this as delta < 1. Actual target
           dcmpl/iflt also takes this branch for NaN (e.g. 0/0 duration). */
        if(!(delta>=1.0)) {
            volatile double difference=border->endDiameter-border->startDiameter;
            volatile double product=difference*delta;
            *out=border->startDiameter+product;ok=true;goto done;
        }
        double target=border->endDiameter;
        if(!WorldBorder_setTransition(border,target))goto done;
    }
    *out=border->startDiameter;ok=true;
done:
    return end(border,&scope,ok);
}
static int32_t negative32(int32_t value) {
    uint32_t bits=UINT32_C(0)-(uint32_t)value;int32_t result;memcpy(&result,&bits,sizeof result);return result;
}
static bool edge(WorldBorder *border,double *out,bool x,bool maximum) {
    MCObjectRootScope scope={0};if(!begin(border,&scope))return false;
    bool ok=false;if(!out)goto done;
    double center=x?WorldBorder_getCenterX(border):WorldBorder_getCenterZ(border),diameter;
    if(MCObjectHeap_failed(border->object.heap))goto done;
    if(!WorldBorder_getDiameter(border,&diameter))goto done;
    double value=maximum?center+diameter/2.0:center-diameter/2.0;
    /* worldSize is read after the mutating diameter/getListeners call. The
       original minimum uses Java int unary negation, including INT_MIN wrap. */
    double limit=maximum?(double)border->worldSize:(double)negative32(border->worldSize);
    if(maximum?value>limit:value<limit)value=limit;
    *out=value;ok=true;
done:
    return end(border,&scope,ok);
}
bool WorldBorder_minXBase(WorldBorder *border,double *out) {return edge(border,out,true,false);}
bool WorldBorder_minZBase(WorldBorder *border,double *out) {return edge(border,out,false,false);}
bool WorldBorder_maxXBase(WorldBorder *border,double *out) {return edge(border,out,true,true);}
bool WorldBorder_maxZBase(WorldBorder *border,double *out) {return edge(border,out,false,true);}
static bool edge_dispatch(WorldBorder *border,double *out,bool x,bool maximum) {
    if(!valid(border))return false;
    const WorldBorderOverrides *v=border->overrides;
    bool (*method)(WorldBorder *,double *)=v?(x?(maximum?v->maxX:v->minX):(maximum?v->maxZ:v->minZ)):NULL;
    if(!method)return edge(border,out,x,maximum);
    MCObjectRootScope scope={0};if(!begin(border,&scope))return false;
    bool ok=out&&MCObjectRootScope_pin(&scope,border->dependencyContext)&&method(border,out);
    return end(border,&scope,ok);
}
bool WorldBorder_minX(WorldBorder *border,double *out) {return edge_dispatch(border,out,true,false);}
bool WorldBorder_minZ(WorldBorder *border,double *out) {return edge_dispatch(border,out,false,false);}
bool WorldBorder_maxX(WorldBorder *border,double *out) {return edge_dispatch(border,out,true,true);}
bool WorldBorder_maxZ(WorldBorder *border,double *out) {return edge_dispatch(border,out,false,true);}
static bool position_coordinate(WorldBorder *border,MCObjectRootScope *scope,BlockPos *position,bool x,int32_t *out) {
    if(!BlockPos_isInstance((MCObject *)position)||!MCObjectRootScope_pin(scope,(MCObject *)position))return false;
    const WorldBorderPositionDependencies *d=border->positionDependencies;
    if(!d)return (x?Vec3i_getX(&position->vec3i,out):Vec3i_getZ(&position->vec3i,out))==NATIVE_ARRAY_OK;
    if(!MCObjectRootScope_pin(scope,border->positionContext))return false;
    return (x?d->getX&&d->getX(border->positionContext,position,out):
        d->getZ&&d->getZ(border->positionContext,position,out))&&!MCObjectHeap_failed(border->object.heap);
}
bool WorldBorder_containsBlockPosBase(WorldBorder *border,BlockPos *position,bool *out) {
    MCObjectRootScope scope={0};if(!begin(border,&scope))return false;
    bool ok=false,result=false;double edgeValue;int32_t coordinate;
    if(!out||!position_coordinate(border,&scope,position,true,&coordinate))goto done;
    uint32_t bits=(uint32_t)coordinate+UINT32_C(1);memcpy(&coordinate,&bits,sizeof coordinate);
    if(!WorldBorder_minX(border,&edgeValue))goto done;
    if(!((double)coordinate>edgeValue))goto answered;
    if(!position_coordinate(border,&scope,position,true,&coordinate))goto done;
    if(!WorldBorder_maxX(border,&edgeValue))goto done;
    if(!((double)coordinate<edgeValue))goto answered;
    if(!position_coordinate(border,&scope,position,false,&coordinate))goto done;
    bits=(uint32_t)coordinate+UINT32_C(1);memcpy(&coordinate,&bits,sizeof coordinate);
    if(!WorldBorder_minZ(border,&edgeValue))goto done;
    if(!((double)coordinate>edgeValue))goto answered;
    if(!position_coordinate(border,&scope,position,false,&coordinate))goto done;
    if(!WorldBorder_maxZ(border,&edgeValue))goto done;
    result=(double)coordinate<edgeValue;
answered:
    *out=result;ok=true;
done:
    return end(border,&scope,ok);
}
bool WorldBorder_containsBlockPos(WorldBorder *border,BlockPos *position,bool *out) {
    if(!valid(border))return false;
    const WorldBorderOverrides *v=border->overrides;
    if(!v||!v->containsBlockPos)return WorldBorder_containsBlockPosBase(border,position,out);
    MCObjectRootScope scope={0};if(!begin(border,&scope))return false;
    /* A virtual override observes even a null position. Validation belongs
       to the reached base/getter body, after the override call. */
    bool ok=out&&MCObjectRootScope_pin(&scope,(MCObject *)position)&&
        MCObjectRootScope_pin(&scope,border->dependencyContext)&&v->containsBlockPos(border,position,out);
    return end(border,&scope,ok);
}
static double java_min(double a,double b) {
    if(isnan(a))return a;
    if(a==0.0&&b==0.0&&signbit(b))return b;
    return a<=b?a:b;
}
bool WorldBorder_getClosestDistance(WorldBorder *border,double x,double z,double *out) {
    MCObjectRootScope scope={0};if(!begin(border,&scope))return false;
    bool ok=false;double bound,d0,d1,d2,d3,d4;if(!out)goto done;
    if(!WorldBorder_minZ(border,&bound))goto done;
    d0=z-bound;
    if(!WorldBorder_maxZ(border,&bound))goto done;
    d1=bound-z;
    if(!WorldBorder_minX(border,&bound))goto done;
    d2=x-bound;
    if(!WorldBorder_maxX(border,&bound))goto done;
    d3=bound-x;
    d4=java_min(d2,d3);d4=java_min(d4,d0);*out=java_min(d4,d1);ok=true;
done:
    return end(border,&scope,ok);
}
bool WorldBorder_getClosestDistanceEntity(WorldBorder *border,Entity *entity,double *out) {
    MCObjectRootScope scope={0};if(!begin(border,&scope))return false;
    bool ok=Entity_isInstance((MCObject *)entity)&&MCObjectRootScope_pin(&scope,(MCObject *)entity);
    if(ok) {
        double x=entity->posX,z=entity->posZ;
        ok=WorldBorder_getClosestDistance(border,x,z,out);
    }
    return end(border,&scope,ok);
}
NativeReferenceList *WorldBorder_getListeners(WorldBorder *border) {
    MCObjectRootScope scope={0};if(!begin(border,&scope))return NULL;
    /* Java references cannot cross native heaps. Check this edge only when
       reached, before a shallow-copy allocation could mutate another graph. */
    NativeReferenceList *list=border->listeners;
    bool sameHeap=NativeReferenceList_isInstance((MCObject *)list)&&list->object.heap==border->object.heap;
    NativeReferenceList *snapshot=sameHeap?NativeReferenceList_copy(list):NULL;
    bool ok=snapshot&&MCObjectRootScope_pin(&scope,(MCObject *)snapshot);
    ok=end(border,&scope,ok);return ok?snapshot:NULL;
}
bool WorldBorder_addListener(WorldBorder *border,MCObject *listener) {
    MCObjectRootScope scope={0};if(!begin(border,&scope))return false;
    NativeReferenceList *list=border->listeners;
    bool ok=NativeReferenceList_isInstance((MCObject *)list)&&list->object.heap==border->object.heap&&
        MCObjectRootScope_pin(&scope,listener)&&NativeReferenceList_add(list,listener);
    return end(border,&scope,ok);
}
typedef enum {SIZE_EVENT,TRANSITION_EVENT,CENTER_EVENT} Event;
static bool notify(WorldBorder *border,MCObjectRootScope *scope,Event event,double a,double b,int64_t time) {
    NativeReferenceList *snapshot=WorldBorder_getListeners(border);
    if(!snapshot||!MCObjectRootScope_pin(scope,(MCObject *)snapshot))return false;
    uint32_t expected=snapshot->modCount;
    /* ArrayList iterator ordering: hasNext compares cursor with the current
       snapshot size; modCount is checked by next, not after the final callback.
       The snapshot keeps source-list mutations separate from this iteration. */
    int32_t cursor=0;
    for(;;) {
        int32_t size=NativeReferenceList_size(snapshot);
        if(MCObjectHeap_failed(border->object.heap))return false;
        if(cursor==size)return true;
        if(snapshot->modCount!=expected)return false;
        MCObject *listener=NativeReferenceList_get(snapshot,cursor);
        if(!listener||MCObjectHeap_failed(border->object.heap)||!MCObjectRootScope_pin(scope,listener))return false;
        const WorldBorderDependencies *deps=border->dependencies;
        MCObject *context=border->dependencyContext;
        if(!deps||!MCObjectRootScope_pin(scope,context))return false;
        bool ok=event==SIZE_EVENT?deps->onSizeChanged&&deps->onSizeChanged(context,listener,border,a):
            event==TRANSITION_EVENT?deps->onTransitionStarted&&deps->onTransitionStarted(context,listener,border,a,b,time):
            deps->onCenterChanged&&deps->onCenterChanged(context,listener,border,a,b);
        if(!ok||MCObjectHeap_failed(border->object.heap))return false;
        ++cursor;
    }
}
bool WorldBorder_setTransition(WorldBorder *border,double newSize) {
    MCObjectRootScope scope={0};if(!begin(border,&scope))return false;
    border->startDiameter=newSize;MCObjectHeap_touch(border->object.heap);
    border->endDiameter=newSize;MCObjectHeap_touch(border->object.heap);
    int64_t now;bool ok=clock_now(border,&scope,&now);
    if(ok) {
        border->endTime=now;border->startTime=border->endTime;MCObjectHeap_touch(border->object.heap);
        ok=notify(border,&scope,SIZE_EVENT,newSize,0,0);
    }
    return end(border,&scope,ok);
}
bool WorldBorder_setTransitionTimed(WorldBorder *border,double oldSize,double newSize,int64_t time) {
    MCObjectRootScope scope={0};if(!begin(border,&scope))return false;
    border->startDiameter=oldSize;MCObjectHeap_touch(border->object.heap);
    border->endDiameter=newSize;MCObjectHeap_touch(border->object.heap);
    int64_t now;bool ok=clock_now(border,&scope,&now);
    if(ok) {
        border->startTime=now;MCObjectHeap_touch(border->object.heap);
        border->endTime=add64(border->startTime,time);MCObjectHeap_touch(border->object.heap);
        ok=notify(border,&scope,TRANSITION_EVENT,oldSize,newSize,time);
    }
    return end(border,&scope,ok);
}
bool WorldBorder_setCenter(WorldBorder *border,double x,double z) {
    MCObjectRootScope scope={0};if(!begin(border,&scope))return false;
    border->centerX=x;border->centerZ=z;MCObjectHeap_touch(border->object.heap);
    bool ok=notify(border,&scope,CENTER_EVENT,x,z,0);return end(border,&scope,ok);
}
bool WorldBorder_setSize(WorldBorder *border,int32_t size) {
    if(!valid(border))return false;
    border->worldSize=size;MCObjectHeap_touch(border->object.heap);return true;
}
int32_t WorldBorder_getSize(WorldBorder *border) {return valid(border)?border->worldSize:0;}
