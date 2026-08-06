/*
 * tstracau.c - libc370 #58: racf_auth() passes the ACEE in the RACHECK plist
 * instead of poking ASXBSENV (MVS target, batch, APF-AUTHORIZED).
 *
 * ISSUE #58: racf_auth() authorized against a caller-supplied ACEE by writing
 * it into ASXBSENV for the duration of the RACHECK and restoring it after,
 * holding an address-space-wide ENQ on the ASXB to keep concurrent callers
 * out.  Neither was necessary - the RACHECK parameter list has an ACEE field
 * at offset X'18' which RAKF resolves first (SRCLIB/ICHSFR00.hlasm:111-115),
 * falling back to ASXBSENV only when it is zero.
 *
 * WHAT THIS TEST CAN AND CANNOT SHOW
 *
 * It cannot show the race the issue is about: that needs a second TCB
 * switching identity while racf_auth() runs, and the window is a few
 * instructions wide.  Nor does "does authorization still work" discriminate -
 * the old code got the right answer too, by swapping ASXBSENV.
 *
 * What IS deterministic, single-task, and needs no RACF profiles is the ENQ.
 * cliblock.h: lock() returns 8 when you already hold the lock.  The old code
 * ignored that return code and unlocked unconditionally, so a caller holding
 * the ASXB lock across racf_auth() got it DEQd out from under them - and the
 * next unlock() in that caller then failed.  Case (3) is that, and it is the
 * red/green: before the fix the lock is gone after the call, after it the
 * caller still has it.  racf_auth() no longer touches the lock at all.
 *
 * Case (5) documents the invariant that ASXBSENV is not disturbed.  It passes
 * both before and after - the old code restored what it saved - so it proves
 * nothing on its own.  It is here so that a future change that starts poking
 * ASXBSENV again fails something.
 *
 * AUTHORIZATION: racf_auth() issues MODESET KEY=ZERO,MODE=SUP.  This module
 * must be linked AC=1 and run from an APF-authorized library, or it S047s in
 * case (1).  The reference system authorizes SYS2.LINKLIB (SYS1.PARMLIB
 * (IEAAPF00)).
 *
 * Two things that cost you if you do not know them: cc370 silently drops
 * -Wl,--ac,1, so the AC has to be set by calling ld370 yourself, and ld370
 * --pack needs --ac 1 again or the packed member loses it (mvslovers/cc370#37).
 * A module that looks authorized and is not ends the job S047, and with an
 * empty SYSPRINT - stdio buffers are lost on an abend even after fflush(),
 * because the DCB is never closed.  An authorized program's WTO shows in the
 * job log WITHOUT the '+' prefix, which is how to tell the AC took.
 *
 * And a module fetched from the LNKLST cannot write its own writable statics -
 * see the note on check() below.
 *
 * BUILD (host) - note the AC=1 goes to ld370, not to the driver:
 *     cc370 -O1 -Iinclude -Wl,--ac,1 test/mvs/tstracau.c -o TSTRACAU \
 *           -flinker-output=xmit
 * Install: RECEIVE into a scratch library, then IEBCOPY the member into an
 *          APF-authorized one.  Do NOT RECEIVE over the APF library itself -
 *          RECEIVE will not merge into an existing PDS and wants the target
 *          deleted first, which for SYS2.LINKLIB would be catastrophic.
 *          Delete the member again when done; it has no business living in a
 *          system library.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdio.h>
#include "racf.h"
#include "cliblock.h"

#define CLASSNAME   "FACILITY"
#define RESOURCE    "LIBC370.TSTRACAU.NOSUCHRES"

/* 0 permitted, 4 not protected, 8 denied - all three mean the RACHECK ran
** and RAKF answered.  Which one comes back is policy, not our business. */
#define SAF_RC(rc)  ((rc) == 0 || (rc) == 4 || (rc) == 8)

/* NO writable statics in this file, and none in any probe that runs from a
** LNKLST library: such a module can read its statics but the first STORE into
** them is an S0C4.  Measured with this probe's first version, which died in
** "bad++" on the failure path - the path only the pre-fix run reaches, so it
** looked like a library defect until the dump said otherwise.  It is not the
** AC and not the RENT attribute: the same module stores fine through a
** STEPLIB (see doc/consumer-notes.md for the matrix).  It is also why libc370
** keeps per-program state on the heap via __wsaget().
**
** So the failure counter lives in main()'s frame and every check returns its
** verdict instead of accumulating one.  Lines are flushed as they are written,
** which helps an interactive reader but does NOT survive an abend - stdio
** buffers are lost with the unclosed DCB.  Use wtof() if you need a trace that
** outlives the failure. */
static int check(const char *what, int got, int want)
{
    int fail = (got != want);

    printf("  %-46s %-5d %s", what, got, fail ? "*** FAIL, expected " : "ok");
    if (fail) printf("%d", want);
    printf("\n");
    fflush(stdout);

    return fail;
}

int main(void)
{
    unsigned    *psa    = (unsigned *)0;
    unsigned    *ascb   = (unsigned *)psa[0x224/4];     /* A(ASCB) */
    unsigned    *asxb   = (unsigned *)ascb[0x6C/4];     /* A(ASXB) */
    ACEE        *acee;
    ACEE        *before;
    ACEE        *after;
    int         rc;
    int         held;
    int         bad     = 0;

    printf("TSTRACAU - libc370 #58 probe\n\n");

    acee = racf_get_acee();
    printf("  ASXBSENV ACEE = %08X, ASXB = %08X\n\n", (unsigned)acee,
           (unsigned)asxb);
    fflush(stdout);

    /* (1) the call itself must still work.  0 (permitted), 4 (not protected)
           and 8 (denied) are all SAF answers - which one depends on RAKF's
           policy, and this test says nothing about policy.  Anything else
           means the RACHECK did not run.  An S047 here is a setup error, not
           a library failure: the module is not APF-authorized. */
    rc = racf_auth(acee, CLASSNAME, RESOURCE, RACHECK_ATTR_READ);
    printf("  %-46s %-5d %s\n", "(1) racf_auth() with our own ACEE", rc,
           SAF_RC(rc) ? "ok" : "*** FAIL");
    fflush(stdout);
    bad += !SAF_RC(rc);

    /* (2) acee==NULL must keep working: the plist field stays zero and RAKF
           falls back to ASXBSENV, exactly as before the change. */
    rc = racf_auth(NULL, CLASSNAME, RESOURCE, RACHECK_ATTR_READ);
    printf("  %-46s %-5d %s\n", "(2) racf_auth(NULL) - ASXBSENV fallback", rc,
           SAF_RC(rc) ? "ok" : "*** FAIL");
    fflush(stdout);
    bad += !SAF_RC(rc);

    /* (3) THE REGRESSION GUARD.  Take the ASXB lock, call racf_auth(), and
           the lock must still be ours afterwards.  Before the fix racf_auth()
           ENQd (rc=8, ignored) and DEQd unconditionally, releasing it. */
    bad += check("(3a) lock(asxb) - expect acquired", lock(asxb, LOCK_EXC), 0);
    bad += check("(3b) testlock(asxb) - expect held by us",
                 testlock(asxb, LOCK_EXC), 8);

    racf_auth(acee, CLASSNAME, RESOURCE, RACHECK_ATTR_READ);

    held = testlock(asxb, LOCK_EXC);
    bad += check("(3c) testlock after racf_auth - STILL ours", held, 8);
    if (held != 8) {
        printf("       ^^ racf_auth() released the caller's ASXB lock (#58)\n");
        fflush(stdout);
    }

    /* (4) and it is still ours to release.  Before the fix this fails too -
           the DEQ inside racf_auth() already took it - which is the same
           defect showing up a second time, in the caller's cleanup path. */
    rc = unlock(asxb, LOCK_EXC);
    bad += check("(4a) unlock(asxb) - expect released by us", rc, 0);
    bad += check("(4b) testlock(asxb) - expect free",
                 testlock(asxb, LOCK_EXC), 0);

    /* (5) ASXBSENV must come back untouched.  Passes before AND after - the
           old code restored what it saved - so this discriminates nothing.
           It is a tripwire for a future change, not evidence for this one. */
    before = racf_get_acee();
    racf_auth(acee, CLASSNAME, RESOURCE, RACHECK_ATTR_READ);
    after  = racf_get_acee();
    bad += check("(5) ASXBSENV unchanged (tripwire, not proof)",
                 (int)(before == after), 1);

    printf("\nTSTRACAU %s\n", bad ? "FAILED" : "PASSED");
    printf("  (3) is the one that discriminates: a pre-fix library loses the\n");
    printf("      caller's ASXB lock inside racf_auth().\n");

    return bad ? 8 : 0;
}
