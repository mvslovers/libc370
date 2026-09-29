/* RCLOSE.C - close dataset opened by ropen() */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <mvssupa.h>
#include "rfile.h"

extern int      __fildef(char *fdddname, char *fnm, int mymode, int type);
extern int      __fdclr(char *fdddname);
extern char *   __getpfx(void);

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

    free(fp);

quit:
    return err;
}
