/* MBTOWC.C */
#include <stdlib.h>
#include <stddef.h>

/* Single-byte encoding: one byte becomes one wchar_t, and the null
   character converts to 0 and returns 0 (C99 7.20.7.2). */
__PDPCLIB_API__ int mbtowc(wchar_t *pwc, const char *s, size_t n)
{
    if (s == NULL) {
        return (0);                 /* no state-dependent encoding */
    }
    if (n == 0) {
        return (-1);
    }
    if (pwc != NULL) {
        *pwc = (unsigned char)*s;
    }
    return ((*s == '\0') ? 0 : 1);
}
