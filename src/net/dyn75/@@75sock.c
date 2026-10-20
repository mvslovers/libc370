/* @@75SOCK.C
** request socket allocation
*/
#include "src/net/dyn75/x75.h"
#include "sys/socket.h"
#include "src/net/dyn75/dyn75.h"
#include "errno.h"
#include "mvs/socket.h"
#include "mvs/crt.h"

/* socket() */
__asm__("\n&FUNC    SETC 'socket'");
extern int
socket(int af, int type, int protocol)
{
    CLIBGRT *grt    = __grtget();
    int     socket  = -1;
    PL75    pl;

#if 0
    memset(&pl, 0, sizeof(pl));
#else
    __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list" : : "r" (&pl) : "memory");
#endif

    if (!(grt->grtflag1 & GRTFLAG1_SOCKINIT)) {
                                    /* Need a socket before anything else... */
        __75init();                 /* so it's the best place to init.       */
                                    /* The Host PC has it's own trigger too! */
    }

    pl.r8   = (unsigned) af << 16;  /* address family */
    pl.r9   = (unsigned) type;      /* socket type */
    pl.r8   = pl.r8 BOR pl.r9;
    pl.r9   = (unsigned) protocol;  /* protocol */
    pl.r7   = (unsigned) 5;         /* function code */

    __75(&pl);

    socket  = (int) pl.r4;          /* socket or error (-1) */
    if (socket >= 0) {
        /* success */
        __soadd(socket, 0, 0);
        goto quit;
    }

    pl.r1   = (unsigned) 0;
    pl.r7   = (unsigned) 3;         /* function code for get error */

    __75(&pl);

    /* GETERROR reads one word for the whole emulator, which a failing
    ** socket(), gethostbyname() or gethostbyaddr() in any address space
    ** writes.  Another task can get in between the two calls, so the word
    ** may not be this call's (#161).  Take it only if socket() can fail
    ** that way; anything else becomes ENOBUFS.
    */
    switch ((int) pl.r4) {
    case EMFILE:
    case ENFILE:
    case ENOBUFS:
    case ENOMEM:
    case EACCES:
    case EAFNOSUPPORT:
    case EPROTONOSUPPORT:
    case EPROTOTYPE:
    case EINVAL:
        errno = (int) pl.r4;
        break;
    default:
        errno = ENOBUFS;
        break;
    }
quit:
    return socket;
}
