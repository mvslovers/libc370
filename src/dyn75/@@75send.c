/* @@75SEND.C
** Send data to a socket
*/
#include "__75.h"
#include "socket.h"
#include "errno.h"

/* send() */
__asm__("\n&FUNC    SETC 'send'");
extern int
send(int ss, const void *buf, int len, int flags)
{
    int         rc  = 0;    /* len <= 0 never enters the loop below */
    const char  *p;
    int         sent;
    PL75        pl;

#if 0
    wtof("__75send(%d,%08X,%d,%d)", ss, buf, len, flags);
    wtof("%-60.60s", buf);
#endif

    for (sent = 0; sent < len; sent += rc) {
        p = ((const char *)buf) + sent;

        /* the whole parameter list has to be rebuilt on every pass:
         * __75() stores R0-R15 back into it, and the X'75' guest-to-host
         * copy loop drains R1 to zero and advances R5 past the bytes it
         * moved.  A retry that reused the list would ask the emulator to
         * send 0 bytes from a pointer that is already past the buffer.
         */
        __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list"
            : : "r" (&pl));
        pl.r1   = (unsigned) (len - sent);  /* byte count to send       */
        pl.r5   = (unsigned) p;             /* source buffer            */
        pl.r7   = (unsigned) 10;            /* function code for send() */
        pl.r8   = (unsigned) ss;            /* socket number            */

        __75(&pl);

        rc = (int) pl.r4;
#if 0
        wtof("__75send() rc=%d", rc);
#endif
        if (rc==-2) {
            /* the host send buffer is full and this is a blocking
             * socket: the emulator asks us to wait and reissue, the
             * same contract recv() and accept() have always had.
             * Nothing was sent, so the whole remainder is retried.
             */
            __asm__("STIMER WAIT,BINTVL==F'8'   0.08 seconds" : : : "0", "1");
            rc = 0;
            continue;
        }

        if (rc <= 0) break;
    }

    if (rc==-1) {
        /* set the errno value for this error */
        pl.r1   = (unsigned) 0;
        pl.r7   = (unsigned) 2;     /* function code for get error */
        __75(&pl);
        errno = (int) pl.r4;
    }

    /* if we sent some data, return the length we sent */
    if (sent) rc = sent;

    return rc;
}
