/*
 * tsttodsc.c - libc370 #390: __todsec() converts a TOD clock value to
 * seconds since 1970 up to the end of the TOD clock in 2042.
 *
 * ISSUE #390: __getclk() divided the microseconds since 1970 with the
 * signed 64/32 D, which abends S0C9 once the quotient leaves 31 bits
 * (2038-01-19 03:14:08 UTC on), and dropped 0.32128 s of the 1900-1970
 * offset, so time() ran 0.32 s ahead.
 *
 * This #includes the real @@getclk.c with the STCK compiled out and checks
 * __todsec() against the conversion done in 64-bit host arithmetic, at
 * the edges and at a spread of values.  The old code was S/370 assembler
 * and cannot run here: the red is the MVS probe test/mvs/tsttodm.c.
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra \
 *        -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tsttodsc.c && ./t
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include "src/time/@@getclk.c"

static int bad;

/* the last whole second of the TOD clock: 2**52 us after 1900 */
#define LAST    ((4503599627370495ULL / 1000000ULL) - 2208988800ULL)

/* the TOD clock at s seconds and us microseconds after 1970 */
static void tod(unsigned long long s, unsigned us, unsigned w[2])
{
    unsigned long long v = ((2208988800ULL + s) * 1000000ULL + us) << 12;
    w[0] = (unsigned)(v >> 32);
    w[1] = (unsigned)v;
}

static void check(unsigned long long s, unsigned us)
{
    unsigned    w[2];
    unsigned    got;

    tod(s, us, w);
    got = __todsec(w);
    if (got != (unsigned)s) {
        printf("FAIL: %llu s + %u us gives %u\n", s, us, got);
        bad++;
    }
}

int main(void)
{
    unsigned long long  s;
    unsigned            w[2];

    check(0, 0);
    check(0, 999999);
    check(1, 0);
    check(1760000000ULL, 0);            /* 2025 */
    check(2147483647ULL, 999999);       /* 2038-01-19 03:14:07 */
    check(2147483648ULL, 0);            /* 2038-01-19 03:14:08 */
    check(LAST, 0);                     /* 2042-09-17 23:53:47 */
    for (s = 0; s <= LAST; s += 999983ULL) {
        check(s, 0);
        check(s, 999999);
    }
    /* the 0.32128 s that the old code dropped: 1970-01-01 00:00:00.7 */
    tod(0, 700000, w);
    if (__todsec(w) != 0) {
        printf("FAIL: 0.7 s after the epoch is not 0\n");
        bad++;
    }
    printf("tsttodsc: %s\n", bad ? "FAILED" : "passed");
    return bad ? 1 : 0;
}
