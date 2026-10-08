/*
 * tstrand.c - libc370 #387: rand() stays within RAND_MAX.
 *
 * ISSUE #387: rand() masked its result with 0x8fff instead of 0x7fff, so
 * it returned only 0..4095 and 32768..36863 - never 4096..32767, and half
 * the time above RAND_MAX (32767).  Measured on MVS through a REXX
 * RANDOM(0,99999): 3000 draws, every one in 3..36863.
 *
 * The fix: mask with RAND_MAX.  The other half of #387, the default seed
 * (C99 7.20.2.2: without srand() rand() behaves as after srand(1)), is set
 * where the CRT is made, in @@crtset.c, which reads the PSA and cannot run
 * on a host; test/mvs/tstrand.c checks it.
 *
 * This test compiles the REAL rand.c and srand.c against a __crtget() shim
 * that hands out one static CLIBCRT.
 *
 * CHECKS
 *   (1) after srand(1) the first three values are 16838, 5758, 10113 -
 *       the sequence of the C standard's example generator             RED
 *   (2) 100000 draws: none above RAND_MAX                              RED
 *   (3) ...and each eighth of 0..RAND_MAX is hit                       RED
 *   (4) srand() with the same seed repeats the sequence         regression
 *   (5) without a CRT rand() returns 0                          regression
 *
 * BUILD / RUN (host, from test/host):
 *
 *     R=../..
 *     cc -std=gnu99 -Wall \
 *        -U__LP64__ -D'__asm__(...)=' -D__volatile__= -D__32BIT__ \
 *        -I $R/include -I $R -o t tstrand.c && ./t
 *
 * RC: 0 = every check passed, 1 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include "mvs/crt.h"

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* ---- the CRT shim ------------------------------------------------------ */

static CLIBCRT  crt;
static int      nocrt;

CLIBCRT *__crtget(void)
{
    return nocrt ? (CLIBCRT *)0 : &crt;
}

/* ---- the real thing ---------------------------------------------------- */

#include "../../src/stdlib/rand.c"
#include "../../src/stdlib/srand.c"

int main(void)
{
    int     a, b, c, i, r, max = 0, above = 0, same = 1;
    int     oct[8] = {0};
    int     first[5];
    char    msg[80];

    printf("=== tstrand: #387 - rand() within RAND_MAX ===\n\n");

    srand(1);
    a = rand(); b = rand(); c = rand();
    printf("  srand(1): %d %d %d\n", a, b, c);
    check(a == 16838 && b == 5758 && c == 10113,
          "(1) srand(1): 16838, 5758, 10113");

    srand(12345);
    for (i = 0; i < 100000; i++) {
        r = rand();
        if (r > max) max = r;
        if (r > RAND_MAX || r < 0) above++;
        else oct[r / ((RAND_MAX + 1) / 8)]++;
    }
    printf("  100000 draws: max %d, %d outside 0..RAND_MAX\n", max, above);
    printf("  per eighth: %d %d %d %d %d %d %d %d\n", oct[0], oct[1],
           oct[2], oct[3], oct[4], oct[5], oct[6], oct[7]);
    check(above == 0, "(2) no value outside 0..RAND_MAX");
    for (i = 0; i < 8 && oct[i] > 0; i++) ;
    sprintf(msg, "(3) every eighth of 0..RAND_MAX is hit (first empty: %d)",
            i < 8 ? i : -1);
    check(i == 8, msg);

    srand(42);
    for (i = 0; i < 5; i++) first[i] = rand();
    srand(42);
    for (i = 0; i < 5; i++) if (rand() != first[i]) same = 0;
    check(same, "(4) srand(42) twice: the same five values");

    nocrt = 1;
    check(rand() == 0, "(5) without a CRT rand() returns 0");

    printf("\n%s (%d failed)\n", bad ? "RED" : "GREEN", bad);
    return bad ? 1 : 0;
}
