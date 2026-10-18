/* @@75IOCT.C
** ioctl for socket
*/
#include "src/net/dyn75/x75.h"
#include "sys/socket.h"
#include "mvs/socket.h"
#include "src/net/dyn75/dyn75.h"
#include "errno.h"

/* ioctlsocket() */
__asm__("\n&FUNC    SETC 'ioctlsocket'");
extern int
ioctlsocket(int ss, int cmd, void *argp)
{
    int     rc;
    PL75    pl;

#if 0
    memset(&pl, 0, sizeof(pl));
#else
    __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list" : : "r" (&pl) : "memory");
#endif

    pl.r7   = (unsigned) 15;        /* function code for ioctlsocket() */
    pl.r7   = (unsigned) pl.r7 BOR (ss << 16);  /* socket in high half */
    pl.r8   = (unsigned) cmd;       /* command code */
    /* FIONBIO takes the value argp points to: the emulator tests the
       register for nonzero, and the pointer was always nonzero, so
       &zero switched to non-blocking too.  NULL stays blocking (#396) */
    if (cmd==FIONBIO) {
        pl.r9   = (unsigned) (argp ? *(unsigned*)argp : 0);
    }
    else {
        pl.r9   = (unsigned) argp;      /* args pointer */
    }

    __75(&pl);

    /* FIONBIO fails too, on a socket that is not one (#396) */
    rc = (int) pl.r4;
    if (rc==-1) {
        /* get error */
        pl.r1   = (unsigned) 0;
        pl.r7   = (unsigned) 2;     /* function code for get error */
        pl.r8   = (unsigned) ss;    /* socket */

        __75(&pl);
#if 0
        __75vect->error = (int) pl.r4;
        errno = __75vect->error;
#else
        errno = (int) pl.r4;
#endif
        goto quit;
    }

    /* success, return result - FIONBIO has none, and *argp is the
       caller's setting */
    if (argp && cmd!=FIONBIO) {
        *(unsigned*)argp = pl.r4;
    }
    rc = 0;

quit:
    return rc;
}
