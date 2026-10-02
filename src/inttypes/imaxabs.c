/* IMAXABS.C */
#include <inttypes.h>
#include <stddef.h>

__PDPCLIB_API__ intmax_t imaxabs(intmax_t j)
{
    if (j < 0)
    {
        j = -j;
    }
    return (j);
}
