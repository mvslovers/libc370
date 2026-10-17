/* MTXCLUP.C - mutex clean up */
#include "mvs/mutex.h"
#include "mvs/lock.h"
#include "src/internal/mtxname.h"
#include "mvs/recovery.h"

__asm__("\n&FUNC    SETC 'cleanup_mutex'");
static void
cleanup_mutex(CLIBMUTX *mutex)
{
    if (mtxheld(mutex)) {
        char    rname[16];

        mutex->owner = 0;
        mutex->count = 0;
        mtxname(rname, mutex);      /* not unlock_resf(): no printf (F4) */
        unlock_res(rname, 0);
    }
}

void
mtxclup(CLIBMUTX *mutex)
{
    if (mutex) try(cleanup_mutex,mutex);
}
