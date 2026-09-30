/* @@TZGET.C */
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include "mvs/crt.h"

int
__tzget(void)
{
    int     tzoffset= 0;
    CLIBCRT *crt    = __crtget();

    if (crt) {
        tzoffset = crt->crttzoff;
    }

    return tzoffset;
}
