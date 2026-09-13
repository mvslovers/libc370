/* @@FPOLD.C */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <mvssupa.h>
#include "svc99.h"
#include "clibary.h"

int
__fpold(FILE *fp)
{
    int         err     = 1;
    unsigned    count   = 0;
    TXT99       **txt99 = NULL;
    RB99        rb99    = {0};

    /* we want the DDNAME returned to us */
    err = __txrddn(&txt99, NULL);
    if (err) goto quit;

    /* allocate this dataset */
    err = __txdsn(&txt99, fp->dataset);
    if (err) goto quit;

    /* DISP=OLD */
    err = __txold(&txt99, NULL);
    if (err) goto quit;

    /* SPACE=(,,RLSE), on request (#167).  It belongs on THIS DD and nowhere
       else: RLSE is honoured at CLOSE of the DCB opened against the DD that
       carried it, and fclose() runs __aclose() before __fpfree() drops the
       DD.  A caller that allocates its own DD, frees it and then fopen()s the
       data set by name - which is what mvslovers/ftpd does - can only get
       partial release from here.
       Not for a PDS member: fopen() falls through to __fpold() when __fpshr()
       fails, and partial release on a PO data set takes the space the next
       member needs. */
    if ((fp->flags & _FILE_FLAG_RLSE) && !fp->member[0]) {
        err = __txrlse(&txt99, NULL);
        if (err) goto quit;
    }

    count = arraycount(&txt99);
    if (!count) goto quit;

    /* Set high order bit to mark end of list */
    count--;
    txt99[count]    = (TXT99*)((unsigned)txt99[count] | 0x80000000);

    /* construct the request block for dynamic allocation */
    rb99.len        = sizeof(RB99);
    rb99.request    = S99VRBAL;
    rb99.flag1      = S99NOCNV;
    rb99.txtptr     = txt99;

    /* SVC 99 */
    err = __svc99(&rb99);
    if (err) goto quit;

    /* return DDNAME */
    memcpy(fp->ddname, txt99[0]->text, 8);
    fp->flags |= _FILE_FLAG_DYNAMIC;

quit:
    if (txt99) FreeTXT99Array(&txt99);

    return err;
}
