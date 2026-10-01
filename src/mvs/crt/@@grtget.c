/* @@GRTGET.C */
#define CLIB_C
#include "mvs/crt.h"
#include "mvs/wto.h"
#include "stddef.h"
#include "ibm/mvs/ihacde.h"

CLIBGRT *
__grtget(void)
{
    CLIBCRT     *crt    = __crtget();
    CLIBGRT     *grt    = crt ? crt->crtgrt : 0;

    return grt;
}
