/* ON@EXIT.C */
#include <stdlib.h>
#include <stddef.h>
#include "cliblock.h"
#include "clibcrt.h"
#include "clibary.h"

int
on_exit(void (*func)(int,void*), void *arg)
{
    CLIBGRT *grt        = __grtget();
    int     rc          = -1;

    if (func && grt) {
        lock(&grt->grtexit,0);
        rc = arrayadd(&grt->grtexit, func);
        if (!rc) {
            rc = arrayadd(&grt->grtexita, arg);
            /* keep the func/arg arrays paired (#85) */
            if (rc) arraydel(&grt->grtexit, arraycount(&grt->grtexit));
        }
        unlock(&grt->grtexit,0);
    }

    return rc;
}
