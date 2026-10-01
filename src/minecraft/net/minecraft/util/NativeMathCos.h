#ifndef C919_NATIVE_MATH_COS_H
#define C919_NATIVE_MATH_COS_H
#include <stdbool.h>
/* Licensed fdlibm numerical dependency for the reached java.lang.Math.cos
   call. This is not a JDK class translation or all-JVM intrinsic-parity claim.
   Requires binary64 nearest-even, gradual underflow, FLT_EVAL_METHOD=0 and
   compilation without fast math or FP contraction. Failure preserves *out. */
bool NativeMathCos_cos(double value,double *out);
#endif
