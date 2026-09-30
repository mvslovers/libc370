/* UNGETC.C */
#include "stdio.h"
#include "mvs/lock.h"

int
ungetc(int c, FILE *fp)
{
    int owned;

    owned = (lock(fp,0) == 0);  /* rc=8 = caller already holds it (#145) */

    if ((fp->ungetch != -1) || (c == EOF)) {
        c = EOF;
    }
    else {
        fp->ungetch = (unsigned char)c;
    }

    if (owned) unlock(fp,0);

    return c;
}
