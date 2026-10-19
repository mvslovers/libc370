/* TMPFILE.C */
#include <stdio.h>

FILE *
tmpfile(void)
{
    char    fn[FILENAME_MAX];

    fn[0] = 0;
    tmpnam(fn);
    /* no name made (no C environment): fn would be passed uninitialized
       (#395) */
    if (fn[0] == 0) return NULL;

    /* "wb+", as C99 7.19.4.3 asks: with "wb" nothing written could be
       read back (#395).  The temporary data set is gone after fclose(),
       so reopening it for reading is no alternative. */
    return (fopen(fn, "wb+"));
}
