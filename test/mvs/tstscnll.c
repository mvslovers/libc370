/*
 * tstscnll.c - libc370 #318 on MVS: scanf's length modifiers hh, ll, j,
 * z, t and L, through the real sscanf().
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
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>

static int run = 0, failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        run++;                                                            \
        if (cond) printf("  PASS: %s\n", (msg));                          \
        else { failed++; printf("  FAIL: %s\n", (msg)); }                 \
    } while (0)

/* the cases call scan(s, f, ...): here that is sscanf() itself (libc370
   has no vsscanf() yet, #325) */
#define scan sscanf

#include "tstscncase.h"

int main(void)
{
    printf("=== tstscnll: scanf length modifiers on MVS (#318) ===\n\n");
    scn_cases();
    printf("\n=== tstscnll: %d/%d passed", run - failed, run);
    if (failed > 0) printf(" (%d FAILED)", failed);
    printf(" ===\n");
    return failed > 0 ? 1 : 0;
}
