#ifndef C919_SOURCE_MATH_HELPER_H
#define C919_SOURCE_MATH_HELPER_H
/* Source SIN_TABLE initialization, its two float lookup methods and the
   original double clamp comparison expression. The
   native one-time initialization and host double sin are platform adapters;
   the remaining MathHelper methods are not declared as implemented. */
float MathHelper_sin(float value);
float MathHelper_cos(float value);
double MathHelper_clamp_double(double value,double minimum,double maximum);
#endif
