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
#include "mvs/lock.h"
#include "libc370/array.h"

int
fclose(FILE *fp)
{
    int     owned;
    int     rc      = 0;
    int     err;

    if (!fp) goto quit;
    if (strcmp(fp->eye, _FILE_EYE)!=0) goto quit;

    /* the whole teardown runs under the FILE lock, so an in-flight
       vfprintf() on the same FILE finishes (or waits) before the DCB
       closes and the buffer goes away (#147); rc=8 = caller already
       holds it (#145) */
    owned = (lock(fp,0) == 0);

    if (fp->flags & _FILE_FLAG_OPEN) {
        /* flush any pending data to disk; __fflush - we hold the lock.
           It sets errno itself on a failure. */
        if (__fflush(fp)) rc = EOF;

        /* close the dataset.  The last, short block is written here and
           not by the flush, so an out-of-space on it only shows up in
           this rc (#182) - the FILE is freed below, so the return value
           is all that can carry it */
        err = __aclose(fp->dcb);
        fp->dcb     = 0;
        fp->asmbuf  = 0;
        if (err) {
            rc    = EOF;
            errno = (err == 12) ? ENOSPC : EIO;
        }
    }

    /* buffer, DD, grtfile, FILE, lock - shared with __fabandon() (#168) */
    __fpterm(fp, owned);

quit:
    return rc;
}
