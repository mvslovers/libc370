/*
 * tstlstvl.c - libc370 #59: the "unable to open" diagnostic in open_vatlst()
 * (src/clib/@@listvl.c) must name the data set it could not open (MVS target,
 * batch).
 *
 * ISSUE #59: the message had TWO %s conversions and ONE argument:
 *
 *     wtof("@@listvl:%s: unable to open \"%s\"", __func__);
 *
 * vwtof() -> vsprintf() walks off the end of the argument list for the second
 * one and formats whatever the varargs area holds as a char *.  In the
 * generated code the parameter list was two words long and vsprintf read a
 * third, from a slot in the save area nothing had stored into.  Best case the
 * message carries garbage; worst case the dereference is an S0C4 - in the
 * handler for a failure that had just been detected and was about to be
 * reported cleanly by returning NULL.  The line runs only when fopen() has
 * already failed, i.e. on a misconfigured system: the code that can turn a
 * recoverable "no VATLST, carry on without comments" into an abend is the code
 * that only ever runs when something is already wrong.
 *
 * ====================================================================
 * WHERE THE VERDICT IS - READ THIS BEFORE TRUSTING THE COND CODE
 * --------------------------------------------------------------------
 * The message text is NOT observable from inside the program: a WTO goes to
 * the console, and the console does not come back.  The COND CODE here proves
 * only the things that ARE observable - that the call returned, that a missing
 * VATLST does not cost the caller its volume list, and that no comment was
 * invented.  Pre-fix those hold too, unless the bad pointer happens to abend,
 * which depends on what is in storage and is not a gate anyone should rely on.
 *
 * THE ACTUAL CHECK IS IN THE JOB LOG.  Each case brackets the call with two
 * WTOs of its own - the same technique as test/mvs/tstdsalc.c case (6), and it
 * works from an ordinary job submit because JESMSGLG carries the WTOs the job
 * issued.  Between the markers of cases (1) and (2) there must be exactly one
 * line, and it must contain the data set name printed below it in SYSPRINT.
 * Case (3) is the control: between ITS markers there must be nothing at all.
 *
 *     zowe jobs submit lf jcl/tstlstvl.jcl --wait-for-output --directory out
 *     grep -A2 'TSTLSTVL: (1)' out/<jobid>/JES2/JESMSGLG.txt
 * ====================================================================
 *
 * SETUP: none.  Every case names a data set that must NOT exist; nothing is
 * created, allocated or deleted.  __listvl() with dolspace=0 issues no LSPACE,
 * so the only line the library can write on these paths is the one under test.
 *
 * PARM='<vatlst>'  optional.  A VATLST that DOES exist (member name, or
 * dsn(member)) adds case (4): the success path must still open it, put nothing
 * on the console, and return at least one comment.  Without a PARM, case (4)
 * is skipped - the reference system is not assumed to have a VATLST.
 *
 * BUILD (host):
 *     cc370 -O1 -Iinclude test/mvs/tstlstvl.c -o TSTLSTVL \
 *           -flinker-output=xmit
 * Install: RECEIVE the XMIT into the STEPLIB of jcl/tstlstvl.jcl.
 *
 * RC: 0 = every check passed, 8 = at least one did not (it is the COND CODE).
 */
#include <stdio.h>
#include <string.h>
#include "clibary.h"    /* array_count() over the returned VOLLIST array */
#include "cliblist.h"   /* __listvl(), __freevl(), VOLLIST */
#include "clibwto.h"    /* wtof() - the markers */

static int bad = 0;

static void check(const char *what, int ok)
{
    printf("  %-52s %s\n", what, ok ? "ok" : "*** FAIL");
    if (!ok) bad++;
}

/* How many entries carry a comment?  A VATLST that could not be opened must
   leave every one of them NULL. */
static unsigned commented(VOLLIST **v)
{
    unsigned n, count, found = 0;

    if (!v) return 0;
    count = array_count(&v);
    for (n = 0; n < count; n++) {
        if (v[n] && v[n]->comment) found++;
    }
    return found;
}

/* A VATLST that cannot be opened: the volumes must still come back, and no
   comment may appear.  The diagnostic itself is between the markers. */
static void expect_diagnostic(int no, const char *vatlst, const char *expect)
{
    VOLLIST **v;
    unsigned count;
    char     what[54];

    wtof("TSTLSTVL: (%d) window opens - one line follows, naming the data set",
         no);
    v = __listvl(NULL, 0, vatlst);
    wtof("TSTLSTVL: (%d) window closes", no);

    /* Reaching this line is itself a result: pre-fix the diagnostic formats a
       pointer it was never passed, and a dereference of it is an S0C4. */
    count = v ? array_count(&v) : 0;

    sprintf(what, "(%d) vatlst=\"%.20s\" survived the diagnostic", no, vatlst);
    check(what, 1);

    sprintf(what, "(%d) volume list came back anyway", no);
    check(what, v != NULL && count > 0);

    sprintf(what, "(%d) no comment invented", no);
    check(what, commented(v) == 0);

    printf("      job log, between the (%d) markers, must contain:\n", no);
    printf("        %s\n", expect);

    if (v) __freevl(&v);
}

int main(int argc, char **argv)
{
    const char *vatlst = NULL;
    char        parm[45];
    VOLLIST   **v;
    unsigned    count;

    if (argc > 1 && argv[1] && argv[1][0] > ' ') {
        unsigned i;

        for (i = 0; i < sizeof(parm) - 1 && argv[1][i] > ' '; i++) {
            parm[i] = argv[1][i];
        }
        parm[i] = 0;
        vatlst = parm;
    }

    printf("TSTLSTVL - libc370 #59 probe\n\n");

    /* (1) member-name form.  Eight characters or fewer, no '.' and no '(',
           so open_vatlst() builds SYS1.PARMLIB(NOSUCHM) itself - which is the
           name the message has to carry, not the name it was handed. */
    expect_diagnostic(1, "NOSUCHM",
                      "open_vatlst: unable to open \"SYS1.PARMLIB(NOSUCHM)\"");

    /* (2) full dsn(member) form, taken as given. */
    expect_diagnostic(2, "IBMUSER.NOSUCH.PARMLIB(NOSUCHM)",
                      "open_vatlst: unable to open "
                      "\"IBMUSER.NOSUCH.PARMLIB(NOSUCHM)\"");

    /* (3) control: no VATLST asked for at all.  open_vatlst() is never
           called, so the window must be EMPTY.  Without this a job log full
           of diagnostics would look the same as one line in the right
           place. */
    wtof("TSTLSTVL: (3) quiet window opens - nothing may follow until it closes");
    v = __listvl(NULL, 0, NULL);
    wtof("TSTLSTVL: (3) quiet window closes");
    count = v ? array_count(&v) : 0;
    check("(3) vatlst=NULL: volume list returned", v != NULL && count > 0);
    check("(3) vatlst=NULL: no comment", commented(v) == 0);
    printf("      job log, between the (3) markers: NOTHING\n");
    if (v) __freevl(&v);

    /* (4) optional: a VATLST that exists.  The success path must stay silent
           and still deliver comments. */
    if (vatlst) {
        wtof("TSTLSTVL: (4) quiet window opens - a VATLST that exists");
        v = __listvl(NULL, 0, vatlst);
        wtof("TSTLSTVL: (4) quiet window closes");
        count = v ? array_count(&v) : 0;
        printf("  (4) vatlst=\"%s\": %u volume(s), %u with a comment\n",
               vatlst, count, commented(v));
        check("(4) volume list returned", v != NULL && count > 0);
        check("(4) at least one comment came back", commented(v) > 0);
        printf("      job log, between the (4) markers: NOTHING\n");
        if (v) __freevl(&v);
    }
    else {
        printf("  (4) skipped - no PARM, so no VATLST known to exist\n");
    }

    printf("\nTSTLSTVL %s\n", bad ? "FAILED" : "PASSED");
    printf("  The COND CODE does not decide #59. Read the job log: one line\n");
    printf("  between the (1) and (2) markers, each naming its data set, and\n");
    printf("  nothing between the (3) markers.\n");

    return bad ? 8 : 0;
}
