/* FCLOSE.C */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include "src/internal/bsam.h"
#include "mvs/crt.h"
#include "mvs/lock.h"
#include "ext/array.h"

int
fclose(FILE *fp)
{
    int     rc      = 0;
    int     err;

    if (!fp) goto quit;
    if (strcmp(fp->eye, _FILE_EYE)!=0) goto quit;

    /* the whole teardown runs under the FILE lock, so an in-flight
       vfprintf() on the same FILE finishes (or waits) before the DCB
       closes and the buffer goes away (#147).  rc=8, the task holds it
       already, is not only an outer caller (#145): a write that abended
       under the caller's own recovery took it and never DEQ'd it.  The
       FILE is freed either way, so the lock goes with it below (#174). */
    lock(fp,0);

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

    /* buffer, DD, grtfile, FILE, lock - shared with __fabandon() (#168).
       The DEQ is RET=HAVE: an outer caller's later unlock() of the freed
       FILE answers 8 and changes nothing */
    __fpterm(fp, 1);

quit:
    return rc;
}
