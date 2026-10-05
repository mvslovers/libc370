/* @@CTDET.C - cthread_detach()
** detach a thread (subtask)
*/
#include "src/internal/fileio.h"
#include "mvs/thread.h"

static int detach(CTHDTASK *task);

/* Has the subtask ended?  MVS posts the ATTACH ECB (termecb) at task end,
** but cthread_wait() clears the ECB it waited on, so after
** cthread_wait(&task->termecb) that bit is gone.  The TCB, which stays until
** the DETACH, still says so: TCBFLGS5 TCBFC, X'80' at TCB+X'21' (#431). */
static int
ended(const CTHDTASK *task)
{
    const unsigned char *tcb = (const unsigned char *)task->tcb;

    if (task->termecb & 0x40000000) return 1;
    return tcb != NULL && (tcb[0x21] & 0x80) != 0;
}

__asm__("\n&FUNC    SETC 'cthread_detach'");
int
cthread_detach(CTHDTASK *task)
{
    int     rc = 0;
#if 0
    char    buf[256] = "";

    __caller(buf);
    wtof("__ctdet called by %s", buf);
#endif
    if (!task) goto quit;
    if (!task->tcb) goto quit;

    /* Never DETACH a subtask that has not ended.
    **
    ** DETACH ...,STAE=YES abnormally terminates a live subtask (S33E), and
    ** every caller in this library goes on to free the CTHDTASK -- whose
    ** allocation CONTAINS that subtask's stack (@@ctcrtx.c newthread:
    ** calloc(1, sizeof(CTHDTASK) + newstack)).  The recovery exit the S33E
    ** drives then runs on storage being reclaimed underneath it, which is the
    ** nested fault #9 had to harden against.  One free-while-running, not two
    ** bugs (#11).
    **
    ** termecb is the ATTACH ECB= that MVS posts at task end (@@ctcrtx.c
    ** attach); after cthread_wait() cleared it, the TCB's own TCBFC flag
    ** says the same (ended() above, #431).
    **
    ** No owner check on top of this, deliberately: MVS already refuses a
    ** DETACH issued by anything but the attaching task, and turning that loud
    ** failure into a silent skip here would hide a real defect rather than
    ** prevent one.
    */
    if (!ended(task)) {
        rc = CTHREAD_DETACH_LIVE;
        goto quit;
    }

    /* we use try() to catch any DETACH failures */
    rc = try(detach, task);

    if (rc==0) rc = task->rc;
    task->tcb = 0;

quit:
    return rc;
}

__asm__("\n&FUNC    SETC 'detach'");
static int
detach(CTHDTASK *task)
{
    void    *tcb = (void*)(task->tcb);
#if 0
    wtof("Issuing DETACH for task=%08X, TCB=%08X", task, task->tcb);
#endif
    __asm("DS\t0H\n\t"
        "DETACH (%1),STAE=YES   detach the subtask\n\t"
        "ST\t15,%0          save the return code"
        : "=m"(task->rc) : "r"(&tcb) : "14","15","0","1" , "memory");
#if 0
    wtof("Return from DETACH for task=%08X, TCB=%08X, RC=%d", task, task->tcb, task->rc);
#endif
    task->tcb = 0;

    return task->rc;
}
