/* @@GRTGET.C */
#define CLIB_C
#include "mvs/crt.h"
#include "mvs/wto.h"
#include "clibos.h"

CLIBGRT *
__grtget(void)
{
    CLIBCRT     *crt    = __crtget();
    CLIBGRT     *grt    = crt ? crt->crtgrt : 0;

    return grt;
}
