/*
 * tstspm.c - libc370 #89 probe: runtime heap subpool round trips
 *            (T1), realloc across the subpool boundary (T2), pinning
 *            against a subpool release (T5), and the 24-bit header
 *            guard (T7).  MVS target, batch.
 *
 * ISSUE #89 makes the malloc subpool a runtime value: @@GETM resolves
 * PPAHEAPS from the current TCB's PPA per call and records the subpool
 * in the high byte of the rounded-size header word at p-8; @@FREEM
 * feeds that word straight to the R-form FREEMAIN.  __setsp() sets the
 * ambient value, __getmsp(size, sp) pins a block to an explicit
 * subpool regardless of the ambient one.
 *
 * What this probe asserts, in order:
 *
 *   T1  __setsp(5) round trip: a spread of sizes lands in subpool 5
 *       (header byte at p-8 reads 5), the requested size at p-4 is
 *       intact, SP5 and SP0 blocks interleave and free in mixed order.
 *   T2  realloc() across the boundary, both directions: contents
 *       preserved, result carries the NEW ambient subpool, old block
 *       freed cleanly.  realloc reads p-4, which #89 must not move.
 *   T5  pinning: under ambient 5, __getmsp(size, 0) and a __setsp(0)
 *       bracket both produce subpool-0 blocks that SURVIVE the
 *       FREEMAIN SP=5 release (the httpd#154 reclaim) and stay
 *       readable and freeable afterwards.
 *   T7  __getm() with a size whose rounded value would reach the
 *       header's subpool byte returns NULL - malloc's 6M cap does not
 *       protect __getm(), which httpd/mvsmf call directly.
 *   T8  the no-PPA contract: on a cthread TCB (which has no PPA)
 *       __getsp() reads 0, __setsp() is a no-op returning 0, and a
 *       malloc lands in subpool 0 even while the MAIN task's ambient
 *       subpool is 5 - resolution is the current TCB's own PPA, no
 *       owner-TCB fallback, so a worker can neither see nor damage
 *       the main task's ambient value.
 *
 * The stdio buffers are primed before any non-zero ambient subpool is
 * set, so no C-runtime storage lands in subpool 5 by accident - the
 * release in T5 must only ever see this probe's own test blocks.
 *
 * Console note: the SP5 release and the guard probes are quiet; only
 * a real failure WTOs.
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstspm.c -flinker-output=iebcopy -o TSTSPM
 *     ld370 --pack TSTSPM.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tstspm.jcl.  Built by hand - libc370 is the cc370
 * sysroot, not an mbt project, so nothing compiles this TU automatically.
 *
 * RC: 0 = all expectations met, 8 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clibos.h>
#include <clibwto.h>
#include <clibthrd.h>
#include <mvssupa.h>

#define TESTSP  5

/* header reads: the subpool byte at p-8, the requested size at p-4 */
#define BLKSP(p)    (((unsigned char *)(p))[-8])
#define BLKREQ(p)   (((unsigned *)(p))[-1])

static int check(const char *what, int ok);

/* T8 worker results: what the no-PPA contract looks like from a
   cthread TCB while the main task's ambient subpool is 5 */
typedef struct npp NPP;
struct npp {
    int             ran;        /* worker reached the end             */
    unsigned char   get0;       /* __getsp() on entry                 */
    unsigned char   setrc;      /* __setsp(77) return value           */
    unsigned char   get1;       /* __getsp() after the attempt        */
    unsigned char   blksp;      /* header byte of a malloc'd block    */
    int             blkok;      /* malloc worked                      */
};

static int nppworker(void *arg1, void *arg2)
{
    NPP     *r = (NPP *)arg1;
    void    *p;

    r->get0  = __getsp();
    r->setrc = __setsp(77);         /* must be a no-op */
    r->get1  = __getsp();
    p = malloc(64);
    r->blkok = (p != NULL);
    if (p) {
        r->blksp = ((unsigned char *)p)[-8];
        free(p);
    }
    r->ran = 1;
    return 0;
}

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

int main(void)
{
    static const unsigned sizes[6] = { 1, 7, 64, 65, 4096, 256 * 1024 };
    void        *a5[6], *a0[6];
    void        *p, *q, *pin, *br;
    unsigned    i;
    int         bad = 0;
    int         allsp = 1, allreq = 1, allz = 1;

    /* primes the stdio buffers while the ambient subpool is still 0 */
    printf("TSTSPM - libc370 #89 probe: runtime heap subpool\n\n");

    /* T1: ambient subpool round trip ---------------------------------- */
    bad += check("(1a) ambient subpool starts at 0", __getsp() == 0);
    bad += check("(1b) __setsp(5) returns previous 0", __setsp(TESTSP) == 0);
    bad += check("(1c) __getsp() now reads 5", __getsp() == TESTSP);

    for (i = 0; i < 6; i++) {
        a5[i] = malloc(sizes[i]);
        if (!a5[i]) break;
        if (BLKSP(a5[i]) != TESTSP) allsp = 0;
        if (BLKREQ(a5[i]) != sizes[i]) allreq = 0;
        memset(a5[i], 0x55, sizes[i]);
    }
    bad += check("(1d) all six SP5 allocations worked", i == 6);
    bad += check("(1e) every header byte reads subpool 5", allsp);
    bad += check("(1f) every requested size at p-4 intact", allreq);

    /* interleave subpool-0 blocks via a __setsp(0) bracket */
    __setsp(0);
    for (i = 0; i < 6; i++) {
        a0[i] = malloc(sizes[i]);
        if (!a0[i]) break;
        if (BLKSP(a0[i]) != 0) allz = 0;
        memset(a0[i], 0xAA, sizes[i]);
    }
    bad += check("(1g) all six SP0 allocations worked", i == 6);
    bad += check("(1h) their header bytes read subpool 0", allz);
    __setsp(TESTSP);

    /* free in mixed order: 5,0,5,0... then the stragglers backwards */
    for (i = 0; i < 6; i += 2) { free(a5[i]); free(a0[i + 1]); }
    for (i = 6; i > 0; i -= 2) { free(a0[i - 2]); free(a5[i - 1]); }
    p = malloc(64);
    bad += check("(1i) heap sane after mixed-order frees", p != NULL);
    free(p);

    /* T2: realloc across the subpool boundary ------------------------- */
    p = malloc(100);                    /* ambient is 5 */
    bad += check("(2a) SP5 source block", p && BLKSP(p) == TESTSP);
    if (p) memset(p, 0x5A, 100);
    __setsp(0);
    q = p ? realloc(p, 200) : NULL;     /* grows under ambient 0 */
    bad += check("(2b) realloc 5->0 carries subpool 0", q && BLKSP(q) == 0);
    bad += check("(2c) ... contents preserved",
                 q && ((unsigned char *)q)[0] == 0x5A
                   && ((unsigned char *)q)[99] == 0x5A);
    free(q);

    p = malloc(100);                    /* ambient is 0 */
    bad += check("(2d) SP0 source block", p && BLKSP(p) == 0);
    if (p) memset(p, 0xA5, 100);
    __setsp(TESTSP);
    q = p ? realloc(p, 200) : NULL;     /* grows under ambient 5 */
    bad += check("(2e) realloc 0->5 carries subpool 5",
                 q && BLKSP(q) == TESTSP);
    bad += check("(2f) ... contents preserved",
                 q && ((unsigned char *)q)[0] == 0xA5
                   && ((unsigned char *)q)[99] == 0xA5);
    free(q);

    /* T5: pinning against the subpool release ------------------------- */
    /* ambient is 5 here.  Three blocks: one ambient (goes with the
       release), one pinned raw, one pinned via the __setsp(0) bracket. */
    p = malloc(300);                    /* ambient SP5, reclaimed below */
    pin = __getmsp(300, 0);             /* pinned raw */
    {
        unsigned char   old = __setsp(0);
        br = malloc(300);               /* pinned via bracket */
        __setsp(old);
    }
    bad += check("(5a) ambient block is SP5", p && BLKSP(p) == TESTSP);
    bad += check("(5b) __getmsp(...,0) block is SP0", pin && BLKSP(pin) == 0);
    bad += check("(5c) __setsp(0)-bracket block is SP0",
                 br && BLKSP(br) == 0);
    bad += check("(5d) ... and the bracket restored ambient 5",
                 __getsp() == TESTSP);
    if (pin) memset(pin, 0xC3, 300);
    if (br)  memset(br, 0xD4, 300);

    bad += check("(5e) FREEMAIN SP=5 release rc 0", rawrel(TESTSP) == 0);
    /* p is gone with the release - it must NOT be freed again */
    bad += check("(5f) pinned block survived the release",
                 pin && ((unsigned char *)pin)[0] == 0xC3
                     && ((unsigned char *)pin)[299] == 0xC3);
    bad += check("(5g) bracket block survived the release",
                 br && ((unsigned char *)br)[0] == 0xD4
                    && ((unsigned char *)br)[299] == 0xD4);
    free(pin);
    free(br);
    p = malloc(64);
    bad += check("(5h) heap sane after the release", p != NULL);
    free(p);

    /* T8: the no-PPA contract on a cthread TCB ------------------------ */
    /* ambient is still 5 on the main task - the worker must neither
       see it nor be able to change it */
    {
        static NPP  r;
        CTHDTASK    *t;

        memset(&r, 0, sizeof(r));
        r.get0 = r.setrc = r.get1 = r.blksp = 0xFF;     /* sentinels */
        t = cthread_create((void *)nppworker, &r, (void *)0);
        if (!t) {
            bad += check("(8a) cthread_create worked", 0);
        }
        else {
            while (!(t->termecb & 0x40000000)) cthread_yield();
            cthread_delete(&t);     /* or the step ends SA03 at exit */
            bad += check("(8a) worker ran to completion", r.ran == 1);
            bad += check("(8b) worker __getsp() reads 0, not main's 5",
                         r.get0 == 0);
            bad += check("(8c) worker __setsp(77) is a no-op returning 0",
                         r.setrc == 0 && r.get1 == 0);
            bad += check("(8d) worker malloc lands in subpool 0",
                         r.blkok && r.blksp == 0);
            bad += check("(8e) main ambient 5 untouched by the worker",
                         __getsp() == TESTSP);
        }
    }
    __setsp(0);

    /* T7: the 24-bit header guard ------------------------------------- */
    /* rounded value would be 0x1000040: the guard must answer, not the
       region size */
    p = __getm(0x00FFFFFF);
    bad += check("(7a) __getm(0xFFFFFF) returns NULL", p == NULL);
    p = __getm((size_t)0x01000000);
    bad += check("(7b) __getm(16M) returns NULL", p == NULL);
    /* boundary that passes the guard: fails here for region size, but
       must fail as NULL, not as an abend or a corrupt header */
    p = __getm(0x00FFFF00);
    bad += check("(7c) __getm(0xFFFF00) NULL in this region", p == NULL);

    printf("\nTSTSPM %s\n", bad ? "FAILED" : "PASSED");
    if (bad) wtof("TSTSPM FAILED (%d checks)", bad);
    else     wtof("TSTSPM PASSED");
    return bad ? 8 : 0;
}

static int check(const char *what, int ok)
{
    printf("  %-52s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok ? 0 : 1;
}
