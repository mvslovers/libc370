/* MBSTOWCS.C */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

/* Widen byte by byte.  This was a strncpy into the wchar_t array, which
   only worked while wchar_t was char (#195).  Returns the number of
   wide characters stored, not counting the terminator; n is counted in
   wide characters.  A NULL pwcs returns the length it would need. */
__PDPCLIB_API__ size_t mbstowcs(wchar_t *pwcs, const char *s, size_t n)
{
    size_t i;

    if (pwcs == NULL) {
        return (strlen(s));
    }
    for (i = 0; i < n; i++) {
        pwcs[i] = (unsigned char)s[i];
        if (s[i] == '\0') {
            return (i);
        }
    }
    return (n);
}
