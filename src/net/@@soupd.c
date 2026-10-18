/* @@SOUPD.C */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "ext/array.h"
#include "mvs/socket.h"
#include "mvs/lock.h"

int
__soupd(int ss, void *name, void *peer)
{
    int         rc      = 0;
    CLIBSOCK    *s      = 0;

    /* Only socket() and accept() make a socket the program's.  A socket
    ** it does not list came from elsewhere - a module LINKed by a server
    ** binds or connects one its caller created - and adopting it here
    ** would close it at the end of this program (#601). */
    if (!__sofind(ss,&s)) goto quit;

    if (name)   memcpy(s->name, name, sizeof(s->name));
    if (peer)   memcpy(s->peer, peer, sizeof(s->peer));

#if 0
    wtodumpf(s, sizeof(CLIBSOCK), "__soupd()");
#endif

quit:
    return rc;
}
