/* @@SOADD.C */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "mvs/crt.h"
#include "ext/array.h"
#include "mvs/socket.h"
#include "mvs/lock.h"

extern void __75clr(int ss);

/* __soend() - close every socket of grt at the end of its program (#601).
** __exit() calls it for the program's own GRT, __ppahrv() for the GRT of a
** LINKed program that abended under try().  It lives with __soadd(): a
** program that registers a socket links it, and the callers reach it
** through a weak reference, so a program without sockets does not.
** Anything that does not validate is left alone. */
__asm__("\n&FUNC    SETC '__soend'");
void
__soend(CLIBGRT *grt)
{
    unsigned    count;
    CLIBSOCK    *s;

    if (!grt || !grt->grtsock) return;

    count = arraycount(&grt->grtsock);
    while (count > 0) {
        count--;
        s = grt->grtsock[count];
        grt->grtsock[count] = 0;
        if (!s || (unsigned)s > 0x00FFFFFF) continue;
        if (memcmp(s->eye, CLIBSOCK_EYE, sizeof(s->eye)) != 0) continue;
        __75clr(s->socket);
        free(s);
    }
    arrayfree(&grt->grtsock);
}

__asm__("\n&FUNC    SETC '__soadd'");
int
__soadd(int ss, void *name, void *peer)
{
    int         rc      = -1;
    CLIBGRT     *grt    = __grtget();
    CLIBSOCK    *s      = calloc(1, sizeof(CLIBSOCK));

    if (!s) goto quit;

    strcpy(s->eye, CLIBSOCK_EYE);
    s->socket   = ss;
    if (name)   memcpy(s->name, name, sizeof(s->name));
    if (peer)   memcpy(s->peer, peer, sizeof(s->peer));
#if 0
    wtodumpf(s, sizeof(CLIBSOCK), "__soadd()");
#endif
    lock(&grt->grtsock,0);
    arrayadd(&grt->grtsock, s);
    unlock(&grt->grtsock,0);

quit:
    return rc;
}
