#include <stdio.h>
#include <mvs/thread.h>
#include <s370/atomic.h>

struct job {
    unsigned total;              /* items queued                  */
    unsigned done;               /* items finished                */
    ECB      alldone;            /* posted by the last worker     */
};

static int worker(void *udata, CTHDWORK *work)
{
    struct job *job = udata;
    char       *item;

    while (cthread_worker_wait(work, &item) != CTHDWORK_POST_SHUTDOWN) {
        if (!item)
            continue;            /* a timer post, no item         */
        printf("worker %08X: %s\n", (unsigned)work, item);
        if (__uinc(&job->done) + 1 == job->total)
            cthread_post(&job->alldone, 0);
    }
    return 0;
}

int main(void)
{
    static char *const items[] = { "one", "two", "three", "four" };
    struct job job = { 4, 0, 0 };
    CTHDMGR    *mgr;
    unsigned   i;

    mgr = cthread_manager_init(4, worker, &job, 0);
    if (!mgr)
        return 8;

    for (i = 0; i < job.total; i++)
        if (cthread_queue_add(mgr, items[i]) != 0)
            break;

    if (i == job.total)
        ecb_wait(&job.alldone);  /* every item has been worked    */
    else
        printf("could not queue item %u\n", i);

    return cthread_manager_term(&mgr) ? 8 : 0;
}
