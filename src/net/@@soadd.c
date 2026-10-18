/* @@SOADD.C */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "mvs/crt.h"
#include "ext/array.h"
#include "mvs/socket.h"
#include "mvs/lock.h"
#include "mvs/recovery.h"

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

/* soexit() - the ESTAE exit: an abend that nothing in the program
** recovered.  Close the program's sockets and percolate.  The GRT is the
** parameter __soestae() gave it; estae() hands the exit the address of the
** pair {function, parameter}.  It is checked before it is trusted. */
__asm__("\n&FUNC    SETC 'soexit'");
static int
soexit(void *sdwa, void *param)
{
    CLIBGRT *grt = (CLIBGRT *)((unsigned *)param)[1];

    (void)sdwa;
    if (grt && (unsigned)grt <= 0x00FFFFFF
            && memcmp(grt->grteye, "CLIBGRT ", 8) == 0) {
        __soend(grt);
    }
    return 0;                       /* percolate */
}

/* __soestae() - __start() calls it, through a weak reference, before
** anything else of the program runs: an abend that nothing recovers ends
** the task without the library's end-of-program code, and the emulator
** keeps the sockets (#601).  One ESTAE level of the ten estae() has. */
__asm__("\n&FUNC    SETC '__soestae'");
void
__soestae(void)
{
    CLIBGRT *grt = __grtget();

    if (!grt) return;
    if (estae(ESTAE_CREATE, soexit, grt) == 0)
        grt->grtflag2 |= GRTFLAG2_SOESTAE;
}

/* __soexit() - __exit() calls it, through a weak reference: delete the
** ESTAE when it is the only one left (a program's own estae() still
** established is newer, and ESTAE 0 would delete that instead - the exit
** then finds the GRT gone and does nothing), and close the sockets. */
__asm__("\n&FUNC    SETC '__soexit'");
void
__soexit(CLIBGRT *grt)
{
    CLIBCRT *crt = __crtget();

    if (grt->grtflag2 & GRTFLAG2_SOESTAE) {
        if (crt && crt->crtestct == 1) estae(ESTAE_DELETE, NULL, NULL);
        grt->grtflag2 &= ~GRTFLAG2_SOESTAE;
    }
    __soend(grt);
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
