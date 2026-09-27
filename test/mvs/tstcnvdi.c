/*
 * tstcnvdi.c - libc370 #190: the remaining libgcc helpers, on MVS.
 *
 * Every helper is reached the way a program reaches it - through a cast
 * or a __builtin_, so cc370 emits the libcall and chooses its name - with
 * one addition: (4) also calls __cmpdi2 by name, because a program only
 * ever reaches it inside (double) of an unsigned long long.  That
 * checks what test/host/tstcnvdi.c cannot: the S/370 code, the libcall
 * ABI (a float argument passed as 4 bytes, a long long returned through
 * the result address, a double in FPR0), and that the compiler's names
 * and libc370's agree.
 *
 *   (1) control: this is HFP, and 1.0 has the bits it should
 *   (2) long long -> double / float, 91 operands   @@FLTDDF, @@FLTDSF
 *   (3) double / float -> long long and unsigned long long, 90 operands
 *                                          @@FIXDFD, @@FXUNDF, @@FIXSFD, @@FXUNSF
 *   (4) (double) of an unsigned long long >= 2^63     @@FLTDDF + @@CMPDI2
 *   (5) the bit builtins against loop references, 64-bit and 32-bit
 *                  @@POPCSI/DI, @@PARTSI/DI, @@CLZSI2/DI2, @@CTZSI2/DI2, @@FFSDI2
 *
 * The expected bits in (2) and (3) come from test/mvs/tstcnvkat.h, which
 * tstcnvkat.py computes with exact Python Fractions.
 *
 * THIS MUST BE COMPILED BY A cc370 THAT CARRIES cc370#471 (merged as
 * f3f7e21): before it the compiler calls @@FIXUNS / @@FLOATD / @@POPCOU /
 * @@PARITY, which no library can define, and the link fails.  Check: the
 * .s of `(unsigned long long)d` must show V(@@FXUNDF), not V(@@FIXUNS).
 * The library itself does not care which cc370 builds it - its names are
 * pinned with asm().
 *
 * Build:   make build
 *          cc370 -O1 -Iinclude -L build/sdk test/mvs/tstcnvdi.c \
 *                -o TSTCNVDI -flinker-output=iebcopy
 *          ld370 --pack TSTCNVDI=TSTCNVDI.iebcopy -o tstcnvdi -xmit \
 *                --dsn IBMUSER.LIBC370.CNVSCR
 * Install: jcl/recvcnv.jcl (its own staging data set, not the mbt one).
 * Run:     jcl/tstcnvdi.jcl.
 *
 * GREEN: mvsdev JOB00483, CC 0000, 565/565, 2026-09-27, built with the
 * installed cc370 (which carries #471).  JOB00477 was the same run before
 * the four zero-operand checks in (5) were added: 563/563.
 *
 * RED the same day, each with a defective TU linked ahead of the library:
 *   - @@fltdi.c truncating instead of rounding: JOB00479, CC 0001, 39
 *     failed, all in (2).
 *   - __fxunsf taking a double - reading 8 bytes where the caller passes
 *     4, and handing them on as bits: JOB00481, CC 0001, 21 failed, the
 *     20 (ull)float rows a host simulation of that defect predicts (the
 *     stray word is the row index an earlier call left at 92(13)) plus
 *     the (3) summary; every other conversion stays green.  A first try
 *     at this control assigned the double to a float inside the helper,
 *     which rounds the extra bytes away again, and passed - a control has
 *     to be checked to break what it claims to break.
 * And against the sysroot libc before #190: the link fails, 16 helpers
 * unresolved.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>

#include "tstcnvkat.h"

typedef long long           ll;
typedef unsigned long long  ull;

typedef union {
    ull u;
    ll  s;
    struct { unsigned hi, lo; } w;
} dw_t;

typedef union {
    double d;
    struct { unsigned hi, lo; } w;
} dbl_t;

typedef union {
    float    f;
    unsigned w;
} flt_t;

int __cmpdi2(ll a, ll b) asm("@@CMPDI2");

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0, quiet = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }         \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }         \
    } while (0)

static void
expect(const char *what, int row, unsigned gh, unsigned gl,
       unsigned wh, unsigned wl)
{
    mbt_run++;
    if (gh == wh && gl == wl) { mbt_passed++; return; }
    mbt_failed++;
    if (++quiet <= 30)
        printf("  FAIL: %s [%d] = %08X %08X, want %08X %08X\n",
               what, row, gh, gl, wh, wl);
}

/* loop references on 32-bit words, so they share no code with the
   helpers and need no 64-bit operation */
static int
ref_popc(unsigned x)
{
    int n = 0;

    while (x) { n += (int)(x & 1); x >>= 1; }
    return n;
}

static int
ref_clz(unsigned x)
{
    int n = 0;

    if (!x) return 32;
    while (!(x & 0x80000000)) { n++; x <<= 1; }
    return n;
}

static int
ref_ctz(unsigned x)
{
    int n = 0;

    if (!x) return 32;
    while (!(x & 1)) { n++; x >>= 1; }
    return n;
}

static volatile unsigned bitv[][2] = {
    { 0x00000000, 0x00000001 }, { 0x80000000, 0x00000000 },
    { 0x00000001, 0x00000000 }, { 0xFFFFFFFF, 0xFFFFFFFF },
    { 0x00000000, 0x80000000 }, { 0x12345678, 0x9ABCDEF0 },
    { 0x00010000, 0x00000000 }, { 0x00000000, 0x00010000 },
    { 0x55555555, 0xAAAAAAAA }, { 0x7FFFFFFF, 0xFFFFFFFE },
    { 0x00000000, 0x00000003 }, { 0xF0000000, 0x0000000F },
};

int
main(void)
{
    int      i, n, bad;
    dw_t     x, r;
    dbl_t    d;
    flt_t    f;
    unsigned hi, lo;

    printf("=== TSTCNVDI: libc370 #190 libgcc helpers ===\n\n");

    printf("(1) control - HFP\n");
    d.d = 1.0;
    CHECK(d.w.hi == 0x41100000 && d.w.lo == 0, "(1) 1.0 is X'4110000000000000'");
    f.f = 1.0f;
    CHECK(f.w == 0x41100000, "(1) 1.0f is X'41100000'");

    printf("(2) long long -> double / float\n");
    n = (int)(sizeof kat_flt / sizeof kat_flt[0]);
    for (i = 0; i < n; i++) {
        x.w.hi = kat_flt[i][0];
        x.w.lo = kat_flt[i][1];
        d.d = (double)x.s;
        expect("(double)ll", i, d.w.hi, d.w.lo, kat_flt[i][2], kat_flt[i][3]);
        f.f = (float)x.s;
        expect("(float)ll", i, f.w, 0, kat_flt[i][4], 0);
    }
    CHECK(quiet == 0, "(2) every long long converts to the expected bits");

    printf("(3) double / float -> long long, unsigned long long\n");
    bad = quiet;
    n = (int)(sizeof kat_fix / sizeof kat_fix[0]);
    for (i = 0; i < n; i++) {
        d.w.hi = kat_fix[i][0];
        d.w.lo = kat_fix[i][1];
        r.s = (ll)d.d;
        expect("(ll)double", i, r.w.hi, r.w.lo, kat_fix[i][2], kat_fix[i][3]);
        r.u = (ull)d.d;
        expect("(ull)double", i, r.w.hi, r.w.lo, kat_fix[i][2], kat_fix[i][3]);
        f.w = kat_fix[i][0];
        r.s = (ll)f.f;
        expect("(ll)float", i, r.w.hi, r.w.lo, kat_fix[i][4], kat_fix[i][5]);
        r.u = (ull)f.f;
        expect("(ull)float", i, r.w.hi, r.w.lo, kat_fix[i][4], kat_fix[i][5]);
    }
    CHECK(quiet == bad, "(3) every HFP value converts to the expected integer");

    printf("(4) (double) of an unsigned long long, through @@CMPDI2\n");
    x.w.hi = 0x80000000;
    x.w.lo = 0;
    d.d = (double)x.u;
    printf("  (double)2^63     = %08X %08X\n", d.w.hi, d.w.lo);
    CHECK(d.d == 9223372036854775808.0, "(4) (double)2^63 is exact");
    x.w.hi = 0xFFFFFFFF;
    x.w.lo = 0xFFFFFFFF;
    d.d = (double)x.u;
    printf("  (double)2^64 - 1 = %08X %08X\n", d.w.hi, d.w.lo);
    /* measured 51100000 00000000, i.e. exactly 2^64: the compiler's own
       fixup adds 2^64 to (double)-1 with AD, which truncates - that is
       cc370's code, not @@FLTDDF rounding up */
    CHECK(d.d > 1.8446744e19 && d.d < 1.8446745e19,
          "(4) (double)(2^64 - 1) is about 1.8446744e19");
    x.w.hi = 0;
    x.w.lo = 12345;
    d.d = (double)x.u;
    CHECK(d.d == 12345.0, "(4) a small unsigned long long is exact");
    CHECK(__cmpdi2(-1, 0) == 0 && __cmpdi2(5, 5) == 1 && __cmpdi2(0, -1) == 2,
          "(4) __cmpdi2 answers 0 / 1 / 2");

    printf("(5) bit builtins\n");
    bad = 0;
    n = (int)(sizeof bitv / sizeof bitv[0]);
    for (i = 0; i < n; i++) {
        hi = bitv[i][0];
        lo = bitv[i][1];
        x.w.hi = hi;
        x.w.lo = lo;
        if (__builtin_popcount(lo) != ref_popc(lo)) bad++;
        if (__builtin_popcountll(x.u) != ref_popc(hi) + ref_popc(lo)) bad++;
        if (__builtin_parity(lo) != (ref_popc(lo) & 1)) bad++;
        if (__builtin_parityll(x.u) != ((ref_popc(hi) + ref_popc(lo)) & 1)) bad++;
        if (lo && __builtin_clz(lo) != ref_clz(lo)) bad++;
        if (lo && __builtin_ctz(lo) != ref_ctz(lo)) bad++;
        if (x.u && __builtin_clzll(x.u) != (hi ? ref_clz(hi) : 32 + ref_clz(lo))) bad++;
        if (x.u && __builtin_ctzll(x.u) != (lo ? ref_ctz(lo) : 32 + ref_ctz(hi))) bad++;
        if (__builtin_ffsll(x.s) != (lo ? ref_ctz(lo) + 1 : hi ? ref_ctz(hi) + 33 : 0)) bad++;
        if (bad && quiet++ < 30)
            printf("  FAIL: bit ops on %08X %08X\n", hi, lo);
        bad = bad ? 1 : 0;
        if (bad) { mbt_failed++; mbt_run++; bad = 0; }
        else { mbt_passed++; mbt_run++; }
    }
    /* the zero operand: ffs is defined (0); clz/ctz are undefined in GCC,
       and @@bitops.c answers the operand width */
    x.u = 0;
    lo = x.w.lo;
    CHECK(__builtin_ffsll(x.s) == 0, "(5) ffsll(0) is 0");
    CHECK(__builtin_clz(lo) == 32 && __builtin_ctz(lo) == 32,
          "(5) clz(0) and ctz(0) are 32");
    CHECK(__builtin_clzll(x.u) == 64 && __builtin_ctzll(x.u) == 64,
          "(5) clzll(0) and ctzll(0) are 64");
    printf("  %d operands\n", n);

    printf("\n=== %d run, %d passed, %d failed ===\n",
           mbt_run, mbt_passed, mbt_failed);
    return mbt_failed ? 1 : 0;
}
