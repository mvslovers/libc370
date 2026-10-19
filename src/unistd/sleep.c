#include <unistd.h>
#include <time.h>	/* our own prototype, so the definition is checked */
#include <mvs/ecb.h>

int
sleep(unsigned seconds)
{
#if 0
    /* 1 BINTVL == .01 seconds */
    int         t       = seconds * 100;  /* convert to hundreths of a second */
#else
    unsigned    timeout_ecb = 0;
#endif
#if 0
    unsigned    *psa    = 0;                            /* low core == PSA      */
    unsigned    *tcb    = (unsigned*)psa[0x21c/4];      /* TCB      == PSATOLD  */

    wtof("%s(%u) TCB(%06X) SLEEPING", __func__, seconds, tcb);
#endif

    /* in pieces: seconds * 100 wrapped above 42949672 seconds.  When the
       timer cannot be set nothing was waited, and the seconds left are the
       result, as POSIX has it; it returned 0, "slept fully", and a retry
       loop around it spun (#413) */
    while (seconds > 0) {
        unsigned    part = seconds > 4000000 ? 4000000 : seconds;

        timeout_ecb = 0;
        if (ecb_timed_wait(&timeout_ecb, part * 100, 0) < 0) return seconds;
        seconds -= part;
    }
#if 0
    wtof("%s(%u) TCB(%06X) AWAKE", __func__, seconds, tcb);
#endif
    return 0;
}
