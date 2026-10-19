/* PERROR.C */
#include <stdio.h>
#include <errno.h>
#include <string.h>

void
perror(const char *s)
{
    /* errno now: the fprintf() below may change it.  strerror() returns
       a text for every value since #420, so no static buffer - which a
       program in the LNKLST cannot write - is needed (#395) */
    const char *str = strerror(errno);

    if ((s != NULL) && (*s != '\0')) {
        fprintf(stderr, "%s: ", s);
    }

    fprintf(stderr, "%s\n", str);

    return;
}
