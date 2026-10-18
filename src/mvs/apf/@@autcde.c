#include <mvs/crt.h>
#include <mvs/wto.h>
#include <stddef.h>
#include <ibm/mvs/ihacde.h>
#include <mvs/apf.h>
#include <errno.h>
#include "src/internal/svkey0.h"

__asm__("\n&FUNC    SETC 'clib_auth_cde'");
int
clib_auth_cde(CDE *cde)
{
    int     rc = 0;

    if (cde) {
        unsigned st;

        /* unauthorized in problem state: EPERM, not S047; and back to the
           caller's own state and key afterwards (#402) */
        if (!__issup() && !__isauth()) return EPERM;
        st = svkey0();
        cde->CDATTR2 |= (CDSYSLIB | CDAUTH);
        svrestore(st);
    }

    return rc;
}
