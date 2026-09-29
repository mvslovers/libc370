/* WCTOMB.C */
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>

/* Single-byte encoding: a wchar_t that does not fit a byte has no
   multibyte form (#195 - wchar_t is int, not char). */
__PDPCLIB_API__ int wctomb(char *s, wchar_t wchar)
{
    if (s == NULL) {
        return (0);                 /* no state-dependent encoding */
    }
    if (wchar < 0 || wchar > UCHAR_MAX) {
        return (-1);
    }
    *s = (char)wchar;
    return (1);
}
