#include "mvs/enq.h"
#include "mvs/lock.h"

int
__lkuntr(const char *rname, int read)
{
    unsigned    opts    = ENQ_USE;

    if (read) opts |= ENQ_SHR;

    return ENQ(LOCKQNAME, rname, opts);
    /* 0==resource is not locked */
}
