/*
 * tststrd.c - libc370 #316 on MVS: strtod() over the whole HFP range.
 *
 * ISSUE #316: strtod() built its power of ten by repeated *10 and applied
 * it in one step, so any exponent past about 75 - either way - overflowed
 * an intermediate.  An HFP exponent overflow is a program check no mask
 * suppresses: strtod("1e76"), ("8e75") and even ("1e-80") ended S0CC
 * (mvsdev JOB01177, 2026-10-03, against the 2.0.0 sysroot libc.a; "1e75"
 * passed).  Now the range is checked before any intermediate is formed:
 * overflow returns +-HUGE_VAL, underflow 0, both with ERANGE.
 *
 * Also pinned: C99's subject sequence - "1e" and "1e-" leave endptr at
 * the 'e', "+" after the 'e' is accepted, nothing converted leaves endptr
 * at nptr - and more significant digits than HFP holds.
 *
 * No host twin: the range is HFP's, and the host's float.h and HUGE_VAL
 * are not.  Values that are not exact in HFP are compared to within
 * 1e-14 relative.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tststrd.c -o TSTSTRD -flinker-output=iebcopy
 *          ld370 --pack TSTSTRD=TSTSTRD.iebcopy -o tststrd -xmit \
 *                --dsn IBMUSER.LIBC370.STRDSCR
 * Install: jcl/recvstrd.jcl.   Run: jcl/tststrd.jcl.
 *
 * GREEN: mvsdev JOB01179, CC 0000, 32/32, 2026-10-03 (RECEIVE JOB01178).
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <float.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

/* strtod(s) is want (exactly when rel is 0, else to within rel), endptr
   at s + off, errno err */
static void cd(const char *s, double want, double rel, long off, int err,
               const char *msg)
{
    char *end = NULL;
    double r;
    int e, ok;

    errno = 0;
    r = strtod(s, &end);
    e = errno;
    if (rel == 0) ok = (r == want);
    else ok = (fabs(r - want) <= fabs(want) * rel);
    CHECK(ok && end == s + off && e == err, msg);
    if (!ok)
        printf("        got %.17g, want %.17g\n", r, want);
    if (end != s + off)
        printf("        endptr at %ld, want %ld\n", (long)(end - s), off);
    if (e != err)
        printf("        errno %d, want %d\n", e, err);
}

int main(void)
{
    static char zeros[100], ones[100];

    printf("=== tststrd: strtod over the HFP range (#316) ===\n\n");

    printf("(1) values:\n");
    cd("1.5", 1.5, 0, 3, 0, "(1) 1.5");
    cd("-2.25", -2.25, 0, 5, 0, "(1) -2.25");
    cd("100", 100.0, 0, 3, 0, "(1) 100");
    cd("1e10", 1e10, 0, 4, 0, "(1) 1e10");
    cd(".5", 0.5, 0, 2, 0, "(1) .5");
    cd("5.", 5.0, 0, 2, 0, "(1) 5.");
    cd("0.1", 0.1, 1e-14, 3, 0, "(1) 0.1");
    cd("123.456", 123.456, 1e-14, 7, 0, "(1) 123.456");
    cd("  42x", 42.0, 0, 4, 0, "(1) blanks, trailing text");
    cd("0e500", 0.0, 0, 5, 0, "(1) 0e500 is 0, no error");
    cd("123456789012345678901234567890", 1.2345678901234568e29, 1e-14,
       30, 0, "(1) 30 digits, more than HFP holds");

    printf("\n(2) the subject sequence:\n");
    cd("1e+3", 1000.0, 0, 4, 0, "(2) 1e+3: the + is accepted");
    cd("1e", 1.0, 0, 1, 0, "(2) 1e: endptr at the e");
    cd("1e-", 1.0, 0, 1, 0, "(2) 1e-: endptr at the e");
    cd("", 0.0, 0, 0, 0, "(2) empty: endptr is nptr");
    cd("-", 0.0, 0, 0, 0, "(2) sign only: endptr is nptr");
    cd(".", 0.0, 0, 0, 0, "(2) point only: endptr is nptr");
    cd("abc", 0.0, 0, 0, 0, "(2) no digits: endptr is nptr");

    printf("\n(3) the edges of the range:\n");
    cd("1e75", 1e75, 1e-14, 4, 0, "(3) 1e75");
    cd("7.2e75", 7.2e75, 1e-14, 6, 0, "(3) 7.2e75, below DBL_MAX");
    cd("1e-78", 1e-78, 1e-14, 5, 0, "(3) 1e-78, above DBL_MIN");
    cd("1e-75", 1e-75, 1e-14, 5, 0, "(3) 1e-75");

    printf("\n(4) out of range: ERANGE, no S0CC:\n");
    cd("1e76", HUGE_VAL, 0, 4, ERANGE, "(4) 1e76");
    cd("8e75", HUGE_VAL, 0, 4, ERANGE, "(4) 8e75");
    cd("-1e76", -HUGE_VAL, 0, 5, ERANGE, "(4) -1e76");
    cd("1e99999", HUGE_VAL, 0, 7, ERANGE, "(4) 1e99999");
    cd("1e-80", 0.0, 0, 5, ERANGE, "(4) 1e-80");
    cd("1e-79", 0.0, 0, 5, ERANGE, "(4) 1e-79, below DBL_MIN");
    cd("-1e-99999", 0.0, 0, 9, ERANGE, "(4) -1e-99999");
    memset(ones, '0', 81);
    ones[0] = '1';
    ones[81] = '\0';
    cd(ones, HUGE_VAL, 0, 81, ERANGE, "(4) 1 and 80 zeros");
    strcpy(zeros, "0.");
    memset(zeros + 2, '0', 80);
    zeros[82] = '1';
    zeros[83] = '\0';
    cd(zeros, 0.0, 0, 83, ERANGE, "(4) 0. 80 zeros 1");
    errno = 0;
    CHECK(atof("1e76") == HUGE_VAL, "(4) atof(\"1e76\"), no S0CC");

    printf("\n=== tststrd: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
