/* FGETC.C */
#include <fileio.h>
#include "stdio.h"
#include "mvs/lock.h"

int
fgetc(FILE *fp)
{
    int             c;
    int             owned;

    owned = (lock(fp, 0) == 0); /* rc=8 = caller already holds it (#145) */

    c = __fgetc(fp);

    if (owned) unlock(fp, 0);

    return c;
}
