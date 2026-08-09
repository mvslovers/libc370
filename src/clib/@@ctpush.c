/* @@CTPUSH.C */
#include <stdlib.h>
#include <stddef.h>
#include "clibthrd.h"
#include "cliblock.h"
#include "clibcrt.h"
#include "clibary.h"

__asm__("\n&FUNC    SETC 'cthread_push'");
int
cthread_push(int (*func)(void*), void *arg)
{
    int     rc      = -1;
    CLIBCRT *crt    = __crtget();

    if (func && crt) {
        lock(&crt->crtpush,0);
        rc = arrayadd(&crt->crtpush, func);
        if (!rc) {
            rc = arrayadd(&crt->crtargs, arg);
            /* keep the func/arg arrays paired (#85) */
            if (rc) arraydel(&crt->crtpush, arraycount(&crt->crtpush));
        }
        unlock(&crt->crtpush,0);
    }

    return rc;
}
