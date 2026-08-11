/*
 * tstppafr.c - libc370 #93 probe: a caught abend in a LINKed program
 *              must not leak the ~262K @@CRT0 stack+PPA block.  MVS
 *              target, batch.  Outer module; the inner ones are
 *              TSTPPAIN (abends S0C1 on ambient subpool 7) and
 *              TSTPPAMD (LINKs TSTPPAIN without an ESTAE), both
 *              LINKed from the same STEPLIB.
 *
 * This is the httpd#172 abending-CGI cost in miniature: pre-#93 every
 * caught abend abandoned the dead program's @@CRT0 stack+PPA at
 * 8(TCBFSAB) - ~262K of subpool 0, unhooked by the #89 restore but
 * never freed.  ___try()'s call() now walks the abandoned chain back
 * to its snapshot and FREEMAINs each validated PPA block.  Legs, in
 * issue #93's numbering:
 *
 *   T5  normal returns first: __linkds(TSTPPAIN) and the nested
 *       __linkds(TSTPPAMD) on ambient 0 return rc 0 - @@EXITA frees
 *       the blocks and pops the chain, and ___try must NOT free
 *       again (a double free would corrupt subpool 0 for the whole
 *       remaining run).
 *   T1  __setsp(7), then 6 x __linkds(TSTPPAIN): each S0C1 is
 *       caught and the dead stack+PPA must come back to the region.
 *   T2  after the abend storm the outer module is intact: its own
 *       subpool-0 block, __crtget(), malloc and stdio all work -
 *       the guard against freeing one frame too many.
 *   T3  3 x __linkds(TSTPPAMD): B LINKs C, C abends - TWO blocks per
 *       round are abandoned, chained through PPASAVE, and the whole
 *       chain back to the snapshot must be released.
 *   T4  a tried function clobbers 8(TCBFSAB) with garbage (a heap
 *       block that is no PPA, zero, a 31-bit value) and abends:
 *       nothing may be freed, the snapshot must be restored.  This
 *       is the leg where the validation-free version is wrong.
 *
 * THE VERDICT is region arithmetic, tstsplnk-style, sized so it does
 * not depend on what IEFUSI grants above the request and tolerates
 * fragmentation: REGION=6M, and after all legs the probe counts how
 * many of four 1M mallocs succeed (no contiguity requirement).
 * Pre-fix each caught abend of a LINKed program costs ~434K durable
 * (measured live: the 262K stack+PPA plus ~170K of CLIBCRT/stdio the
 * #89 scope decision leaves alone), so the 6 + 2*3 = 12 abandoned
 * blocks demand ~4.6M and exhaust the region: nothing fits, and once
 * the region is gone the T3 LINKs may fail rc=12 instead of abending
 * (more red).  Post-fix the stack+PPA blocks come back - successive
 * inner stacks land at the SAME address - and only the out-of-scope
 * ~170K/abend remains: ~3M stay free and the 1M blocks fit again.
 * The threshold of 2 leaves >=1M of margin on either side.
 * Verified red->green on MVS 3.8j: pre-fix JOB00880 (RC 8, 0 of 4
 * blocks fit), post-fix JOB00882 (COND CODE 0000, 3 of 4).
 *
 * Console note: one WTO per TSTPPAIN/TSTPPAMD run; all 12 abends are
 * caught with dumps suppressed by the ESTAE.
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstppafr.c -flinker-output=iebcopy -o TSTPPAFR
 *     cc370 -Iinclude test/mvs/tstppamd.c -flinker-output=iebcopy -o TSTPPAMD
 *     cc370 -Iinclude test/mvs/tstppain.c -flinker-output=iebcopy -o TSTPPAIN
 *     ld370 --pack TSTPPAFR.iebcopy TSTPPAMD.iebcopy TSTPPAIN.iebcopy \
 *           -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tstppafr.jcl.  Built by hand - libc370 is the cc370
 * sysroot, not an mbt project, so nothing compiles this TU automatically.
 *
 * RC: 0 = all expectations met, 8 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clibos.h>
#include <cliblink.h>
#include <clibtry.h>
#include <clibcrt.h>
#include <clibwto.h>

#define N1      6               /* T1: single-level caught abends     */
#define N3      3               /* T3: nested caught abends           */
#define DRAINN  4               /* verdict: 1M blocks attempted;
                                   pre-fix none fit, post-fix >=2
                                   must (see header arithmetic)      */

static int check(const char *what, int ok);

/* the "next" word of the TCB first save area - where @@CRT0 chains
   the PPA and ___try()'s abend path reclaims it */
static unsigned *fsanext(void)
{
    unsigned    *psa = 0;                       /* low core == PSA  */
    unsigned    tcb  = psa[0x21C / 4];          /* PSATOLD          */
    unsigned    fsa  = *(unsigned *)(tcb + 0x70) & 0x00FFFFFF;

    return (unsigned *)(fsa + 8);
}

/* T4 helper, run under try(): clobber 8(TCBFSAB) the way a broken
   LINKed program would, then abend S0C1 */
static int boomg(unsigned *slot, unsigned val)
{
    *slot = val;
    __asm__("DC\tH'0'");
    return 0;
}

int main(void)
{
    unsigned char   *g;
    void            *s0, *p;
    unsigned        myppa;
    unsigned        i;
    int             rc, prc;
    int             bad = 0;

    /* primes the stdio buffers while the ambient subpool is still 0 */
    printf("TSTPPAFR - libc370 #93 probe: caught-abend stack reclaim\n\n");

    myppa = *fsanext();             /* the outer PPA, for every check */
    s0 = malloc(64);                /* outer subpool-0 canary */
    if (s0) memset(s0, 0xC1, 64);
    bad += check("(0a) outer SP0 block allocated", s0 != NULL);
    bad += check("(0b) __crtget() resolves", __crtget() != NULL);

    /* T5: normal returns - @@EXITA frees, ___try must not free again */
    for (i = 0; i < 2; i++) {
        prc = -1;
        rc = __linkds("TSTPPAIN", 0, 0, &prc);
        bad += check("(5a) normal LINK returns rc 0", rc == 0 && prc == 0);
        bad += check("(5b) PPA chain popped by @@EXITA", *fsanext() == myppa);
    }
    prc = -1;
    rc = __linkds("TSTPPAMD", 0, 0, &prc);
    bad += check("(5c) nested normal LINK returns rc 0", rc == 0 && prc == 0);
    bad += check("(5d) PPA chain popped twice", *fsanext() == myppa);

    /* T1: single-level caught abends ----------------------------------- */
    __setsp(7);
    for (i = 0; i < N1; i++) {
        rc = __linkds("TSTPPAIN", 0, 0, &prc);
        if (rc == 0) bad += check("(1a) inner abend was caught", 0);
        if (*fsanext() != myppa) {
            bad += check("(1b) chain restored after catch", 0);
            *fsanext() = myppa;     /* repair so the probe can go on */
        }
    }
    __setsp(0);
    wtof("TSTPPAFR: %u single-level abends caught", i);

    /* T2: the surviving caller is intact ------------------------------- */
    bad += check("(2a) __crtget() still resolves", __crtget() != NULL);
    bad += check("(2b) outer SP0 canary intact",
                 s0 && ((unsigned char *)s0)[0] == 0xC1
                    && ((unsigned char *)s0)[63] == 0xC1);
    p = malloc(1024);
    bad += check("(2c) malloc still works", p != NULL);
    free(p);
    /* stdio is exercised by every check() line reaching SYSPRINT */

    /* T3: nested - A LINKs B, B LINKs C, C abends ---------------------- */
    __setsp(7);
    for (i = 0; i < N3; i++) {
        rc = __linkds("TSTPPAMD", 0, 0, &prc);
        if (rc == 0) bad += check("(3a) nested abend was caught", 0);
        if (*fsanext() != myppa) {
            bad += check("(3b) chain restored after nested catch", 0);
            *fsanext() = myppa;     /* repair so the probe can go on */
        }
    }
    __setsp(0);
    wtof("TSTPPAFR: %u nested abends caught", i);

    /* T4: garbage at 8(TCBFSAB) - nothing may be freed ----------------- */
    g = malloc(64);
    bad += check("(4a) garbage block allocated", g != NULL);
    if (g) {
        memset(g, 0xEE, 64);        /* valid heap storage, no PPA eye */
        rc = try(boomg, fsanext(), (unsigned)g);
        bad += check("(4b) abend with heap garbage caught", rc != 0);
        bad += check("(4c) chain restored over garbage",
                     *fsanext() == myppa);
        bad += check("(4d) garbage block untouched, not freed",
                     g[0] == 0xEE && g[63] == 0xEE);
        free(g);
    }
    rc = try(boomg, fsanext(), 0u);
    bad += check("(4e) abend with zeroed word caught", rc != 0);
    bad += check("(4f) chain restored over zero", *fsanext() == myppa);
    rc = try(boomg, fsanext(), 0xFF000001u);
    bad += check("(4g) abend with 31-bit garbage caught", rc != 0);
    bad += check("(4h) chain restored over 31-bit garbage",
                 *fsanext() == myppa);

    /* T1 verdict: the 12 abandoned blocks must all have come back.
       Count 1M blocks instead of one big malloc so fragmentation
       cannot decide the outcome (see the header arithmetic). */
    {
        void        *drain[DRAINN];
        unsigned    got = 0;

        for (i = 0; i < DRAINN; i++) {
            drain[i] = malloc(1024 * 1024);
            if (drain[i]) got++;
        }
        for (i = 0; i < DRAINN; i++) {
            free(drain[i]);
        }
        wtof("TSTPPAFR: %u of %u 1M blocks fit after the abend storm",
             got, (unsigned)DRAINN);
        bad += check("(1c) after 12 abandoned blocks: 2+ 1M blocks fit",
                     got >= 2);
    }

    bad += check("(6) 8(TCBFSAB) is the outer PPA at the end",
                 *fsanext() == myppa);
    *fsanext() = myppa;             /* belt and braces for teardown */

    free(s0);
    printf("\nTSTPPAFR %s\n", bad ? "FAILED" : "PASSED");
    if (bad) wtof("TSTPPAFR FAILED (%d checks)", bad);
    else     wtof("TSTPPAFR PASSED");
    return bad ? 8 : 0;
}

static int check(const char *what, int ok)
{
    printf("  %-56s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok ? 0 : 1;
}
