#ifndef SRC_INTERNAL_MTXNAME_H
#define SRC_INTERNAL_MTXNAME_H
/* src/internal/mtxname.h - internal: the ENQ resource name of a mutex.
**
** The mutex functions locked "MUTEX.%08X" through lock_resf() and its
** siblings, which format the name with vsprintf() and so linked all of
** printf into every program with a mutex - every threaded one, through
** cthread's cleanup (size analysis F4).  The same name, built by hand.
*/
#include "mvs/mutex.h"
#include "src/internal/wtofmt.h"

/* buf: at least 16 bytes */
static __inline void mtxname(char *buf, const CLIBMUTX *mutex)
{
    int i = 0;

    wtofmt_str(buf, &i, 16, "MUTEX.");
    wtofmt_hex(buf, &i, 16, (unsigned)mutex, 8);
}

#endif /* SRC_INTERNAL_MTXNAME_H */
