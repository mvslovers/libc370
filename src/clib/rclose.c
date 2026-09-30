/* RCLOSE.C - close dataset opened by ropen() */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include "src/internal/bsam.h"
#include "mvs/rfile.h"

extern int      __fdclr(char *fdddname);

int
rclose(RFILE *fp)
{
    int     err = 0;
    int     rc;

    if (!fp) goto quit;

    if (fp->hfile) {
        /* rwrite() fills the block record by record, so the last, short
           block is written here - an out-of-space on it only shows up in
           this rc (#182, #228) */
        rc = __aclose(fp->hfile);
        if (rc) {
            err   = -1;
            errno = (rc == 12) ? ENOSPC : EIO;
        }
    }

    if (fp->dyn) {
        /* ropen() allocated the DD from a data set name; without this the
           allocation outlived the handle until step end (#229).  A close
           failure keeps its errno */
        rc = __fdclr(fp->ddname);
        if (rc && !err) {
            err   = -1;
            errno = EIO;
        }
    }

    free(fp);

quit:
    return err;
}
