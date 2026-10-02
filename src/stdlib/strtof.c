/* STRTOF.C */
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <float.h>

/* float and double share the HFP exponent range, so narrowing cannot
   underflow, and it overflows only where LRER rounds a value just above
   FLT_MAX up past the largest exponent - an exponent-overflow program
   check (S0CC), which no program mask suppresses.  Anything above
   FLT_MAX is therefore answered here, before the conversion. */
__PDPCLIB_API__ float strtof(const char *nptr, char **endptr)
{
    double d = strtod(nptr, endptr);

    if (d > FLT_MAX) {
        errno = ERANGE;
        return (FLT_MAX);
    }
    if (d < -FLT_MAX) {
        errno = ERANGE;
        return (-FLT_MAX);
    }
    return ((float)d);
}
