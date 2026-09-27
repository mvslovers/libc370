/* @@CMPDI2.C - signed long long compare, the libgcc helper cc370 calls.
 *
 * __cmpdi2(a, b) returns 0 if a < b, 1 if a == b and 2 if a > b - libgcc's
 * contract, not -1/0/1.  cc370 does not call it for a plain comparison,
 * which it expands inline; it calls it while converting an unsigned long
 * long to double (#190), next to @@FLTDDF.  Worked on 32-bit halves for
 * the reasons given in @@muldi3.c.
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

int __cmpdi2(long long a, long long b) asm("@@CMPDI2");

int
__cmpdi2(long long a, long long b)
{
    dw_t x, y;

    x.s = a;
    y.s = b;
    /* the high words decide as signed, the low words as unsigned */
    if ((int)x.w.hi < (int)y.w.hi) return 0;
    if ((int)x.w.hi > (int)y.w.hi) return 2;
    if (x.w.lo < y.w.lo) return 0;
    if (x.w.lo > y.w.lo) return 2;
    return 1;
}
