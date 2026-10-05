/* @@JSRD4.C - Read JES Spool Dataset using unsigned int MTTR value */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mvs/wto.h"
#include <stddef.h>
#include "mvs/dynalloc.h"
#include "ext/array.h"
#include "ibm/mvs/dcbd.h"
#include "ibm/mvs/iefjfcbn.h"
#include "mvs/jes2.h"   /* JES Spool prototypes and functions               */

#if 0
__asm("READ  HDECB2,DI,HASPACE,,0,0,QCDAD,MF=L");
#endif
int __jsrd4(HASPJS *js, unsigned mttr, void *buf4k, unsigned buflen)
{
    int         rc      = -1;
    DCB         *dcb;
    unsigned char *p     = buf4k;
    unsigned    work     = mttr;
    unsigned    cc       = 0;
    unsigned    hh       = 0;
    unsigned    decb[8]  = {0};
    unsigned char block[8] = "";
    unsigned    save10;         /* R10 across the READ (#427) */

    if (!js) goto quit;

    dcb = js->dcb;
    if (!dcb) goto quit;

    dcb->dcbblksi = (unsigned short) buflen;

    __asm("XC\t0(8,%4),0(%4)  Initialize MBBCCHHR to zero\n\t"
          "LR\t1,%0           Load MTTR value\n\t"
          "STC\t1,7(%4)       Save R value\n\t"
          "SRL\t1,8           Shift TT value\n\t"
          "N\t1,=F'65535'     Keep just the TT value\n\t"
          "SR\t0,0            Prepare for divide\n\t"
          "DR\t0,%1           Divide by trks per cyl\n\t"
          "ST\t0,0(,%2)       Store head number\n\t"
          "ST\t1,0(,%3)       Store cylinder number\n\t"
          "MVC\t3(2,%4),2(%3)  Copy cylinder number\n\t"
          "MVC\t5(2,%4),2(%2)  Copy head number"
          : :"r"(mttr), "r"(js->trkcyl), "r"(&hh), "r"(&cc), "r"(block) : "memory");
#if 0
    wtof("mttr=%08X, trkcyl=%u, cc=%u, hh=%u", mttr, js->trkcyl, cc, hh);
    wtodumpf(block, 8, "mbbcchhr");
#endif

    rc = 0;
    /* The SYNAD exit below stores the error into rc through R10, which
    ** survives into the exit.  R10 is also cc370's page-table register, so
    ** it is saved here and restored after the CHECK: clobbering it left
    ** the following "L 12,0(,10)" loading rc into the base register (#427). */
    __asm("ST\t10,%0            save R10 (page table)\n\t"
          "USING\tIHADCB,%1      ADDRESSING FOR DCB DSECT\n\t"
          "MVC\tDCBSYNAD+1(3),=AL3(SYNAD)  SET SYNAD ADDR IN DCB\n\t"
          "DROP\t%1             DROP ADDRESSING FOR DCB\n\t"
          "LR\t10,%6            R10 => rc\n\t"
          "READ\t(%2),DI,(%1),(%3),(%4),,(%5),MF=E\n\t"
          "CHECK\t(%2)\n\t"
          "L\t10,%0             restore R10\n\t"
          "B\tQUIT"
          : "=m"(save10)
          : "r"(js->dcb), "r"(decb), "r"(buf4k), "r"(buflen), "r"(block), "r"(&rc)
          : "0", "1", "14", "15", "memory");

    __asm("\n"
          "SYNAD    SYNADAF ACSMETH=BDAM DECODE ERROR CAUSE\n"
          "         L     1,128(,1)      Get DECB address\n"
          "         L     0,0(,1)        Get DECB ECB value\n"
          "         ST    0,0(,10)       Save ECB value as return code\n"
          "         SYNADRLS ,           RELEASE WORK AREA\n"
          "         BR    14             RETURN TO OP SYS\n"
          "QUIT     DS    0H" : : : "memory");

quit:
#if 0
    wtof("__jsrd4() rc=%08X, (%d)", rc, rc);
#endif
    return rc;
}
__asm("DCBD  DSORG=DA,DEVD=DA\n\t"
      "CSECT\t,");

