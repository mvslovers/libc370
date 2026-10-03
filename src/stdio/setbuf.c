/* SETBUF.C */
#include <stdio.h>

/* C99 7.19.5.5: setbuf returns nothing (#339) */
void
setbuf(FILE *stream, char *buf)
{
    if (buf == NULL) {
        (void)setvbuf(stream, NULL, _IONBF, 0);
    }
    else {
        (void)setvbuf(stream, buf, _IOFBF, BUFSIZ);
    }
}
