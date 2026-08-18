/* @@75SEND.C
** Send data to a socket
*/
#include "__75.h"
#include "socket.h"
#include "errno.h"

/* A blocking socket (FIONBIO never set) whose host send buffer is full gets
** X'75' return code -2, "wait and reissue" -- the contract recv() and accept()
** have always had, and reachable from send() since mvslovers/hyperion
** 1a599b0d made the emulator's SEND non-blocking.
**
** The wait is bounded on the same policy httpd applies in its own send layer
** (SEND_STALL_MAX / SEND_STALL_PAUSE, httpd/include/httpd.h): 100 waits of
** 0.10 seconds -- ten seconds without progress -- and then -1 with
** EWOULDBLOCK, so a peer that holds the connection open and never reads gets
** the session torn down instead of hanging it forever.  The pause is the
** STIMER BINTVL below: BINTVL counts hundredths of a second, so F'10' is the
** same 100 ms the other half of this policy uses.
*/
#define SEND_STALL_MAX  100     /* consecutive no-progress attempts */

/* send() */
__asm__("\n&FUNC    SETC 'send'");
extern int
send(int ss, const void *buf, int len, int flags)
{
    int     rc;
    int     stall   = 0;
    PL75    pl;

#if 0
    wtof("__75send(%d,%08X,%d,%d)", ss, buf, len, flags);
    wtof("%-60.60s", buf);
#endif

    for (;;) {
        /* the parameter list has to be rebuilt for every attempt: __75()
         * stores R0-R15 back into it, and the X'75' guest-to-host copy loop
         * drains R1 to zero and advances R5 past the bytes it moved.  A
         * retry that reused the list would ask the emulator to send 0 bytes
         * from a pointer that is already past the buffer.
         */
        __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list"
            : : "r" (&pl));
        pl.r1   = (unsigned) len;
        pl.r5   = (unsigned) buf;
        pl.r7   = (unsigned) 10;        /* function code for send() */
        pl.r8   = (unsigned) ss;

        __75(&pl);

        rc = (int) pl.r4;
#if 0
        wtof("__75send() rc=%d", rc);
#endif
        if (rc!=-2) break;

        /* -2 always means zero bytes went out, so the identical buffer is
         * reissued unchanged.  This is the ONLY thing that retries: a short
         * write is a byte count and is returned as one, because callers
         * build their state machines on exactly that.
         */
        if (++stall > SEND_STALL_MAX) {
            /* the peer never drained.  Cerr[] was never set for a -2, so
             * there is no emulator errno to fetch -- this one is ours.
             */
            errno = EWOULDBLOCK;
            rc = -1;
            goto quit;
        }
        __asm__("STIMER WAIT,BINTVL==F'10'  0.10 seconds" : : : "0", "1");
    }

    if (rc!=-1) goto quit;

    pl.r1   = (unsigned) 0;
    pl.r7   = (unsigned) 2;         /* function code for get error */
    __75(&pl);
#if 0
    __75vect->error = (int) pl.r4;
    errno = __75vect->error;
#else
    errno = (int) pl.r4;
#endif
quit:
    return rc;
}
