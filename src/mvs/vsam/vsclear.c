#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "mvs/vsam.h"
#include "mvs/lock.h"

int
vsclear(VSFILE *vs)
{
    int     rc;

    lock(vs,0);

    rc = __vsclr(vs);

    unlock(vs,0);

    return rc;
}
