/*
 * tstcnvdi.c - libc370 #190: the remaining libgcc helpers cc370 calls.
 *
 * #187 added the long long arithmetic helpers.  This checks the rest:
 *
 *   @@fixdi.c    float/double -> long long, signed and unsigned
 *   @@fltdi.c    long long -> double/float
 *   @@cmpdi2.c   __cmpdi2 (0/1/2)
 *   @@bitops.c   popcount, parity, ffs, clz, ctz, 32- and 64-bit
 *
 * The four TUs are #included.  The conversions are checked through their
 * word-level cores, hfp_fix() and hfp_float(), against
 * test/mvs/tstcnvkat.h - exact expected bits that tstcnvkat.py computes
 * with Python Fractions, and which agree with the three int -> double /
 * float values measured on mvsdev (JOB00464).  Checking the cores rather
 * than the entry points keeps host IEEE hardware away from the HFP bit
 * patterns entirely.  The bit operations and __cmpdi2 are checked against
 * independent loop and native references over edge values and 200,000
 * random operands each.
 *
 * What this cannot see is the S/370 code, the libcall ABI (a float passed
 * as 4 bytes) and the compiler's choice of helper; test/mvs/tstcnvdi.c is
 * the gate for those.
 *
 * BUILD / RUN (host, from the repo root):
 *
 *     cc -std=gnu99 -Wall -Wextra -Werror -O1 -o /tmp/tstcnvdi \
 *        test/host/tstcnvdi.c && /tmp/tstcnvdi
 *
 * GREEN 2026-09-27: 2,200,764 checks, 0 failures.
 *
 * RED controls the same day, one defect per run in a scratch copy:
 *     fixdi:  shift amount one hex digit off              22 failures
 *     fixdi:  negate drops the carry                      56
 *     fixdi:  sign ignored                                39
 *     fltdi:  truncate instead of round                   28
 *     fltdi:  a rounding carry into a new digit ignored    4
 *     fltdi:  exponent off by one                         89
 *     bitops: clzsi2's last step                     181,475
 *     bitops: ctzdi2 with its halves swapped         164,074
 *     bitops: parity of the high word only           100,265
 *     cmpdi2: high words compared unsigned            99,768
 * (Changing only the branch test in hfp_fix() from 312 to 316 survives,
 * and should: both branches shift by 0 at the boundary it moves.)
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>

/* each TU carries its own dw_t and shift helpers; rename so they coexist */
#include "../../src/clib/@@cmpdi2.c"
#define dw_t dw_bit_t
#include "../../src/clib/@@bitops.c"
#undef dw_t
#define dw_t dw_fix_t
#define dbl_t dbl_fix_t
#define flt_t flt_fix_t
#define shl64 shl64_fix
#define shr64 shr64_fix
#include "../../src/clib/@@fixdi.c"
#undef dw_t
#undef dbl_t
#undef flt_t
#undef shl64
#undef shr64
#define dw_t dw_flt_t
#define dbl_t dbl_flt_t
#define flt_t flt_flt_t
#include "../../src/clib/@@fltdi.c"
#undef dw_t
#undef dbl_t
#undef flt_t

#include "../mvs/tstcnvkat.h"

typedef unsigned long long ull;

static long checks, failures;

static void
expect(const char *what, long row, ull got, ull want)
{
    checks++;
    if (got == want)
        return;
    if (++failures <= 20)
        printf("  FAIL: %s [%ld] = %016llX, want %016llX\n",
               what, row, got, want);
}

static ull
join(unsigned hi, unsigned lo)
{
    return ((ull)hi << 32) | lo;
}

/* independent references for the bit operations */
static int ref_popc(ull x) { int n = 0; while (x) { n += (int)(x & 1); x >>= 1; } return n; }
static int ref_clz(ull x, int w) { int n = 0; ull b = 1ULL << (w - 1); if (!x) return w; while (!(x & b)) { n++; b >>= 1; } return n; }
static int ref_ctz(ull x, int w) { int n = 0; if (!x) return w; while (!(x & 1)) { n++; x >>= 1; } return n; }
static int ref_ffs(ull x) { return x ? ref_ctz(x, 64) + 1 : 0; }

static ull rng = 0x0123456789ABCDEFULL;

static ull
next(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
}

static void
bit_ops(ull x)
{
    unsigned lo = (unsigned)x;
    long long a = (long long)x, b = (long long)next();

    expect("popcsi", 0, (ull)__popcsi(lo), (ull)ref_popc(lo));
    expect("popcdi", 0, (ull)__popcdi(x), (ull)ref_popc(x));
    expect("partsi", 0, (ull)__partsi(lo), (ull)(ref_popc(lo) & 1));
    expect("partdi", 0, (ull)__partdi(x), (ull)(ref_popc(x) & 1));
    expect("clzsi2", 0, (ull)__clzsi2(lo), (ull)ref_clz(lo, 32));
    expect("clzdi2", 0, (ull)__clzdi2(x), (ull)ref_clz(x, 64));
    expect("ctzsi2", 0, (ull)__ctzsi2(lo), (ull)ref_ctz(lo, 32));
    expect("ctzdi2", 0, (ull)__ctzdi2(x), (ull)ref_ctz(x, 64));
    expect("ffsdi2", 0, (ull)__ffsdi2(a), (ull)ref_ffs(x));
    expect("cmpdi2", 0, (ull)__cmpdi2(a, b), (ull)(a < b ? 0 : a == b ? 1 : 2));
    expect("cmpdi2 =", 0, (ull)__cmpdi2(a, a), 1);
}

int
main(void)
{
    long i, n;
    unsigned hi, lo;
    dw_fix_t r;
    static const ull edges[] = {
        0, 1, 2, 3, 0x80, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000,
        0xFFFFFFFF, 0x100000000ULL, 0x8000000000000000ULL,
        0x7FFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0x00000001FFFFFFFFULL,
        0xFFFFFFFF00000000ULL, 0x0000000100000000ULL, 0x5555555555555555ULL,
    };

    printf("=== TSTCNVDI: libc370 #190 helpers (host) ===\n");

    n = (long)(sizeof kat_flt / sizeof kat_flt[0]);
    for (i = 0; i < n; i++) {
        long long x = (long long)join(kat_flt[i][0], kat_flt[i][1]);

        hfp_float(x, 14, &hi, &lo);
        expect("fltddf", i, join(hi, lo), join(kat_flt[i][2], kat_flt[i][3]));
        hfp_float(x, 6, &hi, &lo);
        expect("fltdsf", i, join(hi, lo), join(kat_flt[i][4], 0));
    }
    printf("  int -> HFP:  %ld operands\n", n);

    n = (long)(sizeof kat_fix / sizeof kat_fix[0]);
    for (i = 0; i < n; i++) {
        hfp_fix(kat_fix[i][0], kat_fix[i][1], &r);
        expect("fix (double)", i, join(r.w.hi, r.w.lo),
               join(kat_fix[i][2], kat_fix[i][3]));
        hfp_fix(kat_fix[i][0], 0, &r);
        expect("fix (float)", i, join(r.w.hi, r.w.lo),
               join(kat_fix[i][4], kat_fix[i][5]));
    }
    printf("  HFP -> int:  %ld operands\n", n);

    for (i = 0; i < (long)(sizeof edges / sizeof edges[0]); i++) {
        bit_ops(edges[i]);
        bit_ops(~edges[i]);
    }
    for (i = 0; i < 200000; i++)
        bit_ops(next() >> (next() & 63));
    /* the zero operand, against literals rather than the references,
       which make the same choice and so would agree with any answer */
    expect("clzsi2(0)", 0, (ull)__clzsi2(0), 32);
    expect("clzdi2(0)", 0, (ull)__clzdi2(0), 64);
    expect("ctzsi2(0)", 0, (ull)__ctzsi2(0), 32);
    expect("ctzdi2(0)", 0, (ull)__ctzdi2(0), 64);
    expect("ffsdi2(0)", 0, (ull)__ffsdi2(0), 0);
    expect("popcdi(0)", 0, (ull)__popcdi(0), 0);
    printf("  bit ops, cmpdi2: done\n");

    printf("  total: %ld checks, %ld failures\n", checks, failures);
    return failures ? 1 : 0;
}
