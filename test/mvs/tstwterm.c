/*
 * tstwterm.c - libc370 #11 regression probe (MVS target, batch).
 *
 * ISSUE #11: shutting a thread manager down while a worker was busy ended
 * in S33E and an SVC dump.  The issue recorded the drain failure as
 * unexplained and offered a wedged-handler lead.  It is neither wedged nor
 * unexplained - it is a deadlock on the manager's own ENQ, and this probe
 * reproduces it with a handler that is perfectly healthy and merely slow.
 *
 *   dispatch_thread_term()        @@cminit.c   held lock(mgr,0) across its
 *                                              whole teardown loop, wait and
 *                                              all
 *   cthread_worker_wait()         @@cmwwat.c   is the ONLY place a worker
 *                                              observes CTHDWORK_POST_SHUTDOWN,
 *                                              and it OPENS with
 *   cthread_queue_del(&work->queue) @@cmqdel.c which takes that SAME lock
 *                                              whenever the worker still holds
 *                                              a dispatched item
 *   lock()                        @@lk.c       ENQ ...,E,STEP,RET=HAVE - rc 8
 *                                              only for the owning task, any
 *                                              other TCB WAITS
 *
 * So a worker that was executing a request when shutdown began blocks at the
 * top of cthread_worker_wait(), never reaches the wait where the shutdown post
 * would be seen, misses the 5 s window, and used to be force-DETACHed
 * (STAE=YES) - after which cthread_worker_del() freed the CTHDTASK, whose
 * allocation CONTAINS that subtask's stack (@@ctcrtx.c newthread).
 *
 * work->queue is set in exactly one place - dispatch_work()'s post_request
 * branch - which is why this hits busy workers and leaves idle ones (queue
 * already NULL, so cthread_queue_del() returns before touching the lock)
 * draining normally.  That is the whole reproduction recipe: one worker, one
 * queued request, tear down while it runs.
 *
 * ====================================================================
 * WHAT THIS PINS - AND WHAT IT DOES NOT
 * --------------------------------------------------------------------
 * - The discriminator is worker_exited, set by the worker function on the
 *   line before its own return.  It answers the one question that matters:
 *   did the worker terminate OF ITS OWN ACCORD, or was it killed?  A
 *   force-DETACHed worker never reaches that line.  Return codes alone
 *   cannot tell the two apart - the S33E lands on the WORKER's TCB, not on
 *   the main task, so pre-fix this program still runs to completion and
 *   still gets its manager torn down.  It just leaves a corpse and a dump.
 *
 * - Sizing is deliberate and tied to the library's own constants.  count=4
 *   gives mintask=1 (cthread_manager_init: count > 3 ? 1) so exactly one
 *   worker exists at init, and maxtask=4 gives cthread_manager_term a
 *   quiesce window of 20 + 10*4 = 60 x 0.10 s = 6 s before it escalates.
 *   HANDLER_SECS is 8, so the handler is still running when the escalation
 *   fires and returns while dispatch_thread_term() is in its wait - which
 *   is precisely the collision.  Do not "tidy" these numbers apart.
 *
 * - NOT covered: a genuinely wedged worker, i.e. one that never returns at
 *   all.  Post-fix that is retained as CTHDWORK_STATE_STUCK and
 *   cthread_manager_term() answers -1, which is the intended behaviour -
 *   but exercising it would leave a live subtask at end of task, a hazard
 *   of its own and not the one #11 is about.  The retention path is
 *   reviewed as read code, the way #117's error paths were.
 *
 * - NOT covered: @@tmstop.c's timer-thread detach, fixed in the same change
 *   as a third instance of the same ungated force detach.  It needs a timer
 *   thread that ignores its shutdown flag, which is the wedged case above.
 *
 * ====================================================================
 *
 * BUILD (host):
 *     cc370 -Iinclude test/mvs/tstwterm.c -flinker-output=iebcopy -o TSTWTERM
 *     ld370 --pack TSTWTERM.iebcopy -o probe -xmit --dsn <LOADLIB>
 *
 * RUN: see jcl/tstwterm.jcl.  TIME=1 is the watchdog: a regression would hang
 * rather than fail.
 *
 * MEASURED on mvsdev 2026-08-23, both sides from this one source - only the
 * libc370 it is linked against differs (TSTWRED = main, TSTWTERM = fixed):
 *
 *   RED   6 of 7 checks ok, "the worker returned of its own accord *** FAIL",
 *         COND CODE 0008, step elapsed 13.31 s
 *   GREEN 7 of 7 ok, COND CODE 0000, step elapsed 10.21 s
 *
 * The job log times the deadlock directly.  RED:
 *
 *     8.56.10  +TSTWTERM worker handling request, 8 seconds
 *     8.56.10  +TSTWTERM terminating the manager while the worker is busy
 *     8.56.18  +TSTWTERM worker finished the request
 *     8.56.21  COND CODE 0008
 *
 * Three seconds between the handler finishing and the job ending, with the
 * worker producing nothing in between: that is the worker sitting on the
 * manager ENQ until dispatch_thread_term() gave up and detached it.  GREEN
 * puts "worker finished the request" and "worker returning normally" in the
 * SAME second, and the 3.1 s difference in elapsed time is the whole defect.
 *
 * NOTE what RED does NOT show: there is no S33E message in the job log.  The
 * S33E is real - DETACH STAE=YES abnormally terminates an incomplete subtask -
 * but nothing REPORTS it here, because libc370's recovery exit (@@abrpt.c
 * recovery()) is installed only through try()/estae(), and this worker is a
 * bare loop.  httpd#122 was loud precisely because httpd runs its handlers
 * under try().  So the probe proves the worker was KILLED rather than allowed
 * to return, which is the defect; it does not reproduce the console message.
 *
 * RC: 0 = all expectations met, 8 = at least one did not.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <clibwto.h>
#include <clibthrd.h>
#include <clibthdi.h>

#define WORKERS         4       /* -> mintask 1, maxtask 4 (see above)      */
#define HANDLER_SECS    8       /* outlasts the 6 s quiesce window          */
#define STARTUP_LIMIT   20      /* seconds to wait for the handler to begin */

static volatile int handler_entered = 0;    /* worker picked the request up */
static volatile int handler_left    = 0;    /* worker finished the request  */
static volatile int worker_exited   = 0;    /* worker returned BY ITSELF    */

static int check(const char *what, int ok);
static int worker(void *udata, CTHDWORK *work);

int main(void)
{
    CTHDMGR *mgr;
    int      bad = 0;
    int      rc;
    int      n;

    printf("=== tstwterm: worker teardown while busy (#11) ===\n\n");

    mgr = cthread_manager_init(WORKERS, worker, NULL, 64*1024);
    bad += check("thread manager created", mgr != NULL);
    if (!mgr) goto done;

    /* let the manager bring its mintask worker up and park it in the wait */
    sleep(2);

    /* One request.  This is what puts a CTHDQUE on work->queue, and the
    ** queue item is the whole reason the worker will need the manager lock
    ** again before it can see the shutdown post. */
    rc = cthread_queue_add(mgr, "REQUEST");
    bad += check("request queued", rc == 0);

    /* Wait until the worker is genuinely INSIDE the handler.  Tearing down
    ** before that would test the idle path, which never had the defect. */
    for (n = 0; n < STARTUP_LIMIT && !handler_entered; n++) {
        sleep(1);
    }
    bad += check("worker entered the handler", handler_entered != 0);
    if (!handler_entered) goto done;

    /* Tear down WHILE the handler runs.  Pre-fix: the quiesce window expires
    ** at ~6 s, dispatch_thread_term() takes the manager lock, the handler
    ** returns at ~8 s straight into cthread_queue_del()'s lock() and stops
    ** there, the 5 s escalation expires and the worker is force-DETACHed. */
    wtof("TSTWTERM terminating the manager while the worker is busy");
    rc = cthread_manager_term(&mgr);

    bad += check("cthread_manager_term reports success", rc == 0);
    bad += check("manager handle cleared", mgr == NULL);
    bad += check("the handler ran to completion", handler_left != 0);

    /* THE discriminator.  A force-DETACHed worker never reaches the line
    ** that sets this. */
    bad += check("the worker returned of its own accord", worker_exited != 0);

done:
    printf("\nTSTWTERM %s\n", bad ? "FAILED" : "PASSED");

    return bad ? 8 : 0;
}

static int
worker(void *udata, CTHDWORK *work)
{
    char *data;
    int   rc;

    for (;;) {
        rc = cthread_worker_wait(work, &data);

        if (rc == CTHDWORK_POST_SHUTDOWN) break;
        if (rc != CTHDWORK_POST_REQUEST) continue;
        if (!data) continue;

        /* A healthy handler that is merely slow.  Nothing here is wedged,
        ** faulting or blocked on anything the manager owns - the collision
        ** is entirely in the teardown. */
        handler_entered = 1;
        wtof("TSTWTERM worker handling request, %d seconds", HANDLER_SECS);
        sleep(HANDLER_SECS);
        handler_left = 1;
        wtof("TSTWTERM worker finished the request");
    }

    wtof("TSTWTERM worker returning normally");
    worker_exited = 1;

    return 0;
}

static int
check(const char *what, int ok)
{
    printf("  %-50s %s\n", what, ok ? "ok" : "*** FAIL");
    return ok ? 0 : 1;
}
