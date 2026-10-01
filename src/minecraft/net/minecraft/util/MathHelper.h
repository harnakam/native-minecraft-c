#ifndef C919_SOURCE_MATH_HELPER_H
#define C919_SOURCE_MATH_HELPER_H
#include "util/NativeJavaRandom.h"
#include "util/NativeJavaUUID.h"
/* Source SIN_TABLE initialization, its two float lookup methods and the
   original double clamp comparison expression. The
   native one-time initialization and host double sin are platform adapters;
   the remaining MathHelper methods are not declared as implemented. */
float MathHelper_sin(float value);
float MathHelper_cos(float value);
double MathHelper_clamp_double(double value,double minimum,double maximum);
float MathHelper_clamp_float(float value,float minimum,float maximum);
/* Original double-to-int cast, comparison and decrement; Java saturation and
   signed decrement wrap are preserved for NaN/infinity and out-of-range input. */
int32_t MathHelper_floor_double(double value);
/* Original two nextLong calls and UUID version/variant masks. The Java Random
   and UUID values use explicit native platform adapters. */
NativeJavaUUID *MathHelper_getRandomUuid(NativeJavaRandom *random);
#endif
