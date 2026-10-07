#ifndef SRC_INTERNAL_LKNAME_H
#define SRC_INTERNAL_LKNAME_H
/* lkname.h - internal: the CLIBLOCK resource name of a lock address.
**
** Builds what sprintf(rname, LOCKRNAME, thing) builds, "LOCK." and eight
** upper-case hex digits, without the printf engine: sprintf() was a quarter
** of a lock()/unlock() pair (#453).
*/
#include "mvs/lock.h"

static inline void
__lkname(char *rname, const void *thing)
{
    static const char hex[] = "0123456789ABCDEF";
    unsigned    v = (unsigned)thing;
    int         i;

    memcpy(rname, "LOCK.", 5);
    for (i = 12; i >= 5; i--) {
        rname[i] = hex[v & 0xF];
        v >>= 4;
    }
    rname[13] = '\0';
}

#endif /* SRC_INTERNAL_LKNAME_H */
