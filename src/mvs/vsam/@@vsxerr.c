#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "mvs/vsam.h"

/* This is the VSAM common error handler for LERAD and SYNAD errors.

   The return, component and reason codes come from the RPL feedback
   word, which VSAM sets before it takes the exit.  SHOWCB FIELDS=FDBK,
   used up to 2.23.0, gives the reason code alone on MVS 3.8j, so rc
   stayed 0 (#680, measured). */
int
__vsxerr(RPL *rpl, int code)
{
    VSFILE          *vs     = (VSFILE*)((char*)rpl - offsetof(VSFILE,rpl));
    int (*reset)(VSFILE *);

    vs->flags   |= VSFILE_FLAG_ERROR;

    vs->rc          = rpl->rplrtncd;
    vs->component   = rpl->rplcmpon;
    vs->rsn         = rpl->rplerrcd;

    if (vs->vsreset) {
        if ((vs->flags & VSFILE_FLAG_STARTGE) ||
            (vs->flags & VSFILE_FLAG_WRITE)) {
            reset = vs->vsreset;
            reset(vs);
        }
    }

    return code;
}
