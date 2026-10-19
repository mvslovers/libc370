#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "mvs/vsam.h"

int
__vsclos(VSFILE *vs)
{
    unsigned    pl[2] = {0};
    int         rc    = 0;

    if (vs) {
        if (vs->flags & VSFILE_FLAG_OPEN) {
            /* CLOSE's R15 is the return value; it was always 0 (#411) */
            __asm__("MVC\t0($CLSLEN,%2),CLSMODEL\n\t"
                    "CLOSE ((%1)),MF=(E,(%2))\n\t"
                    "ST    15,%0"
                : "=m"(rc) : "r"(&vs->acb), "r"(pl)
                : "0", "1", "14", "15", "memory");
        }
        free(vs);
    }

    return rc;
}
__asm__("DS\t0F\nCLSMODEL CLOSE (*-*),MF=L\n$CLSLEN\tEQU\t*-CLSMODEL");
