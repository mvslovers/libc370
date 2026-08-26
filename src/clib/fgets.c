/* FGETS.C */
#include "clibio.h"
#include "cliblock.h"

char *
fgets(char *s, int n, FILE *fp)
{
    int owned;

    owned = (lock(fp, 0) == 0); /* rc=8 = caller already holds it (#145) */

    s = __fgets(s,n,fp);

    if (owned) unlock(fp, 0);

    return s;
}
