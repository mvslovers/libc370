#include <stdio.h>
#include "mvs/enq.h"
#include "mvs/lock.h"
#include <string.h>
#include "src/internal/lkname.h"

__asm__("\n&FUNC    SETC 'unlock'");
int
unlock(void *thing, int read)
{
    unsigned    opts    = ENQ_HAVE;
    char        rname[LOCKRNAMESZ];

    __lkname(rname, thing);

    return DEQ(LOCKQNAME, rname, opts);
}
