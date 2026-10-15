/*
 * tstcrtrs.c - libc370 #546: what does one normal run of a LINKed C
 *              module leave behind in the caller's heap subpool?
 *
 * Every C module entered by LINK builds a runtime of its own: @@CRT0
 * GETMAINs PPA + stack, __CRTSET adds the module's CLIBCRT to the new
 * PPA's ppacrt array (arraynew: a 128-byte heap block, eye catcher
 * ARRY), __GRTSET callocs a CLIBGRT.  At the end __CRTRES removes the
 * CRT from the array and frees it, but - before #546 - not the array;
 * @@EXITA then FREEMAINs the PPA, the only pointer to it.  The block
 * stays in the heap subpool the module inherited from its caller (0):
 * 128 bytes per run, the per-request loss an HTTP server measured.
 *
 * The probe LINKs the inner module (PARM, default TSTCRTRI) ROUNDS x
 * RUNS times, normally - no abend - and counts free storage by drain
 * (64K units, then 4K units for the tail) before and after each round.
 * Round 1 carries first-call costs; rounds 2..ROUNDS are the verdict:
 *
 *   (t1) every LINK returned 0
 *   (t2) steady-state loss per round under 16K - before #546 it is
 *        about RUNS x 128 = 64K
 *
 * RC 0 = every check passed, 8 = a check failed.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstcrtrs.c -o TSTCRTRS -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror -Iinclude -L build/sdk \
 *                test/mvs/tstcrtri.c -o TSTCRTRI -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Werror test/mvs/tstcrtri.c \
 *                -o TSTCRTRR -flinker-output=iebcopy    (red: installed)
 *          ld370 --pack TSTCRTRS=TSTCRTRS.iebcopy TSTCRTRI=TSTCRTRI.iebcopy \
 *                TSTCRTRR=TSTCRTRR.iebcopy -o tstcrtrs -xmit \
 *                --dsn IBMUSER.LIBC370.CRSSCR
 * Upload:  tstcrtrs.xmit to IBMUSER.LIBC370.CRSXMIT (FB 80, binary)
 * Install: jcl/recvcrs.jcl.   Run: jcl/tstcrtrs.jcl.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>
#include <mvs/link.h>

#define ROUNDS  4
#define RUNS    500
#define UNIT    (64 * 1024)

static int  bad = 0;

static void check(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", what);
    wtof("TSTCRTRS %s %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* free storage in K: malloc 64K units, then 4K units, then free all */
static unsigned drainK(void)
{
    void        *chain = 0;
    void        *p;
    unsigned    n64 = 0, n4 = 0;

    while ((p = malloc(UNIT - 16)) != 0) {
        *(void **)p = chain;
        chain = p;
        n64++;
    }
    while ((p = malloc(4096 - 16)) != 0) {
        *(void **)p = chain;
        chain = p;
        n4++;
    }
    while (chain) {
        p = chain;
        chain = *(void **)chain;
        free(p);
    }
    return n64 * 64 + n4 * 4;
}

int main(int argc, char **argv)
{
    char        pgm[9];
    unsigned    before, after;
    int         lossK, worst = 0;
    int         r, n, rc, prc;
    int         linkbad = 0;

    memset(pgm, 0, sizeof(pgm));
    strncpy(pgm, (argc > 1 && argv[1][0]) ? argv[1] : "TSTCRTRI", 8);

    printf("=== tstcrtrs: #546 - heap left behind by a LINKed C module ===\n");
    printf("    inner %s, %d rounds of %d LINKs\n", pgm, ROUNDS, RUNS);

    before = drainK();
    for (r = 1; r <= ROUNDS; r++) {
        for (n = 0; n < RUNS; n++) {
            prc = -1;
            rc = __link(pgm, 0, 0, &prc);
            if (rc != 0 || prc != 0) linkbad++;
        }
        after = drainK();
        lossK = (int)before - (int)after;
        printf("      round %d: free %uK -> %uK, lost %dK (%d bytes per run)\n",
               r, before, after, lossK, lossK * 1024 / RUNS);
        wtof("TSTCRTRS %s round %d free %uK->%uK lost %dK",
             pgm, r, before, after, lossK);
        if (r > 1 && lossK > worst) worst = lossK;
        before = after;
    }

    check(linkbad == 0, "(t1) every LINK returned 0");
    check(worst < 16,   "(t2) steady-state loss per round under 16K");

    printf("=== tstcrtrs: %d check(s) failed ===\n", bad);
    wtof("TSTCRTRS %s VERDICT failed=%d worst=%dK", pgm, bad, worst);
    return bad ? 8 : 0;
}
