#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "mvs/vsam.h"

unsigned
__vslrec(VSFILE *vs)
{
    unsigned    lrecl   = 0;
    unsigned    pl[16]  = {0};

    /* get LRECL of data component */
    __asm__("SHOWCB ACB=(%0),FIELDS=LRECL,AREA=(%1),LENGTH=4,MF=(G,(%2))"
        : : "r"(&vs->acb), "r"(&lrecl), "r"(pl) : "0", "1", "14", "15", "memory");

quit:
    return lrecl;
}
