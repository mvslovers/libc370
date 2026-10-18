/* @@ARADF.C
** Dynamic array
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include "ext/array.h"

/* The string is measured first and formatted straight into its own
** storage.  It went through a 1024-byte buffer on the stack, which a
** longer string overran, and the copy was leaked when arrayadd() failed
** (#398). */
/* fmt is a printf format: said here, for this file only - the attribute
** in <ext/array.h> would reach every caller's -Werror build (#68) */
extern int arrayaddf(void *varray, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

__asm__("\n&FUNC    SETC 'arrayaddf'");
int
arrayaddf( void * varray, const char *fmt, ... )
{
    int     rc      = -1;
    va_list tlist;
    int     len;
    char    *ptr;

    if (varray) {
        va_start(tlist, fmt);
        len = vsnprintf(NULL, 0, fmt, tlist);
        va_end(tlist);
        if (len < 0) goto quit;

        ptr = calloc(1, len + 1);
        if (!ptr) {
            errno = ENOMEM;
            goto quit;
        }

        va_start(tlist, fmt);
        vsnprintf(ptr, len + 1, fmt, tlist);
        va_end(tlist);

        rc = arrayadd(varray, ptr);
        if (rc) free(ptr);
    }

quit:
    return rc;
}
