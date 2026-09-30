/* MTXNEW.C */
#include <stdlib.h>
#include "mvs/mutex.h"

CLIBMUTX *
mtxnew(void)
{
    CLIBMUTX    *mutex  = calloc(1, sizeof(CLIBMUTX));

    if (mutex) {
        mtxinit(mutex);
    }

    return mutex;
}
