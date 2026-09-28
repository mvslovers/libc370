/*
 * tstzjt.c - libc370 #211: the C99 length modifiers z, t, j and hh, on MVS.
 *
 * __examin() (src/clib/@@examin.c) is the one parser behind the whole
 * printf family.  It knew h, l, ll, L and LL; "%6zd" took the 'z' for the
 * specifier, printed nothing for it, left the size_t on the va_list and let
 * the 'd' through as text - brexx370's trace line read "d *-* ".  Every
 * later argument of the call was then read one slot early.
 *
 * test/host/tstvsnp.c pins z, t and hh on the host.  What only MVS can
 * check is j: intmax_t is 64 bit and goes through the __64 helpers, which
 * are big-endian only and stubbed on the host.  So this probe runs every
 * modifier through both string sinks:
 *
 *   snprintf -> vsnprintf -> __examin
 *   sprintf  -> vsprintf  -> vvprintf -> __examin
 *
 * and prints the brexx370 line once through printf, the FILE sink, for the
 * eye.  Each case is followed by a plain %d so an unconsumed argument shows
 * as a wrong trailing value, not only as a wrong field.
 *
 *   (1) controls: %ld and %lld, unchanged by #211
 *   (2) z: %6zd, %zu, %zx, %-6zd, %06zu
 *   (3) t: %td of a negative ptrdiff_t
 *   (4) j: %jd, %ju, %jx of 64-bit values
 *   (5) hh: %hhd
 *
 * Not a check, printed only: %jd of -1.  The 64-bit path is unsigned only
 * (@@examin.c, "__64 values are unsigned"), so it prints 2^64-1, exactly as
 * %lld does today.
 *
 * Build:   make build
 *          cc370 -O1 -Iinclude -L build/sdk test/mvs/tstzjt.c \
 *                -o TSTZJT -flinker-output=iebcopy
 *          ld370 --pack TSTZJT=TSTZJT.iebcopy -o tstzjt -xmit \
 *                --dsn IBMUSER.LIBC370.ZJTSCR
 * Install: jcl/recvzjt.jcl (its own staging data set, not the mbt one).
 * Run:     jcl/tstzjt.jcl.
 *
 * GREEN: mvsdev JOB00640 step GREEN, CC 0000, 24/24, 2026-09-29.  The
 * FILE sink printed "[    15 *-* ]"; %jd of -1 printed 18446744073709551615.
 *
 * RED, same job, step RED: the same source linked against the sysroot libc
 * installed from f3292f2 (edge, before #211), CC 0001, 20 of 24 failed -
 * the four controls pass and every z/t/j/hh case fails in both sinks.  The
 * trailing %d shows the unconsumed argument each time: "%6zd *-* |%d" gave
 * "d *-* |15", "%jd|%d" gave "d|28" (28 = X'1C', the high word of
 * 123456789012), and printf wrote "[d *-* ]".
 *
 * cc370 warns "ptrdiff_t format, int arg" on (3): the compiler's own
 * ptrdiff_t is long, libc370's <stddef.h> says int.  Both are 32 bit, so
 * the value is right, but a -Werror consumer cannot pass libc370's
 * ptrdiff_t to %td.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

static void check_str(const char *got, const char *want, const char *msg)
{
    mbt_run++;
    if (strcmp(got, want) == 0) {
        mbt_passed++;
        printf("  PASS: %s\n", msg);
    }
    else {
        mbt_failed++;
        printf("  FAIL: %s (got \"%s\", want \"%s\")\n", msg, got, want);
    }
}

/* Every case goes through both sinks.  The buffers are cleared first so a
** conversion that writes nothing cannot pass on a previous case's text. */
#define BOTH(want, msg, ...)                                              \
    do {                                                                  \
        memset(b1, 0, sizeof(b1));                                        \
        memset(b2, 0, sizeof(b2));                                        \
        snprintf(b1, sizeof(b1), __VA_ARGS__);                            \
        sprintf(b2, __VA_ARGS__);                                         \
        check_str(b1, (want), "snprintf " msg);                           \
        check_str(b2, (want), "sprintf  " msg);                           \
    } while (0)

int main(void)
{
    char b1[64];
    char b2[64];

    printf("=== tstzjt: C99 length modifiers z, t, j, hh (#211) ===\n\n");

    printf("(1) controls:\n");
    BOTH("-15|7", "(1) %ld", "%ld|%d", -15L, 7);
    BOTH("123456789012|7", "(1) %lld", "%lld|%d", 123456789012LL, 7);

    printf("\n(2) z:\n");
    BOTH("    15 *-* |7", "(2) %6zd, the brexx370 trace line",
         "%6zd *-* |%d", (size_t)15, 7);
    BOTH("4096|X|7", "(2) %zu, then %s", "%zu|%s|%d", (size_t)4096, "X", 7);
    BOTH("c4|7", "(2) %zx", "%zx|%d", (size_t)0xC4, 7);
    BOTH("15    |7", "(2) %-6zd", "%-6zd|%d", (size_t)15, 7);
    BOTH("000015|7", "(2) %06zu", "%06zu|%d", (size_t)15, 7);

    printf("\n(3) t:\n");
    BOTH("-3|7", "(3) %td", "%td|%d", (ptrdiff_t)-3, 7);

    printf("\n(4) j:\n");
    BOTH("123456789012|7", "(4) %jd", "%jd|%d", (intmax_t)123456789012LL, 7);
    BOTH("0|7", "(4) %ju of 0", "%ju|%d", (uintmax_t)0, 7);
    BOTH("123456789a|7", "(4) %jx", "%jx|%d", (uintmax_t)0x123456789ALL, 7);

    printf("\n(5) hh:\n");
    BOTH("5|7", "(5) %hhd", "%hhd|%d", 5, 7);

    printf("\nprinted only (the FILE sink, and the known %%jd limit):\n");
    printf("  printf \"%%6zd *-* \": [%6zd *-* ]\n", (size_t)15);
    snprintf(b1, sizeof(b1), "%jd", (intmax_t)-1);
    printf("  %%jd of -1: %s  (unsigned-only 64-bit path)\n", b1);

    printf("\n=== tstzjt: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
