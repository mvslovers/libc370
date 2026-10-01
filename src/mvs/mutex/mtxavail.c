/* MTXAVAIL.C */
#include "mvs/mutex.h"
#include "mvs/lock.h"

int
mtxavail(CLIBMUTX *mutex)
{
    /* return true if mutex is not locked */
    return (mutex->count == 0 && mutex->owner == 0);
}
