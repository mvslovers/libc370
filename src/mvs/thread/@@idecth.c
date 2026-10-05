#include <mvs/crt.h>
#include <mvs/wto.h>
#include <stddef.h>
#include <ibm/mvs/ihacde.h>
#include <mvs/thread.h>

__asm__("\n&FUNC    SETC 'clib_identify_cthread'");
int
clib_identify_cthread(void)
{
    int     rc = 0;

    /* CTHREAD does not already exist, identify it now */
    /* wtof("%s preparing to IDENTIFY CTHREAD", __func__); */

    __asm__("L     1,=V(CTHREAD)    A(thread driver routine)\n\t"
            "LA    0,=CL8'CTHREAD'\n\t"
            "IDENTIFY EPLOC=(0),ENTRY=(1)\n\t"
            "ST    15,%0\n" : "=m" (rc) : : "0", "1", "14", "15", "memory");
    /* rc is an OUTPUT: passed as an input pointer, the compiler kept the
       0 it was initialised with and returned that (#427) */
    __asm__("\n*\n");

    return rc;
}
