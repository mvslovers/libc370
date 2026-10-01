#include "src/internal/fileio.h"
#include "mvs/enq.h"
#include "mvs/lock.h"
#include "stdio.h"

__asm__("\n&FUNC    SETC 'lock'");
int
lock(void *thing, int read)
{
    unsigned    opts    = ENQ_HAVE;
    char        rname[LOCKRNAMESZ];
#if 0
    wtof("Function \"%s\" called lock() with %08X pointer",
        __caller(0), thing);
#endif
    if (read) opts |= ENQ_SHR;

    sprintf(rname, LOCKRNAME, thing);

    return ENQ(LOCKQNAME, rname, opts);
}
