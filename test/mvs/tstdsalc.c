/*
 * tstdsalc.c - libc370 #43: __dsalc() reports through its return code, not
 * to the operator console (MVS target, batch).
 *
 * ISSUE #43: creating a data set that already exists - an ordinary outcome -
 * put three lines on the console: a wtof(), a 20-byte hex dump of the SVC 99
 * request block, and that once per attempt for a client that retries.  On
 * MVS 3.8j the console IS the SYSLOG.  The caller already knows whether the
 * failure was expected; the library does not.
 *
 * Case (6) is that scenario.  "No WTO" cannot be asserted from inside the
 * program, so the test brackets the failing call with two WTOs of its own -
 * which is the rule in action: the console belongs to the program.  In the
 * SYSLOG the two markers must be ADJACENT.  Before the fix, three lines from
 * __dsalc() sat between them.
 *
 * Cases (3)-(5) are the part that is a return code and can be checked here.
 * Removing the WTO from the three DISP= positions was not a deletion: an
 * unrecognized token used to be reported to the console while err stayed 0,
 * so the allocation went ahead with NO disposition text unit at all - and
 * SVC 99 then applied its own defaults.  All three returned 0 before the fix
 * and must return nonzero now.  A pre-fix library fails (3), (4) and (5) and
 * allocates a DD it was never asked for, which the test frees, so a red run
 * leaves nothing behind either.
 *
 * Every case works on the test's own data set, never on a system one: a
 * pre-fix run of case (3) allocates with no status text unit, and SVC 99's
 * default for that is exclusive.  Pointing it at SYS1.MACLIB would ENQ the
 * macro library of a running system to prove a point about a return code.
 *
 * SETUP: none.  The work data set is created by case (1) and deleted by (7);
 * it must NOT exist when the test starts.
 *
 * PARM='<dsn>'   (default IBMUSER.TSTDSALC.WORK)
 *
 * UNIT=SYSDA below is the esoteric name on the reference system - change it
 * if yours differs, the test says nothing about unit names.
 *
 * BUILD (host):
 *     cc370 -O1 -Iinclude test/mvs/tstdsalc.c -o TSTDSALC \
 *           -flinker-output=xmit
 * Install: RECEIVE the XMIT into the STEPLIB of jcl/tstdsalc.jcl.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include "clibio.h"     /* __dsalcf(), __dsfree() */
#include "clibwto.h"    /* wtof() - the markers of case (6) */

#define CREATE  "DSN=%s;DISP=(NEW,CATLG,DELETE);DSORG=PS;RECFM=FB;"          \
                "LRECL=80;BLKSIZE=3120;UNIT=SYSDA;SPACE=TRK(1,1)"

static int  bad = 0;
static char dd[9];

/* Expect success.  The DD is freed again, so the test leaves no allocation
   behind. */
static void expect_ok(const char *what, int rc)
{
    printf("  %-44s rc=%-5d %s\n", what, rc, rc ? "*** FAIL" : "ok");
    if (rc) bad++;
    else    __dsfree(dd);
}

/* Expect failure.  A pre-fix library returns 0 here AND has allocated, so
   free what it left behind before reporting the failure. */
static void expect_err(const char *what, int rc)
{
    printf("  %-44s rc=%-5d %s\n", what, rc, rc ? "ok" : "*** FAIL");
    if (!rc) {
        bad++;
        __dsfree(dd);
        printf("  %-44s allocated anyway - freed dd '%s'\n", "", dd);
    }
}

int main(int argc, char **argv)
{
    const char *dsn = "IBMUSER.TSTDSALC.WORK";
    char        buf[45];
    int         rc;

    if (argc > 1 && argv[1] && argv[1][0] > ' ') {
        unsigned i;

        for (i = 0; i < sizeof(buf) - 1 && argv[1][i] > ' '; i++) {
            buf[i] = argv[1][i];
        }
        buf[i] = 0;
        dsn = buf;
    }

    printf("TSTDSALC - libc370 #43 probe, work dsn '%s'\n\n", dsn);

    /* (1) create the work data set - the free below catalogs it */
    rc = __dsalcf(dd, CREATE, dsn);
    expect_ok("(1) create the work data set", rc);
    if (rc) {
        printf("\nTSTDSALC FAILED - nothing to test against\n");
        return 8;
    }

    /* (2) the plain case, so a failure below means what it says */
    rc = __dsalcf(dd, "DSN=%s;DISP=SHR", dsn);
    expect_ok("(2) allocate it SHR", rc);

    /* (3)-(5) an unrecognized token in each of the three DISP= positions.
       All three returned 0 before the fix. */
    rc = __dsalcf(dd, "DSN=%s;DISP=(BOGUS)", dsn);
    expect_err("(3) DISP=(BOGUS)          expect nonzero", rc);

    rc = __dsalcf(dd, "DSN=%s;DISP=(SHR,PASS)", dsn);
    expect_err("(4) DISP=(SHR,PASS)       expect nonzero", rc);

    rc = __dsalcf(dd, "DSN=%s;DISP=(SHR,KEEP,PASS)", dsn);
    expect_err("(5) DISP=(SHR,KEEP,PASS)  expect nonzero", rc);

    /* (6) the issue's own scenario: create it a second time.  What matters
           is not only the rc - it is that the SYSLOG shows the two markers
           below with NOTHING between them. */
    wtof("TSTDSALC: quiet window opens - nothing may follow until it closes");
    rc = __dsalcf(dd, CREATE, dsn);
    wtof("TSTDSALC: quiet window closes - __dsalc() rc=%d", rc);
    expect_err("(6) create it again       expect nonzero", rc);

    /* (7) clean up: OLD,DELETE and let the free do it */
    rc = __dsalcf(dd, "DSN=%s;DISP=(OLD,DELETE)", dsn);
    expect_ok("(7) delete the work data set", rc);

    printf("\nTSTDSALC %s\n", bad ? "FAILED" : "PASSED");
    printf("  (6) also needs an eye on the SYSLOG: the two TSTDSALC markers\n");
    printf("      must be adjacent.  Three lines between them = #43 is back.\n");

    return bad ? 8 : 0;
}
