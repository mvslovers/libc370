/*
 * tstpool.c - libc370 #700 on MVS: a worker pool of 1, 2 or 3 workers
 * hands a queued item to a worker; #708: a pool of 0 is refused.
 *
 * cthread_manager_init() starts no worker for a count of 3 or less, and
 * the dispatcher created one on demand only when the pool already had a
 * worker: the "items still queued" result of dispatch_thread_work() was
 * computed inside "if (count)".  An empty pool stayed empty and every
 * item stayed in the queue.  A count of 4 starts one worker at once and
 * is the control.
 *
 * A pool of 0 workers (#708) never started one at all: the manager was
 * returned and took items it would never hand out.  It is refused now:
 * cthread_manager_init() returns NULL with errno EINVAL.
 *
 * Per pool size the probe queues an item and waits up to 6 seconds for
 * a worker to receive it.  The worker then ends on a second item, which
 * leaves the pool with no worker, and a third item must again reach a
 * (new) worker.  The pool never holds more workers than its count.
 *
 * BUILD (from the repository root, after make; S a scratch directory,
 * R a directory holding a libc.a built from origin/main):
 *
 *     cc370 -O1 -Wall -Werror -Iinclude -L build/sdk test/mvs/tstpool.c \
 *         -o $S/TSTPLG -flinker-output=iebcopy
 *     cc370 -O1 -Wall -Werror -Iinclude -L $R test/mvs/tstpool.c \
 *         -o $S/TSTPLR -flinker-output=iebcopy
 *     cd $S && ld370 --pack TSTPLG=TSTPLG.iebcopy \
 *         TSTPLR=TSTPLR.iebcopy -o pl -xmit --dsn IBMUSER.LIBC370.POOLSCR
 *
 * then jcl/recvpool.jcl and jcl/tstpool.jcl.
 *
 * RC 0 = passed, 8 = failed.
 */
#include <errno.h>
#include <mvs/wto.h>
#include <mvs/ecb.h>
#include <mvs/thread.h>
#include <ext/array.h>

#define POOL_MAX        4
#define WAIT_CS         600             /* 6 seconds, in 1/100 s */
#define POST_ITEM       1
#define POST_TIMEOUT    99

static int  bad;
static ECB  got_ecb;                    /* posted by the worker */
static char token = 'T';                /* an item to receive */
static char leave = 'L';                /* an item that ends the worker */

static void want(int cond, unsigned size, const char *what)
{
    wtof("TSTPOOL %s: pool of %u: %s", cond ? "PASS" : "FAIL", size, what);
    if (!cond) bad++;
}

static int worker(void *udata, CTHDWORK *work)
{
    char    *data = NULL;
    int     rc;

    (void)udata;

    for (;;) {
        rc = cthread_worker_wait(work, &data);
        if (rc == CTHDWORK_POST_SHUTDOWN) break;
        if (rc != CTHDWORK_POST_REQUEST || !data) continue;

        cthread_post(&got_ecb, POST_ITEM);
        if (data == &leave) break;      /* this worker ends here */
    }

    return 0;
}

/* Queues data and returns 1 when a worker received it within WAIT_CS. */
static int handed(CTHDMGR *mgr, char *data)
{
    got_ecb = 0;
    if (cthread_queue_add(mgr, data) != 0) return 0;
    return cthread_timed_wait(&got_ecb, WAIT_CS, POST_TIMEOUT) == POST_ITEM;
}

static void probe(unsigned size)
{
    CTHDMGR     *mgr;
    unsigned    workers;
    int         got;

    mgr = cthread_manager_init(size, worker, NULL, 64 * 1024);
    want(mgr != NULL, size, "cthread_manager_init()");
    if (!mgr) return;

    got = handed(mgr, &token);
    workers = arraycount(&mgr->worker);
    wtof("TSTPOOL   pool of %u: item %s, %u worker(s)", size,
         got ? "received" : "NOT RECEIVED", workers);
    want(got, size, "a queued item reaches a worker");
    want(workers <= size, size, "no more workers than the count");

    if (got) {
        got = handed(mgr, &leave);
        want(got, size, "the worker takes the item that ends it");
        got = got && handed(mgr, &token);
        want(got, size, "an item after the worker ended reaches a worker");
    }

    cthread_manager_term(&mgr);
}

/* A pool of 0 workers is refused (#708); before, it took items and
 * served none of them. */
static void probe_zero(void)
{
    CTHDMGR     *mgr;
    int         err;
    int         got;

    errno = 0;
    mgr = cthread_manager_init(0, worker, NULL, 64 * 1024);
    err = errno;
    wtof("TSTPOOL   pool of 0: %s, errno %d", mgr ? "manager" : "NULL", err);
    want(mgr == NULL && err == EINVAL, 0,
         "cthread_manager_init() returns NULL with EINVAL");
    if (!mgr) return;

    got = handed(mgr, &token);
    wtof("TSTPOOL   pool of 0: item %s", got ? "received" : "NOT RECEIVED");
    cthread_manager_term(&mgr);
}

int main(void)
{
    unsigned size;

    probe_zero();
    for (size = 1; size <= POOL_MAX; size++)
        probe(size);

    wtof("TSTPOOL %s", bad ? "FAILED" : "PASSED");
    return bad ? 8 : 0;
}
