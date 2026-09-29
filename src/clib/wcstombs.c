/* WCSTOMBS.C */
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>

/* Narrow element by element.  This was a strncpy from the wchar_t
   array, which only worked while wchar_t was char (#195).  Returns the
   number of bytes stored, not counting the terminator, or (size_t)-1
   for a wide character that does not fit a byte.  A NULL s returns the
   length it would need. */
__PDPCLIB_API__ size_t wcstombs(char *s, const wchar_t *pwcs, size_t n)
{
    size_t i;

    for (i = 0; s == NULL || i < n; i++) {
        if (pwcs[i] == 0) {
            if (s != NULL) {
                s[i] = '\0';
            }
            return (i);
        }
        if (pwcs[i] < 0 || pwcs[i] > UCHAR_MAX) {
            return ((size_t)-1);
        }
        if (s != NULL) {
            s[i] = (char)pwcs[i];
        }
    }
    return (n);
}
