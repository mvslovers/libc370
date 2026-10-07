/* FFLUSH.C */
#include "src/internal/fileio.h"
#include "stdio.h"
#include "mvs/lock.h"

int
fflush(FILE *fp)
{
    int             err     = 0;
    int             owned;

    owned = __flock(fp);  /* rc=8 = caller already holds it (#145) */

    err = __fflush(fp);

    if (owned) unlock(fp,0);

    return err;
}
