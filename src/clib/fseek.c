/* FSEEK.C */
#include "clibio.h"
#include "cliblock.h"

int
fseek(FILE *fp, long int offset, int whence)
{
    int     rc;
    int     owned;

    owned = (lock(fp,0) == 0);  /* rc=8 = caller already holds it (#145) */

    rc = __fseek(fp,offset,whence);

    if (owned) unlock(fp,0);

    return rc;
}
