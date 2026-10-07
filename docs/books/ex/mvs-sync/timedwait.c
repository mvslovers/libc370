#include <stdio.h>
#include <mvs/ecb.h>

#define TIMEOUT 0x0FFF          /* our own post code for a timeout */

int wait_for_work(ECB *work)    /* posted by another task */
{
    int rc;

    rc = ecb_timed_wait(work, 500, TIMEOUT);     /* 5 seconds */
    if (rc < 0) {
        printf("no timer available, rc=%d\n", rc);
        return -1;
    }
    if ((*work & ECB_VALUE_MASK) == TIMEOUT) {
        *work = 0;
        return 0;               /* timed out */
    }
    rc = *work & ECB_VALUE_MASK; /* the poster's code */
    *work = 0;                  /* clear before the next wait */
    return rc;
}
