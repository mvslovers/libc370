#include <clibtmr.h>

__asm__("\n&FUNC    SETC 'tmr_func'");
TQEID tmr_func(int (*func)(void *, TQE*), void *udata, unsigned bintvl)
{
    TMR         *tmr    = tmr_get();
    TQE         *tqe    = NULL;
    TQEID       id      = 0;
    int         lockrc;

    if (!tmr) return 0;         /* no TMR anchor: no timer services (#85) */

    tmr_start();

    tqe = tqe_new(NULL, func, udata, bintvl, 0);
    if (tqe) {
        id = tqe->id;

        lockrc = lock(tmr, 0);
        if (array_add(&tmr->tqe, tqe)) {
            /* not queued: it would never fire, report failure (#85) */
            free(tqe);
            id = 0;
        }
        else {
            ecb_post(&tmr->wakeup, 0);
        }
        if (lockrc==0) unlock(tmr, 0);
    }

    return id;
}
