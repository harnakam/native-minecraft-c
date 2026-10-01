/* C11 adaptation of licensed Netlib fdlibm 5.3. Word operations use
   memcpy; initialized word-construction temporaries avoid indeterminate
   reads; exponent-word shifts use defined unsigned arithmetic. */

/* @(#)s_cos.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/* cos(x)
 * Return cosine function of x.
 *
 * kernel function:
 *	NativeMath_kernelSin		... sine function on [-pi/4,pi/4]
 *	NativeMath_kernelCos		... cosine function on [-pi/4,pi/4]
 *	NativeMath_remPio2	... argument reduction routine
 *
 * Method.
 *      Let S,C and T denote the sin, cos and tan respectively on
 *	[-PI/4, +PI/4]. Reduce the argument x to y1+y2 = x-k*pi/2
 *	in [-pi/4 , +pi/4], and let n = k mod 4.
 *	We have
 *
 *          n        sin(x)      cos(x)        tan(x)
 *     ----------------------------------------------------------
 *	    0	       S	   C		 T
 *	    1	       C	  -S		-1/T
 *	    2	      -S	  -C		 T
 *	    3	      -C	   S		-1/T
 *     ----------------------------------------------------------
 *
 * Special cases:
 *      Let trig be any of sin, cos, or tan.
 *      trig(+-INF)  is NaN, with signals;
 *      trig(NaN)    is that NaN;
 *
 * Accuracy:
 *	TRIG(x) returns trig(x) nearly rounded
 */

#include "util/NativeMathTrigInternal.h"
#include "util/NativeMathCos.h"
#include <fenv.h>
#include <float.h>

        double NativeMath_cosRaw(double x)

{
        double y[2],z=0.0;
        int n, ix;

    /* High word of x. */
        ix = NativeMath_high(x);

    /* |x| ~< pi/4 */
        ix &= 0x7fffffff;
        if(ix <= 0x3fe921fb) return NativeMath_kernelCos(x,z);

    /* cos(Inf or NaN) is NaN */
        else if (ix>=0x7ff00000) return x-x;

    /* argument reduction needed */
        else {
            n = NativeMath_remPio2(x,y);
            switch(n&3) {
                case 0: return  NativeMath_kernelCos(y[0],y[1]);
                case 1: return -NativeMath_kernelSin(y[0],y[1],1);
                case 2: return -NativeMath_kernelCos(y[0],y[1]);
                default:
                        return  NativeMath_kernelSin(y[0],y[1],1);
            }
        }
}

/* Native numerical environment validation, not a translated JDK body. */
bool NativeMathCos_cos(double value,double *out) {
    if(!out||FLT_RADIX!=2||DBL_MANT_DIG!=53||DBL_MAX_EXP!=1024||
       sizeof(double)!=8||FLT_EVAL_METHOD!=0||fegetround()!=FE_TONEAREST)return false;
    volatile double normal=DBL_MIN,half=0.5;
    volatile double subnormal=normal*half;
    volatile double tiny=0x1p-1074,one=1.0;
    volatile double preserved=tiny*one;
    if(subnormal!=0x1p-1023||preserved!=0x1p-1074)return false;
    double result=NativeMath_cosRaw(value);
    *out=result;return true;
}
