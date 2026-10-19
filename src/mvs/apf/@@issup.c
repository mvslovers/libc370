#include <stddef.h>
#include <ibm/mvs/ihacde.h>
#include <mvs/wto.h>
#include <mvs/apf.h>
#include <mvs/apf.h>
#include <errno.h>

__asm__("\n&FUNC    SETC '__issup'");
int __issup(void)
{
    int     rc  = 0;

    /* check for SUPERVISOR mode */
    __asm__("TESTAUTH\tFCTN=0,STATE=YES,KEY=NO,RBLEVEL=1\n\tST\t15,0(,%0)" : : "r"(&rc) : "0", "1", "14", "15", "memory");
    if (rc==0) {
        return 1;   /* currently in SUPERVISOR mode */
    }

    return 0;   /* not in SUPERVISOR mode */
}
