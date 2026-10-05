#include <stddef.h>
#include <ibm/mvs/ihacde.h>
#include <mvs/wto.h>
#include <mvs/link.h>

__asm__("\n&FUNC    SETC '__call'");
int
__call(void *func, void *plist)
{
    int         rc;

    __asm__(
    "LR\t15,%1          => function to call \n\t"
    "LR\t1,%2           => parameter list\n\t"
    "BALR\t14,15         call function\n\t"
    "LR\t%0,15          save return code"
    : "=r" (rc)
    : "r" (func), "r" (plist)
    : "0", "1", "14", "15", "memory");

    return rc;
}
