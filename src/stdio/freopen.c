/* FREOPEN.C */
#include "src/internal/fileio.h"
#include "stdio.h"
#include "mvs/lock.h"

FILE *
freopen(const char *fn, const char *mode, FILE *fp)
{
    FILE    *f;
    int     owned;

    owned = __flock(fp);  /* rc=8 = caller already holds it (#145) */

    f = __reopen(fn,mode,fp);

    if (owned) unlock(fp,0);

    return f;
}
