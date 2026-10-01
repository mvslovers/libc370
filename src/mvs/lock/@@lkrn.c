#include "mvs/enq.h"
#include "mvs/lock.h"
#include "stdio.h"

int
__lkrn(const char *rname, int read)
{
    unsigned    opts    = ENQ_HAVE;

    if (read) opts |= ENQ_SHR;

    return ENQ(LOCKQNAME, rname, opts);
}
