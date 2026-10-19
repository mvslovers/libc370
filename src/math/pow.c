/* POW.C */
#if 1
#include "math.h"
#include "float.h"
#include "errno.h"
#include "stddef.h"

#if 0
/*

  Some constants to make life easier elsewhere
  (These should I guess be in math.h)

*/
static const double pi   = 3.1415926535897932384626433832795;
static const double ln10 = 2.3025850929940456840179914546844;
static const double ln2 = 0.69314718055994530941723212145818 ;
#else
#define pi      (3.1415926535897932384626433832795)
#define ln10    (2.3025850929940456840179914546844)
#define ln2     (0.69314718055994530941723212145818)
#endif

#else
#include <stddef.h>
#endif

/* integral y with |y| < 2**31: x**y by squaring, x not 0 (#389).  The
   result's size is checked first through y * log|x|, so no product can
   pass the HFP maximum; a negative y works on 1/x, whose powers only get
   smaller.  It multiplied |y| times, 2e9 multiplications for
   pow(1.0, 2e9), and 1/x**|y| overflowed before the division. */
static double
ipow(double x, double y)
{
    double          base    = (y < 0) ? 1.0 / x : x;
    double          result  = 1.0;
    double          ly      = y * log(fabs(x));
    unsigned long   k       = (unsigned long)fabs(y);

    if (ly > 174.6731) {
        errno = ERANGE;
        return (x < 0 && (k & 1)) ? -HUGE_VAL : HUGE_VAL;
    }
    while (k) {
        if (k & 1) result = result * base;
        k >>= 1;
        if (k) base = base * base;
    }
    return result;
}

/*
   x to the power y: by squaring for an integral y, else exp(y log x).
*/
__PDPCLIB_API__ double pow(double x,double y)
{
    double  r;

    if (y == 0.0) return (1.0);
    if (x == 0.0) {
        if (y > 0.0) return (0.0);
        /* a pole: it divided by zero, S0CF (#389) */
        errno = ERANGE;
        return (HUGE_VAL);
    }
    if (floor(y) == y) {
        if (fabs(y) < 2147483648.0) return ipow(x, y);
        /* larger: exp, and the sign from the parity of y */
        r = exp(y * log(fabs(x)));
        return (x < 0.0 && fmod(y, 2.0) != 0.0) ? -r : r;
    }
    if (x < 0.0)
    {
         errno=EDOM;
         return(0.0);
    }
    return (exp(y*log(x)));
}
