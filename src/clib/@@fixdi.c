/* @@FIXDI.C - float and double to long long, the libgcc helpers cc370
 * calls for them (#190).
 *
 *   (long long)double              __fixdfdi   @@FIXDFD
 *   (long long)float               __fixsfdi   @@FIXSFD
 *   (unsigned long long)double     __fxundf    @@FXUNDF  (cc370#470)
 *   (unsigned long long)float      __fxunsf    @@FXUNSF  (cc370#470)
 *
 * The operands are IBM hexadecimal floating point: a sign bit, a 7-bit
 * exponent in excess 64 giving a power of 16, and a 56-bit (double) or
 * 24-bit (float) fraction.  The integer part is taken from those bits
 * directly - no floating-point arithmetic, so no guard-digit or
 * normalization question arises, and an unnormalized operand converts by
 * its value like any other.
 *
 * Semantics, chosen to match what cc370's inline (int) and (unsigned)
 * conversions do (measured on mvsdev, JOB00464):
 *   - the fraction is truncated toward zero: -1.5 -> -1;
 *   - out of range, the result is the integer part modulo 2^64, as the
 *     32-bit inline code gives it modulo 2^32: (int)4294967296.0 == 0;
 *   - a negative value converts to unsigned through the signed result,
 *     as (unsigned)-1.0 == 0xFFFFFFFF: (unsigned long long)-1.0 is all
 *     ones.
 * With those three rules the signed and the unsigned conversion give the
 * same 64 bits, so all four entry points share one routine.
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

long long           __fixdfdi(double d)  asm("@@FIXDFD");
long long           __fixsfdi(float f)   asm("@@FIXSFD");
unsigned long long  __fxundf(double d)   asm("@@FXUNDF");
unsigned long long  __fxunsf(float f)    asm("@@FXUNSF");

/* (h,l) <<= n and (h,l) >>= n, logical, for any n >= 0 */
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

/* the integer part of the long HFP value (hi,lo), modulo 2^64; a short
   value is the same with lo = 0 */
static void
hfp_fix(unsigned hi, unsigned lo, dw_t *r)
{
    unsigned fh = hi & 0x00FFFFFF, fl = lo;
    int      e  = (int)((hi >> 24) & 0x7F);

    /* value = fraction * 16^(e - 64 - 14), i.e. a shift of 4e - 312 bits */
    if (4 * e - 312 >= 0)
        shl64(&fh, &fl, 4 * e - 312);
    else
        shr64(&fh, &fl, 312 - 4 * e);
    if (hi & 0x80000000) {
        fl = ~fl + 1;
        fh = ~fh + (fl == 0);
    }
    r->w.hi = fh;
    r->w.lo = fl;
}

long long
__fixdfdi(double d)
{
    dbl_t v;
    dw_t  r;

    v.d = d;
    hfp_fix(v.w.hi, v.w.lo, &r);
    return r.s;
}

unsigned long long
__fxundf(double d)
{
    dbl_t v;
    dw_t  r;

    v.d = d;
    hfp_fix(v.w.hi, v.w.lo, &r);
    return r.u;
}

long long
__fixsfdi(float f)
{
    flt_t v;
    dw_t  r;

    v.f = f;
    hfp_fix(v.w, 0, &r);
    return r.s;
}

unsigned long long
__fxunsf(float f)
{
    flt_t v;
    dw_t  r;

    v.f = f;
    hfp_fix(v.w, 0, &r);
    return r.u;
}
