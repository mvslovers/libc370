/* @@GTERR.C */
#include <stdio.h>
#include "clibcrt.h"

FILE **
__gterr(void)
{
    CLIBGRT *grt    = __grtget();

    if (!grt) return NULL;  /* no GRT: no stdio anchors (#85) */
    return((FILE**)&grt->grterr);
}
