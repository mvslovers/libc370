#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "mvs/vsam.h"
#include "mvs/lock.h"

int
vsdelete(VSFILE *vs, void *rec, int reclen)
{
    int     rc;

    lock(vs,0);

    rc = __vsdel(vs, rec, reclen);

    unlock(vs,0);

    return rc;
}
