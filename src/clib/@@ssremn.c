#include <mvs/subsys.h>
#include <stddef.h>
#include <ibm/mvs/ihacde.h>
#include <mvs/wto.h>

__asm__("\n&FUNC    SETC 'ssct_remove_by_name'");
int ssct_remove_by_name(const char *name)
{
    SSCT            *ssct       = ssct_find(name);      /* get subsystem            */
    int             rc          = 4;                    /* assume not found         */

    if (ssct) {
        rc = ssct_remove(ssct);
    }

    return rc;
}
