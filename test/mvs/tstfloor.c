/*
 * tstfloor.c - libc370 #273 on MVS: floor(), ceil(), modf() and fmod()
 * beyond 2**31, in hexadecimal floating point.
 *
 * ISSUE #273: all four took the integral part through a 32-bit int, so
 * |x| >= 2**31 came back as garbage (10540800000 % 1 = 1950865408 through
 * brexx370, mvsdev JOB01005/JOB01006).  test/host/tstfloor.c checks the
 * same cases (test/mvs/tstfloorcase.h) in IEEE; this run is the one that
 * says whether the new integral part (src/internal/ipart.h) is exact on HFP.
 *
 * Built twice from this source: TSTFLR against this tree's libc.a, TSTFLRR
 * against the installed sysroot libc.a (before #273) - the red control.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstfloor.c -o TSTFLR -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstfloor.c -o TSTFLRR -flinker-output=iebcopy
 *          ld370 --pack TSTFLR=TSTFLR.iebcopy TSTFLRR=TSTFLRR.iebcopy \
 *                -o tstfloor -xmit --dsn IBMUSER.LIBC370.FLRSCR
 * Install: jcl/recvflr.jcl.   Run: jcl/tstfloor.jcl.
 *
 * mvsdev JOB01337, 2026-10-04 (RECEIVE JOB01336): GREEN CC 0000, 40/40;
 * RED (the installed sysroot libc.a) CC 0001, 25 of 40 failed.  The first
 * run, JOB01335, had literal arguments and passed the old floor()/ceil()
 * on all of them: cc370 had folded the calls - see V() in the cases.
 *
 * A failing check prints both doubles in hex: printf's %f is not exact for
 * large values on HFP (#225), the bytes are.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <math.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#include "tstfloorcase.h"

int main(void)
{
    printf("=== tstfloor: floor/ceil/modf/fmod beyond 2**31 on MVS (#273) ===\n\n");
    floor_cases();
    printf("\n=== tstfloor: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
