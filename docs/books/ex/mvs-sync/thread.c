#include <stdio.h>
#include <mvs/thread.h>

static int worker(void *arg1, void *arg2)
{
    int *count = arg1;

    *count += 1;
    return 42;                   /* becomes task->rc */
}

int main(void)
{
    int      count = 0;
    int      rc;
    CTHDTASK *task;

    task = cthread_create(worker, &count, NULL);
    if (!task) {
        printf("cannot create thread\n");
        return 8;
    }

    ecb_wait(&task->termecb);    /* posted by MVS at task end */
    rc = task->rc;               /* read it before the delete */
    cthread_delete(&task);

    printf("thread ended rc=%d, count=%d\n", rc, count);
    return 0;
}
