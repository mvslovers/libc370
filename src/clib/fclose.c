/* FCLOSE.C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <mvssupa.h>
#include "clibcrt.h"
#include "cliblock.h"
#include "clibary.h"

int
fclose(FILE *fp)
{
    int     owned;
    if (!fp) goto quit;
    if (strcmp(fp->eye, _FILE_EYE)!=0) goto quit;

    /* the whole teardown runs under the FILE lock, so an in-flight
       vfprintf() on the same FILE finishes (or waits) before the DCB
       closes and the buffer goes away (#147); rc=8 = caller already
       holds it (#145) */
    owned = (lock(fp,0) == 0);

    if (fp->flags & _FILE_FLAG_OPEN) {
        /* flush any pending data to disk; __fflush - we hold the lock */
        __fflush(fp);

        /* close the dataset */
        __aclose(fp->dcb);
        fp->dcb     = 0;
        fp->asmbuf  = 0;
    }

    /* buffer, DD, grtfile, FILE, lock - shared with __fabandon() (#168) */
    __fpterm(fp, owned);

quit:
    return 0;
}
