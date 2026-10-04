/*
 * tstpfarg.c - libc370 #383 on MVS: every conversion printf hands to its
 * engine takes its argument off the list.
 *
 * ISSUE #383: __examin() printed nothing for %c with a width or a flag,
 * %n with a length modifier, %a and %A, and took no argument for them, so
 * every later conversion read the argument meant for the one before it.
 * test/host/tstpfarg.c runs the same cases (test/mvs/tstpfargcase.h)
 * through vsnprintf(); this run adds vsprintf(), the vvprintf() path.
 *
 * Built twice from this source: TSTPFA against this tree's libc.a,
 * TSTPFAR against the installed sysroot libc.a (before #383) - the red
 * control.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstpfarg.c -o TSTPFA -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstpfarg.c -o TSTPFAR -flinker-output=iebcopy
 *          ld370 --pack TSTPFA=TSTPFA.iebcopy TSTPFAR=TSTPFAR.iebcopy \
 *                -o tstpfarg -xmit --dsn IBMUSER.LIBC370.PFASCR
 * Install: jcl/recvpfa.jcl.   Run: jcl/tstpfarg.jcl.
 *
 * mvsdev JOB01364, 2026-10-05 (RECEIVE JOB01363): GREEN CC 0000, 34/34;
 * RED (the installed 2.2.0 libc.a) CC 0001, 0 of 34 passed.
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

#define PFARG_SPRINTF
#include "tstpfargcase.h"

int main(void)
{
    printf("=== tstpfarg: printf takes every argument on MVS (#383) ===\n\n");
    pfarg_cases();
    printf("\n=== tstpfarg: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
