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
 *   T1  __setsp(7), then 12 x __linkds(TSTPPAIN): each S0C1 is
 *       caught and the dead stack+PPA must come back to the region.
 *   T2  after the abend storm the outer module is intact: its own
 *       subpool-0 block, __crtget(), malloc and stdio all work -
 *       the guard against freeing one frame too many.
 *   T3  6 x __linkds(TSTPPAMD): B LINKs C, C abends - TWO blocks per
 *       round are abandoned, chained through PPASAVE, and the whole
 *       chain back to the snapshot must be released.
 *   T4  a tried function clobbers 8(TCBFSAB) with garbage (a heap
 *       block that is no PPA, zero, a 31-bit value) and abends:
 *       nothing may be freed, the snapshot must be restored.  This
 *       is the leg where the validation-free version is wrong.
 *
 * THE VERDICT is region arithmetic, tstsplnk-style: 12 + 2*6 = 24
 * abandoned blocks are ~6.2M pre-fix, so in REGION=8M the final
 * malloc(4M) FAILS pre-fix and succeeds post-fix, independent of
 * WTO-level noise.  Pre-fix: RC 8 (check (1c)).  Post-fix: RC 0.
 *
 * Console note: one WTO per TSTPPAIN/TSTPPAMD run; all 21 abends are
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

#define N1      12              /* T1: single-level caught abends     */
#define N3      6               /* T3: nested caught abends           */
#define PROBE   (4 * 1024 * 1024)   /* region-whole probe: fails on
                                       ~6.2M of leaked stacks, succeeds
                                       when they are reclaimed */

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

    /* T1 verdict: 24 abandoned blocks must all have come back ---------- */
    p = malloc(PROBE);
    bad += check("(1c) after 24 abandoned blocks: malloc(4M) works",
                 p != NULL);
    if (p) free(p);

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
