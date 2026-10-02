/* IMAXDIV.C */
#include <inttypes.h>
#include <stddef.h>

__PDPCLIB_API__ imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom)
{
    imaxdiv_t x;

    x.quot = numer / denom;
    x.rem = numer % denom;
    return (x);
}
