/*
 * tstcs.c - libc370 #48: __cs() must store new_value, not the word at it.
 *
 * MVS target only - __cs() is inline S/370 assembler (src/clib/@@cs.c), so
 * there is nothing to run on the host.
 *
 * THE DISCRIMINATING TRICK: pass a new_value that is ALSO a valid address, of
 * a word holding a different, recognisable value.
 *
 *     decoy  = 0xDEADBEEF
 *     __cs(&target, (unsigned)&decoy)
 *
 *     correct   target == &decoy      (the value we passed)
 *     defective target == 0xDEADBEEF  (the word AT the address we passed)
 *
 * Both outcomes are readable storage, so the case reports instead of abending -
 * which the obvious repro (passing a plain integer like 42 and watching it
 * dereference low storage) does not guarantee.
 *
 * Build:   cc370 -O1 -Iinclude test/mvs/tstcs.c -o TSTCS -flinker-output=xmit
 * Install: RECEIVE the XMIT into the STEPLIB of jcl/tstcs.jcl.
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

static unsigned target;
static unsigned decoy;
static unsigned decoy2;

int main(int argc, char **argv)
{
    unsigned old;

    printf("TSTCS - libc370 #48: __cs() stores new_value\n\n");

    /* ------------------------------------------------------------------
     * (1) the value passed is also a valid address
     * ---------------------------------------------------------------- */
    printf("(1) new_value that is also an address\n");
    decoy  = 0xDEADBEEF;
    target = 0xAAAAAAAA;

    old = __cs(&target, (unsigned)&decoy);

    printf("      &decoy=%08X  decoy=%08X  target=%08X  rc=%08X\n",
           (unsigned)&decoy, decoy, target, old);

    CHECK_HEX(target, (unsigned)&decoy, "(1) target holds the value passed");
    CHECK(target != 0xDEADBEEF, "(1) target does NOT hold the word at that address");
    CHECK_HEX(old, 0xAAAAAAAA, "(1) return value is the previous content");
    CHECK_HEX(decoy, 0xDEADBEEF, "(1) the decoy itself is untouched");

    /* ------------------------------------------------------------------
     * (2) two swaps in a row - the return value has to chain
     * ---------------------------------------------------------------- */
    printf("(2) consecutive swaps\n");
    decoy2 = 0x5A5A5A5A;

    old = __cs(&target, (unsigned)&decoy2);

    CHECK_HEX(target, (unsigned)&decoy2, "(2) target holds the second value");
    CHECK_HEX(old, (unsigned)&decoy, "(2) return value is what (1) stored");
    CHECK_HEX(decoy2, 0x5A5A5A5A, "(2) the second decoy is untouched");

    /* ------------------------------------------------------------------
     * (3) a NULL target must not abend
     * ---------------------------------------------------------------- */
    printf("(3) NULL target\n");
    old = __cs((void *)0, (unsigned)&decoy);
    CHECK_HEX(old, 0, "(3) NULL target returns 0 and does not abend");

    printf("\n=== TSTCS: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");

    return mbt_failed ? 1 : 0;
}
