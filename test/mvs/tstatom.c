/*
 * tstatom.c - libc370 #48: the atomics, __cas() and __swap().
 *
 * MVS target only - both are inline S/370 assembler, so there is nothing to
 * run on the host.
 *
 * __swap() is the function formerly called __cs(), which stored the word AT
 * new_value instead of new_value itself.  Case (1) keeps that from coming
 * back, using the trick that made it reportable rather than fatal: pass a
 * value that is ALSO a valid address, of a word holding something else, so
 * both the correct and the defective outcome land in readable storage.
 *
 * __cas() is new.  What has to be true of it, and what a plain exchange
 * cannot give you, is case (4): when the comparison fails, memory must be
 * left ALONE and the caller must be told what is there instead.
 *
 * Build:   cc370 -O1 -Iinclude test/mvs/tstatom.c -o TSTATOM \
 *                -flinker-output=xmit
 * Install: RECEIVE the XMIT into the STEPLIB of jcl/tstatom.jcl.
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include "clibos.h"

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }         \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }         \
    } while (0)

#define CHECK_HEX(got, want, msg)                                         \
    do {                                                                  \
        unsigned g_ = (unsigned)(got), w_ = (unsigned)(want);             \
        mbt_run++;                                                        \
        if (g_ == w_) { mbt_passed++; printf("  PASS: %s\n", (msg)); }     \
        else { mbt_failed++;                                              \
               printf("  FAIL: %s (got %08X, want %08X)\n", (msg), g_, w_); } \
    } while (0)

#define SLOT_FREE   0xFFFFFFFF

static unsigned target;
static unsigned decoy;
static unsigned decoy2;
static unsigned slot;

int main(int argc, char **argv)
{
    unsigned old;
    unsigned want;
    int      rc;

    printf("TSTATOM - libc370 #48: __cas() and __swap()\n\n");

    /* ------------------------------------------------------------------
     * (1) __swap() stores the value, not the word at it  [#48 regression]
     * ---------------------------------------------------------------- */
    printf("(1) __swap() with a value that is also an address\n");
    decoy  = 0xDEADBEEF;
    target = 0xAAAAAAAA;

    old = __swap(&target, (unsigned)&decoy);

    printf("      &decoy=%08X  decoy=%08X  target=%08X  rc=%08X\n",
           (unsigned)&decoy, decoy, target, old);

    CHECK_HEX(target, (unsigned)&decoy, "(1) target holds the value passed");
    CHECK(target != 0xDEADBEEF, "(1) target does NOT hold the word at that address");
    CHECK_HEX(old, 0xAAAAAAAA, "(1) return value is the previous content");
    CHECK_HEX(decoy, 0xDEADBEEF, "(1) the decoy itself is untouched");

    /* ------------------------------------------------------------------
     * (2) __swap() chains: what comes back is what went in before
     * ---------------------------------------------------------------- */
    printf("(2) consecutive exchanges\n");
    decoy2 = 0x5A5A5A5A;

    old = __swap(&target, (unsigned)&decoy2);

    CHECK_HEX(target, (unsigned)&decoy2, "(2) target holds the second value");
    CHECK_HEX(old, (unsigned)&decoy, "(2) return value is what (1) stored");

    printf("(3) __swap() with a NULL target\n");
    old = __swap((unsigned *)0, 1);
    CHECK_HEX(old, 0, "(3) returns 0 and does not abend");

    /* ------------------------------------------------------------------
     * (4) __cas() when the comparison holds
     * ---------------------------------------------------------------- */
    printf("(4) __cas() succeeds\n");
    target = 0x11111111;
    want   = 0x11111111;

    rc = __cas(&target, &want, 0x22222222);

    CHECK_HEX(rc, 0, "(4) returns 0 - swapped");
    CHECK_HEX(target, 0x22222222, "(4) memory holds the new value");
    CHECK_HEX(want, 0x11111111, "(4) expect is left as the caller set it");

    /* ------------------------------------------------------------------
     * (5) __cas() when it does not - the case a plain exchange cannot do
     * ---------------------------------------------------------------- */
    printf("(5) __cas() fails\n");
    target = 0x33333333;
    want   = 0x11111111;            /* stale: memory moved on without us */

    rc = __cas(&target, &want, 0x44444444);

    CHECK_HEX(rc, 1, "(5) returns 1 - not swapped");
    CHECK_HEX(target, 0x33333333, "(5) memory is LEFT ALONE");
    CHECK_HEX(want, 0x33333333, "(5) expect now holds what is really there");

    /* ------------------------------------------------------------------
     * (6) bad arguments are distinguishable from both outcomes
     * ---------------------------------------------------------------- */
    printf("(6) __cas() with NULL arguments\n");
    want = 0;
    CHECK_HEX(__cas((unsigned *)0, &want, 1), (unsigned)-1, "(6) NULL mem -> -1");
    CHECK_HEX(__cas(&target, (unsigned *)0, 1), (unsigned)-1, "(6) NULL expect -> -1");
    CHECK_HEX(target, 0x33333333, "(6) nothing was stored");

    /* ------------------------------------------------------------------
     * (7) the pattern rexx370 had to fake: claim a slot, and let the
     *     loser find out who won without ever publishing a bogus value
     *     (irx#anch.c swapped a value in and back out again to do this)
     * ---------------------------------------------------------------- */
    printf("(7) claiming a slot\n");
    slot = SLOT_FREE;

    want = SLOT_FREE;
    rc   = __cas(&slot, &want, 0x0BADCAFE);
    CHECK_HEX(rc, 0, "(7) first claim wins");
    CHECK_HEX(slot, 0x0BADCAFE, "(7) the slot is ours");

    want = SLOT_FREE;
    rc   = __cas(&slot, &want, 0x0C0FFEE0);
    CHECK_HEX(rc, 1, "(7) second claim loses");
    CHECK_HEX(slot, 0x0BADCAFE, "(7) the winner's value survives untouched");
    CHECK_HEX(want, 0x0BADCAFE, "(7) the loser is told who holds it");

    printf("\n=== TSTATOM: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");

    return mbt_failed ? 1 : 0;
}
