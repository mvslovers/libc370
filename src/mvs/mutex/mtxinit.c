/* MTXINIT.C */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "mvs/mutex.h"

void
mtxinit(CLIBMUTX *mutex)
{
    mutex->owner    = 0;
    mutex->count    = 0;
}
