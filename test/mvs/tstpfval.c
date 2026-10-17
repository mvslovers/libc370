/*
 * tstpfval.c - libc370 #220/#384 on MVS: printf values that were wrong,
 * through the real HFP __dblcvt().  The cases are tstpfvalcase.h, shared
 * with test/host/tstpfval.c.
 *
 * Built twice: TSTPFV against this tree's libc.a, TSTPFVR against the
 * installed one (2.16.0) - the red control.
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

#include "tstpfvalcase.h"

int main(void)
{
    printf("=== tstpfval: printf values on MVS (#220, #384) ===\n\n");
    pfval_cases();
    printf("\n=== tstpfval: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
