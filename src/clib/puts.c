/* PUTS.C */
#include <stdio.h>
#include "cliblock.h"

int
puts(const char *s)
{
    FILE    *fp = stdout;
    int     ret;
    int     owned;

    /* one critical section for the string AND its newline, so a
       concurrent printf cannot split the line at the '\n' (#147);
       rc=8 = caller already holds it (#145) */
    owned = (lock(fp,0) == 0);

    ret = __fputs(s, fp);
    if (ret != EOF) {
        ret = __fputc('\n', fp);
    }

    if (owned) unlock(fp,0);

    return ret;
}
