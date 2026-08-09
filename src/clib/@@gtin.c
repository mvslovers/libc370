/* @@GTIN.C */
#include <stdio.h>
#include "clibcrt.h"

FILE **__gtin(void)
{
    CLIBGRT *grt    = __grtget();

    if (!grt) return NULL;  /* no GRT: no stdio anchors (#85) */
    return((FILE**)&grt->grtin);
}
