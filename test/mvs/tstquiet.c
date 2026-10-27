/*
 * tstquiet.c - libc370 #712 on MVS: what the console shows when a job is
 * cancelled while its subtask sits in a recovery routine.
 *
 * The job step starts one subtask and waits for the operator.  The
 * operator cancels the job (C jobname): the step task ends S222, RTM
 * ends the subtask and runs its recovery routines with SDWACLUP on
 * (cleanup only).  PARM selects the subtask:
 *
 *   A  abendrpt(ESTAE_CREATE, DUMP_DEFAULT | ABRPT_QUIET_TERM), waits
 *      expected: no "libc370 recovery:" line
 *   B  try() inside try() around the wait, no abendrpt()
 *      expected: no "@@@try.c failed()" line (two before #712)
 *   C  abendrpt(ESTAE_CREATE, DUMP_DEFAULT), waits
 *      expected: one "libc370 recovery:" line still
 *
 * The verdict is the job log of the cancelled job, so this program has no
 * return code of its own: the step ends S222.  Count the lines in
 * JESMSGLG / SYSLOG:
 *
 *   TSTQUIET A: "libc370 recovery:" 0   "@@@try.c" 0
 *   TSTQUIET B: "libc370 recovery:" 0   "@@@try.c" 0
 *   TSTQUIET C: "libc370 recovery:" 1   "@@@try.c" 0
 *
 * A library before #712 writes one "libc370 recovery:" line for A and one
 * for C, and two "@@@try.c" lines for B.  The program also builds against
 * a header without ABRPT_QUIET_TERM, for that red control.
 *
 * BUILD (from the repository root, after make; S a scratch directory,
 * MAIN a build of origin/main for the red control):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstquiet.c \
 *         -o $S/TSTQUIG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -I$MAIN/include -L $MAIN/build/sdk \
 *         test/mvs/tstquiet.c -o $S/TSTQUIR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTQUIG=TSTQUIG.iebcopy \
 *         TSTQUIR=TSTQUIR.iebcopy -o qui -xmit --dsn IBMUSER.LIBC370.QUISCR
 *
 * then jcl/recvquie.jcl, and jcl/tstquiet.jcl once per PARM and library
 * (6 jobs), each cancelled when it writes "TSTQUIET x READY".
 */
#include <stdio.h>
#include <unistd.h>
#include <mvs/wto.h>
#include <mvs/ecb.h>
#include <mvs/thread.h>
#include <mvs/recovery.h>

#ifndef ABRPT_QUIET_TERM
#define ABRPT_QUIET_TERM 0x100
#endif

static ECB  subecb;         /* never posted: the subtask waits here */
static ECB  mainecb;        /* never posted: the step task waits here */
static char role;

static int wait_in_try(void *unused)
{
    (void)unused;
    ecb_wait(&subecb);
    return 0;
}

static int try_inside_try(void *unused)
{
    (void)unused;
    try(wait_in_try, 0);
    return 0;
}

static int subtask(void *arg1, void *arg2)
{
    (void)arg1;
    (void)arg2;

    switch (role) {
    case 'A':
        abendrpt(ESTAE_CREATE, DUMP_DEFAULT | ABRPT_QUIET_TERM);
        ecb_wait(&subecb);
        break;
    case 'B':
        try(try_inside_try, 0);
        break;
    case 'C':
        abendrpt(ESTAE_CREATE, DUMP_DEFAULT);
        ecb_wait(&subecb);
        break;
    }
    return 0;
}

int main(int argc, char **argv)
{
    CTHDTASK    *t;

    if (argc < 2 || (argv[1][0] != 'A' && argv[1][0] != 'B' &&
                     argv[1][0] != 'C')) {
        wtof("TSTQUIET PARM must be A, B or C");
        return 8;
    }
    role = argv[1][0];

    t = cthread_create(subtask, 0, 0);
    if (!t) {
        wtof("TSTQUIET %c cthread_create failed", role);
        return 8;
    }
    sleep(3);                                   /* the subtask is waiting by then */
    wtof("TSTQUIET %c READY: cancel the job", role);
    ecb_wait(&mainecb);
    return 0;
}
