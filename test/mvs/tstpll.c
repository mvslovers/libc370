/*
 * tstpll.c - libc370 #321 on MVS: %lld, %lli and %jd print a negative
 * value with its sign.
 *
 * ISSUE #321: @@examin.c handed a 64-bit value to the __64 digit loop
 * with `neg = 0; // __64 values are unsigned`, so every negative long long
 * came out as its unsigned reading: -5 printed 18446744073709551611.
 * Found by test/mvs/tstinttyp.c (mvsdev JOB01163), since PRId64 is "lld".
 *
 * Built twice from this source: TSTPLL against this tree's libc.a,
 * TSTPLLR against the installed sysroot libc.a (2.0.0, before the fix) -
 * the red control.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstpll.c -o TSTPLL -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstpll.c -o TSTPLLR -flinker-output=iebcopy
 *          ld370 --pack TSTPLL=TSTPLL.iebcopy TSTPLLR=TSTPLLR.iebcopy \
 *                -o tstpll -xmit --dsn IBMUSER.LIBC370.PLLSCR
 * Install: jcl/recvpll.jcl.   Run: jcl/tstpll.jcl.
 *
 * mvsdev JOB01165, 2026-10-02 (RECEIVE JOB01164): GREEN CC 0000, 17/17;
 * RED CC 0001, 11 of 17 failed - every signed case but 0 and the
 * positive ones.  The three unsigned cases pass in both.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

static void fmt(const char *want, const char *f, ...)
{
    char b[48];
    va_list ap;

    va_start(ap, f);
    vsnprintf(b, sizeof(b), f, ap);
    va_end(ap);
    mbt_run++;
    if (strcmp(b, want) == 0) {
        mbt_passed++;
        printf("  PASS: \"%s\" -> \"%s\"\n", f, b);
    }
    else {
        mbt_failed++;
        printf("  FAIL: \"%s\" -> \"%s\", want \"%s\"\n", f, b, want);
    }
}

int main(void)
{
    printf("=== tstpll: signed 64-bit printf (#321) ===\n\n");
    fmt("-5", "%lld", -5LL);
    fmt("-1", "%lld", -1LL);
    fmt("0", "%lld", 0LL);
    fmt("-5", "%lli", -5LL);
    fmt("-5", "%jd", (intmax_t)-5);
    fmt("-4294967296", "%lld", -4294967296LL);
    fmt("-4294967295", "%lld", -4294967295LL);
    fmt("9223372036854775807", "%lld", INT64_MAX);
    fmt("-9223372036854775808", "%lld", INT64_MIN);
    fmt("-9223372036854775807", "%lld", INT64_MIN + 1);
    fmt("    -5", "%6lld", -5LL);
    fmt("-5    |", "%-6lld|", -5LL);
    fmt("+5", "%+lld", 5LL);
    fmt("-5|7", "%lld|%d", -5LL, 7);
    /* unsigned conversions are not signed: unchanged */
    fmt("18446744073709551615", "%llu", -1LL);
    fmt("ffffffffffffffff", "%llx", -1LL);
    fmt("1777777777777777777777", "%llo", -1LL);

    printf("\n=== tstpll: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
