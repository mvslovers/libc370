/* ATOLL.C */
#include <stdlib.h>
#include <stddef.h>

__PDPCLIB_API__ long long int atoll(const char *nptr)
{
    return (strtoll(nptr, (char **)NULL, 10));
}
