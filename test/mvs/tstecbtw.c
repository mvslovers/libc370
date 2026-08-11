/*
 * tstecbtw.c - libc370 #94 probe: ecb_timed_waitlist() and the STIMER
 *              ERRET guard.  MVS target, batch, single module.
 *
 * #94: ecb_timed_waitlist() stored the STIMER REAL failure code from
 * ERRET and never looked at it - it WAITed regardless.  For a
 * caller-local ECB the timer exit is the only poster in the address
 * space, so a failed STIMER meant a WAIT nothing would ever end: the
 * frozen httpd worker of mvslovers/httpd#159, reachable whenever
 * storage is tight enough for STIMER REAL to fail.  The fix returns
 * -rc without waiting (and without touching any ECB), plus one WTO
 * per task the first time.
 *
 * Legs:
 *   (1) timer post, plain: ecb_timed_wait() on a zero ECB must
 *       return 0 in ~bintvl with the ECB posted and the postcode
 *       masked to ECB_VALUE_MASK.
 *   (2) the parked plist slot: fsa[0] (where the exit finds its
 *       plist) must read the same before and after every call.
 *   (3) waitlist form: two ECBs, the timer posts timeecb only.
 *   (4) THE #94 LEG: drain the region with malloc until nothing
 *       fits, then call ecb_timed_wait() with everything still
 *       drained.  Two legitimate outcomes, both bounded post-fix:
 *         A: STIMER fails -> negative rc, ECB untouched, immediate
 *            return, libc's one-time WTO in the job log; a second
 *            drained call returns the error again with NO second
 *            WTO.  A pre-#94 libc HANGS at this exact call (the
 *            marker WTO right before it is the last sign of life) -
 *            that hang is the red run, cancel the job to end it.
 *         B: STIMER survives the drain (LSQA is fenced off from the
 *            region on a healthy system) -> the wait completes
 *            normally under storage pressure.  No hang either way -
 *            the leg then documents that this system cannot produce
 *            the failure from problem state and the red run needs a
 *            degraded system (httpd#159's field state).
 *       During the drained window nothing may touch the heap:
 *       results land in locals, wtof() only (stack buffer + SVC 35),
 *       printf() resumes after the drain is released.
 *   (5) after releasing the drain the timer path must be whole
 *       again.
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstecbtw.c -flinker-output=iebcopy -o TSTECBTW
 *     ld370 --pack TSTECBTW.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tstecbtw.jcl.  Built by hand - libc370 is the cc370
 * sysroot, not an mbt project, so nothing compiles this TU automatically.
 *
 * RC: 0 = all expectations met, 8 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <clibecb.h>
#include <clibwto.h>

static int check(const char *what, int ok);

/* fsa[0] - the word where ecb_timed_waitlist() parks the exit plist */
static unsigned fsaword0(void)
{
    unsigned    *psa = 0;                       /* low core == PSA  */
    unsigned    tcb  = psa[0x21C / 4];          /* PSATOLD          */
    unsigned    fsa  = *(unsigned *)(tcb + 0x70) & 0x00FFFFFF;

    return *(unsigned *)fsa;
}

int main(void)
{
    ECB         e1, e2, e3;
    ECB         *list[2];
    unsigned    fsa0;
    unsigned    i;
    time_t      t0, t1;
    int         rc, rc2;
    int         bad = 0;

    printf("TSTECBTW - libc370 #94 probe: STIMER ERRET guard\n\n");

    fsa0 = fsaword0();

    /* (1) timer post, plain ------------------------------------------- */
    e1 = 0;
    t0 = time(0);
    rc = ecb_timed_wait(&e1, 100, 0x123);       /* 1 second */
    t1 = time(0);
    bad += check("(1a) timed wait returns 0", rc == 0);
    bad += check("(1b) ECB posted by the timer exit",
                 (e1 & ECB_POSTED_BIT) != 0);
    bad += check("(1c) postcode delivered masked",
                 (e1 & ECB_VALUE_MASK) == 0x123);
    bad += check("(1d) wait was ~bintvl, not forever",
                 (t1 - t0) >= 0 && (t1 - t0) <= 5);
    bad += check("(2a) fsa[0] restored after the call",
                 fsaword0() == fsa0);

    /* (3) waitlist form: timer posts timeecb only ---------------------- */
    e1 = 0;
    e2 = 0;
    list[0] = &e1;
    list[1] = (ECB *)((unsigned)&e2 | 0x80000000);
    rc = ecb_timed_waitlist(list, &e2, 100, 0x77);
    bad += check("(3a) waitlist returns 0", rc == 0);
    bad += check("(3b) timeecb posted with the code",
                 (e2 & ECB_POSTED_BIT) && (e2 & ECB_VALUE_MASK) == 0x77);
    bad += check("(3c) the other ECB was not posted",
                 (e1 & ECB_POSTED_BIT) == 0);
    bad += check("(2b) fsa[0] restored after the call",
                 fsaword0() == fsa0);

    /* (4) the #94 leg: timed wait with the region drained -------------- */
    {
        static const unsigned   sizes[4] = { 1024*1024, 65536, 4096, 256 };
        void        *chain  = 0;
        void        *p;
        unsigned    dbytes  = 0;
        unsigned    stimer_failed;
        time_t      delapsed;
        ECB         de3;

        for (i = 0; i < 4; i++) {
            while ((p = malloc(sizes[i] - 16)) != 0) {
                *(void **)p = chain;            /* chain THROUGH the blocks */
                chain = p;
                dbytes += sizes[i];
            }
        }
        wtof("TSTECBTW: drained ~%uK, calling ecb_timed_wait -"
             " a pre-#94 libc hangs HERE if STIMER fails", dbytes / 1024);

        e3 = 0;
        t0 = time(0);
        rc = ecb_timed_wait(&e3, 100, 0x456);
        t1 = time(0);
        delapsed        = t1 - t0;
        de3             = e3;
        stimer_failed   = (rc < 0);
        rc2             = 0;
        if (stimer_failed) {
            /* second drained call: error again, but no second WTO -
               the report is once per task */
            rc2 = ecb_timed_wait(&e3, 100, 0x456);
        }
        wtof("TSTECBTW: drained call rc=%d elapsed=%d -> outcome %s",
             rc, (int)delapsed, stimer_failed ? "A: STIMER failed,"
             " guard returned without waiting" : "B: STIMER survived"
             " the drain, wait completed");

        /* release the drain before any printf touches the heap */
        while (chain) {
            p = chain;
            chain = *(void **)chain;
            free(p);
        }

        bad += check("(4a) drained call returned (bounded, no hang)", 1);
        if (stimer_failed) {
            bad += check("(4b) outcome A: ECB untouched, no wait",
                         de3 == 0 && delapsed <= 2);
            bad += check("(4c) outcome A: second call fails the same",
                         rc2 < 0);
        }
        else {
            bad += check("(4b) outcome B: posted normally under pressure",
                         rc == 0 && (de3 & ECB_POSTED_BIT)
                                 && (de3 & ECB_VALUE_MASK) == 0x456);
        }
        bad += check("(2c) fsa[0] restored after the drained call",
                     fsaword0() == fsa0);
    }

    /* (5) whole again after the drain is released ---------------------- */
    e1 = 0;
    rc = ecb_timed_wait(&e1, 100, 0x321);
    bad += check("(5a) timer path whole after release",
                 rc == 0 && (e1 & ECB_POSTED_BIT)
                         && (e1 & ECB_VALUE_MASK) == 0x321);
    bad += check("(2d) fsa[0] restored at the end", fsaword0() == fsa0);

    printf("\nTSTECBTW %s\n", bad ? "FAILED" : "PASSED");
    if (bad) wtof("TSTECBTW FAILED (%d checks)", bad);
    else     wtof("TSTECBTW PASSED");
    return bad ? 8 : 0;
}

static int check(const char *what, int ok)
{
    printf("  %-56s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok ? 0 : 1;
}
