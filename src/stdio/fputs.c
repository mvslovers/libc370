/* FPUTS.C */
#include "src/internal/fileio.h"
#include "stdio.h"
#include "mvs/lock.h"

int
fputs(const char *s, FILE *fp)
{
    int rc;
    int owned;

    owned = __flock(fp);  /* rc=8 = caller already holds it (#145) */

    rc = __fputs(s,fp);

    if (owned) unlock(fp,0);

    return rc;
}
