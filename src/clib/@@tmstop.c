#include <clibtmr.h>

__asm__("\n&FUNC    SETC 'tmr_stop'");
int tmr_stop(void)
{
    TMR         *tmr    = tmr_get();
    int         lockrc;
    int         i;

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

    /* if we have a timer thread that is shut down, delete it */
    lockrc = lock(tmr, 0);
    if (tmr->task && (tmr->flags & TMR_FLAG_SHUTDOWN)) {
        wtof("%s thread DELETE", __func__);
        cthread_delete(&tmr->task);
        tmr->task = NULL;
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
