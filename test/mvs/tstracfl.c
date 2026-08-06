/*
 * tstracfl.c - libc370 #64: racf_login() and racf_logout() stop taking the
 * AS-wide ASXB ENQ, and racf_logout() stops parking a foreign ACEE in
 * ASXBSENV (MVS target, batch, APF-AUTHORIZED, threads).
 *
 * ISSUE #64, the follow-up to #58.  racf_auth() was only one of three RACF
 * entry points holding lock(asxb); the other two kept it, and racf_logout()
 * additionally read ASXBSENV on entry and wrote the observed value back on
 * exit.  In a server with one TCB per user that observed value is routinely
 * ANOTHER session's ACEE, so the restore re-pins an identity its owner has
 * already moved on from - the chain in mvslovers/ftpd#64.
 *
 * The ACEE to delete travels in the RACINIT parameter list (offset X'34'),
 * which is where RACINIT looks first; ASXBSENV is only its fallback.  So the
 * poke was unnecessary and the restore was harmful.
 *
 * The issue also expected RACINIT to clear ASXBSENV itself when it holds the
 * ACEE being deleted.  **It does not** - case (4) below measured that, and
 * the first attempt at this fix, which dropped libc370's hand-coded clear on
 * that assumption, left the address space resting on a freed ACEE.  So the
 * clear stays; it is now a __cas() against the dead pointer, which cannot
 * clobber a concurrent writer and therefore still needs no ENQ.
 *
 * WHAT EACH CASE PROVES
 *
 *   (2)(3)  discriminate.  A caller holding the ASXB lock across the call
 *           must still hold it afterwards.  Pre-fix both routines DEQ it out
 *           from under them, exactly as racf_auth() did before #58.
 *   (4)     does NOT discriminate old from new - both clear the field - but
 *           it is the case that earns its keep: it caught the first version
 *           of this fix, which delegated the clear to RACINIT and left
 *           ASXBSENV pointing at a deleted ACEE.  A dangling ACEE is worse
 *           than none, because the next authorization decision follows it.
 *   (5)     records the invariant that a foreign ACEE is left alone.  Also
 *           the same both ways in one task - see (6).
 *   (6)     discriminates, and is the defect itself.  A worker TCB parks NULL
 *           in ASXBSENV while the main task logs in and out; nothing else
 *           writes that field, so any foreign value the worker reads back is
 *           racf_logout()'s save/restore landing on top of it.
 *
 * No password needed: racf_login(user, NULL, ...) takes the PASSCHK=NO path,
 * which an authorized caller may use.  Two logins of the same userid give two
 * distinct ACEEs, which is all (5) and (6) need.
 *
 * AUTHORIZATION and probe hygiene - see test/mvs/tstracau.c and
 * doc/consumer-notes.md: AC=1 via ld370 twice, no writable statics if this
 * runs from a LNKLST library, WTO for anything that must survive an abend.
 *
 * BUILD (host):
 *     cc370 -O1 -Iinclude -c test/mvs/tstracfl.c -o tstracfl.o
 *     ld370 --entry @@CRT0 --ac 1 -o TSTRACFL build/sdk/crt0.o tstracfl.o \
 *           -Lbuild/sdk -lc
 *     ld370 --pack TSTRACFL=TSTRACFL --ac 1 -o probe -xmit --dsn <LOADLIB>
 * Install: RECEIVE into a scratch library, IEBCOPY into an APF-authorized
 *          one, delete the member again afterwards.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdlib.h>
#include <clibwto.h>
#include <clibthrd.h>
#include "racf.h"
#include "cliblock.h"

#define ROUNDS  200             /* login/logout pairs on the main TCB.
                                ** 60 was not enough: the pre-fix run hit the
                                ** window 0 times in 569 worker loops, 26 times
                                ** in 3026.  A guard that does not fire is not
                                ** a guard. */
#define SPINS   200000          /* worker bound, so it cannot run forever */

typedef struct {
    volatile int    stop;
    int             loops;
    int             mismatch;
    ACEE           *seen;
} WRK;

static int check(const char *what, int got, int want)
{
    int fail = (got != want);

    wtof("TSTRACFL %-42s %-10d %s", what, got, fail ? "*** FAIL" : "ok");
    if (fail) wtof("TSTRACFL %-42s expected %d", "", want);

    return fail;
}

/* PASSCHK=NO login; the group is optional on this system, try without one
** first and fall back to SYS1 so the probe reports which worked. */
static ACEE *login(int *rc_out)
{
    ACEE   *a;
    int     rc = 0;

    a = racf_login("IBMUSER", (const char *)0, (const char *)0, &rc);
    if (!a) a = racf_login("IBMUSER", (const char *)0, "SYS1", &rc);
    if (rc_out) *rc_out = rc;

    return a;
}

static int worker(void *arg1, void *arg2)
{
    WRK *w = (WRK *)arg1;
    int  n;

    for (n = 0; n < SPINS && !w->stop; n++) {
        ACEE *got;

        racf_set_acee((ACEE *)0);
        got = racf_get_acee();
        if (got) {
            w->mismatch++;
            if (!w->seen) w->seen = got;
        }
        w->loops++;
    }
    return 0;
}

int main(void)
{
    unsigned   *psa    = (unsigned *)0;
    unsigned   *ascb   = (unsigned *)psa[0x224/4];      /* A(ASCB) */
    unsigned   *asxb   = (unsigned *)ascb[0x6C/4];      /* A(ASXB) */
    ACEE       *resting;
    ACEE       *a;
    ACEE       *b;
    WRK        *w;
    CTHDTASK   *t;
    int         bad = 0;
    int         rc  = 0;
    int         i;

    resting = racf_get_acee();
    wtof("TSTRACFL start: resting ASXBSENV ACEE=%08X, ASXB=%08X",
         resting, asxb);

    /* (1) we need an ACEE of our own to work with */
    a = login(&rc);
    bad += check("(1) racf_login() PASSCHK=NO", a ? 0 : rc, 0);
    if (!a) {
        wtof("TSTRACFL: no ACEE, nothing to test - stopping");
        return 8;
    }
    wtof("TSTRACFL      ACEE A=%08X", a);

    /* (2) the ASXB lock must survive racf_login() */
    bad += check("(2a) lock(asxb) before login", lock(asxb, LOCK_EXC), 0);
    b = login(&rc);
    bad += check("(2b) testlock after racf_login - STILL ours",
                 testlock(asxb, LOCK_EXC), 8);
    unlock(asxb, LOCK_EXC);
    if (!b) {
        wtof("TSTRACFL: second login failed rc=%d - stopping", rc);
        return 8;
    }
    wtof("TSTRACFL      ACEE B=%08X", b);

    /* (3) and it must survive racf_logout() */
    bad += check("(3a) lock(asxb) before logout", lock(asxb, LOCK_EXC), 0);
    rc = racf_logout(&b);
    bad += check("(3b) testlock after racf_logout - STILL ours",
                 testlock(asxb, LOCK_EXC), 8);
    unlock(asxb, LOCK_EXC);
    bad += check("(3c) racf_logout() rc", rc, 0);

    /* (4) THE PREMISE.  Park A in ASXBSENV and delete A: the field must end
           up cleared, and after this change only RACINIT can do that. */
    racf_set_acee(a);
    rc = racf_logout(&a);
    bad += check("(4a) racf_logout() of the resting ACEE rc", rc, 0);
    bad += check("(4b) ASXBSENV cleared, not dangling",
                 racf_get_acee() ? 1 : 0, 0);
    if (racf_get_acee()) {
        wtof("TSTRACFL      ASXBSENV still %08X - points at a deleted ACEE",
             racf_get_acee());
    }

    /* (5) a foreign ACEE must be left alone by a logout of a different one */
    a = login(&rc);
    b = login(&rc);
    if (!a || !b) {
        wtof("TSTRACFL: could not build two ACEEs - stopping");
        return 8;
    }
    racf_set_acee(a);
    racf_logout(&b);
    bad += check("(5) foreign ACEE in ASXBSENV untouched",
                 racf_get_acee() == a, 1);

    /* (6) THE DEFECT.  Worker parks NULL while the main task churns
           login/logout; any foreign value it reads back came from
           racf_logout()'s save/restore. */
    w = (WRK *)calloc(1, sizeof(WRK));
    if (!w) return 8;

    racf_set_acee((ACEE *)0);
    t = cthread_create((void *)worker, w, (void *)0);
    if (!t) {
        wtof("TSTRACFL: cthread_create failed - (6) not run");
        bad++;
    }
    else {
        for (i = 0; i < ROUNDS; i++) {
            ACEE *tmp = login(&rc);
            if (tmp) racf_logout(&tmp);
        }
        w->stop = 1;
        while (!(t->termecb & 0x40000000)) cthread_yield();
        cthread_delete(&t);             /* or the step ends SA03 at exit */

        wtof("TSTRACFL      worker loops=%d, foreign reads=%d, first=%08X",
             w->loops, w->mismatch, w->seen);
        bad += check("(6) no foreign ACEE reached the worker",
                     w->mismatch, 0);
    }

    racf_logout(&a);
    racf_set_acee(resting);             /* put the address space back */

    wtof("TSTRACFL %s", bad ? "FAILED" : "PASSED");
    wtof("TSTRACFL (2)(3) and (6) discriminate; (4) is the premise, (5) the "
         "invariant");

    return bad ? 8 : 0;
}
