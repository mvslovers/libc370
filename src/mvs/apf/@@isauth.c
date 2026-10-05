/* @@ISAUTH.C */
#include "mvs/apf.h"

int
__isauth(void)
{
    int     rc      = 0;

    /* check for APF authorization */
    __asm__("TESTAUTH\tFCTN=1,STATE=NO,KEY=NO,RBLEVEL=1\n\tST\t15,%0" : "=m" (rc) : : "memory");
    if (rc==0) {
        rc = 1;
    }
    else {
        rc = 0;
    }

    return rc;
}
