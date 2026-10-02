/* LLABS.C */
#include <stdlib.h>
#include <stddef.h>

#ifdef llabs
#undef llabs
#endif
__PDPCLIB_API__ long long int llabs(long long int j)
{
    if (j < 0)
    {
        j = -j;
    }
    return (j);
}
