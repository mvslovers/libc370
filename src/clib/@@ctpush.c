/* @@CTPUSH.C */
#include <stdlib.h>
#include <stddef.h>
#include "mvs/thread.h"
#include "mvs/lock.h"
#include "mvs/crt.h"
#include "ext/array.h"

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
