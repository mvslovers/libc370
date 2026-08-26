/*
 * tstenqdq.c - libc370 #147 item 4: __enqdeq()'s DEQ branch must keep
 * the scope bits.
 *
 * ISSUE #147: @@enqdeq.c resolves options into pl.opt in two steps -
 * the scope switch first (|= ENQ_OPT_SYSTEM / ENQ_OPT_SYSTEMS), then
 * the ENQ/DEQ split.  The DEQ branch wrote `pl.opt = ENQ_OPT_HAVE;` -
 * a plain assignment that throws the scope away, so every DEQ went to
 * SVC 48 as SCOPE=STEP.  Scope is part of the resource identity: a
 * DEQ issued STEP against an ENQ held SYSTEM addresses a DIFFERENT
 * resource, answers rc=8 with RET=HAVE, and the system-scope lock is
 * never released.  sysunlock() (@@slunlk.c, ENQ_SYSTEM) can therefore
 * not release what syslock() took - measured on the target by
 * test/mvs/tstslk.c.  lock()/unlock() are unaffected (STEP is the
 * default on both sides), which is why nothing ever noticed.
 *
 * This test compiles the REAL @@enqdeq.c with the two SVC statements
 * replaced by a capture of the parameter list the SVC would have been
 * issued with, and asserts on pl.opt directly:
 *
 *   1. ENQ  STEP / SYSTEM / SYSTEMS carry their scope (control cases -
 *      the ENQ branch always |='d correctly);
 *   2. DEQ  STEP is RET=HAVE with scope STEP (unchanged by the fix);
 *   3. DEQ  SYSTEM keeps ENQ_OPT_SYSTEM  - RED: pl.opt == 0x01,
 *      GREEN: 0x41;
 *   4. DEQ  SYSTEMS keeps ENQ_OPT_SYSTEMS - RED: 0x01, GREEN: 0x49;
 *   5. the queue name is blank-padded to 8 and the rname length is
 *      carried (sanity on the capture itself).
 *
 * BUILD / RUN (host, from test/host - NOTE: no -D'__asm__(...)=' here,
 * the capture macro below replaces the SVC statements instead):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall -Wextra -U__LP64__ -D__32BIT__ \
 *        -I $R/include -o t tstenqdq.c && ./t
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <string.h>

#include "../../include/enqpl.h"

/* ---- capture: the SVC parameter list at the moment of the call ------- */

static ENQPL    captured;
static int      captures;

static void pl_capture(ENQPL *p)
{
    captured = *p;
    captures++;
}

/* Both SVC statements in @@enqdeq.c are __asm__("..." : : "m"(pl) ...)
   with pl in scope; this macro turns each into a capture of that pl.
   The erased SVC leaves pl.rc = 0, so __enqdeq() returns 0 - fine, the
   assertions here are on the list, not the return. */
#define __asm__(...) pl_capture(&pl)
#include "../../src/clib/@@enqdeq.c"
#undef __asm__

/* ---- harness ---------------------------------------------------------- */

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK_HEX(got, want, msg)                                          \
    do {                                                                   \
        mbt_run++;                                                         \
        if ((got) == (want)) { mbt_passed++; printf("  PASS: %s\n", (msg)); } \
        else { mbt_failed++;                                               \
               printf("  FAIL: %s (got 0x%02X, want 0x%02X)\n",            \
                      (msg), (unsigned)(got), (unsigned)(want)); }         \
    } while (0)

static int mbt_test_summary(const char *name)
{
    printf("\n=== %s: %d/%d passed", name, mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}

int main(void)
{
    printf("=== tstenqdq: __enqdeq() DEQ keeps the scope bits "
           "(#147 item 4) ===\n\n");

    printf("(1) ENQ scope resolution (control):\n");
    __enqdeq("CLIBLOCK", "LOCK.00000000", ENQ_STEP, 0);
    CHECK_HEX(captured.opt, ENQ_OPT_HAVE, "(1) ENQ STEP = RET=HAVE only");
    __enqdeq("CSYSLOCK", "G.LOCK.00000000", ENQ_SYSTEM, 0);
    CHECK_HEX(captured.opt, ENQ_OPT_SYSTEM | ENQ_OPT_HAVE,
              "(1) ENQ SYSTEM carries the scope");
    __enqdeq("CSYSLOCK", "G.LOCK.00000000", ENQ_SYSTEMS, 0);
    CHECK_HEX(captured.opt, ENQ_OPT_SYSTEMS | ENQ_OPT_HAVE,
              "(1) ENQ SYSTEMS carries the scope");

    printf("\n(2) DEQ STEP (unchanged by the fix):\n");
    __enqdeq("CLIBLOCK", "LOCK.00000000", ENQ_STEP, 1);
    CHECK_HEX(captured.opt, ENQ_OPT_HAVE, "(2) DEQ STEP = RET=HAVE only");

    printf("\n(3) DEQ SYSTEM keeps the scope - the sysunlock() case:\n");
    __enqdeq("CSYSLOCK", "G.LOCK.00000000", ENQ_SYSTEM, 1);
    CHECK_HEX(captured.opt, ENQ_OPT_SYSTEM | ENQ_OPT_HAVE,
              "(3) DEQ SYSTEM = scope + RET=HAVE");

    printf("\n(4) DEQ SYSTEMS keeps the scope:\n");
    __enqdeq("CSYSLOCK", "G.LOCK.00000000", ENQ_SYSTEMS, 1);
    CHECK_HEX(captured.opt, ENQ_OPT_SYSTEMS | ENQ_OPT_HAVE,
              "(4) DEQ SYSTEMS = scope + RET=HAVE");

    printf("\n(5) capture sanity:\n");
    __enqdeq("CLIB", "LOCK.00000000", ENQ_STEP, 1);
    CHECK_HEX(captured.len, 13, "(5) rname length carried");
    CHECK_HEX(((char *)captured.qname)[4], ' ', "(5) qname blank-padded");
    CHECK_HEX(captures, 7, "(5) every call captured exactly once");

    return mbt_test_summary("tstenqdq");
}
