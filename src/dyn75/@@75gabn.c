/* @@75GABN.C
** Get address by name
*/
#include "src/internal/__75.h"
#include "sys/socket.h"
#include "netdb.h"
#include "libc370/socket.h"
#include "stdlib.h"
#include "string.h"

extern unsigned char *ebc2asc;

/* gethostbyname() */
__asm__("\n&FUNC    SETC 'getaddrbyname'");
extern unsigned
getaddrbyname(const char *name)
{
    unsigned    addr    = 0;
    int         len;
    PL75        pl;

    __asm__("XC\t0(64,%0),0(%0)     clear __75 parameter list" : : "r" (&pl));

    len = strlen(name);
    if (!len) goto quit;

    pl.r1   = (unsigned) len;
    pl.r5   = (unsigned) name;
    pl.r7   = (unsigned) 4;

    __75(&pl);

    addr    = pl.r4;    /* returned ip address */

quit:
    return addr;
}
