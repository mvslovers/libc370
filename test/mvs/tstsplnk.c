/*
 * tstsplnk.c - libc370 #89 probe: subpool inheritance through LINK
 *              (T3) and the abend-path reclaim (T4).  MVS target,
 *              batch.  Outer module; the inner one is TSTSPINR
 *              (test/mvs/tstspinr.c), LINKed from the same STEPLIB.
 *
 * This is the httpd#154 scenario in miniature and without httpd:
 *
 *   T3  __setsp(5), __linkds(TSTSPINR): the inner module's @@CRT0
 *       must INHERIT ambient subpool 5 through the validated PPA
 *       chain (it reports __getsp() as its rc), its malloc lands in
 *       subpool 5, and after it returns normally the outer module
 *       reclaims with FREEMAIN SP=5 while its own subpool-0 storage
 *       stays intact.  A normal return must also pop the PPA chain:
 *       8(TCBFSAB) is the outer PPA again.
 *
 *   T4  __setsp(6), __linkds(TSTSPINR) which abends S0C1 holding a
 *       1M block.  Green leg: release SP6 after each of 3 abends,
 *       then malloc(2M) must still work - the reclaim keeps the
 *       region whole.  Red control leg: 5 more abends with NO
 *       release, malloc(2M) must FAIL - proving the probe can see
 *       the leak the reclaim prevents.  A final release recovers the
 *       storage and malloc(2M) works again.  (The inner module's own
 *       @@CRT0 stack+PPA stay subpool 0 by #89's scope decision and
 *       leak ~260K per abend on BOTH legs - REGION=8M absorbs it.)
 *
 * THE CHAIN ASSERTION.  After an abend the inner module's @@EXITA
 * never runs, so its PPA would stay chained at 8(TCBFSAB) and every
 * CRT-anchored libc call in the outer module - stdio, __crtget(),
 * the ambient subpool - would resolve through the DEAD module's
 * environment.  The first version of this probe died S0C4 exactly
 * that way (JOB00790).  ___try()'s call() now snapshots the word
 * before the tried function and restores it on the abend path, so
 * this probe ASSERTS after every caught abend that 8(TCBFSAB) is the
 * outer PPA again.  Pre-fix: S0C4.  Post-fix: COND CODE 0000.
 *
 * Console note: each TSTSPINR run WTOs one line; the red leg ends in
 * malloc's 'Out of memory' WTO + traceback by design.  All eight
 * S0C1s are caught by __linkds with dumps suppressed.
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstsplnk.c -flinker-output=iebcopy -o TSTSPLNK
 *     cc370 -Iinclude test/mvs/tstspinr.c -flinker-output=iebcopy -o TSTSPINR
 *     ld370 --pack TSTSPLNK.iebcopy TSTSPINR.iebcopy -o probe -xmit \
 *           --dsn <LOADLIB>
 *
 * RUN: see jcl/tstsplnk.jcl.  Built by hand - libc370 is the cc370
 * sysroot, not an mbt project, so nothing compiles this TU automatically.
 *
 * RC: 0 = all expectations met, 8 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clibos.h>
#include <cliblink.h>
#include <clibwto.h>

#define GREENN  3               /* abend+release iterations           */
#define REDN    5               /* abend-without-release iterations   */
#define PROBE   (2 * 1024 * 1024)   /* region-whole probe size: must
                                       succeed when leaks are reclaimed,
                                       fail after the red leg's ~6M */

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

/* the "next" word of the TCB first save area - where @@CRT0 chains
   the PPA and @@EXITA is supposed to unchain it */
static unsigned *fsanext(void)
{
    unsigned    *psa = 0;                       /* low core == PSA  */
    unsigned    tcb  = psa[0x21C / 4];          /* PSATOLD          */
    unsigned    fsa  = *(unsigned *)(tcb + 0x70) & 0x00FFFFFF;

    return (unsigned *)(fsa + 8);
}

int main(void)
{
    void        *s0;
    void        *p;
    unsigned    myppa;
    unsigned    i;
    int         rc, prc;
    int         bad = 0;

    /* primes the stdio buffers while the ambient subpool is still 0 */
    printf("TSTSPLNK - libc370 #89 probe: inheritance through LINK\n\n");

    myppa = *fsanext();             /* the outer PPA, for the drift check */
    s0 = malloc(64);                /* outer subpool-0 storage */
    if (s0) memset(s0, 0xC1, 64);
    bad += check("(0) outer SP0 block allocated", s0 != NULL);

    /* T3: inheritance through LINK, normal return --------------------- */
    bad += check("(3a) __setsp(5) from ambient 0", __setsp(5) == 0);
    prc = -1;
    rc = __linkds("TSTSPINR", 0, 0, &prc);
    bad += check("(3b) __linkds rc 0 (no abend)", rc == 0);
    bad += check("(3c) inner inherited ambient 5", prc == 5);
    bad += check("(3d) outer ambient current again", __getsp() == 5);
    bad += check("(3e) PPA chain popped by normal return",
                 *fsanext() == myppa);
    bad += check("(3f) FREEMAIN SP=5 reclaims inner's malloc",
                 rawrel(5) == 0);
    __setsp(0);
    bad += check("(3g) outer SP0 block intact after reclaim",
                 s0 && ((unsigned char *)s0)[0] == 0xC1
                    && ((unsigned char *)s0)[63] == 0xC1);
    p = malloc(PROBE);
    bad += check("(3h) region whole: malloc(2M) works", p != NULL);
    free(p);

    /* T4 green leg: abend + release each time -------------------------- */
    __setsp(6);
    for (i = 0; i < GREENN; i++) {
        rc = __linkds("TSTSPINR", 0, 0, &prc);
        if (rc == 0) bad += check("(4a) inner abend was caught", 0);
        if (*fsanext() != myppa) {
            bad += check("(4x) ___try restored 8(TCBFSAB)", 0);
            *fsanext() = myppa;     /* repair so the probe can go on */
        }
        rc = rawrel(6);
        if (rc) bad += check("(4b) release after abend rc 0", 0);
    }
    wtof("TSTSPLNK: green leg done, %u abends, each released", i);
    __setsp(0);
    p = malloc(PROBE);
    bad += check("(4c) after 3 abends WITH release: malloc(2M) works",
                 p != NULL);
    free(p);

    /* T4 red control leg: abends with NO release ----------------------- */
    __setsp(6);
    for (i = 0; i < REDN; i++) {
        rc = __linkds("TSTSPINR", 0, 0, &prc);
        if (rc == 0) bad += check("(4d) inner abend was caught", 0);
        if (*fsanext() != myppa) {
            bad += check("(4y) ___try restored 8(TCBFSAB)", 0);
            *fsanext() = myppa;     /* repair so the probe can go on */
        }
    }
    __setsp(0);
    p = malloc(PROBE);
    bad += check("(4e) after 5 abends with NO release: malloc(2M) FAILS",
                 p == NULL);
    if (p) free(p);

    bad += check("(4f) final FREEMAIN SP=6 rc 0", rawrel(6) == 0);
    /* The reclaimed 1M holes sit BETWEEN the leaked inner stacks, so
       one contiguous 2M block may not exist - recovery is proven by
       re-filling the holes the red leg leaked, one chunk at a time. */
    {
        void        *back[REDN - 1];
        unsigned    got = 0;

        for (i = 0; i < REDN - 1; i++) {
            back[i] = malloc(1024 * 1024);
            if (back[i]) got++;
        }
        bad += check("(4g) the release recovered the leaked chunks",
                     got == REDN - 1);
        for (i = 0; i < REDN - 1; i++) {
            free(back[i]);
        }
    }

    /* the chain must be clean at the end without any (4x)/(4y) repair */
    bad += check("(6) 8(TCBFSAB) is the outer PPA after all abends",
                 *fsanext() == myppa);
    *fsanext() = myppa;             /* belt and braces for teardown */

    free(s0);
    printf("\nTSTSPLNK %s\n", bad ? "FAILED" : "PASSED");
    if (bad) wtof("TSTSPLNK FAILED (%d checks)", bad);
    else     wtof("TSTSPLNK PASSED");
    return bad ? 8 : 0;
}

static int check(const char *what, int ok)
{
    printf("  %-56s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok ? 0 : 1;
}
