/* STRTOUMAX.C */
#include <inttypes.h>
#include <stdlib.h>
#include <stddef.h>

/* uintmax_t is unsigned long long */
__PDPCLIB_API__ uintmax_t strtoumax(const char *nptr, char **endptr,
                                    int base)
{
    return (strtoull(nptr, endptr, base));
}
