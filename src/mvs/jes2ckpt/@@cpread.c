/* @@CPREAD.C - Read JES Checkpoint Dataset */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "mvs/wto.h"
#include "ibm/mvs/dcbd.h"
#include "ibm/mvs/iefjfcbn.h"
#include "mvs/jes2ckpt.h"     /* JES Checkpoint prototypes and functions            */

int __cpread(HASPCP *cp, void *buf4k)
{
    int         rc      = -1;
    unsigned    save10;         /* R10 across the READ, as in __jsrd4 */

    if (cp) {
        DCB         *dcb    = cp->dcb;
        /* a BSAM DECB is 20 bytes: READ fills the word at +16, which was
           past the end of decb[4] (#406) */
        unsigned    decb[8] = {0};

        if (dcb->dcboflgs & DCBOFOPN) {
            /* End of file and an I/O error come back as -1 through EODAD
            ** and SYNAD.  Without them CHECK abended (S337, S001) (#406).
            ** R10 points at rc in both exits; it is cc370's page-table
            ** register, so it is saved and restored (#427). */
            rc = 0;
            __asm("ST\t10,%0            save R10 (page table)\n\t"
                  "USING\tIHADCB,%2      ADDRESSING FOR DCB DSECT\n\t"
                  "MVC\tDCBSYNAD+1(3),=AL3(CPSYNAD)\n\t"
                  "MVC\tDCBEODAD+1(3),=AL3(CPEODAD)\n\t"
                  "DROP\t%2\n\t"
                  "LR\t10,%4            R10 => rc\n\t"
                  "READ  (%1),SF,(%2),(%3),4096,MF=E\n\t"
                  "CHECK (%1)\n\t"
                  "B\tCPDONE\n"
                  "CPEODAD  MVC   0(4,10),=F'-1'  end of file\n"
                  "CPDONE   L     10,%0         restore R10"
                  : "=m"(save10)
                  : "r"(decb), "r"(dcb), "r"(buf4k), "r"(&rc)
                  : "0", "1", "14", "15", "memory");
            __asm("\n"
                  "         B     CPQUIT\n"
                  "CPSYNAD  SYNADAF ACSMETH=BSAM\n"
                  "         MVC   0(4,10),=F'-1' I/O error\n"
                  "         SYNADRLS ,\n"
                  "         BR    14             back to CHECK's caller\n"
                  "CPQUIT   DS    0H" : : : "memory");
        }
    }

    return rc;
}

__asm("DCBD  DSORG=PS,DEVD=DA\n\t"
      "CSECT\t,");

