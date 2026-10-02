/*
 * tstl.c - libc370 #316 on MVS: strtol, strtoul, atoi and atol after
 * their rewrite on the shape of strtoll/strtoull.
 *
 * test/host/tstl.c checks the same cases (test/mvs/tstlcase.h) with the
 * host compiler.  What only MVS answers: the EBCDIC letters ("JR", "sz"
 * in base 36 - the old c - 'A' + 10 read 'J' as 26), and the 32-bit
 * limits in the type's own width.
 *
 * Built twice from this source: TSTL against this tree's libc.a, TSTLR
 * against the installed sysroot libc.a (before #316) - the red control.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstl.c -o TSTL -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstl.c -o TSTLR -flinker-output=iebcopy
 *          ld370 --pack TSTL=TSTL.iebcopy TSTLR=TSTLR.iebcopy \
 *                -o tstl -xmit --dsn IBMUSER.LIBC370.LSCR
 * Install: jcl/recvl.jcl.   Run: jcl/tstl.jcl.
 *
 * mvsdev JOB01173, 2026-10-03 (RECEIVE JOB01172): GREEN CC 0000, 35/35;
 * RED CC 0001, 17 of 35 failed, the EBCDIC "JR" and "sz" among them.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

#include "tstlcase.h"

int main(void)
{
    printf("=== tstl: strtol, strtoul, atoi, atol on MVS (#316) ===\n\n");
    l_cases();
    printf("\n=== tstl: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
