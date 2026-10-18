/* @@75CLOS.C
** Close socket
*/
#include "src/net/dyn75/x75.h"
#include "sys/socket.h"
#include "mvs/socket.h"

/* __75clr() - the close itself, without the program's socket list: for
** __soend(), which closes the sockets of a GRT that may not be the
** caller's (#601) */
__asm__("\n&FUNC    SETC '__75clr'");
extern void
__75clr(int ss)
{
    PL75    pl;

    __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list" : : "r" (&pl) : "memory");

    pl.r7   = (unsigned) 12;        /* function code for closesocket() */
    pl.r8   = (unsigned) ss;        /* socket */

    __75(&pl);
}

/* closesocket() */
__asm__("\n&FUNC    SETC 'closesocket'");
extern int
closesocket(int ss)
{
    __75clr(ss);
    __sodel(ss);

    return 0;
}
