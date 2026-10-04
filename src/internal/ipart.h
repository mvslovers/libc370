#ifndef SRC_INTERNAL_IPART_H
#define SRC_INTERNAL_IPART_H

/* The integral part of a, truncated toward zero, for a >= 0 (#273).

   floor(), ceil(), modf() and fmod() used to take it through an int, which
   is wrong beyond 2**31: 10540800000 came back as 10540800000 mod 2**32.

   From 2**52 = 16**13 up a double has no fraction left: a long HFP fraction
   is 14 hex digits, so at that magnitude its last digit already counts units
   (IEEE: 52 fraction bits, the same bound).  a is returned as it is.

   Below it, the integral part is put together from three int conversions:
   of a / 2**32, of the remainder below 2**32 divided by 2**16, and of what
   is left below 2**16.  Dividing or multiplying by a power of 16 changes
   only the exponent in HFP (and by a power of 2 only the exponent in IEEE),
   the pieces do not overlap, and each fits an int - so no step rounds.  A
   long long conversion would do the same in one line, but it links cc370's
   @@FIXDFD and @@FLTDDF (about 5 KB) into every module that calls floor(). */
static __inline double __ipart(double a)
{
    double hi;
    double mid;

    if (a >= 4503599627370496.0) return (a);              /* 2**52 */
    hi  = (double)(int)(a / 4294967296.0) * 4294967296.0;  /* < 2**52 */
    a  -= hi;                                              /* [0, 2**32) */
    mid = (double)(int)(a / 65536.0) * 65536.0;            /* < 2**32 */
    a  -= mid;                                             /* [0, 2**16) */
    return (hi + mid + (double)(int)a);
}

#endif
