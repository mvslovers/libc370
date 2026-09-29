/*
 * tstdblcv.c - libc370 #209: printf of a double with a precision above
 * ~14 must not print a rounding error of its own, on MVS.
 *
 * __dblcvt() capped its rounding digit at DBL_MANT_DIG, which is 14 on
 * S/370 and counts hex digits, so every precision >= 14 added a fixed
 * 5e-15: "%.30g" of 1.0 was 1.000000000000004884981308350688 (brexx370,
 * JOB00606).  test/host/tstdblcv.c pins the logic with IEEE arithmetic;
 * this is the gate on HFP, reached the way a program reaches it -
 * snprintf with a precision, through __examin().
 *
 *   (1) the issue's table, %.30g of values HFP holds exactly
 *   (2) %.20f and %.20e of the same values - the same rounding step
 *   (3) 0.0, where the addend used to print as a stray 5
 *   (4) 1/3: only the first 16 digits (the tail is the HFP value's
 *       expansion, digit noise from *10 - not #209)
 *   (5) low precision, which was correct and must stay so
 *
 * Build:   make build
 *          cc370 -O1 -Iinclude -L build/sdk test/mvs/tstdblcv.c \
 *                -o TSTDBLCV -flinker-output=iebcopy
 *          ld370 --pack TSTDBLCV=TSTDBLCV.iebcopy -o tstdblcv -xmit \
 *                --dsn IBMUSER.LIBC370.DBLSCR
 * Install: jcl/recvdbl.jcl (its own staging data set, not the mbt one).
 * Run:     jcl/tstdblcv.jcl.
 *
 * GREEN: mvsdev JOB00714, CC 0000, 29/29, 2026-09-29.
 * RED the same day, the same source linked against the installed sysroot
 * libc (main, before the fix): JOB00712, CC 0001, 22 of 29 failed - 1.0
 * under %.30g came out 1.000000000000004884981308350688, the issue's
 * tail to the digit; the 7 low-precision checks in (5) passed.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

static void
check(const char *fmt, int prec, double v, const char *want, int prefix)
{
    char got[128];
    int  ok;

    snprintf(got, sizeof(got), fmt, prec, v);
    ok = prefix ? strncmp(got, want, strlen(want)) == 0
                : strcmp(got, want) == 0;
    mbt_run++;
    if (ok) {
        mbt_passed++;
    }
    else {
        mbt_failed++;
        printf("  FAIL: %s prec %d: got \"%s\", want %s\"%s\"\n",
               fmt, prec, got, prefix ? "prefix " : "", want);
    }
}

int
main(void)
{
    double one = 1.0, three = 3.0;

    /* (1) */
    check("%.*g", 30, 1.0,    "1",    0);
    check("%.*g", 30, 3.0,    "3",    0);
    check("%.*g", 30, 5000.0, "5000", 0);
    check("%.*g", 30, 0.25,   "0.25", 0);
    check("%.*g", 30, 0.125,  "0.125", 0);
    check("%.*g", 30, -2.5,   "-2.5", 0);
    check("%.*g", 16, 1.0,    "1",    0);
    check("%.*g", 17, 1.0,    "1",    0);
    check("%.*g", 40, 1.0,    "1",    0);

    /* (2) */
    check("%.*f", 20, 1.0,    "1.00000000000000000000", 0);
    check("%.*f", 20, 0.25,   "0.25000000000000000000", 0);
    check("%.*f", 20, 5000.0, "5000.00000000000000000000", 0);
    check("%.*f", 20, -2.5,   "-2.50000000000000000000", 0);
    check("%.*e", 20, 1.0,    "1.00000000000000000000E+00", 0);
    check("%.*e", 20, 0.25,   "2.50000000000000000000E-01", 0);
    check("%.*e", 20, 5000.0, "5.00000000000000000000E+03", 0);

    /* (3) */
    check("%.*g", 20, 0.0, "0", 0);
    check("%.*g", 30, 0.0, "0", 0);
    check("%.*f", 20, 0.0, "0.00000000000000000000", 0);
    check("%.*e", 20, 0.0, "0.00000000000000000000E+00", 0);

    /* (4) computed at run time, so HFP division makes the value */
    check("%.*g", 30, one / three, "0.3333333333333333", 1);
    check("%.*e", 30, one / three, "3.333333333333333", 1);

    /* (5) */
    check("%.*g", 9, one / three, "0.333333333", 0);
    check("%.*g", 9, 0.1 + 0.2,   "0.3", 0);
    check("%.*f", 2, 2.0 / three, "0.67", 0);
    check("%.*f", 2, 123.456,     "123.46", 0);
    check("%.*f", 2, 0.006,       "0.01", 0);
    check("%.*e", 3, 2.0 / three, "6.667E-01", 0);
    check("%.*f", 6, 0.0,         "0.000000", 0);

    printf("TSTDBLCV: %d checks, %d passed, %d failed\n",
           mbt_run, mbt_passed, mbt_failed);
    return mbt_failed ? 1 : 0;
}
