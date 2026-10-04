/* FMOD.C */
#if 0
#include "math.h"
#include "float.h"
#include "errno.h"
#include "stddef.h"

/*

  Some constants to make life easier elsewhere
  (These should I guess be in math.h)

*/
static const double pi   = 3.1415926535897932384626433832795;
static const double ln10 = 2.3025850929940456840179914546844;
static const double ln2 = 0.69314718055994530941723212145818 ;
#else
#include <stddef.h>
#endif
#include "src/internal/ipart.h"

__PDPCLIB_API__ double fmod(double x, double y)
{
    double ax = x < 0.0 ? -x : x;
    double ay = y < 0.0 ? -y : y;
    double r;

    if (y == 0.0) return (0.0);
    /* |x| - trunc(|x/y|) * |y|, the quotient no longer through an int,
       which was wrong beyond 2**31 (#273).  Not exact as C99 asks: the
       quotient and the product round as they always did, and above 2**52
       the quotient has no digits left for the remainder - so the result is
       only brought back into [0, |y|), the range C99 promises. */
    r = ax - __ipart(ax / ay) * ay;
    if (r < 0.0) r += ay;
    if (r >= ay) r -= ay;
    if (r < 0.0 || r >= ay) r = 0.0;
    return (x < 0.0 ? -r : r);
}
