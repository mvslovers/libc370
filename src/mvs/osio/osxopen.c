/* OSXOPEN.C - open DD for EXCP access */
#include <ctype.h>
#include <stdlib.h>
#include "mvs/osio.h"
#include "mvs/recovery.h"
#include <mvs/dd.h>

static void opendcb(DCB *dcb, int typej, unsigned opt, int *rc);

int
osxopen(DCB *dcb, int typej)
{
    return osxopenm(dcb, typej, NULL);
}

/* mode: NULL or "u" opens for UPDAT, as osxopen() always did; "r" for
   INPUT, which a data set the program may only read needs (#412) */
int
osxopenm(DCB *dcb, int typej, const char *mode)
{
    int         rc   = 8;   /* not open, also when OPEN abended */
    JFCB        jfcb = {0};
    unsigned    opt;

    /* no DCB: OPEN was given address 0, and DCBOFLGS was read from low
       storage, with rc left unset (#412) */
    if (!dcb) return 8;

    switch (mode ? mode[0] : 'u') {
    case 'r': case 'R': case 'i': case 'I':
        opt = 0x00;             /* INPUT */
        break;
    case 'u': case 'U':
        opt = 0x04;             /* UPDAT */
        break;
    default:
        return 8;               /* no such mode: not opened */
    }

    /* use try() in case of abend */
    try(opendcb, dcb, typej, opt, &rc);

    /* make sure the dataset was opened */
    if (!(dcb->dcboflg & DCBOFOPN)) {
        rc = 8;    /* dataset not open */
    }

    if (rc) {
        goto quit;
    }

    /* DCB's for EXCP access do not fill in the BSAM/QSAM related fields
    ** so we'll use RDJFCB to get those field values and populate the
    ** remaining DCB fields that we might find useful.
    */

    /* get dataset information */
    rdjfcb(dcb, &jfcb);

    dcb->dcbblksi = jfcb.jfcblksi;
    dcb->dcblrecl = jfcb.jfclrecl;

quit:
    return rc;
}

__asm__("\n&FUNC    SETC 'opendcb'");
static void
opendcb(DCB *dcb, int typej, unsigned opt, int *rc)
{
    /* the last entry, the open option, the DCB */
    unsigned    plist   = 0x80000000 | (opt << 24) | (unsigned)dcb;

    if (!typej) {
        /* normal OPEN for a single dataset */
        __asm__("LR    1,%1\n\tSVC   19         OPEN\n\tST\t15,0(,%0)"
            : : "r"(rc), "r"(&plist) : "0", "1", "14", "15", "memory");
    }
    else {
        /* OPEN type J for a single dataset */
        __asm__("LR    1,%1\n\tSVC   22         OPENJ\n\tST\t15,0(,%0)"
            : : "r"(rc), "r"(&plist) : "0", "1", "14", "15", "memory");
    }

    return;
}
