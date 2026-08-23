/* @@CMTERM.C - cthread_manager_term()
*/
#include "clibthdi.h"
#include "clibwto.h"     /* wtof(): a prototype decides linkage here (#39) */

#if 0
#define WTODEBUG    /* define for wtof() debug messages */
#endif

__asm__("\n&FUNC    SETC 'cthread_manager_term'");
int
cthread_manager_term(CTHDMGR **cthdmgr)
{
    int         rc      = 0;
    int         locked;
    int         joined  = 0;
    CTHDMGR     *mgr;
    CTHDTASK    *task;
    unsigned    maxtask;
    unsigned    quiesce_tries;
    unsigned    shutdown_tries;
    unsigned    count;
    unsigned    n;
    unsigned    stuck   = 0;
    CTHDWORK    *work;
    CTHDTASK    *wtask;

    if (!cthdmgr) goto quit;
    mgr = *cthdmgr;
#ifdef WTODEBUG
    wtof("cthmterm(%08X) enter", mgr);
#endif
    if (!mgr) goto quit;

    /* Stop accepting new work and ask the dispatch thread to drain and
     * shut its workers down gracefully.
     */
    locked = lock(mgr,0);
    if (mgr->state < CTHDMGR_STATE_QUIESCE) {
        mgr->state = CTHDMGR_STATE_QUIESCE;
    }
#ifdef WTODEBUG
    wtof("cthmterm(%08X) POST(CTHDMGR_POST_QUIESCE)", mgr);
#endif
    cthread_post(&mgr->wait, CTHDMGR_POST_QUIESCE);
    if (locked==0) {
        unlock(mgr,0);
    }

    /* The dispatch thread owns worker teardown (dispatch_thread_term).
     * We must *join* it - wait until MVS posts its ATTACH ECB (termecb)
     * - before deleting its task or freeing mgr.  Otherwise the
     * dispatch thread is left walking mgr->worker over freed storage
     * (a use-after-free): it force-DETACHes a still-active worker (S33E)
     * and the worker's ESTAE then faults on the torn-down save area
     * (nested S0C4).
     */
    task = mgr->task;
    if (!task) {
        /* dispatch thread was never created (init failure path) */
        joined = 1;
        goto cleanup;
    }

    /* The bounds are sized against maxtask: on the escalation path the
     * dispatch thread's dispatch_thread_term() force-shuts-down each
     * still-active worker via cthread_worker_shutdown(), which costs up
     * to ~5s (50 x 0.10s) per worker before it detaches.  The join must
     * outlast that, else free(mgr) races the dispatch thread again.
     */
    maxtask         = mgr->maxtask ? mgr->maxtask : 1;
    quiesce_tries   = 20 + (10 * maxtask);      /* graceful drain, .10s units */
    shutdown_tries  = 50 + (60 * maxtask);      /* force shutdown, .10s units */

    /* phase 1: join while the workers drain gracefully */
    for(n=0; n < quiesce_tries; n++) {
        if (task->termecb & 0x40000000) {
            /* dispatch thread has ended */
            joined = 1;
            goto cleanup;
        }
        __asm__("STIMER WAIT,BINTVL==F'10'   0.10 seconds" : : : "0", "1", "14", "15");
        if (mgr->state < CTHDMGR_STATE_QUIESCE) {
            mgr->state = CTHDMGR_STATE_QUIESCE;
        }
        cthread_post(&mgr->wait, CTHDMGR_POST_QUIESCE);
    }

    /* phase 2: graceful drain stalled - escalate to an immediate
     * shutdown and keep joining until the dispatch thread ends.
     */
#ifdef WTODEBUG
    wtof("cthmterm(%08X) POST(CTHDMGR_POST_SHUTDOWN)", mgr);
#endif
    cthread_post(&mgr->wait, CTHDMGR_POST_SHUTDOWN);
    for(n=0; n < shutdown_tries; n++) {
        if (task->termecb & 0x40000000) {
            /* dispatch thread has ended */
            joined = 1;
            goto cleanup;
        }
        __asm__("STIMER WAIT,BINTVL==F'10'   0.10 seconds" : : : "0", "1", "14", "15");
        cthread_post(&mgr->wait, CTHDMGR_POST_SHUTDOWN);
    }

cleanup:
    if (!joined) {
        /* The dispatch thread never signalled termination.  DETACHing
         * its task or freeing mgr now would race the thread still using
         * them, so we deliberately retain the storage rather than risk a
         * use-after-free.  The address space is coming down and MVS
         * reclaims the region; a wedged-thread leak is the lesser evil.
         */
        wtof("cthread_manager_term(%08X): dispatch thread did not stop, "
            "storage retained to avoid use-after-free", mgr);
        rc = -1;
        goto quit;
    }

    /* The dispatch thread ending is no longer sufficient licence to free
     * mgr.  Since #11 it does not force-DETACH a worker that will not
     * stop: it leaves that worker in mgr->worker as CTHDWORK_STATE_STUCK
     * and returns normally.  Such a worker is a LIVE TCB that still
     * reaches this manager through work->mgr and its own ecb through
     * work->wait, so freeing mgr here would convert an S33E in a dying
     * address space into a use-after-free on a running task - strictly
     * worse than what #11 set out to fix.
     *
     * The count is derived from the array rather than kept in a counter,
     * which costs nothing here and keeps CTHDMGR's layout unchanged -
     * httpd decodes this control block field by field (httpcons.c).
     */
    if (mgr->worker) {
        locked = lock(mgr,0);
        count = arraycount(&mgr->worker);
        for(n=1; n <= count; n++) {
            work = arrayget(&mgr->worker, n);
            if (!work) continue;
            wtask = work->task;
            if (!wtask) continue;
            if (!wtask->tcb) continue;
            if (wtask->termecb & 0x40000000) continue;
            stuck++;
        }
        if (locked==0) {
            unlock(mgr,0);
        }
    }

    if (stuck) {
        wtof("cthread_manager_term(%08X): %u worker(s) still running, "
            "manager storage retained to avoid use-after-free", mgr, stuck);
        rc = -1;
        goto quit;
    }

    /* The dispatch thread has ended: it already tore down every worker,
     * so no subtask is active and mgr is ours alone.  cthread_delete()
     * DETACHes an already-terminated task, which is safe.
     */
    if (task) {
#ifdef WTODEBUG
        wtof("cthmterm(%08X) delete task(%08X)", mgr, task);
#endif
        cthread_delete(&task);
        mgr->task = 0;
    }

    /* free any work the caller queued that was never dispatched */
    if (mgr->queue) {
        count = arraycount(&mgr->queue);
#ifdef WTODEBUG
        wtof("cthmterm(%08X) queue cleanup, %u items", mgr, count);
#endif
        for(n=count; n > 0; n--) {
            CTHDQUE *queue = arrayget(&mgr->queue, n);

            if (!queue) continue;
            cthread_queue_del(&queue);
        }

        arrayfree(&mgr->queue);
    }

#ifdef WTODEBUG
    wtof("cthmterm(%08X) free", mgr);
#endif
    free(mgr);
    *cthdmgr = 0;

quit:
#ifdef WTODEBUG
    wtof("cthmterm exit, rc=%d", rc);
#endif
    return rc;
}
