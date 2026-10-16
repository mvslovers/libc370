/* @@GTIN.C */
#include <stdio.h>
#include "mvs/crt.h"

extern void __stdlz(int which);

FILE **__gtin(void)
{
    CLIBGRT *grt    = __grtget();

    if (!grt) return NULL;  /* no GRT: no stdio anchors (#85) */
    /* opened on first use (#564) */
    if (!grt->grtin && (grt->grtflag1 & GRTFLAG1_STDLAZY)) __stdlz(0);
    return((FILE**)&grt->grtin);
}
