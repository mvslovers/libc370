/* MTXFREE.C */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "mvs/mutex.h"

void
mtxfree(CLIBMUTX *mutex)
{
    if (!mutex) goto quit;

    /* Held by this task: release it completely through mtxunlk(), which
    ** DEQs the mutex's own resource and takes it out of the task's list.
    ** unlock() DEQed the LOCK name, not the MUTEX one, so the ENQ stayed,
    ** and the list kept the freed mutex for the cleanup at task end (#403).
    ** A mutex another task holds cannot be released from here. */
    if (mtxheld(mutex)) {
        mutex->count = 1;
        mtxunlk(mutex);
    }

    memset(mutex, 0, sizeof(CLIBMUTX));

    free(mutex);

quit:
    return;
}
