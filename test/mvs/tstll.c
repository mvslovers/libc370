/*
 * tstll.c - libc370 #314 on MVS: the long long family of <stdlib.h> --
 * strtoll, strtoull, atoll, llabs, lldiv -- and the limits LLONG_MIN,
 * LLONG_MAX and ULLONG_MAX.
 *
 * test/host/tstll.c checks the C logic of the five TUs with the host
 * compiler.  This run takes the same cases (test/mvs/tstllcase.h) to the
 * S/370 code linked from libc.a, which adds what only MVS can answer:
 *
 *   - the EBCDIC digit table: base-36 letters past 'I' ("JR", "sz") are
 *     where c - 'A' + 10 goes wrong, since 'J' is X'D1' and 'A' X'C1'
 *   - the 64-bit multiply, divide, remainder and negate, which cc370 calls
 *     as @@MULDI3, @@UDIVDI, @@UMODDI, @@DIVDI3, @@MODDI3 and @@NEGDI2
 *   - that the five names link at all: before #314 this program stopped at
 *     ld370 with STRTOLL STRTOULL ATOLL LLDIV unresolved
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstll.c -o TSTLL -flinker-output=iebcopy
 *          ld370 --pack TSTLL=TSTLL.iebcopy -o tstll -xmit \
 *                --dsn IBMUSER.LIBC370.LLSCR
 * Install: jcl/recvll.jcl (its own staging data set, not the mbt one).
 * Run:     jcl/tstll.jcl.
 *
 * GREEN: mvsdev JOB01155, CC 0000, 64/64, 2026-10-02 (RECEIVE JOB01154),
 * linked against build/sdk from the PR branch with cc370 1.0.0.  There
 * is no red step: before #314 this program does not link.
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

#include "tstllcase.h"

int main(void)
{
    printf("=== tstll: the long long family on MVS (#314) ===\n\n");
    ll_cases();
    printf("\n=== tstll: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
