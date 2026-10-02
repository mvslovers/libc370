/* STRTOIMAX.C */
#include <inttypes.h>
#include <stdlib.h>
#include <stddef.h>

/* intmax_t is long long */
__PDPCLIB_API__ intmax_t strtoimax(const char *nptr, char **endptr, int base)
{
    return (strtoll(nptr, endptr, base));
}
