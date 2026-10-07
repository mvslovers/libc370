#include <stdio.h>
#include "mvs/enq.h"
#include "mvs/lock.h"
#include <string.h>
#include "src/internal/lkname.h"

__asm__("\n&FUNC    SETC 'trylock'");
int
trylock(void *thing, int read)
{
    unsigned    opts    = ENQ_USE;
    char        rname[LOCKRNAMESZ];

    if (read) opts |= ENQ_SHR;

    __lkname(rname, thing);

    return ENQ(LOCKQNAME, rname, opts);
    /* 0==resource locked */
}
