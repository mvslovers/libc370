/*
 * tstdblrb.c - libc370 #222: __dblcvt() must not write past the end of
 * the buffer it formats into.
 *
 * ISSUE #222: __dblcvt() built its result with strcat into a caller
 * buffer it had no length for - numbuf[50] on the plain %f path of
 * vsnprintf()/vvprintf(), work[80] in __examin() - and padded through
 * its own work[125].  Plain printf("%f", 1e41) was enough: sign position
 * + 42 digits + '.' + 6 + NUL = 51 > 50.  On MVS every one of those
 * buffers is an automatic, so the caller's stack frame was overwritten.
 *
 * __dblcvt() now takes the size of its result buffer.  What does not fit
 * is cut off, in this order of precedence: the NUL always, the exponent
 * of an e-style result before any fraction digit, digits before padding.
 *
 * Every buffer here is a heap allocation of exactly the size handed to
 * __dblcvt(), so one byte past it is a diagnosable overflow: RUN THIS
 * UNDER ASAN.  Digits are only compared as a prefix: the /10 scaling
 * loop makes everything past ~16 digits noise, on IEEE and HFP alike.
 *
 * BUILD / RUN (host, from the repo root):
 *
 *     cc -std=gnu99 -Wall -Wextra -Werror -O1 -fsanitize=address \
 *        -o /tmp/tstdblrb test/host/tstdblrb.c && /tmp/tstdblrb
 *
 * RED against the pre-fix source (add -DDBLCVT_NO_RSIZE, which drops the
 * size argument the old __dblcvt() does not have): ASAN
 * heap-buffer-overflow, WRITE in strcat, on case (1), %f of 1e60 into 50;
 * with -fsanitize-recover=address and ASAN_OPTIONS=halt_on_error=0, 18
 * reports and 12 failed checks.  test/mvs/tstdblrb.c is the gate on HFP.
 *
 * GREEN 2026-09-29: 79 checks, 0 failures, no ASAN report.
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

/* the target's value; @@dblcvt.c's own #include <float.h> is then a no-op */
#undef  DBL_MANT_DIG
#define DBL_MANT_DIG 14

#include "../../src/clib/@@dblcvt.c"

#ifdef DBLCVT_NO_RSIZE
#define CVT(v, t, w, p, buf, size) __dblcvt(v, t, w, p, buf)
#else
#define CVT(v, t, w, p, buf, size) __dblcvt(v, t, w, p, buf, size)
#endif

/* ---- harness ---------------------------------------------------------- */
static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

static void check(int ok, const char *what, const char *got)
{
    mbt_run++;
    if (ok) {
        mbt_passed++;
    }
    else {
        mbt_failed++;
        printf("  FAIL: %s: got \"%s\"\n", what, got);
    }
}

/*
 * Convert v into a heap buffer of exactly size bytes and check the
 * result:
 *   len     - its exact length, or -1 for "anything that fits"
 *   prefix  - what it must start with (NULL: no check)
 *   suffix  - what it must end with (NULL: no check)
 *   dot     - index of the '.', or -1 for no check
 */
static void cvt(const char *what, double v, char type, size_t width, int prec,
                size_t size, int len, const char *prefix, const char *suffix,
                int dot)
{
    char   *buf = malloc(size);
    size_t  n;
    char    *p;

    memset(buf, 'x', size);
    CVT(v, type, width, prec, buf, size);

    n = strnlen(buf, size);
    check(n < size, what, "(no NUL within the buffer)");
    if (n >= size) {
        free(buf);
        return;
    }
    if (len >= 0) {
        check(n == (size_t)len, what, buf);
    }
    if (prefix != NULL) {
        check(strncmp(buf, prefix, strlen(prefix)) == 0, what, buf);
    }
    if (suffix != NULL) {
        check(n >= strlen(suffix) &&
              strcmp(buf + n - strlen(suffix), suffix) == 0, what, buf);
    }
    if (dot >= 0) {
        p = strchr(buf, '.');
        check(p != NULL && p - buf == dot, what, buf);
    }
    free(buf);
}

int main(void)
{
    /* (1) the issue's table, at the caller buffer sizes before the fix -
           these are the rows that overflowed; now they are cut short */
    cvt("%f 1e60 into 50", 1e60, 'f', 0, 6, 50,
        49, "100000000000000", NULL, -1);
    cvt("%f 1e41 into 50", 1e41, 'f', 0, 6, 50,
        49, "100000000000000", NULL, 42);
    cvt("%f 1e40 into 50 (fitted before)", 1e40, 'f', 0, 6, 50,
        48, "100000000000000", NULL, 41);
    cvt("%.30f 1e50 into 80", 1e50, 'f', 0, 30, 80,
        79, "100000000000000", NULL, 51);
    cvt("%.100f 1.0 into 80", 1.0, 'f', 0, 100, 80,
        79, "1.000000000000000", NULL, 1);
    cvt("%f 9e119 into 200 (__dblcvt's own work[125])", 9e119, 'f', 0, 6, 200,
        127, "9", NULL, 120);

    /* (2) width: padding used to go through work[125] and strcat into the
           caller's buffer; now it yields to the digits */
    cvt("%90f 1.0 into 80", 1.0, 'f', 90, 6, 80,
        79, "      ", " 1.000000", -1);
    cvt("%130f 1.0 into 200 (padding round trip, work[125])",
        1.0, 'f', 130, 6, 200, 130, "      ", " 1.000000", -1);
    cvt("%10f 1.0 into 80 (fitted before)", 1.0, 'f', 10, 6, 80,
        10, "  1.000000", NULL, -1);

    /* (3) the 0.xxx leading-zero loop is unbounded too (its count past the
           precision is #220; here only the bound is checked) */
    cvt("%f 1e-60 into 50", 1e-60, 'f', 0, 6, 50,
        49, "0.000000000", NULL, 1);
    cvt("%f -1e-60 into 50", -1e-60, 'f', 0, 6, 50,
        49, "-0.000000000", NULL, 2);

    /* (4) e-style keeps its exponent when the fraction is cut */
    cvt("%.100e 1.0 into 80", 1.0, 'e', 0, 100, 80,
        79, "1.0000000", "E+00", 1);
    cvt("%.60e -1.5 into 50", -1.5, 'e', 0, 60, 50,
        49, "-1.5000000", "E+00", 2);
    cvt("%.100e 1e-30 into 80", 1e-30, 'e', 0, 100, 80,
        79, "1.0000000", "E-30", 1);
    cvt("%.6e 1e-30 into 80 (fitted before)", 1e-30, 'e', 0, 6, 80,
        12, "1.000000E-30", NULL, 1);

    /* (5) %g, both styles */
    cvt("%.100g 1e40 into 80", 1e40, 'g', 0, 100, 80,
        -1, "100000000000000", NULL, -1);
    cvt("%.6g 1e40 into 80 (e-style, fitted before)", 1e40, 'g', 0, 6, 80,
        12, "1.000000E+40", NULL, 1);

    /* (6) the sizes the callers use after the fix: the plain path's
           numbuf[96] takes every value HFP can hold (about 7.2e75) */
    cvt("%f 1e60 into 96", 1e60, 'f', 0, 6, 96,
        68, "100000000000000", NULL, 61);
    cvt("%f -7.2e75 into 96", -7.2e75, 'f', 0, 6, 96,
        84, "-72000000000000", NULL, 77);

    /* (7) degenerate sizes: nothing but the NUL, or not even that */
    cvt("%f 1.0 into 1", 1.0, 'f', 0, 6, 1, 0, NULL, NULL, -1);
    cvt("%e 1.0 into 3", 1.0, 'e', 0, 6, 3, -1, NULL, NULL, -1);
    {
        char c = 'x';

        CVT(1.0, 'f', 0, 6, &c, 0);
        check(c == 'x', "%f 1.0 into 0 writes nothing", "(byte changed)");
    }

    printf("TSTDBLRB: %d checks, %d passed, %d failed\n",
           mbt_run, mbt_passed, mbt_failed);
    return mbt_failed ? 1 : 0;
}
