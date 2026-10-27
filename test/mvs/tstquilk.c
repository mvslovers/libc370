/*
 * tstquilk.c - libc370 #714 on MVS: what the console shows when a job is
 * cancelled while a task sits inside __linkds() / __linkdsx(), and that an
 * abend in the called program is reported as before.
 *
 * PARM selects the case.  A and B start a subtask that LINKs TSTQUIIN
 * (role W: waits) and wait for the operator, who cancels the job
 * (C jobname); the step task ends S222 and RTM runs the recovery routines
 * of the subtask with SDWACLUP on (cleanup only).
 *
 *   A  subtask: __linkds("TSTQUIIN", W)       expected: no "libc370 recovery:"
 *   B  subtask: __linkdsx("TSTQUIIN", W)      expected: no "libc370 recovery:"
 *
 * X and Y need no cancel.  The step task LINKs TSTQUIIN (role X: S0C1) and
 * ends RC 0; the abend report must be the same on every build.
 *
 *   X  __linkds("TSTQUIIN", X)                expected: "ABEND S0C1 detected",
 *                                             rc S0C1 (X'000C1000')
 *   Y  __linkdsx("TSTQUIIN", X)               expected: the same
 *
 * The verdict is the job log (JESMSGLG / SYSLOG).  Count the lines
 * "libc370 recovery:" for A and B, compare the "ABEND S0C1 detected ..." and
 * "PSW:" lines for X and Y with the same job on a build of main:
 *
 *   TSTQUILK A: "libc370 recovery:" 0   (red build of main: 1)
 *   TSTQUILK B: "libc370 recovery:" 0   (red build of main: 1)
 *   TSTQUILK X, Y: same lines on both builds
 *
 * BUILD (from the repository root, after make; S a scratch directory,
 * MAIN a build of origin/main for the red control).  The outer program
 * LINKs the member named by the macro INNER:
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstquiin.c \
 *         -o $S/TSTQUIIG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstquilk.c \
 *         -o $S/TSTQUILG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -I$MAIN/include -L $MAIN/build/sdk \
 *         test/mvs/tstquiin.c -o $S/TSTQUIIR -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -I$MAIN/include -L $MAIN/build/sdk \
 *         -DINNER='"TSTQUIIR"' test/mvs/tstquilk.c -o $S/TSTQUILR \
 *         -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTQUIIG=TSTQUIIG.iebcopy \
 *         TSTQUILG=TSTQUILG.iebcopy TSTQUIIR=TSTQUIIR.iebcopy \
 *         TSTQUILR=TSTQUILR.iebcopy -o qui -xmit --dsn IBMUSER.LIBC370.QUISCR
 *
 * then jcl/recvquie.jcl, and jcl/tstquilk.jcl once per PARM and library
 * (8 jobs); A and B are cancelled about ten seconds after they start.
 */
#include <stdio.h>
#include <unistd.h>
#include <mvs/wto.h>
#include <mvs/ecb.h>
#include <mvs/thread.h>
#include <mvs/link.h>

#ifndef INNER
#define INNER "TSTQUIIG"
#endif

static ECB  mainecb;        /* never posted: the step task waits here */
static char role;

static int link_inner(char inrole)
{
    struct { unsigned short len; char text[2]; } parm;
    unsigned    plist[1];
    unsigned    abcode  = 0;
    int         prc     = -1;
    int         rc;

    parm.len     = 1;
    parm.text[0] = inrole;
    plist[0]     = (unsigned)&parm | 0x80000000;

    if (role == 'B' || role == 'Y') {
        rc = __linkdsx(INNER, 0, plist, &prc, &abcode);
    }
    else {
        rc = __linkds(INNER, 0, plist, &prc);
    }
    wtof("TSTQUILK %c link rc=%08X prc=%d abcode=%08X", role, (unsigned)rc,
         prc, abcode);
    return rc;
}

static int subtask(void *arg1, void *arg2)
{
    (void)arg1;
    (void)arg2;
    link_inner('W');
    return 0;
}

int main(int argc, char **argv)
{
    CTHDTASK    *t;

    if (argc < 2 || (argv[1][0] != 'A' && argv[1][0] != 'B' &&
                     argv[1][0] != 'X' && argv[1][0] != 'Y')) {
        wtof("TSTQUILK PARM must be A, B, X or Y");
        return 8;
    }
    role = argv[1][0];

    if (role == 'X' || role == 'Y') {
        link_inner('X');
        return 0;
    }

    t = cthread_create(subtask, 0, 0);
    if (!t) {
        wtof("TSTQUILK %c cthread_create failed", role);
        return 8;
    }
    sleep(3);                                   /* the subtask is waiting by then */
    wtof("TSTQUILK %c READY: cancel the job", role);
    ecb_wait(&mainecb);
    return 0;
}
