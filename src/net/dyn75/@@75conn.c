/* @@75CONN.C
** Connect socket to destination
*/
#include "src/net/dyn75/x75.h"
#include "netinet/in.h"
#include "sys/socket.h"
#include "src/net/dyn75/dyn75.h"
#include "errno.h"
#include "mvs/socket.h"

/* connect() */
__asm__("\n&FUNC    SETC 'connect'");
extern int
connect(int ss, __CONST_SOCKADDR_ARG addr, int length)
{
    const struct sockaddr_in  *name = addr.__sockaddr_in__;
    int     rc;
    int     i;
    PL75    pl;

#if 0
    memset(&pl, 0, sizeof(pl));
#else
    __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list" : : "r" (&pl) : "memory");
#endif

    pl.r7   = (unsigned) 1;

    for(i=0; i < 2; i++) {
        pl.r1   = (unsigned) 0;
        pl.r7   = (unsigned) (pl.r7 << 24);
        pl.r7   = (unsigned) (pl.r7 >> 16);
        pl.r7   = (unsigned) (pl.r7 + 7);           /* connect */
        pl.r7   = (unsigned) (pl.r7 + (ss << 16));  /* socket */

        pl.r8   = (unsigned) (name->sin_addr.s_addr);
        pl.r9   = (unsigned)
            ((name->sin_family << 16) BOR name->sin_port) & 0x00FFFFFF;

        __75(&pl);

        if (pl.r4!=-2) break;

        pl.r7   = (unsigned) (pl.r7<<16);
        pl.r7   = (unsigned) (pl.r7>>24);
        if (pl.r7==0) {
            rc              = -1;
#if 0
            __75vect->error = 61;   /* ECONNREFUSED */
#else
            errno = ECONNREFUSED;
#endif
            goto quit;
        }

        pl.r7   = (unsigned) (pl.r7 - 1);

        /* wait one second */
        __asm__("STIMER WAIT,BINTVL==F'100'     1.00 seconds"
        : : : "0", "1", "14", "15", "memory");
    }

    /* check for error */
    rc      = (unsigned) pl.r4;
    if (rc>=0) {
        struct sockaddr     local = {0};
        struct sockaddr_in  peer = *name;
        int                 addrlen = sizeof(local);
        __75snam(ss, &local, &addrlen);
        __soupd(ss, &local, &peer);
        goto quit;
    }

    /* get error code */
    pl.r1   = (unsigned) 0;
    pl.r8   = (unsigned) ss;    /* socket */
    pl.r7   = (unsigned) 2;     /* get error(sock) function */

    __75(&pl);

#if 0
    __75vect->error = (int) pl.r4;  /* remember the error */
    errno = __75vect->error;
#else
    errno = (int) pl.r4;
#endif

quit:
    return rc;
}
