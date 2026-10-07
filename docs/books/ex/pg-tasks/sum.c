#include <stdio.h>
#include <stdlib.h>
#include <mvs/thread.h>

#define N 1000

struct part {                    /* one per thread                */
    const int *from;
    int        count;
    long       sum;
};

static int add(void *arg1, void *arg2)
{
    struct part *p = arg1;
    int         i;

    for (i = 0; i < p->count; i++)
        p->sum += p->from[i];
    return 0;
}

int main(void)
{
    struct part part[2];
    CTHDTASK    *task[2];
    int         *num;
    int         i, n;

    num = malloc(N * sizeof *num);
    if (!num)
        return 12;
    for (i = 0; i < N; i++)
        num[i] = i + 1;

    for (n = 0; n < 2; n++) {
        part[n].from  = num + n * (N / 2);
        part[n].count = N / 2;
        part[n].sum   = 0;
        task[n] = cthread_create(add, &part[n], NULL);
        if (!task[n])
            break;               /* still wait for the others     */
    }

    for (i = 0; i < n; i++) {
        ecb_wait(&task[i]->termecb);   /* posted when it ends     */
        if (task[i]->rc != 0)
            printf("thread %d ended rc=%d\n", i, task[i]->rc);
        cthread_delete(&task[i]);
    }
    free(num);

    if (n < 2) {
        printf("cannot create thread %d\n", n);
        return 8;
    }
    printf("sum %ld\n", part[0].sum + part[1].sum);
    return 0;
}
