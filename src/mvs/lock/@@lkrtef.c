#include <stdio.h>
#include <stdarg.h>
#include "mvs/enq.h"
#include "mvs/lock.h"

int
__lkrtef(const char *fmt, int read, ...)
{
    va_list list;
    char    rname[256];

    va_start(list, read);
    vsprintf(rname, fmt, list);
    va_end(list);

    return testlock_res(rname, read);
}
