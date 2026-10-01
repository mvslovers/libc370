/* @@BITOPS.C - the bit-counting libgcc helpers cc370 calls for the
 * __builtin_ popcount, parity, ffs, clz and ctz families (#190).
 *
 *   __builtin_popcount   / ll   @@POPCSI / @@POPCDI
 *   __builtin_parity     / ll   @@PARTSI / @@PARTDI
 *   __builtin_ffsll             @@FFSDI2   (__builtin_ffs is inline)
 *   __builtin_clz        / ll   @@CLZSI2 / @@CLZDI2
 *   __builtin_ctz        / ll   @@CTZSI2 / @@CTZDI2
 *
 * The popcount and parity names are cc370's own (cc370#470): libgcc's
 * __popcountsi2/__popcountdi2 and __paritysi2/__paritydi2 both cut to one
 * 8-character name.  They are tiny, so they share one member.
 *
 * ffs of 0 is 0, as in libgcc.  clz and ctz of 0 are undefined in GCC;
 * here they return the operand width (32 or 64), so a caller that forgets
 * the zero case gets a bounded answer rather than a loop that never ends.
 *
 * The 64-bit forms work on 32-bit halves for the reasons given in
 * @@muldi3.c.
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

int __popcsi(unsigned x)            asm("@@POPCSI");
int __popcdi(unsigned long long x)  asm("@@POPCDI");
int __partsi(unsigned x)            asm("@@PARTSI");
int __partdi(unsigned long long x)  asm("@@PARTDI");
int __ffsdi2(long long x)           asm("@@FFSDI2");
int __clzsi2(unsigned x)            asm("@@CLZSI2");
int __clzdi2(unsigned long long x)  asm("@@CLZDI2");
int __ctzsi2(unsigned x)            asm("@@CTZSI2");
int __ctzdi2(unsigned long long x)  asm("@@CTZDI2");

int
__popcsi(unsigned x)
{
    x = x - ((x >> 1) & 0x55555555);
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    return (int)((x + (x >> 8) + (x >> 16) + (x >> 24)) & 0x3F);
}

int
__popcdi(unsigned long long x)
{
    dw_t v;

    v.u = x;
    return __popcsi(v.w.hi) + __popcsi(v.w.lo);
}

int
__partsi(unsigned x)
{
    x ^= x >> 16;
    x ^= x >> 8;
    x ^= x >> 4;
    x ^= x >> 2;
    x ^= x >> 1;
    return (int)(x & 1);
}

int
__partdi(unsigned long long x)
{
    dw_t v;

    v.u = x;
    return __partsi(v.w.hi ^ v.w.lo);
}

int
__clzsi2(unsigned x)
{
    int n = 0;

    if (x == 0) return 32;
    if (!(x & 0xFFFF0000)) { n += 16; x <<= 16; }
    if (!(x & 0xFF000000)) { n += 8;  x <<= 8;  }
    if (!(x & 0xF0000000)) { n += 4;  x <<= 4;  }
    if (!(x & 0xC0000000)) { n += 2;  x <<= 2;  }
    if (!(x & 0x80000000)) { n += 1; }
    return n;
}

int
__clzdi2(unsigned long long x)
{
    dw_t v;

    v.u = x;
    if (v.w.hi) return __clzsi2(v.w.hi);
    return 32 + __clzsi2(v.w.lo);
}

int
__ctzsi2(unsigned x)
{
    int n = 0;

    if (x == 0) return 32;
    if (!(x & 0x0000FFFF)) { n += 16; x >>= 16; }
    if (!(x & 0x000000FF)) { n += 8;  x >>= 8;  }
    if (!(x & 0x0000000F)) { n += 4;  x >>= 4;  }
    if (!(x & 0x00000003)) { n += 2;  x >>= 2;  }
    if (!(x & 0x00000001)) { n += 1; }
    return n;
}

int
__ctzdi2(unsigned long long x)
{
    dw_t v;

    v.u = x;
    if (v.w.lo) return __ctzsi2(v.w.lo);
    return 32 + __ctzsi2(v.w.hi);
}

int
__ffsdi2(long long x)
{
    dw_t v;

    v.s = x;
    if (v.w.lo) return 1 + __ctzsi2(v.w.lo);
    if (v.w.hi) return 33 + __ctzsi2(v.w.hi);
    return 0;
}
