#include "mvs/enq.h"
#include "mvs/lock.h"

__asm__("\n&FUNC    SETC 'trylock'");
int
trylock(void *thing, int read)
{
    unsigned    opts    = ENQ_USE;
    char        rname[LOCKRNAMESZ];

    if (read) opts |= ENQ_SHR;

    sprintf(rname, LOCKRNAME, thing);

    return ENQ(LOCKQNAME, rname, opts);
    /* 0==resource locked */
}
