/*
 * tstdblcv.c - libc370 #209: __dblcvt() must not add a rounding error of
 * its own when the precision exceeds what a double carries.
 *
 * ISSUE #209: __dblcvt() rounds by adding 0.5 / 10^j to the scaled value
 * b in [1,10), and capped j at DBL_MANT_DIG - "the precision a double
 * has".  On the target that macro is 14 and counts HEX digits
 * (include/float.h:32), so every precision >= 14 added a fixed 5e-15:
 * snprintf("%.30g", 1.0) printed 1.000000000000004884981308350688 on
 * mvsdev (brexx370, JOB00606).  The addend only has to be bounded to stay
 * representable; past ~17 decimal digits it is below half an ulp of b and
 * changes no digit, so that is where the cap now sits.
 *
 * The same addend also landed on 0.0, where no ulp absorbs it:
 * "%.20g" of 0.0 printed 0.000000000000005.  Zero is now not rounded.
 *
 * This test forces the TARGET's DBL_MANT_DIG (14) before including the
 * TU, so the pre-fix source reproduces the defect with host IEEE
 * arithmetic.  Only values a double holds exactly are compared byte for
 * byte; for 1/3 only the first 16 digits are - the digits after that come
 * from repeated *10 and are noise on either float format (not #209).
 * test/mvs/tstdblcv.c is the gate on HFP.
 *
 * Expected strings are __dblcvt's own format: 'E' in upper case, no
 * leading blank for a positive number, %g stripping trailing zeros only
 * in f-style.
 *
 * BUILD / RUN (host, from the repo root):
 *
 *     cc -std=gnu99 -Wall -Wextra -Werror -O1 -o /tmp/tstdblcv \
 *        test/host/tstdblcv.c && /tmp/tstdblcv
 *
 * RED against the pre-fix source: 1.0 under %.30g comes out
 * 1.00000000000000510702591327572 (the IEEE twin of the MVS tail).
 *
 * GREEN 2026-09-29: 108 checks, 0 failures.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <string.h>
#include <float.h>

/* the target's value; @@dblcvt.c's own #include <float.h> is then a no-op */
#undef  DBL_MANT_DIG
#define DBL_MANT_DIG 14

#include "../../src/clib/@@dblcvt.c"

/* ---- harness ---------------------------------------------------------- */
static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

static void check_cvt(double v, char type, int prec, const char *want)
{
    char got[256];

    __dblcvt(v, type, 0, prec, got);
    mbt_run++;
    if (strcmp(got, want) == 0) {
        mbt_passed++;
    }
    else {
        mbt_failed++;
        printf("FAIL %%.%d%c of %.17g: got \"%s\", want \"%s\"\n",
               prec, type, v, got, want);
    }
}

static void check_prefix(double v, char type, int prec, const char *want)
{
    char got[256];

    __dblcvt(v, type, 0, prec, got);
    mbt_run++;
    if (strncmp(got, want, strlen(want)) == 0) {
        mbt_passed++;
    }
    else {
        mbt_failed++;
        printf("FAIL %%.%d%c of %.17g: got \"%s\", want prefix \"%s\"\n",
               prec, type, v, got, want);
    }
}

int main(void)
{
    /* (1) the issue's table: %.30g of exact values */
    check_cvt(1.0,    'g', 30, "1");
    check_cvt(3.0,    'g', 30, "3");
    check_cvt(5000.0, 'g', 30, "5000");
    check_cvt(0.25,   'g', 30, "0.25");

    /* (2) every precision from 14 up, where the cap used to bite */
    {
        int p;
        for (p = 14; p <= 40; p++) {
            check_cvt(1.0,   'g', p, "1");
            check_cvt(0.125, 'g', p, "0.125");
            check_cvt(-2.5,  'g', p, "-2.5");
        }
    }

    /* (3) %f and %e take the same rounding step */
    check_cvt(1.0,    'f', 20, "1.00000000000000000000");
    check_cvt(0.25,   'f', 20, "0.25000000000000000000");
    check_cvt(5000.0, 'f', 20, "5000.00000000000000000000");
    check_cvt(-2.5,   'f', 20, "-2.50000000000000000000");
    check_cvt(1.0,    'e', 20, "1.00000000000000000000E+00");
    check_cvt(0.25,   'e', 20, "2.50000000000000000000E-01");
    check_cvt(5000.0, 'e', 20, "5.00000000000000000000E+03");

    /* (4) zero: nothing to absorb the addend */
    check_cvt(0.0, 'g', 20, "0");
    check_cvt(0.0, 'g', 30, "0");
    check_cvt(0.0, 'f', 20, "0.00000000000000000000");
    check_cvt(0.0, 'e', 20, "0.00000000000000000000E+00");

    /* (5) inexact value: the digits a double carries */
    check_prefix(1.0 / 3, 'g', 30, "0.3333333333333333");
    check_prefix(1.0 / 3, 'e', 30, "3.333333333333333");

    /* (6) low precision still rounds - the path that was correct */
    check_cvt(1.0 / 3,   'g', 9, "0.333333333");
    check_cvt(0.1 + 0.2, 'g', 9, "0.3");
    check_cvt(2.0 / 3,   'f', 2, "0.67");
    check_cvt(9.995,     'f', 1, "10.0");
    check_cvt(123.456,   'f', 2, "123.46");
    check_cvt(2.0 / 3,   'e', 3, "6.667E-01");
    check_cvt(0.0,       'f', 6, "0.000000");
    check_cvt(0.006,     'f', 2, "0.01");   /* rounds into the digit before
                                               b's first (j = -1) */
    check_cvt(0.000123456, 'e', 2, "1.23E-04");
    check_cvt(0.00096,   'f', 3, "0.001");

    printf("tstdblcv: %d checks, %d passed, %d failed\n",
           mbt_run, mbt_passed, mbt_failed);
    return mbt_failed ? 1 : 0;
}
