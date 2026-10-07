/* @@FPPUT.C - "*PUTLINE": a stream written through the TMP's PUTLINE (#463) */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "src/internal/bsam.h"

/* PUTLINE takes one line per call; 256 bytes with the RDW, as other TSO
   programs send.  A longer line is written as several. */
#define PUTLINE_LRECL   256

int
__fpput(FILE *fp)
{
    int     mode    = 0x81;     /* IOFTERM + IOFOUT: PUTLINE in @@aopen */
    int     recfm   = 1;        /* V */
    int     lrecl   = PUTLINE_LRECL;
    int     blksize = PUTLINE_LRECL;
    void    *asmbuf = 0;
    void    *dcb;

    /* write only: GETLINE in a batch TMP is not measured yet */
    if ((fp->flags & _FILE_FLAG_READ) || !(fp->flags & _FILE_FLAG_WRITE)) {
        errno = EINVAL;
        return 1;
    }

    /* @@aopen finds ECT and UPT through PSAAOLD -> ASCBASXB -> ASXBLWA;
       without a TMP there is no LWA and the open fails */
    dcb = __aopen("PUTLINE ", &mode, &recfm, &lrecl, &blksize, &asmbuf, NULL);
    if ((int)dcb < 0) {
        errno = ((int)dcb == -12) ? ENOMEM : ENODEV;
        return 1;
    }

    fp->dcb     = dcb;
    fp->asmbuf  = asmbuf;
    fp->ungetch = -1;
    fp->flags  |= _FILE_FLAG_OPEN | _FILE_FLAG_DCBOUT;
    fp->xflags |= _FILE_XFLAG_PUTLINE;
    fp->recfm   = _FILE_RECFM_V;
    fp->lrecl   = PUTLINE_LRECL;
    fp->blksize = PUTLINE_LRECL;
    memcpy(fp->ddname, "PUTLINE", 8);

    /* a line of up to LRECL - 4 bytes, the RDW is __fflush()'s */
    fp->buf = calloc(1, PUTLINE_LRECL - 4 + 8);
    if (!fp->buf) {
        errno = ENOMEM;
        return 1;               /* fclose() closes the open handle */
    }
    fp->upto   = fp->buf;
    fp->endbuf = fp->buf + PUTLINE_LRECL - 4;
    return 0;
}
