/* @@DIVDI3.C - long long divide and remainder, the libgcc helpers cc370
 * calls for them.
 *
 *   __divdi3   signed   /      @@DIVDI3
 *   __moddi3   signed   %      @@MODDI3
 *   __udivdi3  unsigned /      @@UDIVDI
 *   __umoddi3  unsigned %      @@UMODDI
 *
 * Before #187 nothing defined them and any program dividing a long long
 * by a variable failed to link.  They share one TU because they share
 * udivmod(); a program that only multiplies does not pull this in.
 *
 * Everything below works on 32-bit halves, for the reasons given in
 * @@muldi3.c: a 64-bit operation here could call back into this file,
 * and cc370 miscompiles a 64-bit `<<` (cc370#468) and a signed divide by
 * a constant (cc370#467).  None of those shapes may appear in this file.
 *
 * C semantics: the quotient truncates toward zero and the remainder takes
 * the sign of the dividend.  LLONG_MIN / -1 wraps to LLONG_MIN and
 * LLONG_MIN % -1 is 0, as in libgcc.  A zero divisor abends S0C9, which
 * is what an int divided by zero does on this machine; libgcc traps the
 * same way.
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

long long           __divdi3(long long a, long long b)          asm("@@DIVDI3");
long long           __moddi3(long long a, long long b)          asm("@@MODDI3");
unsigned long long  __udivdi3(unsigned long long a,
                              unsigned long long b)             asm("@@UDIVDI");
unsigned long long  __umoddi3(unsigned long long a,
                              unsigned long long b)             asm("@@UMODDI");

/* the same fixed-point divide exception as `int / 0` */
static unsigned
divide_by_zero(void)
{
    volatile int zero = 0;
    int one = 1;

    return (unsigned)(one / zero);
}

/* n / d and n % d, all unsigned; d is not zero */
static void
udivmod(dw_t *n, dw_t *d, dw_t *q, dw_t *r)
{
    unsigned nh = n->w.hi, nl = n->w.lo;
    unsigned dh = d->w.hi, dl = d->w.lo;
    unsigned h = 0, l = 0, xh = 0, xl = 0;
    int i;

    if (nh == 0 && dh == 0) {
        /* both fit in 32 bits: one hardware divide */
        q->w.hi = 0;
        q->w.lo = nl / dl;
        r->w.hi = 0;
        r->w.lo = nl - q->w.lo * dl;
        return;
    }

    /* restoring division, one quotient bit per round, high bit first */
    for (i = 0; i < 64; i++) {
        h  = (h << 1) | (l >> 31);
        l  = (l << 1) | (nh >> 31);
        nh = (nh << 1) | (nl >> 31);
        nl <<= 1;
        xh = (xh << 1) | (xl >> 31);
        xl <<= 1;
        if (h > dh || (h == dh && l >= dl)) {
            h -= dh + (l < dl);
            l -= dl;
            xl |= 1;
        }
    }
    q->w.hi = xh;
    q->w.lo = xl;
    r->w.hi = h;
    r->w.lo = l;
}

static void
negate(dw_t *v)
{
    v->w.lo = ~v->w.lo + 1;
    v->w.hi = ~v->w.hi + (v->w.lo == 0);
}

unsigned long long
__udivdi3(unsigned long long a, unsigned long long b)
{
    dw_t n, d, q, r;

    n.u = a;
    d.u = b;
    if (d.w.hi == 0 && d.w.lo == 0) {
        q.w.hi = 0;
        q.w.lo = divide_by_zero();
        return q.u;
    }
    udivmod(&n, &d, &q, &r);
    return q.u;
}

unsigned long long
__umoddi3(unsigned long long a, unsigned long long b)
{
    dw_t n, d, q, r;

    n.u = a;
    d.u = b;
    if (d.w.hi == 0 && d.w.lo == 0) {
        r.w.hi = 0;
        r.w.lo = divide_by_zero();
        return r.u;
    }
    udivmod(&n, &d, &q, &r);
    return r.u;
}

long long
__divdi3(long long a, long long b)
{
    dw_t n, d, q, r;
    int negative = 0;

    n.s = a;
    d.s = b;
    if (d.w.hi == 0 && d.w.lo == 0) {
        q.w.hi = 0;
        q.w.lo = divide_by_zero();
        return q.s;
    }
    if (n.w.hi & 0x80000000) { negate(&n); negative = !negative; }
    if (d.w.hi & 0x80000000) { negate(&d); negative = !negative; }
    udivmod(&n, &d, &q, &r);
    if (negative) negate(&q);
    return q.s;
}

long long
__moddi3(long long a, long long b)
{
    dw_t n, d, q, r;
    int negative = 0;

    n.s = a;
    d.s = b;
    if (d.w.hi == 0 && d.w.lo == 0) {
        r.w.hi = 0;
        r.w.lo = divide_by_zero();
        return r.s;
    }
    if (n.w.hi & 0x80000000) { negate(&n); negative = 1; }
    if (d.w.hi & 0x80000000) negate(&d);
    udivmod(&n, &d, &q, &r);
    if (negative) negate(&r);
    return r.s;
}
