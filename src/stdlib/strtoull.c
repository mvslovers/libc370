/* STRTOULL.C */
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

/* The value of digit c, or 36 when c is no digit in any base.  A table
   rather than c - 'A' + 10: the EBCDIC letters are not contiguous
   (A-I, J-R, S-Z). */
static int digval(int c)
{
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    int i;

    c = tolower(c);
    for (i = 0; i < 36; i++) {
        if (digits[i] == c) {
            return (i);
        }
    }
    return (36);
}

__PDPCLIB_API__ unsigned long long int strtoull(
    const char *nptr, char **endptr, int base)
{
    const char *s = nptr;
    unsigned long long x = 0;
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
        && digval((unsigned char)s[2]) < 16) {
        s += 2;
        base = 16;
    }
    else if (base == 0) {
        base = (s[0] == '0') ? 8 : 10;
    }

    cutoff = ULLONG_MAX / (unsigned long long)base;
    cutlim = (int)(ULLONG_MAX % (unsigned long long)base);

    for (;; s++) {
        d = digval((unsigned char)*s);
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
        return (ULLONG_MAX);
    }
    return (neg ? -x : x);
}
