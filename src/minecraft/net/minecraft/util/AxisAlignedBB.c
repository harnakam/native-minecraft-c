#include "util/AxisAlignedBB.h"
#include <math.h>
static const MCObjectClass klass={"net.minecraft.util.AxisAlignedBB",MCObjectHeap_plainClone,NULL,NULL};
bool AxisAlignedBB_isInstance(const MCObject *object) {return object&&object->klass==&klass&&MCObjectHeap_objectSize(object)>=sizeof(AxisAlignedBB);}
static double java_min(double a,double b) {
    if(isnan(a))return a;
    if(a==0.0&&b==0.0&&signbit(b))return b;
    return a<=b?a:b;
}
static double java_max(double a,double b) {
    if(isnan(a))return a;
    if(a==0.0&&b==0.0&&signbit(a))return b;
    return a>=b?a:b;
}
AxisAlignedBB *AxisAlignedBB_new(MCObjectHeap *heap,double x1,double y1,double z1,double x2,double y2,double z2) {
    AxisAlignedBB *box=(AxisAlignedBB *)MCObjectHeap_alloc(heap,sizeof *box,&klass);
    if(box) {
        box->minX=java_min(x1,x2);box->minY=java_min(y1,y2);box->minZ=java_min(z1,z2);
        box->maxX=java_max(x1,x2);box->maxY=java_max(y1,y2);box->maxZ=java_max(z1,z2);
    }
    return box;
}
