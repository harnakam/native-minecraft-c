/* Internal C11 word access and licensed fdlibm trig dependency declarations. */
#ifndef C919_NATIVE_MATH_TRIG_INTERNAL_H
#define C919_NATIVE_MATH_TRIG_INTERNAL_H
#include <stdint.h>
#include <limits.h>
_Static_assert(CHAR_BIT==8&&sizeof(double)==8&&sizeof(int)==4,"fdlibm requires 32-bit words and binary64 storage");
#include <string.h>
#include <math.h>
static inline int32_t NativeMath_high(double x){uint64_t u;int32_t i;memcpy(&u,&x,8);uint32_t h=(uint32_t)(u>>32);memcpy(&i,&h,4);return i;}
static inline uint32_t NativeMath_low(double x){uint64_t u;memcpy(&u,&x,8);return (uint32_t)u;}
static inline void NativeMath_setHigh(double *x,uint32_t h){uint64_t u;memcpy(&u,x,8);u=(u&UINT64_C(0xffffffff))|((uint64_t)h<<32);memcpy(x,&u,8);}
static inline void NativeMath_setLow(double *x,uint32_t l){uint64_t u;memcpy(&u,x,8);u=(u&UINT64_C(0xffffffff00000000))|l;memcpy(x,&u,8);}
double NativeMath_cosRaw(double);
double NativeMath_kernelCos(double,double);
double NativeMath_kernelSin(double,double,int);
int NativeMath_remPio2(double,double *);
int NativeMath_kernelRemPio2(double *,double *,int,int,int,const int *);
#endif
