/* @@CMWSHU.C - cthread_worker_shutdown()
*/
#include "clibthdi.h"
#include "clibwto.h"     /* wtof(): a prototype decides linkage here (#39) */

__asm__("\n&FUNC    SETC 'cthread_worker_shutdown'");
int
cthread_worker_shutdown(CTHDWORK *work)
{
    int         rc      = 0;
    int         i;
    CTHDTASK    *task;

    if (!work) goto quit;
#if 0
    wtof("__cmwshu() shutdown of worker thread %08X, task %08X starting", work, work->task);
#endif

    /* if the worker has already stopped then we're done */
    task = work->task;
    if (task) {
        if (task->termecb & 0x40000000) {
            /* subtask has terminated */
            if (task->tcb) cthread_detach(task);
            goto quit;
        }
    }

#if 0
    wtof("__cmwshu() posting work wait ecb %08X", work->wait);
#endif
    /* post the worker thread for shutdown */
    cthread_post(&work->wait, CTHDWORK_POST_SHUTDOWN);
#if 0
    wtof("__cmwshu() posted work wait ecb %08X", work->wait);
#endif

    if (task && task->tcb) {
        /* we have a subtask */
#if 0
        wtof("__cmwshu() shutdown of task %08X starting", task);
#endif
        for(i=0; i < 50; i++) {
            if (task->termecb & 0x40000000) {
                /* subtask has terminated */
                if (task->tcb) cthread_detach(task);
                goto quit;
            }
#if 0
            wtof("__cmwshu() shutdown waiting for task %08X termination", task);
#endif
#if 0
            wtof("__cmwshu() posting work wait ecb %08X", work->wait);
#endif
            cthread_post(&work->wait, CTHDWORK_POST_SHUTDOWN);
#if 0
            wtof("__cmwshu() posted work wait ecb %08X", work->wait);
#endif
            /* STIMER, deliberately, NOT ecb_timed_wait(&task->termecb,..):
            ** ecb_timed_wait() passes the SAME ecb as both the waitlist entry
            ** and the timeout ecb (@@ecbtw.c), so on timeout it posts the very
            ** flag this loop tests -- the next pass would read "terminated"
            ** and detach a live subtask.  ecb_timed_waitlist() with a separate
            ** timeout ecb is the other correct form.
            */
            __asm__("STIMER WAIT,BINTVL==F'10'   0.10 seconds" : : : "0", "1", "14", "15");
        }

        /* The worker never posted termecb, so its subtask is STILL RUNNING.
        ** This used to DETACH ...,STAE=YES anyway, which abnormally terminated
        ** a live subtask (S33E) -- and the caller then freed the CTHDTASK,
        ** whose allocation CONTAINS that subtask's stack (@@ctcrtx.c
        ** newthread: calloc(1, sizeof(CTHDTASK) + newstack)), so the recovery
        ** exit driven by the S33E was standing on freed storage.  The S33E and
        ** the nested fault behind it were one free-while-running, not two bugs.
        **
        ** Retain instead: mark the worker STUCK and leave every byte it owns
        ** alone.  The address space is coming down and MVS reclaims the region;
        ** a wedged-worker leak is the lesser evil, and it is the same trade
        ** cthread_manager_term() already makes for the dispatch thread (#11).
        */
        work->state = CTHDWORK_STATE_STUCK;
        wtof("cthread_worker_shutdown(%08X): worker did not stop, TCB(%06X) "
            "and its stack retained to avoid terminating a live task",
            work, task->tcb);
        rc = -1;
    }

quit:
    return rc;
}
