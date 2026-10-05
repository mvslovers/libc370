#include <stddef.h>
#include <ibm/mvs/ihacde.h>
#include <mvs/wto.h>
#include <mvs/apf.h>
#include <mvs/apf.h>
#include <errno.h>

__asm__("\n&FUNC    SETC '__pswkey'");
int __pswkey(unsigned char *savekey)
{
    int     rc  = 0;

    if (!__isauth()) return EPERM;    /* operation not permitted  */

    if (__issup()) {
        /* get current PSW key */
        __asm__("IPK\t0\n\tSTC\t2,0(,%0)" : : "r"(savekey) : "2", "memory");
        goto quit;
    }

    /* switch to supervisor mode */
    __super(PSWKEYNONE, savekey);

    /* switch back to problem mode */
    __prob(PSWKEYNONE, (void*)0);

quit:
    return rc;
}
