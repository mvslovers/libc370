/*
 * tstscnll.c - libc370 #318 on MVS: scanf's length modifiers hh, ll, j,
 * z, t and L, through the real vsscanf() (#338).
 *
 * test/host/tstscnll.c runs the same cases (test/mvs/tstscncase.h) on the
 * host, where long is 64 bits and the %lld defect cannot show.  Here it
 * can: before #318, %lld stored a long - the HIGH word of the long long
 * on S/370 - and left the low word as it was.
 *
 * Built twice from this source: TSTSCN against this tree's libc.a,
 * TSTSCNR against the installed sysroot libc.a (before #318) - the red
 * control.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstscnll.c -o TSTSCN -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstscnll.c -o TSTSCNR -flinker-output=iebcopy
 *          ld370 --pack TSTSCN=TSTSCN.iebcopy TSTSCNR=TSTSCNR.iebcopy \
 *                -o tstscnll -xmit --dsn IBMUSER.LIBC370.SCNSCR
 * Install: jcl/recvscn.jcl.   Run: jcl/tstscnll.jcl.
 *
 * mvsdev JOB01171, 2026-10-03 (RECEIVE JOB01170): GREEN CC 0000, 23/23;
 * RED CC 0001, 18 of 23 failed - every %lld/%llx/%llu case among them,
 * the half the host cannot show.
 *
 * With #316's digit and float cases: mvsdev JOB01181 (RECEIVE JOB01180),
 * GREEN CC 0000, 43/43; RED ABEND S0CC, its output lost with the abend.
 * The shared cases ran without one under the same library (JOB01171) and
 * the new shared float cases stay below 1e30, so the S0CC is in
 * hfp_cases() - scanf's own float parser past about 1e75.
 *
 * Through vsscanf() (#338): mvsdev JOB01311, GREEN CC 0000, 43/43.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>

static int run = 0, failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        run++;                                                            \
        if (cond) printf("  PASS: %s\n", (msg));                          \
        else { failed++; printf("  FAIL: %s\n", (msg)); }                 \
    } while (0)

/* the cases call scan(s, f, ...): through vsscanf(), the C99 entry point
   since #338 (it was sscanf() itself before) */
static int scan(const char *s, const char *f, ...)
{
    va_list ap;
    int r;

    va_start(ap, f);
    r = vsscanf(s, f, ap);
    va_end(ap);
    return (r);
}

#include "tstscncase.h"

/* HFP only: the range the host's IEEE double does not share (#316).
   Before, scanf's own float parser ended S0CC past about 1e75. */
static void hfp_cases(void)
{
    double d;
    float f;
    unsigned long u;
    static char ones[100];

    printf("\n(HFP) range, no S0CC:\n");
    d = 0;
    CHECK(sscanf("1e76", "%lf", &d) == 1 && d == HUGE_VAL, "%lf 1e76");
    d = 1;
    CHECK(sscanf("1e-80", "%lf", &d) == 1 && d == 0.0, "%lf 1e-80");
    d = 0;
    CHECK(sscanf("-1e999", "%lf", &d) == 1 && d == -HUGE_VAL,
          "%lf -1e999");
    memset(ones, '0', 81);
    ones[0] = '1';
    ones[81] = '\0';
    d = 0;
    CHECK(sscanf(ones, "%lf", &d) == 1 && d == HUGE_VAL,
          "%lf 1 and 80 zeros");
    f = 0;
    CHECK(sscanf("7.2370055e75", "%f", &f) == 1 && f == FLT_MAX,
          "%f above FLT_MAX is FLT_MAX");
    memcpy(&u, &f, sizeof(u));
    CHECK(u == 0x7FFFFFFFUL, "    ... X'7FFFFFFF'");
    d = 0;
    CHECK(sscanf("7.2e75", "%lf", &d) == 1 && d > 7.19e75 && d < 7.21e75,
          "%lf 7.2e75, in range");
}

int main(void)
{
    printf("=== tstscnll: scanf length modifiers on MVS (#318) ===\n\n");
    scn_cases();
    hfp_cases();
    printf("\n=== tstscnll: %d/%d passed", run - failed, run);
    if (failed > 0) printf(" (%d FAILED)", failed);
    printf(" ===\n");
    return failed > 0 ? 1 : 0;
}
