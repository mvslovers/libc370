/* RWRITE.C - write dataset record */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <mvssupa.h>
#include "rfile.h"
#include "osdcb.h"

int
rwrite(RFILE *fp, const void *ptr, size_t size)
{
    int                 err     = 0;
    unsigned char       *dptr   = fp->asmbuf;
    const unsigned char *rec    = ptr;
    DCB                 *dcb    = fp->hfile;
    size_t              max     = (size_t)fp->lrecl;

    if (!size) size = fp->lrecl;

    /* #232: @@AWRITE abends 002 on a V record it cannot take, and asmbuf
       holds LRECL bytes.  The OPEN exit sets spanned whenever LRECL+4 does
       not fit the block, so a non-spanned record of up to LRECL always
       fits; a spanned one @@AWRITE limits to LRECL-4. */
    if (fp->recfm == RFILE_RECFM_V && (dcb->dcbrecfm & DCBRECSB)) max -= 4;
    if (size > max) {
        errno = EINVAL;
        return 1;
    }
    if (fp->recfm == RFILE_RECFM_V) {
        if (size < 4 || (size_t)((rec[0] << 8) | rec[1]) != size
            || rec[2] || rec[3]) {
            errno = EINVAL;
            return 1;
        }
    }

    memcpy(dptr, ptr, size);
    err = __awrite(fp->hfile, &dptr, &size);
    if (err) {
        /* 12 is the x37 exit, out of space; 8 an I/O error (#147, #176) */
        errno = (err == 12) ? ENOSPC : EIO;
        err = 1;
    }

    return err;
}
