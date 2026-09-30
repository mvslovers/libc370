#include "mvs/enq.h"
#include "mvs/lock.h"

int
__lkrnte(const char *rname, int read)
{
    unsigned    opts    = ENQ_TEST;

    if (read) opts |= ENQ_SHR;

    return ENQ(LOCKQNAME, rname, opts);
    /* 0==resource is not locked */
}
