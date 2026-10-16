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
        /* the TZ variable on first use (#564): the start-up no longer
           calls tzset().  Per task, as crttzoff is - a thread's own
           anchor starts from CVTTZ (@@crtset.c) */
        if (!(crt->crtflag & CRTFLAG_TZSET)) tzset();
        tzoffset = crt->crttzoff;
    }

    return tzoffset;
}
