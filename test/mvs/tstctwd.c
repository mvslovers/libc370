/*
 * tstctwd.c - libc370 #431 on MVS: cthread_wait() on the termination ECB,
 * then cthread_delete(), frees the finished subtask.
 *
 * ISSUE #431 (from mvslovers/httpd#262): cthread_wait() clears the ECB it
 * waited on, and cthread_delete()/cthread_detach() took "ended" from the
 * posted bit of that very ECB.  So after cthread_wait(&task->termecb) the
 * delete refused ("has not ended, task and stack retained"), the subtask
 * was never DETACHed, and the step ended ABEND SA03.  The library now
 * also asks the TCB: TCBFC (TCBFLGS5 X'80', TCB+X'21', "task terminated").
 *
 * Built twice: TSTCTWD against this tree's libc.a, TSTCTWDR against the
 * installed sysroot libc.a (2.3.0) - the red control, which is expected to
 * end ABEND SA03.  Both print the TCB flags after the wait.
 *
 * Build:   make build
 *          cc370 -O1 -Wall -Iinclude -L build/sdk \
 *                test/mvs/tstctwd.c -o TSTCTWD -flinker-output=iebcopy
 *          cc370 -O1 -Wall -Iinclude \
 *                test/mvs/tstctwd.c -o TSTCTWDR -flinker-output=iebcopy
 *          ld370 --pack TSTCTWD=TSTCTWD.iebcopy TSTCTWDR=TSTCTWDR.iebcopy \
 *                -o tstctwd -xmit --dsn IBMUSER.LIBC370.CTWSCR
 * Install: jcl/recvctw.jcl.   Run: jcl/tstctwd.jcl.
 *
 * mvsdev JOB01388, 2026-10-05 (RECEIVE JOB01387): GREEN CC 0000, 4/4;
 * RED (installed 2.3.0) "has not ended", 3/4, ABEND SA03.  After the
 * wait termecb is 00000000 while TCBFLGS5 is X'80' (TCBFC) in both.
 *
 * (mvs/thread.h includes a header that fails -Werror on #pragma pack,
 * #415, so no -Werror here.)
 *
 * RC: 0 = every check passed, 1 = a check failed (it is the COND CODE).
 */
#include <stdio.h>
#include <mvs/thread.h>

static int mbt_run = 0, mbt_passed = 0, mbt_failed = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        mbt_run++;                                                        \
        if (cond) { mbt_passed++; printf("  PASS: %s\n", (msg)); }        \
        else      { mbt_failed++; printf("  FAIL: %s\n", (msg)); }        \
    } while (0)

static int worker(void *a, void *b)
{
    (void)a;
    (void)b;
    return 9;
}

int main(void)
{
    CTHDTASK *task;
    unsigned char *tcb;
    int post;
    int rc;

    printf("=== tstctwd: cthread_wait(termecb), then cthread_delete (#431) ===\n");

    task = cthread_create((void *)worker, NULL, NULL);
    CHECK(task != NULL, "cthread_create() attached the subtask");
    if (task == NULL)
        goto done;

    post = cthread_wait(&task->termecb);
    tcb = (unsigned char *)task->tcb;
    printf("  cthread_wait() = %d, termecb now %08X, rc %d\n",
           post, task->termecb, task->rc);
    if (tcb != NULL)
        printf("  TCB %06X: TCBFLGS1 %02X, TCBFLGS5 %02X (TCBFC is X'80')\n",
               (unsigned)tcb, tcb[0x1D], tcb[0x21]);
    CHECK(tcb != NULL && (tcb[0x21] & 0x80), "the TCB says the task terminated");

    rc = task->rc;
    CHECK(rc == 9, "the thread function's return code came back");

    cthread_delete(&task);
    CHECK(task == NULL, "cthread_delete() freed the finished subtask");

done:
    printf("=== tstctwd: %d/%d passed", mbt_passed, mbt_run);
    if (mbt_failed > 0)
        printf(" (%d FAILED)", mbt_failed);
    printf(" ===\n");
    return (mbt_failed > 0 ? 1 : 0);
}
