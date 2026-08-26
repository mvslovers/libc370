/* FPUTC.C */
#include "clibio.h"
#include "cliblock.h"

int
fputc(int c, FILE *fp)
{
    int     rc;
    int     owned;

    owned = (lock(fp,0) == 0);  /* rc=8 = caller already holds it (#145) */

    rc = __fputc(c,fp);

    if (owned) unlock(fp,0);

    return rc;
}
