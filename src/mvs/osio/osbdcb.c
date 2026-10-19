/* OSBDCB.C - allocate a DCB for BSAM access */
#include <stdio.h>
#include <stdlib.h>
#include "mvs/osio.h"

/* the library's default exits, in oscheck.c (#412) */
extern void __osdfx(DCB *dcb, int eodad)    asm("@@OSDFX");

/* the DD name and the exit list, in @@osfill.c (#412) */
extern DCB *__osfill(DCB *dcb, const char *ddname, EXITLIST *exitlist)
    asm("@@OSFILL");

DCB *
osbdcb(const char *ddname, EXITLIST *exitlist)
{
    DCB     *dcb    = calloc(1, sizeof(DCB));

    if (!dcb) return NULL;

    __asm__("MVC   0(PROTOLEN,%0),PROTODCB" : : "r"(dcb) : "memory");

    /* end of data and an I/O error come back from oscheck() as 4 and 8,
       not as S337 and S001.  A caller's own exit, stored after this,
       wins (#412). */
    __osdfx(dcb, 1);

    return __osfill(dcb, ddname, exitlist);
}
__asm__("\n"
"PROTODCB DCB DSORG=PS,MACRF=R\n"
"PROTOLEN EQU *-PROTODCB");

#if 0
#define OFF(a) (unsigned) &(((DCB*)0)->a)

int
main(int argc, char**argv)
{
    wtof("DCBDDNAM  %02X", OFF(dcbddnam));
    wtof("DCBTIOT   %02X", OFF(dcbtiot));
    wtof("DCBGERR   %02X", OFF(dcbgerr));
    wtof("DCBIOBA   %02X", OFF(dcbioba));
    wtof("DCBDIRCT  %02X", OFF(dcbdirct));
    wtof("DCBLRECL  %02X", OFF(dcblrecl));
    wtof("DCBEOB    %02X", OFF(dcbeob));
    wtof("DCB       %02X", sizeof(DCB));
}
#endif
