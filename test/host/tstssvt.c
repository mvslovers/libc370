/*
 * tstssvt.c - libc370 #240: ssvt_set() and ssvt_funcmap() return 0 on
 * success.
 *
 * ISSUE #240: both fell off the end of the success path.  Measured in the
 * cc370 -O1 assembler: R15 then held 0x100 | (caller's PSW key << 4) - 384
 * for a key-8 caller - so `if (ssvt_set(...))` read every success as a
 * failure, and success could not be told from EPERM.  Found by -Wall
 * ("control reaches end of non-void function").
 *
 * Runs the REAL @@svset.c and @@svfmap.c on the host with their IPK/SPKA
 * assembler erased (-D'__asm__(...)='): the key switch is not what is under
 * test, the return value and the slot written are.  On the old source the
 * success return is whatever the host leaves in its return register -
 * undefined, but nonzero on macOS arm64: 8/13, the five success returns
 * failing.  The target-side evidence is the assembler: the success path now
 * ends SLR 15,15 before PDPEPIL in @@svset.s and @@svfmap.s.  Not run on
 * MVS: ssvt_set() stores into a live subsystem's SSVT in key 0.
 *
 * BUILD AND RUN (host, from test/host)
 *
 *     cc -std=gnu99 -Wall -Wextra -fsanitize=address \
 *        -D'__asm__(...)=' -D'asm(...)=' -D__32BIT__ \
 *        -I ../../include -I ../.. -o tstssvt tstssvt.c
 *     ./tstssvt                                       # rc 0 when green
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/mvs/subsys/@@svset.c"
#include "../../src/mvs/subsys/@@svfmap.c"

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static void fn(void) { }

int main(void)
{
    /* ssvtfrtn[] is declared [0]: the 256 routine addresses follow the
       struct, as they follow the SSVT in storage */
    SSVT *sp = calloc(1, sizeof(SSVT) + 256 * sizeof(void *));
#define ssvt (*sp)

    printf("=== tstssvt: ssvt_set()/ssvt_funcmap() return 0 (#240) ===\n\n");

    CHECK(ssvt_set(&ssvt, 1, (void *)fn) == 0, "ssvt_set(index 1): 0");
    CHECK(ssvt.ssvtfrtn[0] == (void *)fn, "ssvt_set(index 1): slot 0 set");
    CHECK(ssvt_set(&ssvt, 256, (void *)fn) == 0, "ssvt_set(index 256): 0");
    CHECK(ssvt_set(&ssvt, 1, (void *)0) == 0, "ssvt_set(reset): 0");
    CHECK(ssvt.ssvtfrtn[0] == (void *)0, "ssvt_set(reset): slot cleared");
    CHECK(ssvt_set(&ssvt, 0, (void *)fn) == EPERM, "ssvt_set(index 0): EPERM");
    CHECK(ssvt_set(&ssvt, 257, (void *)fn) == EPERM, "ssvt_set(index 257): EPERM");
    CHECK(ssvt_set(NULL, 1, (void *)fn) == EPERM, "ssvt_set(NULL): EPERM");

    CHECK(ssvt_funcmap(&ssvt, 1, 4) == 0, "ssvt_funcmap(1, func 4): 0");
    CHECK(ssvt.ssvtfcod[3] == 1, "ssvt_funcmap: code 3 maps to index 1");
    CHECK(ssvt_funcmap(&ssvt, 0, 4) == 0, "ssvt_funcmap(disable): 0");
    CHECK(ssvt_funcmap(&ssvt, 1, 0) == EPERM, "ssvt_funcmap(func 0): EPERM");
    CHECK(ssvt_funcmap(&ssvt, 257, 4) == EPERM, "ssvt_funcmap(index 257): EPERM");

#undef ssvt
    free(sp);

    printf("\n=== tstssvt: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
