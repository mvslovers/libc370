/* STRTOLD.C */
#include <stdlib.h>
#include <stddef.h>

/* long double is double under cc370 (8 bytes, LDBL_MANT_DIG 14) */
__PDPCLIB_API__ long double strtold(const char *nptr, char **endptr)
{
    return (strtod(nptr, endptr));
}
