/* STRTOLL.C */
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include "src/internal/digval.h"

__PDPCLIB_API__ long long int strtoll(const char *nptr, char **endptr,
                                      int base)
{
    const char *s = nptr;
    unsigned long long x = 0;
    unsigned long long limit;
    unsigned long long cutoff;
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

    /* the magnitude may reach LLONG_MAX, or one more when negative */
    limit = neg ? (unsigned long long)LLONG_MAX + 1
                : (unsigned long long)LLONG_MAX;
    cutoff = limit / (unsigned long long)base;
    cutlim = (int)(limit % (unsigned long long)base);

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
        x = x * (unsigned long long)base + (unsigned long long)d;
    }

    if (endptr != NULL) {
        *endptr = (char *)(any ? s : nptr);
    }
    if (overflow) {
        errno = ERANGE;
        return (neg ? LLONG_MIN : LLONG_MAX);
    }
    if (neg) {
        return (x == limit ? LLONG_MIN : -(long long)x);
    }
    return ((long long)x);
}
