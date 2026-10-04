#include <stdio.h>
#include <mvs/thread.h>
#include <mvs/ecb.h>

typedef struct job {
    const char *text;
    ECB         done;           /* posted when a worker has finished */
} JOB;

static int worker(void *udata, void *arg2)
{
    CTHDWORK *work = arg2;
    char     *data;

    while (cthread_worker_wait(work, &data) != CTHDWORK_POST_SHUTDOWN) {
        JOB *job = (JOB *)data;

        if (job) {
            printf("worker %08X: %s\n", (unsigned)work, job->text);
            ecb_post(&job->done, 0);
        }
    }
    return 0;
}

int main(void)
{
    JOB      jobs[3] = { { "one", 0 }, { "two", 0 }, { "three", 0 } };
    CTHDMGR  *mgr;
    int      i, n = 0;

    mgr = cthread_manager_init(4, worker, NULL, 64 * 1024);
    if (!mgr)
        return 8;

    for (i = 0; i < 3; i++) {
        if (cthread_queue_add(mgr, &jobs[i]) != 0)
            break;
        n = i + 1;
    }

    /* Termination discards items no worker has taken: wait for each
       item queued before shutting the pool down. */
    for (i = 0; i < n; i++)
        ecb_wait(&jobs[i].done);

    return cthread_manager_term(&mgr) ? 8 : 0;
}
