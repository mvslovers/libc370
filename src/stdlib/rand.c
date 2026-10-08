/* RAND.C */
#define STDLIB_C
#include <stdlib.h>
#include <stddef.h>
#include "mvs/crt.h"

__PDPCLIB_API__ int rand(void)
{
    int ret = 0;    /* no CRT: no seed, and no residue to hand back */
    CLIBCRT *crt = __crtget();

    if (crt) {
        crt->crtseed = crt->crtseed * 1103515245UL + 12345;
        /* RAND_MAX is 32767: 0x8fff here returned 0..4095 and
           32768..36863 only (#387) */
        ret = (int)(((crt->crtseed) >> 16) & RAND_MAX);
    }

    return (ret);
}
