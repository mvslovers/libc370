/* @@FLTDI.C - long long to double and float, the libgcc helpers cc370
 * calls for them (#190).
 *
 *   (double)long long     __fltddf   @@FLTDDF  (cc370#470)
 *   (float)long long      __fltdsf   @@FLTDSF  (cc370#470)
 *
 * (double) of an unsigned long long needs no helper of its own: cc370
 * converts it as signed through @@FLTDDF, tests the sign with @@CMPDI2
 * and adds 2^64 back in double arithmetic.
 *
 * The result is IBM hexadecimal floating point, built from the integer
 * directly - no floating-point arithmetic.  A double holds 14 hex digits
 * (56 bits) and a float 6 (24 bits).  A value with more significant digits
 * is ROUNDED the way LOAD ROUNDED (LRER) rounds, which is what cc370's own
 * double -> float conversion uses: add one half of the last kept digit to
 * the magnitude, then truncate.  Measured on mvsdev (JOB00464):
 * (float)2147483647 is 2^31.  The float result is rounded once, from the
 * exact integer, not by way of a rounded double.
 *
 * Worked on 32-bit halves for the reasons given in @@muldi3.c.
 */

typedef union {
    unsigned long long  u;
    long long           s;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    struct { unsigned lo, hi; } w;      /* little-endian host test only */
#else
    struct { unsigned hi, lo; } w;      /* S/370 is big-endian */
#endif
} dw_t;

typedef union {
    double              d;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    struct { unsigned lo, hi; } w;
#else
    struct { unsigned hi, lo; } w;
#endif
} dbl_t;

typedef union {
    float               f;
    unsigned            w;
} flt_t;

double  __fltddf(long long x)  asm("@@FLTDDF");
float   __fltdsf(long long x)  asm("@@FLTDSF");

static void
shl64(unsigned *h, unsigned *l, int n)
{
    if (n >= 64)      { *h = 0; *l = 0; }
    else if (n >= 32) { *h = *l << (n - 32); *l = 0; }
    else if (n > 0)   { *h = (*h << n) | (*l >> (32 - n)); *l <<= n; }
}

static void
shr64(unsigned *h, unsigned *l, int n)
{
    if (n >= 64)      { *h = 0; *l = 0; }
    else if (n >= 32) { *l = *h >> (n - 32); *h = 0; }
    else if (n > 0)   { *l = (*l >> n) | (*h << (32 - n)); *h >>= n; }
}

/* hexadecimal digits in a non-zero word */
static int
hexdigits(unsigned x)
{
    int n = 0;

    while (x) { n++; x >>= 4; }
    return n;
}

/* the HFP form of the signed integer x with DIGITS fraction digits (14
   for a double, 6 for a float): *hi carries sign, exponent and the top of
   the fraction, *lo the rest (0 for a float, whose fraction ends in *hi) */
static void
hfp_float(long long x, int digits, unsigned *hi, unsigned *lo)
{
    dw_t     v;
    unsigned mh, ml, sign = 0;
    int      n, d;

    v.s = x;
    mh = v.w.hi;
    ml = v.w.lo;
    if (mh & 0x80000000) {
        sign = 0x80000000;
        ml = ~ml + 1;
        mh = ~mh + (ml == 0);           /* LLONG_MIN stays 2^63 unsigned */
    }
    if (mh == 0 && ml == 0) {
        *hi = 0;
        *lo = 0;
        return;
    }
    n = mh ? 8 + hexdigits(mh) : hexdigits(ml);

    if (n <= digits) {
        shl64(&mh, &ml, 4 * (digits - n));
    } else {
        /* round: add half of the last kept digit, then drop d digits;
           the magnitude is at most 2^63, so the add cannot carry out */
        d = n - digits;
        if (4 * d - 1 >= 32)
            mh += 1u << (4 * d - 1 - 32);
        else {
            unsigned half = 1u << (4 * d - 1);
            ml += half;
            mh += (ml < half);
        }
        shr64(&mh, &ml, 4 * d);
        /* the round carried into a new digit: 0.FFF.. became 1.000.. */
        if (digits == 14 ? (mh & 0xFF000000) != 0 : (ml & 0xFF000000) != 0) {
            shr64(&mh, &ml, 4);
            n++;
        }
    }
    if (digits == 14) {
        *hi = sign | ((unsigned)(64 + n) << 24) | mh;
        *lo = ml;
    } else {
        *hi = sign | ((unsigned)(64 + n) << 24) | ml;
        *lo = 0;
    }
}

double
__fltddf(long long x)
{
    dbl_t r;

    hfp_float(x, 14, &r.w.hi, &r.w.lo);
    return r.d;
}

float
__fltdsf(long long x)
{
    flt_t    r;
    unsigned lo;

    hfp_float(x, 6, &r.w, &lo);
    return r.f;
}
