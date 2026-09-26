/*
 * tstdi3.c - libc370 #187: the libgcc long long helpers cc370 calls.
 *
 * cc370 compiles long long `*`, `/`, `%` and unary `-` into calls to
 * __muldi3, __divdi3, __moddi3, __udivdi3, __umoddi3 and __negdi2
 * (@@MULDI3, @@DIVDI3, @@MODDI3, @@UDIVDI, @@UMODDI, @@NEGDI2), and libc370
 * - which is cc370's libgcc - defined none of them, so such a program did
 * not link.  src/clib/@@muldi3.c, @@divdi3.c and @@negdi2.c add them.
 *
 * This test #includes those three TUs and checks every entry point against
 * the host's native 64-bit arithmetic: a table of edge operands crossed
 * with itself, then 2,000,000 pseudo-random pairs whose operand widths are
 * drawn so that both udivmod() paths (32-bit fast path, 64-round loop) and
 * divisors with only the high word set are all hit.
 *
 * The helpers work on 32-bit halves through a union, and the union's word
 * order is the one thing that differs on a little-endian host: the TUs
 * pick it from __BYTE_ORDER__, which cc370 does not define.  So what this
 * test proves is the arithmetic, not the S/370 code generation - cc370
 * miscompiles some 64-bit shapes (cc370#467, cc370#468) and no host run
 * can see that.  test/mvs/tstdi3.c is the gate for the target.
 *
 * A zero divisor is not exercised here: on the target it abends S0C9 like
 * `int / 0`, on an arm64 host an integer divide by zero does not trap at
 * all.  test/mvs/tstdi3.c checks the abend.
 *
 * BUILD / RUN (host, from the repo root):
 *
 *     cc -std=gnu99 -Wall -Wextra -Werror -O1 -o /tmp/tstdi3 \
 *        test/host/tstdi3.c && /tmp/tstdi3
 *
 * GREEN 2026-09-26: 11,989,502 checks, 0 failures.
 *
 * RED controls, 2026-09-26: this file and the three TUs copied to a
 * scratch tree, one defect injected per run.  Every one fails:
 *
 *     muldi3: a cross term dropped                  1,197,411 failures
 *     muldi3: mul32's carry out of mid dropped        807,961
 *     divdi3: the borrow in the restoring step        891,091
 *     divdi3: fast path taken when only nh == 0       368,326
 *     divdi3: % takes the divisor's sign too          750,410
 *     divdi3: / ignores the divisor's sign            299,603
 *     negdi2: the carry into the high word            254,261
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <limits.h>

/* each TU carries its own dw_t; rename two of them so the three coexist */
#include "../../src/clib/@@muldi3.c"
#define dw_t dw_div_t
#include "../../src/clib/@@divdi3.c"
#undef dw_t
#define dw_t dw_neg_t
#include "../../src/clib/@@negdi2.c"
#undef dw_t

typedef long long ll;
typedef unsigned long long ull;

static long checks, failures;

#define FAIL_LIMIT 20

static void
fail(const char *op, ull a, ull b, ull got, ull want)
{
    failures++;
    if (failures <= FAIL_LIMIT)
        printf("  FAIL: %s(%016llX, %016llX) = %016llX, want %016llX\n",
               op, a, b, got, want);
}

#define EXPECT(op, a, b, got, want)                                      \
    do {                                                                 \
        ull g_ = (ull)(got), w_ = (ull)(want);                           \
        checks++;                                                        \
        if (g_ != w_) fail((op), (ull)(a), (ull)(b), g_, w_);            \
    } while (0)

/* every entry point against native arithmetic for one operand pair */
static void
check_pair(ull a, ull b)
{
    ll sa = (ll)a, sb = (ll)b;

    EXPECT("muldi3", a, b, __muldi3(sa, sb), a * b);
    EXPECT("negdi2", a, 0, __negdi2(sa), 0 - a);
    if (b == 0)
        return;
    EXPECT("udivdi3", a, b, __udivdi3(a, b), a / b);
    EXPECT("umoddi3", a, b, __umoddi3(a, b), a % b);
    if (sa == LLONG_MIN && sb == -1) {
        /* overflows in C; libgcc wraps, and so must we */
        EXPECT("divdi3", a, b, __divdi3(sa, sb), (ull)LLONG_MIN);
        EXPECT("moddi3", a, b, __moddi3(sa, sb), 0);
        return;
    }
    EXPECT("divdi3", a, b, __divdi3(sa, sb), sa / sb);
    EXPECT("moddi3", a, b, __moddi3(sa, sb), sa % sb);
}

static const ull edges[] = {
    0x0000000000000000ULL, 0x0000000000000001ULL, 0x0000000000000002ULL,
    0x0000000000000003ULL, 0x000000000000000AULL, 0x000000000000FFFFULL,
    0x0000000000010000ULL, 0x000000007FFFFFFFULL, 0x0000000080000000ULL,
    0x0000000080000001ULL, 0x00000000FFFFFFFEULL, 0x00000000FFFFFFFFULL,
    0x0000000100000000ULL, 0x0000000100000001ULL, 0x00000001FFFFFFFFULL,
    0x0000FFFF0000FFFFULL, 0x123456789ABCDEF0ULL, 0x7FFFFFFF00000000ULL,
    0x7FFFFFFFFFFFFFFEULL, 0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL,
    0x8000000000000001ULL, 0x80000000FFFFFFFFULL, 0xDEADBEEFCAFEBABEULL,
    0xFFFFFFFF00000000ULL, 0xFFFFFFFF7FFFFFFFULL, 0xFFFFFFFF80000000ULL,
    0xFFFFFFFFFFFFFFF6ULL, 0xFFFFFFFFFFFFFFFDULL, 0xFFFFFFFFFFFFFFFEULL,
    0xFFFFFFFFFFFFFFFFULL,
};

static ull rng = 0x9E3779B97F4A7C15ULL;

static ull
next(void)
{
    /* xorshift64 */
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
}

/* a random value of random width, so small operands are as common as
   full-width ones - uniform 64-bit operands almost never take the
   32-bit fast path or leave the divisor's low word empty */
static ull
operand(void)
{
    ull v = next();
    unsigned shape = (unsigned)(next() & 7);

    switch (shape) {
    case 0: return v & 0xFFFFULL;
    case 1: return v & 0xFFFFFFFFULL;
    case 2: return v & 0xFFFFFFFF00000000ULL;       /* low word empty */
    case 3: return v >> (next() & 63);
    case 4: return 0 - (v & 0xFFFFFFFFULL);         /* small negative */
    default: return v;
    }
}

int
main(void)
{
    size_t i, j, n = sizeof edges / sizeof edges[0];
    long k;

    printf("=== TSTDI3: libc370 #187 long long helpers (host) ===\n");

    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            check_pair(edges[i], edges[j]);
    printf("  edges:  %ld checks, %ld failures\n", checks, failures);

    for (k = 0; k < 2000000L; k++)
        check_pair(operand(), operand());
    printf("  total:  %ld checks, %ld failures\n", checks, failures);

    return failures ? 1 : 0;
}
