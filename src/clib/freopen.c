/* FREOPEN.C */
#include "clibio.h"
#include "cliblock.h"

FILE *
freopen(const char *fn, const char *mode, FILE *fp)
{
    FILE    *f;
    int     owned;

    owned = (lock(fp,0) == 0);  /* rc=8 = caller already holds it (#145) */

    f = __reopen(fn,mode,fp);

    if (owned) unlock(fp,0);

    return f;
}
