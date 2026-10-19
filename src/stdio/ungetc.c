/* UNGETC.C */
#include "src/internal/fileio.h"
#include "stdio.h"
#include "mvs/lock.h"

int
ungetc(int c, FILE *fp)
{
    int owned;

    owned = __flock(fp);  /* rc=8 = caller already holds it (#145) */

    if ((fp->ungetch != -1) || (c == EOF)) {
        c = EOF;
    }
    else {
        fp->ungetch = (unsigned char)c;
        /* C99 7.19.7.11: a successful ungetc() clears the end-of-file
           indicator, and the next read returns the character (#395) */
        fp->flags &= 0xFFFF - _FILE_FLAG_EOF;
    }

    if (owned) unlock(fp,0);

    return c;
}
