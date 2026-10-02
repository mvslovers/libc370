/* STRTOL.C */
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include "src/internal/digval.h"

__PDPCLIB_API__ long int strtol(const char *nptr, char **endptr, int base)
{
    const char *s = nptr;
    unsigned long x = 0;
    unsigned long limit;
    unsigned long cutoff;
    int cutlim;
    int d;
    int neg = 0;
    int any = 0;
    int overflow = 0;

    if (base < 0 || base == 1 || base > 36) {
        if (endptr != NULL) {
            *endptr = (char *)nptr;
        }
        errno = EINVAL;
        return (0);
    }

    while (isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == '-') {
        neg = 1;
        s++;
    }
    else if (*s == '+') {
        s++;
    }

    /* "0x" is a prefix only when a hex digit follows it; otherwise the
       subject sequence is the "0" alone */
    if ((base == 0 || base == 16) && s[0] == '0'
        && (s[1] == 'x' || s[1] == 'X')
        && __digval((unsigned char)s[2]) < 16) {
        s += 2;
        base = 16;
    }
    else if (base == 0) {
        base = (s[0] == '0') ? 8 : 10;
    }

    /* the magnitude may reach LONG_MAX, or one more when negative */
    limit = neg ? (unsigned long)LONG_MAX + 1
                : (unsigned long)LONG_MAX;
    cutoff = limit / (unsigned long)base;
    cutlim = (int)(limit % (unsigned long)base);

    for (;; s++) {
        d = __digval((unsigned char)*s);
        if (d >= base) {
            break;
        }
        any = 1;
        if (overflow) {
            continue;
        }
        if (x > cutoff || (x == cutoff && d > cutlim)) {
            overflow = 1;
            continue;
        }
        x = x * (unsigned long)base + (unsigned long)d;
    }

    if (endptr != NULL) {
        *endptr = (char *)(any ? s : nptr);
    }
    if (overflow) {
        errno = ERANGE;
        return (neg ? LONG_MIN : LONG_MAX);
    }
    if (neg) {
        return (x == limit ? LONG_MIN : -(long)x);
    }
    return ((long)x);
}
