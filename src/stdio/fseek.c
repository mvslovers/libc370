/* FSEEK.C */
#include "src/internal/fileio.h"
#include "stdio.h"
#include "mvs/lock.h"

int
fseek(FILE *fp, long int offset, int whence)
{
    int     rc;
    int     owned;

    owned = __flock(fp);  /* rc=8 = caller already holds it (#145) */

    rc = __fseek(fp,offset,whence);

    if (owned) unlock(fp,0);

    return rc;
}
