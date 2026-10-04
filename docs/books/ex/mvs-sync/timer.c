#include <stdio.h>
#include <mvs/timer.h>

static int tick(void *udata, TQE *tqe)
{
    int *ticks = udata;

    *ticks += 1;                 /* runs on the timer thread */
    return 0;
}

int main(void)
{
    int      ticks = 0;          /* main outlives the timer */
    ECB      done = 0;
    TQEID    every;

    every = tmr_func_every(tick, &ticks, 100);  /* each second */
    if (!every || !tmr_ecb(&done, 550))         /* once, 5.5 s */
        return 8;

    ecb_wait(&done);             /* post code is the TQEID */
    tqe_purge(every);
    tmr_stop();

    printf("%d ticks\n", ticks);
    return 0;
}
