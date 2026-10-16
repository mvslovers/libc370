#define __CRTGET__
#define CLIB_C
#include <stdlib.h>
#include <string.h>
#include "mvs/wto.h"
#include "stddef.h"
#include "ibm/mvs/ihacde.h"
#include "mvs/crt.h"
#include "ext/array.h"
#include "mvs/lock.h"
#include "src/internal/wtofmt.h"

CLIBCRT *
__CRTGET(void)
{
    int         locked  = 0;
    unsigned    *psa    = 0;                        /* low core == PSA      */
    void        *tcb    = (void*)psa[0x21c/4];      /* TCB      == PSATOLD  */
    CLIBPPA     *ppa    = __PPAGET();               /* Pgm Props Anchor     */
    CLIBCRT     *crt    = (CLIBCRT *)0;             /* CLIBCRT  or NULL     */
    unsigned    count   = 0;
    unsigned    n;

    if (!ppa) goto quit;

    locked  = lock(ppa,1);      /* 1=read only (shared) */
    if (locked!=0) goto quit;

    count = arraycou(&ppa->ppacrt);

    for(n=0;n < count; n++) {
        if (ppa->ppacrt[n]->crttcb == tcb) {
            crt = ppa->ppacrt[n];
            break;
        }
    }

    if (locked==0) {
        unlock(ppa,1);
    }

quit:
    if (!crt) {
        /* without the printf family (#564): every program links
           __crtget(), and wtof()/wtodumpf() made it link printf and
           half of stdio.  The same message; the dump of PPACRT is now
           its first entries in hex on a second line. */
        char    msg[120];
        int     i = 0;

        wtofmt_str(msg, &i, sizeof(msg), "__CRTGET CRT for TCB(");
        wtofmt_hex(msg, &i, sizeof(msg), (unsigned)tcb, 8);
        wtofmt_str(msg, &i, sizeof(msg), ") was not found in PPA(");
        wtofmt_hex(msg, &i, sizeof(msg), (unsigned)ppa, 8);
        wtofmt_str(msg, &i, sizeof(msg), ")");
        wto(msg);
        if (ppa && count) {
            i = 0;
            wtofmt_str(msg, &i, sizeof(msg), "PPACRT");
            for (n = 0; n < count && n < 8; n++) {
                wtofmt_str(msg, &i, sizeof(msg), " ");
                wtofmt_hex(msg, &i, sizeof(msg), (unsigned)ppa->ppacrt[n], 8);
            }
            wto(msg);
        }
    }
    return crt;
}
