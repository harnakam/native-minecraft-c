#ifndef C919_SOURCE_AXIS_ALIGNED_BB_H
#define C919_SOURCE_AXIS_ALIGNED_BB_H
#include "util/MCObjectHeap.h"
/* Original six-double constructor. Fields are immutable Java final values;
   native callers retain the actual managed reference, not a rebuilt mirror.
   Other geometry/BlockPos overloads remain separate source methods. */
typedef struct AxisAlignedBB { MCObject object; double minX,minY,minZ,maxX,maxY,maxZ; } AxisAlignedBB;
AxisAlignedBB *AxisAlignedBB_new(MCObjectHeap *,double x1,double y1,double z1,double x2,double y2,double z2);
bool AxisAlignedBB_isInstance(const MCObject *);
#endif
