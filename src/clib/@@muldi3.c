/* @@MULDI3.C - long long multiply, the libgcc helper cc370 calls for it.
 *
 * cc370 compiles `long long * long long` (signed or unsigned - the low 64
 * bits of the product are the same either way) into a call to __muldi3,
 * which reaches the linker as @@MULDI3.  Before #187 nothing defined it
 * and any program multiplying two long longs failed to link.
 *
 * The work is done on 32-bit halves only.  That is deliberate twice over:
 * a 64-bit operation here could be compiled into a call to this very
 * function, and two of the inline 64-bit shapes are miscompiled by cc370
 * today - `<<` is emitted as SLDA and drops bit 63 (cc370#468), a signed
 * divide by a constant becomes a bare DR (cc370#467).  The halves are
 * joined through a union, never by a shift.
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

long long __muldi3(long long a, long long b) asm("@@MULDI3");

/* 32 x 32 -> 64 unsigned, from 16-bit partial products: MR is a signed
   multiply, so it cannot give the high word of an unsigned product */
static void
mul32(unsigned a, unsigned b, unsigned *hi, unsigned *lo)
{
    unsigned a0 = a & 0xFFFF, a1 = a >> 16;
    unsigned b0 = b & 0xFFFF, b1 = b >> 16;
    unsigned p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    unsigned mid = (p00 >> 16) + (p01 & 0xFFFF) + (p10 & 0xFFFF);

    *lo = (p00 & 0xFFFF) | (mid << 16);
    *hi = p11 + (p01 >> 16) + (p10 >> 16) + (mid >> 16);
}

long long
__muldi3(long long a, long long b)
{
    dw_t x, y, r;

    x.s = a;
    y.s = b;
    mul32(x.w.lo, y.w.lo, &r.w.hi, &r.w.lo);
    /* the cross terms reach the high word only through their low 32 bits,
       which a plain 32-bit multiply gives whatever the operands' signs */
    r.w.hi += x.w.lo * y.w.hi + x.w.hi * y.w.lo;
    return r.s;
}
