/* @@GTOUT.C */
#include <stdio.h>
#include "clibcrt.h"

FILE **
__gtout(void)
{
    CLIBGRT *grt    = __grtget();

    if (!grt) return NULL;  /* no GRT: no stdio anchors (#85) */
    return((FILE**)&grt->grtout);
}
