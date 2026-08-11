/*
 * tstcrtlk.c - libc370 #96 T7: what exactly does a caught abend of a
 *              LINKed program still cost after #93, and how much of
 *              it does the #89 ambient-subpool release recover?
 *              MVS target, batch.  Outer module; the inner one is
 *              TSTPPAIN from the #93 probe set (abends S0C1 when the
 *              ambient heap subpool it inherits is 7), LINKed from
 *              the same STEPLIB.
 *
 * Post-#93 the dead program's stack+PPA block comes back, yet
 * tstppafr measured ~150-170K of durable cost per caught abend.
 * The safe-to-free CRT-side blocks are tiny (CLIBCRT 392 bytes,
 * CLIBGRT 80, the ppacrt array ~100), so the bulk must be something
 * else - the working hypothesis is abandoned module copies in the
 * job pack area (subpool 251/252, ~70K per load, unreachable by any
 * subpool release).  This probe measures instead of guessing:
 *
 *   free storage is counted by drain: malloc 64K blocks until
 *   nothing fits (chained through the blocks, freed after
 *   counting), plus a 4K pass for the tail.  Per round r:
 *
 *     Dprev   free units before the round
 *     Dafter  after __setsp(7) + __linkds(TSTPPAIN) abends + catch
 *     Drel    after FREEMAIN R,SP=7 releases the dead heap
 *
 *     total   = Dprev - Dafter    per-abend durable cost
 *     heap    = Drel  - Dafter    the part the #89 release recovers
 *     residue = Dprev - Drel      the part NO subpool release reaches
 *
 *   Four rounds; round 1 carries the outer module's first-call
 *   footprint (stdio priming, abendrpt buffers), rounds 2-4 are the
 *   steady state and carry the verdict: pre-#96 every round costs
 *   172K durable (40K ambient heap + 132K of open stdio FILEs no
 *   subpool release can reach), post-#96 the walk's __ppahrv()
 *   closes and frees it all and the steady state is ~0K.  Checks
 *   (t1)/(t2) assert <= 16K.  Measured red JOB00903 (172K, RC 8),
 *   green JOB00906 (0K, COND CODE 0000).  The experiment legs that
 *   pinned the composition stay in: module size does not matter
 *   (T7b), RTM recovery itself is free (T7c), a normal LOADed run
 *   is free and LOAD+DELETE does not help the abend case (T7d) -
 *   the cost was always the dead program's runtime, not MVS.
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstcrtlk.c -flinker-output=iebcopy -o TSTCRTLK
 *     ld370 --pack TSTCRTLK.iebcopy -o probe -xmit --dsn <LOADLIB>
 *     (TSTPPAIN travels with the #93 probe set, already in the LOADLIB)
 *
 * RUN: see jcl/tstcrtlk.jcl.  RC: 0 = mechanics ok, numbers on the
 * console; 8 = a mechanic broke, numbers suspect.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clibos.h>
#include <cliblink.h>
#include <clibtry.h>
#include <clibwto.h>

#define ROUNDS  4
#define UNIT    (64 * 1024)

static int check(const char *what, int ok);

/* conditional whole-subpool release, the httpd#154 reclaim */
static int rawrel(unsigned sp)
{
    int     rc = 0;

    __asm__("FREEMAIN RC,SP=(%1)\n\t"
            "LR\t%0,15"
            : "=r"(rc)
            : "r"(sp)
            : "0", "1", "14", "15");
    return rc;
}

/* T7c helper: abend right here, no LINKed program involved */
static int boomg(unsigned unused)
{
    __asm__("DC\tH'0'");
    return 0;
}

/* the "next" word of the TCB first save area - the #93 chain check */
static unsigned *fsanext(void)
{
    unsigned    *psa = 0;                       /* low core == PSA  */
    unsigned    tcb  = psa[0x21C / 4];          /* PSATOLD          */
    unsigned    fsa  = *(unsigned *)(tcb + 0x70) & 0x00FFFFFF;

    return (unsigned *)(fsa + 8);
}

/* count free storage: 64K units + 4K tail units, then free it all.
   subpool 0 only - the ambient must be 0 when this is called. */
static void drainfree(unsigned *units64, unsigned *units4)
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
    *units64 = n64;
    *units4  = n4;
}

int main(void)
{
    unsigned    myppa;
    unsigned    r;
    unsigned    p64, p4;            /* previous (Dprev)             */
    unsigned    a64, a4;            /* after the abend (Dafter)     */
    unsigned    l64, l4;            /* after the release (Drel)     */
    int         tK, rK;             /* round total/residue in K     */
    int         worstt = 0;         /* worst steady-state total     */
    int         worstr = 0;         /* worst steady-state residue   */
    int         rc, rc2, prc;
    int         bad = 0;

    printf("TSTCRTLK - libc370 #96 T7: per-abend cost composition\n\n");

    myppa = *fsanext();
    drainfree(&p64, &p4);
    wtof("TSTCRTLK: baseline free %u x64K + %u x4K", p64, p4);
    bad += check("(0) region drainable at baseline", p64 > 10);

    for (r = 1; r <= ROUNDS; r++) {
        __setsp(7);
        rc = __linkds("TSTPPAIN", 0, 0, &prc);
        __setsp(0);
        if (rc == 0) bad += check("(1) inner abend was caught", 0);
        if (*fsanext() != myppa) {
            bad += check("(2) chain restored after catch", 0);
            *fsanext() = myppa;     /* repair so the probe can go on */
        }

        drainfree(&a64, &a4);
        rc = rawrel(7);
        if (rc) bad += check("(3) FREEMAIN SP=7 rc 0", 0);
        drainfree(&l64, &l4);

        tK = ((int)(p64 - a64) * 64) + ((int)(p4 - a4) * 4);
        rK = ((int)(p64 - l64) * 64) + ((int)(p4 - l4) * 4);
        wtof("TSTCRTLK: round %u: total=%dK heap=%dK residue=%dK"
             " (free now %u x64K + %u x4K)",
             r, tK,
             ((int)(l64 - a64) * 64) + ((int)(l4 - a4) * 4),
             rK, l64, l4);
        if (r >= 2) {               /* round 1 carries first-call noise */
            if (tK > worstt) worstt = tK;
            if (rK > worstr) worstr = rK;
        }

        p64 = l64;
        p4  = l4;
    }

    /* THE #96 VERDICT.  Pre-#96 a caught abend costs 172K durable of
       which 132K survives even a whole-subpool release (the dead
       program's open stdio FILEs); post-#96 the walk's __ppahrv()
       closes and frees it all and the steady-state cost is ~0 (a
       round of fragmentation noise allowed).  16K threshold: far
       above noise, far below the 172K/132K it guards against. */
    bad += check("(t1) steady per-abend total <= 16K", worstt <= 16);
    bad += check("(t2) steady residue after release <= 16K", worstr <= 16);

    /* T7b: same rounds with TSTPPBIG, whose load module carries a
       128K pad.  If the residue scales with the module size, the
       residue IS abandoned module copies in the job pack area. */
    for (r = 1; r <= 2; r++) {
        __setsp(7);
        rc = __linkds("TSTPPBIG", 0, 0, &prc);
        __setsp(0);
        if (rc == 0) bad += check("(5) big inner abend was caught", 0);
        if (*fsanext() != myppa) {
            bad += check("(6) chain restored after catch", 0);
            *fsanext() = myppa;
        }
        drainfree(&a64, &a4);
        rc = rawrel(7);
        if (rc) bad += check("(7) FREEMAIN SP=7 rc 0", 0);
        drainfree(&l64, &l4);
        wtof("TSTCRTLK: big round %u: total=%dK heap=%dK residue=%dK",
             r,
             ((int)(p64 - a64) * 64) + ((int)(p4 - a4) * 4),
             ((int)(l64 - a64) * 64) + ((int)(l4 - a4) * 4),
             ((int)(p64 - l64) * 64) + ((int)(p4 - l4) * 4));
        p64 = l64;
        p4  = l4;
    }

    /* T7c: a local abend with NO inner program at all - isolates
       what RTM recovery itself costs per caught abend. */
    for (r = 1; r <= 2; r++) {
        rc = try(boomg, 0);
        if (rc == 0) bad += check("(8) local abend was caught", 0);
        drainfree(&a64, &a4);
        wtof("TSTCRTLK: local round %u: total=%dK",
             r, ((int)(p64 - a64) * 64) + ((int)(p4 - a4) * 4));
        p64 = a64;
        p4  = a4;
    }

    /* T7d: LOAD + BALR + DELETE, first without an abend (ambient 0,
       the inner returns 0), then with one.  ep and rc are logged -
       the earlier run's second abend round cost 0K and never reached
       the inner's WTO, which needs explaining before anything is
       built on this mechanism. */
    for (r = 1; r <= 4; r++) {
        unsigned    ep = 0;
        int         abend = (r > 2);    /* rounds 1-2 normal, 3-4 abend */

        __asm__("LOAD EP=TSTPPAIN\n\t"
                "LR\t%0,0"
                : "=r"(ep)
                :
                : "0", "1", "14", "15");
        if (!ep) {
            bad += check("(9) LOAD EP=TSTPPAIN worked", 0);
            break;
        }
        if (abend) __setsp(7);
        rc = try((void *)ep, 0);
        if (abend) __setsp(0);
        if (abend && rc == 0)
            bad += check("(10) loaded inner abend was caught", 0);
        if (!abend && rc != 0)
            bad += check("(11) loaded inner returned normally", 0);
        if (*fsanext() != myppa) {
            bad += check("(12) chain restored", 0);
            *fsanext() = myppa;
        }
        __asm__("DELETE EP=TSTPPAIN" : : : "0", "1", "15");
        drainfree(&a64, &a4);
        rc2 = rawrel(7);
        if (rc2) bad += check("(13) FREEMAIN SP=7 rc 0", 0);
        drainfree(&l64, &l4);
        wtof("TSTCRTLK: load round %u (%s): ep=%08X rc=%08X"
             " total=%dK heap=%dK residue=%dK",
             r, abend ? "abend" : "normal", ep, (unsigned)rc,
             ((int)(p64 - a64) * 64) + ((int)(p4 - a4) * 4),
             ((int)(l64 - a64) * 64) + ((int)(l4 - a4) * 4),
             ((int)(p64 - l64) * 64) + ((int)(p4 - l4) * 4));
        p64 = l64;
        p4  = l4;
    }

    bad += check("(4) 8(TCBFSAB) is the outer PPA at the end",
                 *fsanext() == myppa);
    *fsanext() = myppa;

    printf("\nTSTCRTLK %s - numbers are on the console\n",
           bad ? "FAILED" : "PASSED");
    if (bad) wtof("TSTCRTLK FAILED (%d checks)", bad);
    else     wtof("TSTCRTLK PASSED");
    return bad ? 8 : 0;
}

static int check(const char *what, int ok)
{
    printf("  %-56s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok ? 0 : 1;
}
