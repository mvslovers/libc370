#include <stdio.h>
#include <mvs/timer.h>
#include <s370/atomic.h>

struct beat {
    unsigned ticks;              /* changed on the timer thread   */
};

static int tick(void *udata, TQE *tqe)
{
    struct beat *b = udata;

    __uinc(&b->ticks);           /* one CS: no lock needed        */
    return 0;
}

int main(void)
{
    struct beat b = { 0 };       /* automatic, not static         */
    ECB         done = 0;
    TQEID       every;
    int         rc = 0;

    every = tmr_func_every(tick, &b, 100);       /* each second   */
    if (!every)
        return 8;

    if (tmr_ecb(&done, 550))                     /* once, 5.5 s   */
        ecb_wait(&done);
    else
        rc = 8;

    tqe_purge(every);
    tmr_stop();                  /* before the program ends       */

    printf("%u ticks\n", b.ticks);
    return rc;
}
