/* REWIND.C */
#include <stdio.h>

void
rewind(FILE *stream)
{
    /* C99 7.19.9.5: fseek() to the start, and both indicators cleared -
       fseek() alone keeps the error indicator (#178) */
    (void)fseek(stream, 0L, SEEK_SET);
    clearerr(stream);
    return;
}
