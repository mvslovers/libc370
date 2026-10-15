#define __CRTRES__
#define CLIB_C
#include <stdlib.h>
#include <string.h>
#include "mvs/wto.h"
#include "stddef.h"
#include "ibm/mvs/ihacde.h"
#include "mvs/crt.h"
#include "ext/array.h"
#include "mvs/lock.h"

int
__CRTRES(void)
{
    int         rc      = -1;
    int         locked  = 0;
    unsigned    *psa    = 0;                        /* low core == PSA      */
    unsigned    *tcb    = (unsigned*)psa[0x21c/4];  /* TCB      == PSATOLD  */
    CLIBPPA     *ppa    = __PPAGET();               /* Pgm Props Anchor     */
    CLIBCRT     *crt    = (CLIBCRT *)0;             /* CLIBCRT  or NULL     */
    unsigned    count;
    unsigned    n;

    if (!ppa) goto quit;

    locked  = lock(ppa,0);
    if (locked!=0) goto quit;

    count = arraycou(&ppa->ppacrt);
    for(n=0;n < count; n++) {
        crt = ppa->ppacrt[n];
        if (crt && crt->crttcb == (void*)tcb) {
            arraydel(&ppa->ppacrt, n+1);
            free(crt);
            rc = 0;
            break;
        }
    }

    /* #546: the last CRT is gone, so free the array as well.  Otherwise
       @@EXITA FREEMAINs the PPA - the only pointer to it - and the block
       (arraynew, 128 bytes after rounding) stays in the heap subpool:
       one per run of every LINKed C module.  __ppahrv() frees it on the
       abend path; this is the normal one.  arrayadd() builds a new array
       if a CRT is added to this PPA again. */
    if (ppa->ppacrt && arraycou(&ppa->ppacrt) == 0) {
        arrayfree(&ppa->ppacrt);
    }

    if (locked==0) {
        unlock(ppa,0);
    }

quit:
    return rc;
}
