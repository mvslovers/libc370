#include <mvs/crt.h>
#include <mvs/wto.h>
#include <stddef.h>
#include <ibm/mvs/ihacde.h>
#include <mvs/apf.h>

__asm__("\n&FUNC    SETC 'clib_auth_cde'");
int
clib_auth_cde(CDE *cde)
{
    int     rc = 0;

    if (cde) {
        __asm__("MODESET KEY=ZERO,MODE=SUP\n" : : : "0", "1", "14", "15");
        cde->CDATTR2 |= (CDSYSLIB | CDAUTH);
        __asm__("MODESET KEY=NZERO,MODE=PROB" : : : "0", "1", "14", "15");
    }

    return rc;
}
