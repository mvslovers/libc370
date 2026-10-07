#include <stdio.h>
#include "mvs/enq.h"
#include "mvs/lock.h"
#include <string.h>
#include "src/internal/lkname.h"

__asm__("\n&FUNC    SETC 'testlock'");
int
testlock(void *thing, int read)
{
    unsigned    opts    = ENQ_TEST;
    char        rname[LOCKRNAMESZ];

    if (read) opts |= ENQ_SHR;

    __lkname(rname, thing);

    return ENQ(LOCKQNAME, rname, opts);
    /* 0==resource is not locked */
}
