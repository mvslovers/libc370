#include <string.h>
#include <mvs/console.h>
#include <mvs/ecb.h>
#include <mvs/wto.h>

#define TICK 0x0001                     /* post code of our timer */

static void do_work(void)
{
    /* the periodic work of the task */
}

int main(void)
{
    COM      *com = __gtcom();
    CIB      *cib;
    ECB      timer = 0;
    ECB      *list[2];
    char     text[64];
    int      stop = 0;

    if (!com) return 8;

    cib = __cibget();                   /* drop the START command */
    if (cib && cib->cibverb == CIBSTART) __cibdel(cib);
    __cibset(1);

    list[0] = com->comecbpt;            /* MODIFY and STOP          */
    list[1] = (ECB *)((unsigned)&timer | 0x80000000); /* last entry */

    wtof("DEMO001I STARTED");
    while (!stop) {
        if (ecb_timed_waitlist(list, &timer, 6000, TICK) < 0) {
            wtof("DEMO009E NO TIMER, ENDING");
            break;
        }
        if (timer & ECB_POSTED_BIT) {   /* one minute has passed    */
            timer = 0;
            do_work();
        }
        while ((cib = __cibget()) != NULL) {
            if (cib->cibverb == CIBSTOP) {
                stop = 1;
            } else if (cib->cibverb == CIBMODFY) {
                unsigned n = cib->cibdatln < sizeof(text) - 1
                           ? cib->cibdatln : sizeof(text) - 1;
                memcpy(text, cib->cibdata, n);
                text[n] = 0;
                if (strcmp(text, "RUN") == 0)
                    do_work();
                else
                    wtof("DEMO002I UNKNOWN COMMAND '%s'", text);
            }
            __cibdel(cib);              /* the last one clears the ECB */
        }
    }
    wtof("DEMO003I ENDED");
    return 0;
}
