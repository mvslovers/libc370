/*
 * tstthrdm.c - libc370 #403 on MVS: mtxfree() and the thread helpers.
 *
 *   1 mtxfree() of a mutex this task holds releases its ENQ: a second
 *     thread can then take a new mutex at the same address (the old one
 *     DEQed the LOCK name, so the MUTEX ENQ stayed held)
 *   2 mtxfree(NULL) returns (it stored into the PSA: S0C4)
 *   3 a thread with a stack of exactly 1 MB runs a function that uses
 *     8 KB of it, and cthread_detach() returns 0 while task->rc keeps the
 *     function's value (the stack was masked to 0 bytes, and DETACH's
 *     R15 was stored over the rc)
 *   4 cthread_queue_add() on a quiescing manager returns -1 (it returned 0)
 *
 * __ascb() and the highest ASID (also #403) is not probed: the highest
 * ASID is seldom in use, so both versions return NULL for it.
 *
 * CASE=0 runs 1, 3 and 4 (this tree); CASE=2 case 2 alone; the red builds
 * against the installed library run one case each, CASE=1..4.
 *
 * BUILD (from the repository root, after make; S a scratch directory):
 *
 *     cc370 -O1 -Wall -Werror -DCASE=0 -Iinclude -L build/sdk \
 *         test/mvs/tstthrdm.c -o $S/TSTTHRDG -flinker-output=iebcopy
 *     (CASE=2 as TSTTHRG2; CASE=1..4 without -Iinclude/-L as TSTTHRR1..4)
 *     cd $S && ld370 --pack TSTTHRDG=TSTTHRDG.iebcopy ... -o th -xmit \
 *         --dsn IBMUSER.LIBC370.THSCR
 *
 * then jcl/recvthrdm.jcl and jcl/tstthrdm.jcl.
 *
 * RC 0 = every case passed, 8 = one did not.
 */
#include <stdlib.h>
#include <string.h>
#include <mvs/wto.h>
#include <mvs/mutex.h>
#include <mvs/thread.h>
#include <mvs/ecb.h>

#ifndef CASE
#define CASE 0
#endif

static int bad;

static void want(int cond, const char *what)
{
    wtof("TSTTHRDM %s: %s", cond ? "PASS" : "FAIL", what);
    if (!cond) bad++;
}

/* run fn(arg) on a thread of the given stack size: its rc as the thread
   left it, or -99; *detrc is cthread_detach()'s result and *after
   task->rc once detached */
__attribute__((unused))
static int on_thread(void *fn, void *arg, unsigned stack, int *detrc,
                     int *after)
{
    CTHDTASK    *task = cthread_create_ex(fn, arg, NULL, stack);
    int         rc;

    if (!task) return -99;
    ecb_wait(&task->termecb);
    rc = task->rc;
    *detrc = cthread_detach(task);
    *after = task->rc;
    cthread_delete(&task);
    return rc;
}

__attribute__((unused))
static int trymtx(void *a1, void *a2)
{
    CLIBMUTX    *m = a1;
    int         rc = mtxtry(m);

    (void)a2;
    if (rc == 0) mtxunlk(m);
    return rc;
}

__attribute__((unused))
static int deep(void *a1, void *a2)
{
    volatile char   buf[8192];
    int             i, sum = 0;

    (void)a1; (void)a2;
    for (i = 0; i < (int)sizeof(buf); i++) buf[i] = (char)i;
    for (i = 0; i < (int)sizeof(buf); i++) sum += buf[i];
    return sum == 0 ? 0 : 7;
}

int main(void)
{
    int det = 0, after = 0;

    (void)det; (void)after;

#if CASE == 0 || CASE == 1
    {
        CLIBMUTX    *m = mtxnew();
        CLIBMUTX    *m2;
        int         rc;

        mtxlock(m);
        mtxfree(m);
        m2 = mtxnew();
        if (m2 != m) wtof("TSTTHRDM new mutex at %08X, old %08X",
                          (unsigned)m2, (unsigned)m);
        rc = on_thread(trymtx, m2, 0, &det, &after);
        want(m2 == m && rc == 0,
             "1 after mtxfree() another thread takes the same mutex");
        mtxfree(m2);
    }
#endif
#if CASE == 2
    mtxfree(NULL);
    want(1, "2 mtxfree(NULL) returns");
#endif
#if CASE == 0 || CASE == 3
    {
        int rc = on_thread(deep, NULL, 0x100000, &det, &after);

        want(rc == 7, "3 a 1 MB stack runs, the function returns 7");
        want(det == 0 && after == 7,
             "3 cthread_detach() returns 0, task->rc stays 7");
    }
#endif
#if CASE == 0 || CASE == 4
    {
        CTHDMGR *mgr = calloc(1, sizeof(CTHDMGR));
        int     rc;

        strcpy(mgr->eye, CTHDMGR_EYE);
        mgr->state = CTHDMGR_STATE_QUIESCE;
        rc = cthread_queue_add(mgr, "X");
        want(rc == -1, "4 cthread_queue_add() on a quiescing manager is -1");
        free(mgr);
    }
#endif
    wtof("TSTTHRDM VERDICT failed=%d", bad);
    return bad ? 8 : 0;
}
