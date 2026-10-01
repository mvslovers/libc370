/* MBLEN.C */
#include <stdlib.h>
#include <stddef.h>

/* The only multibyte encoding is the single-byte one (MB_CUR_MAX 1):
   every character is one byte, and there is no shift state. */
__PDPCLIB_API__ int mblen(const char *s, size_t n)
{
    if (s == NULL) {
        return (0);                 /* no state-dependent encoding */
    }
    if (n == 0) {
        return (-1);
    }
    return ((*s == '\0') ? 0 : 1);
}
