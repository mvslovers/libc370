#include <stdio.h>
#include <string.h>
#include <mvs/thread.h>

#define REQ_WORK  1              /* post codes of the request ECB */
#define REQ_QUIT  2
#define TIMEOUT   0x0FFF         /* post code of the time-out ECB */

struct chan {
    ECB req;                     /* posted by main()              */
    ECB done;                    /* posted by the thread          */
    int value;
    int result;
};

static int square(void *arg1, void *arg2)
{
    struct chan *ch = arg1;

    while (cthread_wait(&ch->req) == REQ_WORK) {
        ch->result = ch->value * ch->value;
        cthread_post(&ch->done, 1);
    }
    return 0;                    /* REQ_QUIT                      */
}

int main(void)
{
    struct chan ch;
    CTHDTASK    *task;
    ECB         tmo = 0;
    ECB         *list[2];
    int         i;

    memset(&ch, 0, sizeof ch);
    task = cthread_create(square, &ch, NULL);
    if (!task)
        return 8;

    for (i = 1; i <= 3; i++) {
        ch.value = i;
        cthread_post(&ch.req, REQ_WORK);
        cthread_wait(&ch.done);
        printf("%d squared is %d\n", i, ch.result);
    }

    /* Ask the thread to end, and give it 5 seconds.              */
    cthread_post(&ch.req, REQ_QUIT);
    list[0] = &task->termecb;
    list[1] = (ECB *)((unsigned)&tmo | 0x80000000);  /* last one  */
    if (ecb_timed_waitlist(list, &tmo, 500, TIMEOUT) < 0
     || !(task->termecb & ECB_POSTED_BIT)) {
        printf("thread has not ended yet, waiting\n");
        ecb_wait(&task->termecb);
    }
    cthread_delete(&task);
    return 0;
}
