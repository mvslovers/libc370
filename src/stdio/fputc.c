/* FPUTC.C */
#include "src/internal/fileio.h"
#include "stdio.h"
#include "mvs/lock.h"

int
fputc(int c, FILE *fp)
{
    int     rc;
    int     owned;

    owned = __flock(fp);  /* rc=8 = caller already holds it (#145) */

    rc = __fputc(c,fp);

    if (owned) unlock(fp,0);

    return rc;
}
