/* @@NEGDI2.C - long long negation, the libgcc helper cc370 calls for it.
 *
 * cc370 compiles a unary minus on a long long, signed or unsigned, into a
 * call to __negdi2 (@@NEGDI2).  Before #187 nothing defined it.  Worked on
 * 32-bit halves for the reasons given in @@muldi3.c.  -LLONG_MIN wraps to
 * LLONG_MIN, as in libgcc.
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

long long __negdi2(long long a) asm("@@NEGDI2");

long long
__negdi2(long long a)
{
    dw_t v;

    v.s = a;
    v.w.lo = ~v.w.lo + 1;
    v.w.hi = ~v.w.hi + (v.w.lo == 0);
    return v.s;
}
