/*
 * tstpfflt.c - libc370 #355 on MVS: printf's flags around %f, through the
 * real HFP __dblcvt().
 *
 * ISSUE #355: the 0 and - flags were ignored for %f and a + or space sign
 * landed outside the width ("%05.1f" of 2.5 gave "  2.5", mvsdev
 * JOB01310).  test/host/tstpfflt.c checks the same cases
 * (test/mvs/tstpffltcase.h) with a stand-in __dblcvt(); this run uses the
 * library's own.
 *
 * Built twice from this source: TSTPFF against this tree's libc.a,
 * TSTPFFR against the installed sysroot libc.a (before #355) - the red
 * control.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstpfflt.c -o TSTPFF -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstpfflt.c -o TSTPFFR -flinker-output=iebcopy
 *          ld370 --pack TSTPFF=TSTPFF.iebcopy TSTPFFR=TSTPFFR.iebcopy \
 *                -o tstpfflt -xmit --dsn IBMUSER.LIBC370.PFFSCR
 * Install: jcl/recvpff.jcl.   Run: jcl/tstpfflt.jcl.
 *
 * mvsdev JOB01315, 2026-10-03 (RECEIVE JOB01314): GREEN CC 0000, 19/19;
 * RED (the installed 2.1.0 libc.a) CC 0001, 13 of 19 failed.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#include "tstpffltcase.h"

int main(void)
{
    printf("=== tstpfflt: printf flags around %%f on MVS (#355) ===\n\n");
    pfflt_cases();
    printf("\n=== tstpfflt: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
