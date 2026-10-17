/* MTXLOCK.C */
#include "mvs/mutex.h"
#include "mvs/lock.h"
#include "src/internal/mtxname.h"
#include "mvs/crt.h"
#include "ext/array.h"

void
mtxlock(CLIBMUTX *mutex)
{
    unsigned    *psa    = (unsigned*)0;         /* MVS prefixed save area */
    unsigned    tcb     = psa[0x21C/4];         /* get our TCB address */
    int         rc;
    char        rname[16];

    /* get exclusive control of resource; the name built without printf,
       not with lock_resf() (size analysis F4) */
    mtxname(rname, mutex);
    rc = lock_res(rname, 0);

    mutex->owner = tcb; /* set owner id */
    mutex->count++;     /* increment lock count */

    if (rc==0) {
        /* add mutex to array of mutex's for this thread */
        CLIBCRT *crt    = __crtget();
        if (crt) arrayadd(&crt->crtmutx, mutex);
    }
}
