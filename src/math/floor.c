/* FLOOR.C */
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

__PDPCLIB_API__ double floor(double x)
{
    double y;

    /* not through an int: wrong beyond 2**31 (#273) */
    if (x < 0.0)
    {
        y = -__ipart(-x);
        if (y != x)
        {
            y -= 1.0;
        }
        return (y);
    }
    return (__ipart(x));
}
