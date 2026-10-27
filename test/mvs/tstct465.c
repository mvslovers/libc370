/*
 * tstct465.c - libc370 #465 on MVS: a subtask that ends while cthread_create()
 * is still returning, on a machine with more than one CPU.
 *
 * ISSUE #465: tstctwd (create, cthread_wait(termecb), cthread_delete) ended
 * ABEND S43E once in 26 runs.  ATTACH starts the subtask at once; with a
 * second CPU it can run, and end, before the owner has stored ATTACH's
 * results.  attach() kept the ATTACH return code in task->rc, which is also
 * where the driver stores the thread function's return code (CTHDRC), so
 *
 *   - the owner's "ST 15,task->rc" overwrote the thread's return code, and
 *   - the driver's store landing between that store and attach()'s read of
 *     task->rc made cthread_create_ex() see a nonzero "ATTACH rc", call it
 *     a failed ATTACH, and free the CTHDTASK -- ECB, stack and all -- under
 *     a subtask that was still ending.  MVS then posted the freed ECB at
 *     the subtask's end: S43E on the owner (or SA03, S0C4 ...).
 *
 * The same race hid a second one: task->tcb is stored by the owner after
 * ATTACH too, so a thread function that asked cthread_self() first thing got
 * NULL.  The driver now stores its own TCB address before it calls the
 * function.
 *
 * The program creates a subtask LOOPS times (PARM = LOOPS, default 300).
 * The thread function returns 9 when cthread_self() finds its handle, 7
 * when it does not.  Per round: cthread_create() returns a handle, the
 * return code that comes back through cthread_wait(&task->termecb) /
 * task->rc is 9, and cthread_delete() frees the handle.
 *
 * It can fail only with two CPUs (MVS/CE stock has two; the mvsce-target
 * image runs one unless started with MVSCE_NUMCPU=2).  On one CPU it
 * passes with or without the fix: the subtask is dispatched only when the
 * owner waits.
 *
 * Built twice: TSTC465 against this tree's libc.a, TSTC465R against a
 * libc.a built from main before the fix - the red control.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Iinclude -L build/sdk \
 *                test/mvs/tstct465.c -o TSTC465 -flinker-output=iebcopy
 *          (TSTC465R: the same from a tree without the fix)
 *          ld370 --pack TSTC465=TSTC465.iebcopy TSTC465R=TSTC465R.iebcopy \
 *                -o tstct465 -xmit --dsn IBMUSER.LIBC370.C465SCR
 * Install: jcl/recvct465.jcl.   Run: jcl/tstct465.jcl.
 *
 * (mvs/thread.h includes a header that fails -Werror on #pragma pack,
 * #415, so no -Werror here.)
 *
 * RC: 0 = every round passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <stdlib.h>
#include <mvs/thread.h>

static int worker(void *a, void *b)
{
    (void)a;
    (void)b;
    return cthread_self() != NULL ? 9 : 7;
}

int main(int argc, char **argv)
{
    int loops = argc > 1 ? atoi(argv[1]) : 300;
    int n;
    int nocreate = 0;
    int badrc = 0;
    int noself = 0;
    int stuck = 0;
    int badfree = 0;

    printf("=== tstct465: %d x create, wait, delete (#465) ===\n", loops);

    for (n = 0; n < loops; n++) {
        CTHDTASK *task = cthread_create((void *)worker, NULL, NULL);
        int post;
        int rc;

        if (task == NULL) {
            nocreate++;
            printf("  round %d: cthread_create() returned NULL\n", n);
            fflush(stdout);
            continue;
        }

        post = cthread_wait(&task->termecb);
        rc = task->rc;
        if (rc == 7)
            noself++;
        else if (rc != 9)
            badrc++;
        if (post != 9 && post != 7)
            stuck++;
        if (rc != 9 || post != 9) {
            printf("  round %d: wait=%d task->rc=%d\n", n, post, rc);
            fflush(stdout);
        }

        cthread_delete(&task);
        if (task != NULL)
            badfree++;
    }

    printf("  create failed: %d, thread rc wrong: %d, cthread_self() NULL: %d,"
           " ECB code wrong: %d, handle kept: %d\n",
           nocreate, badrc, noself, stuck, badfree);
    printf("=== tstct465: %s ===\n",
           nocreate + badrc + noself + stuck + badfree ? "FAILED" : "passed");
    return nocreate + badrc + noself + stuck + badfree ? 1 : 0;
}
