/* STRTOD.C */
#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>

/* HFP long has 14 hex digits; 17 decimal digits are more than it holds */
#define SIGDIGITS 17

/* 10**k for 0 <= k <= 75 from the powers 10**(2**i): at most 7
   multiplications, and no partial product exceeds the result */
static double tenpow(int k)
{
    static const double p2[] = { 1e1, 1e2, 1e4, 1e8, 1e16, 1e32, 1e64 };
    double r = 1.0;
    int i;

    for (i = 0; k != 0; i++, k >>= 1) {
        if (k & 1) {
            r *= p2[i];
        }
    }
    return (r);
}

/* Every intermediate is checked against the HFP range before it is
   formed: an exponent overflow is a program check (S0CC) that no program
   mask suppresses, and the old repeated *10 met one for any exponent past
   about 75 either way ("1e76", "8e75", "1e-80" - #316). */
__PDPCLIB_API__ double strtod(const char *nptr, char **endptr)
{
    const char *s = nptr;
    const char *t;
    double x = 0.0;
    double p;
    int neg = 0;
    int any = 0;
    int nd = 0;         /* significant digits in x */
    int dexp = 0;       /* decimal exponent of x's last digit */
    int eexp = 0;
    int eneg = 0;
    int e;
    int big;

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

    for (; isdigit((unsigned char)*s); s++) {
        any = 1;
        if (nd == 0 && *s == '0') {
            continue;               /* a leading zero */
        }
        if (nd < SIGDIGITS) {
            x = x * 10 + (*s - '0');
            nd++;
        }
        else {
            dexp++;                 /* a digit past the precision */
        }
    }
    if (*s == '.') {
        for (s++; isdigit((unsigned char)*s); s++) {
            any = 1;
            if (nd == 0 && *s == '0') {
                dexp--;             /* a leading zero after the point */
                continue;
            }
            if (nd < SIGDIGITS) {
                x = x * 10 + (*s - '0');
                nd++;
                dexp--;
            }
        }
    }
    if (!any) {
        if (endptr) {
            *endptr = (char *)nptr;
        }
        return (0.0);
    }

    /* an exponent counts only with a digit in it */
    if (*s == 'e' || *s == 'E') {
        t = s + 1;
        if (*t == '-') {
            eneg = 1;
            t++;
        }
        else if (*t == '+') {
            t++;
        }
        if (isdigit((unsigned char)*t)) {
            for (; isdigit((unsigned char)*t); t++) {
                if (eexp < 10000) {
                    eexp = eexp * 10 + (*t - '0');
                }
            }
            s = t;
        }
    }
    if (endptr) {
        *endptr = (char *)s;
    }
    if (nd == 0) {
        return (neg ? -0.0 : 0.0);
    }

    /* x holds nd digits and stands for x * 10**e; normalized, the value
       is d.ddd * 10**(e + nd - 1).  HFP long reaches about 7.2e75 and
       down to about 5.4e-79. */
    e = dexp + (eneg ? -eexp : eexp);
    if (e + nd - 1 > 75) {
        goto overflow;
    }
    if (e + nd - 1 < -79) {
        goto underflow;
    }
    if (e >= 0) {
        p = tenpow(e);
        if (x > DBL_MAX / p) {
            goto overflow;
        }
        x *= p;
    }
    else {
        e = -e;
        big = 0;
        if (e > 64) {
            x /= 1e64;
            e -= 64;
            big = 1;
        }
        p = tenpow(e);
        if (big && x < DBL_MIN * p) {
            goto underflow;
        }
        x /= p;
    }
    return (neg ? -x : x);

overflow:
    errno = ERANGE;
    return (neg ? -HUGE_VAL : HUGE_VAL);
underflow:
    errno = ERANGE;
    return (neg ? -0.0 : 0.0);
}
