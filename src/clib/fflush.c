/* FFLUSH.C */
#include "clibio.h"
#include "cliblock.h"

int
fflush(FILE *fp)
{
    int             err     = 0;
    int             owned;

    owned = (lock(fp,0) == 0);  /* rc=8 = caller already holds it (#145) */

    err = __fflush(fp);

    if (owned) unlock(fp,0);

    return err;
}
