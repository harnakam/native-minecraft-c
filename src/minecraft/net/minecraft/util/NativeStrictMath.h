#ifndef C919_NATIVE_STRICT_MATH_H
#define C919_NATIVE_STRICT_MATH_H
#include <stdbool.h>
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
/* Native numerical dependency for Java8 API semantics, not a JDK class port.
   log/sqrt bodies are licensed Netlib derivatives; see docs/third-party.md.
   Call isSupported before numerical use. Require nearest-even, gradual
   underflow, no FTZ/DAZ, and compile this dependency AND its callers without
   fast math or floating contraction. Other rounding modes are unsupported. */
#if defined(__FAST_MATH__)
#error "StrictMath dependency requires fast math disabled"
#endif
_Static_assert(CHAR_BIT==8 && sizeof(double)==8 && sizeof(uint64_t)==8,
               "StrictMath requires binary64 widths");
_Static_assert(FLT_RADIX==2 && DBL_MANT_DIG==53 && DBL_MAX_EXP==1024 && DBL_MIN_EXP==-1021,
               "StrictMath requires IEEE binary64");
_Static_assert(FLT_EVAL_METHOD==0,"StrictMath requires no excess evaluation precision");
bool NativeStrictMath_isSupported(void);
double NativeStrictMath_log(double);
double NativeStrictMath_sqrt(double);
/* Internal representation helpers replace historical aliasing int-pointer
   access. Integer interpretation is explicit and independent of host byte
   order when double and uint64_t use the ordinary IEEE representation. */
static inline uint64_t c919_strictmath_bits(double value){
    uint64_t result;memcpy(&result,&value,sizeof result);return result;
}
static inline double c919_strictmath_number(uint64_t bits){
    double result;memcpy(&result,&bits,sizeof result);return result;
}
static inline uint32_t c919_strictmath_hi(double value){return (uint32_t)(c919_strictmath_bits(value)>>32);}
static inline uint32_t c919_strictmath_lo(double value){return (uint32_t)c919_strictmath_bits(value);}
static inline int32_t c919_strictmath_hi_signed(double value){
    uint32_t bits=c919_strictmath_hi(value);
    return bits<=INT32_MAX ? (int32_t)bits : -1-(int32_t)(UINT32_MAX-bits);
}
static inline void c919_strictmath_set_hi(double *value,uint32_t hi){
    *value=c919_strictmath_number(((uint64_t)hi<<32)|c919_strictmath_lo(*value));
}
#endif
