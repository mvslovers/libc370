#include <stddef.h>
#include <string.h>
#include "mvs/vsam.h"

/* __vsfdbk() - the outcome of the last request on the handle (#625).

   VSAM sets the RPL's feedback word, RECLEN and RBA on every request, so
   they describe the last GET, PUT, POINT or ERASE and nothing earlier --
   unlike vs->rc/vs->rsn, which only the error exits write and which stay
   until vsclear().  The return code byte of the feedback word is the R15
   VSAM gives back (the library's exit routines return their own codes
   in R15, so R15 itself is not kept). */
int
__vsfdbk(VSFILE *vs, VSFDBK *fb)
{
    unsigned        pl[16]  = {0};
    unsigned        area[2];        /* RECLEN, RBA */
    int             rc      = 0;

    if (!vs || !fb) return -1;

    memset(fb, 0, sizeof(*fb));
    area[0] = area[1] = 0;

    /* return code, component and reason code: the RPL's feedback word.
       SHOWCB FIELDS=FDBK gives the reason code alone on MVS 3.8j, with
       zero where the RPL holds the return code (measured, #625) */
    fb->r15     = vs->rpl.rplrtncd;
    fb->comp    = vs->rpl.rplcmpon;
    fb->fdbk    = vs->rpl.rplerrcd;

    __asm__("\n"
    "\tSHOWCB RPL=(%1),                                               X\n"
    "\t\tFIELDS=(RECLEN,RBA),AREA=(%2),LENGTH=8,MF=(G,(%3))\n\t"
            "ST\t15,%0"
        : "=m"(rc) : "r"(&vs->rpl), "r"(area), "r"(pl)
        : "0", "1", "14", "15", "memory");
    if (rc) return rc;

    fb->reclen  = area[0];
    fb->rba     = area[1];

    return 0;
}
