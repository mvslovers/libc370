/* EXP.C */
#if 1
#include "math.h"
#include "float.h"
#include "errno.h"
#include "stddef.h"

#if 0
/*

  Some constants to make life easier elsewhere
  (These should I guess be in math.h)

*/
static const double pi   = 3.1415926535897932384626433832795;
static const double ln10 = 2.3025850929940456840179914546844;
static const double ln2 = 0.69314718055994530941723212145818 ;
#else
#define pi      (3.1415926535897932384626433832795)
#define ln10    (2.3025850929940456840179914546844)
#define ln2     (0.69314718055994530941723212145818)
#endif

#else
#include <stddef.h>
#endif

/* ln 2 in two parts (#389): LN2HI has 32 significant bits, so n * LN2HI
   is exact for every n this function uses, and LN2LO carries the rest */
#define LN2HI   6.93147180369123816490e-01
#define LN2LO   1.90821492927058770002e-10
/* the largest and the smallest x whose e**x is a normalized double:
   ln(16**63 * (1 - 16**-14)) and ln(16**-65) */
#define EXPMAX  174.6731
#define EXPMIN  (-180.2182)

/*
   exp(x) = 2**n * e**r with x = n ln2 + r and |r| <= ln2/2 (#389).  It
   summed the series of e**x itself: an intermediate term of x**k/k!
   passed the HFP maximum from about x = 173 (S0CC), and for x < 0 the
   alternating sum lost every digit (exp(-30) came out negative).

   e**r = 1 + r + r**2/2 + ... converges in about 17 terms for |r| <= 0.35.
   2**n is applied as 2**(n mod 4) times 16**(n div 4), the last by adding
   to the hexadecimal characteristic: exact, and no intermediate can leave
   the range.
*/
__PDPCLIB_API__ double exp (double x)
{
    union {
        double      d;
        unsigned    w[2];
    } v;
    double  r, term, sum;
    int     n, m, q, c, i;

    if (x > EXPMAX) {
        errno = ERANGE;
        return HUGE_VAL;
    }
    if (x < EXPMIN) {
        errno = ERANGE;
        return 0.0;
    }

    /* n = x / ln2, rounded to the nearest integer */
    r = x / (LN2HI + LN2LO);
    n = (int)(r < 0 ? r - 0.5 : r + 0.5);
    r = (x - n * LN2HI) - n * LN2LO;

    term = sum = 1.0;
    for (i = 1; i < 30; i++) {
        term = term * r / i;
        if (sum + term == sum) break;
        sum = sum + term;
    }

    /* n = 4q + m with m in 0..3 */
    m = ((n % 4) + 4) % 4;
    q = (n - m) / 4;
    v.d = sum * (double)(1 << m);

    c = (int)((v.w[0] >> 24) & 0x7F) + q;
    if (c > 127) {
        errno = ERANGE;
        return HUGE_VAL;
    }
    if (c < 0) {
        errno = ERANGE;
        return 0.0;
    }
    v.w[0] = (v.w[0] & 0x80FFFFFFU) | ((unsigned)c << 24);
    return v.d;
}
