/*
 * tstrand.c - libc370 #387 on MVS: rand() within RAND_MAX, and a default
 * seed of 1.
 *
 * rand() masked its result with 0x8fff instead of 0x7fff, so it returned
 * only 0..4095 and 32768..36863, half the time above RAND_MAX.  And the
 * CRT that holds the seed is calloc'd, so without srand() the sequence
 * started from seed 0; C99 7.20.2.2 wants it to behave as after srand(1).
 *
 *   DEFAULT  the first three values without srand() equal those after
 *            srand(1): 16838, 5758, 10113
 *   RANGE    100000 draws: none above RAND_MAX, every eighth hit
 *   REPEAT   srand(42) twice gives the same values
 *
 * Built twice from this source: TSTRND against this tree's libc.a, TSTRNDR
 * against the installed one, the red control (DEFAULT, SRAND1 and RANGE
 * fail).
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstrand.c -o TSTRND -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude \
 *                test/mvs/tstrand.c -o TSTRNDR -flinker-output=iebcopy
 *          ld370 --pack TSTRND=TSTRND.iebcopy TSTRNDR=TSTRNDR.iebcopy \
 *                -o tstrand -xmit --dsn IBMUSER.LIBC370.RNDSCR
 * Install: jcl/recvrnd.jcl.   Run: jcl/tstrand.jcl.
 *
 * mvsdev JOB01712, 2026-10-08 (RECEIVE JOB01711): GREEN CC 0000, 5/5;
 * RED (installed 2.6.2) CC 0001, 1/5: DEFAULT 0 33756 34564, SRAND1 454
 * 34430 1921, RANGE max 36863 with 49785 of 100000 above RAND_MAX.
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <stdlib.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

int main(void)
{
    int     a, b, c, i, r, max = 0, outside = 0, same = 1;
    int     oct[8] = {0};
    int     first[5];

    printf("=== tstrand: rand() within RAND_MAX, default seed (#387) ===\n\n");

    /* DEFAULT: no srand() has run yet in this program */
    a = rand(); b = rand(); c = rand();
    printf("  DEFAULT  %d %d %d\n", a, b, c);
    CHECK(a == 16838 && b == 5758 && c == 10113,
          "DEFAULT without srand(): 16838, 5758, 10113 as after srand(1)");
    srand(1);
    a = rand(); b = rand(); c = rand();
    printf("  SRAND1   %d %d %d\n", a, b, c);
    CHECK(a == 16838 && b == 5758 && c == 10113,
          "SRAND1 srand(1): 16838, 5758, 10113");

    /* RANGE */
    srand(12345);
    for (i = 0; i < 100000; i++) {
        r = rand();
        if (r > max) max = r;
        if (r < 0 || r > RAND_MAX) outside++;
        else oct[r / ((RAND_MAX + 1) / 8)]++;
    }
    printf("  RANGE    max %d, %d outside, eighths %d %d %d %d %d %d %d %d\n",
           max, outside, oct[0], oct[1], oct[2], oct[3], oct[4], oct[5],
           oct[6], oct[7]);
    CHECK(outside == 0, "RANGE no value above RAND_MAX");
    for (i = 0; i < 8 && oct[i] > 0; i++) ;
    CHECK(i == 8, "RANGE every eighth of 0..RAND_MAX hit");

    /* REPEAT */
    srand(42);
    for (i = 0; i < 5; i++) first[i] = rand();
    srand(42);
    for (i = 0; i < 5; i++) if (rand() != first[i]) same = 0;
    CHECK(same, "REPEAT srand(42) twice: the same values");

    printf("\n=== tstrand: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0) printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return mbt_failed > 0 ? 1 : 0;
}
