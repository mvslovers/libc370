/*********************************************************************/
/*                                                                   */
/*  This Program Written by Paul Edwards.                            */
/*  Released to the Public Domain                                    */
/*                                                                   */
/*********************************************************************/
/*********************************************************************/
/*                                                                   */
/*  math.h - math header file.                                       */
/*                                                                   */
/*********************************************************************/

#ifndef __MATH_INCLUDED
#define __MATH_INCLUDED
#include <sys/_cc370.h>

/*
  Some constants - at present these are only defined for IBM
*/
#if defined(__MVS__) || defined (__CMS__) || defined(__VSE__)
/*
 This is about the nearest thing we can get to inf wthout
*/
#define HUGE_VAL 9.999999999999999999999E72

#else

#define HUGE_VAL 9.999999999999999999999E72

#endif

double ceil(double x)                   __asm__("CEIL");
double fabs(double x)                   __asm__("FABS");
double floor(double x)                  __asm__("FLOOR");
double fmod(double x, double y)         __asm__("FMOD");

/* unimplemented: */


double atan2(double y, double x)        __asm__("ATAN2");
double frexp(double value, int *exp)    __asm__("FREXP");
double ldexp(double x, int exp)         __asm__("LDEXP");
double modf(double value, double *iptr) __asm__("MODF");

/*
  Implemented by d.wade - April 2006
*/

double pow(double x, double y)          __asm__("POW");
double sqrt(double x)                   __asm__("SQRT");
double acos(double x)                   __asm__("ACOS");
double asin(double x)                   __asm__("ASIN");
double atan(double x)                   __asm__("ATAN");
double cos(double x)                    __asm__("COS");
double sin(double x)                    __asm__("SIN");
double tan(double x)                    __asm__("TAN");
double cosh(double x)                   __asm__("COSH");
double sinh(double x)                   __asm__("SINH");
double tanh(double x)                   __asm__("TANH");
double exp(double x)                    __asm__("EXP");
double log(double x)                    __asm__("LOG");
double log10(double x)                  __asm__("LOG10");

/*
  C99 7.12: the long double functions (#340).  long double is double on
  this target (<float.h>: LDBL_MANT_DIG == DBL_MANT_DIG), so each is the
  double function under its own MVS name.  The double declarations above
  carry the same asm label: cc370 maps sqrt and sqrtl to one 8-character
  name either way, and it warns about two C names for one MVS name unless
  both are spelled as that label.
*/
long double ceill(long double x)                    __asm__("CEIL");
long double fabsl(long double x)                    __asm__("FABS");
long double floorl(long double x)                   __asm__("FLOOR");
long double fmodl(long double x, long double y)     __asm__("FMOD");
long double atan2l(long double y, long double x)    __asm__("ATAN2");
long double frexpl(long double value, int *exp)     __asm__("FREXP");
long double ldexpl(long double x, int exp)          __asm__("LDEXP");
long double modfl(long double value, long double *iptr) __asm__("MODF");
long double powl(long double x, long double y)      __asm__("POW");
long double sqrtl(long double x)                    __asm__("SQRT");
long double acosl(long double x)                    __asm__("ACOS");
long double asinl(long double x)                    __asm__("ASIN");
long double atanl(long double x)                    __asm__("ATAN");
long double cosl(long double x)                     __asm__("COS");
long double sinl(long double x)                     __asm__("SIN");
long double tanl(long double x)                     __asm__("TAN");
long double coshl(long double x)                    __asm__("COSH");
long double sinhl(long double x)                    __asm__("SINH");
long double tanhl(long double x)                    __asm__("TANH");
long double expl(long double x)                     __asm__("EXP");
long double logl(long double x)                     __asm__("LOG");
long double log10l(long double x)                   __asm__("LOG10");


#ifdef __WATCOMC__
#pragma intrinsic(cos, sin, tan, exp, log, log10, sqrt)
#endif

#ifdef __IBMC__
double _Builtin __fabs(double x);
#define fabs(x) (__fabs((x)))
double _Builtin __fsin(double x);
#define sin(x) (__fsin((x)))
double _Builtin __fcos(double x);
#define cos(x) (__fcos((x)))
double _Builtin __fptan(double x);
#define tan(x) (__fptan((x)))
double _Builtin __fpatan(double x);
#define atan(x) (__fpatan((x)))
double _Builtin __facos(double x);
#define acos(x) (__facos((x)))
double _Builtin __fasin(double x);
#define asin(x) (__fasin((x)))
double _Builtin __fsqrt(double x);
#define sqrt(x) (__fsqrt((x)))
#endif

#ifdef __BORLANDC__
double __sin__(double x);
#define sin(x) (__sin__((x)))
double __cos__(double x);
#define cos(x) (__cos__((x)))
#endif

#endif

