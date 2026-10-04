#include <stdio.h>
#include <mvs/thread.h>

static int worker(void *udata, void *arg2)
{
    CTHDWORK *work = arg2;
    char     *data;

    while (cthread_worker_wait(work, &data) != CTHDWORK_POST_SHUTDOWN) {
        if (data)
            printf("worker %08X: %s\n", (unsigned)work, data);
    }
    return 0;
}

int main(void)
{
    static char *jobs[] = { "one", "two", "three" };
    CTHDMGR  *mgr;
    int      i;

    mgr = cthread_manager_init(4, worker, NULL, 64 * 1024);
    if (!mgr)
        return 8;

    for (i = 0; i < 3; i++)
        cthread_queue_add(mgr, jobs[i]);

    cthread_yield();
    return cthread_manager_term(&mgr) ? 8 : 0;
}
