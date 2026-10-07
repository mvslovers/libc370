/* FGETC.C */
#include "src/internal/fileio.h"
#include "stdio.h"
#include "mvs/lock.h"

int
fgetc(FILE *fp)
{
    int             c;
    int             owned;

    owned = __flock(fp); /* rc=8 = caller already holds it (#145) */

    c = __fgetc(fp);

    if (owned) unlock(fp, 0);

    return c;
}
