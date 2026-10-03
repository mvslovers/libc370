#include <mvs/timer.h>

__asm__("\n&FUNC    SETC 'tmr_stop'");
int tmr_stop(void)
{
    TMR         *tmr    = tmr_get();
    int         lockrc;
    int         i;
    CTHDTASK    *task;

    if (!tmr) return -1;        /* no TMR anchor: no timer services (#85) */

    /* initialize the timer handle if needed */
    tmr_init();

    /* if we have a timer thread that is running, quiesce it */
    lockrc = lock(tmr, 0);
    if (tmr->task && (tmr->flags & TMR_FLAG_RUNNING)) {
        wtof("%s QUIESCE posted", __func__);
        tmr->flags |= TMR_FLAG_QUIESCE;
        ecb_post(&tmr->wakeup, 0);
    }
    if (lockrc==0) unlock(tmr, 0);

    /* if we have a timer thread that is quiesced, shut it down */
    lockrc = lock(tmr, 0);
    if (tmr->task && (tmr->flags & TMR_FLAG_RUNNING)) {
        wtof("%s SHUTDOWN posted", __func__);
        tmr->flags |= TMR_FLAG_SHUTDOWN;
        ecb_post(&tmr->wakeup, 0);
    }
    if (lockrc==0) unlock(tmr, 0);

    /* Wait, bounded, for the timer thread to end before deleting it.  It
    ** ends promptly once it sees SHUTDOWN, but it has to run to see it,
    ** and cthread_delete() refuses a subtask that has not posted termecb
    ** (#11).  Deleting at once used to find it still alive every time:
    ** the delete was refused, the handle dropped regardless, and the
    ** program then returned with the subtask attached - ABEND SA03
    ** (#345, mvsdev JOB01194).  The wait runs outside the lock, which the
    ** thread takes on its way out, and on a separate timeout ECB, so
    ** termecb is only ever posted by MVS.
    */
    task = NULL;
    lockrc = lock(tmr, 0);
    if (tmr->task && (tmr->flags & TMR_FLAG_SHUTDOWN)) task = tmr->task;
    if (lockrc==0) unlock(tmr, 0);
    if (task && !(task->termecb & ECB_POSTED_BIT)) {
        ECB     tecb = 0;
        ECB     *waitlist[2];

        waitlist[0] = &task->termecb;
        waitlist[1] = (ECB *)((unsigned)&tecb | 0x80000000);
        ecb_timed_waitlist(waitlist, &tecb, 500, 0);   /* 5 seconds at most */
    }

    /* if we have a timer thread that is shut down, delete it; a refused
    ** delete leaves tmr->task set, keeping the handle that is still needed */
    lockrc = lock(tmr, 0);
    if (tmr->task && (tmr->flags & TMR_FLAG_SHUTDOWN)) {
        wtof("%s thread DELETE", __func__);
        cthread_delete(&tmr->task);
    }
    if (lockrc==0) unlock(tmr, 0);

    /* If the timer thread never acknowledged the shutdown it is still
    ** running, and this used to DETACH it anyway -- the same ungated force
    ** detach as the worker teardown in #11, with the same consequence: the
    ** thread's stack lives inside its CTHDTASK (@@ctcrtx.c newthread), so
    ** terminating it here and then dropping the only pointer to it leaves a
    ** live TCB standing on storage nobody owns any more.
    ** cthread_detach() now refuses a subtask that has not posted termecb, so
    ** keep the handle instead of losing the reference we would need to clean
    ** it up later.
    */
    lockrc = lock(tmr, 0);
    if (tmr->task && !(tmr->flags & TMR_FLAG_SHUTDOWN)) {
        wtof("%s thread DETACH", __func__);
        if (cthread_detach(tmr->task)==CTHREAD_DETACH_LIVE) {
            wtof("%s timer thread did not stop, TCB(%06X) retained",
                __func__, tmr->task->tcb);
        }
        else {
            tmr->task = NULL;
        }
    }
    if (lockrc==0) unlock(tmr, 0);

quit:
    return 0;
}
