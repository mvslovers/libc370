/* ATEXIT.C */
#include <stdlib.h>
#include <stddef.h>
#include "cliblock.h"
#include "clibcrt.h"
#include "clibary.h"

__PDPCLIB_API__ int atexit(void (*func)(void))
{
    CLIBGRT *grt        = __grtget();
    int     rc          = -1;

    if (func && grt) {
        lock(&grt->grtexit,0);
        rc = arrayadd(&grt->grtexit, func);
        if (!rc) {
            rc = arrayadd(&grt->grtexita, 0);
            /* keep the func/arg arrays paired (#85) */
            if (rc) arraydel(&grt->grtexit, arraycount(&grt->grtexit));
        }
        unlock(&grt->grtexit,0);
    }

    return rc;
}
