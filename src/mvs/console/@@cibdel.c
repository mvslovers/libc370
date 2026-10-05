/* @@CIBDEL.C */
#include "mvs/console.h"

int
__cibdel(CIB *cib)
{
    int     rc      = 0;
    COM     *com    = __gtcom();

    if (cib) {
        __asm__("QEDIT ORIGIN=(%1),BLOCK=(%2)\n\tLR\t%0,15"
            : "=r" (rc) : "r" (&com->comcibpt), "r" (cib) : "0", "1", "14", "15", "memory");
    }

quit:
    return rc;
}
