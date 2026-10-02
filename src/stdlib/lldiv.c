/* LLDIV.C */
#include <stdlib.h>
#include <stddef.h>

__PDPCLIB_API__ lldiv_t lldiv(long long int numer, long long int denom)
{
    lldiv_t x;

    x.quot = numer / denom;
    x.rem = numer % denom;
    return (x);
}
