/* @@FPGET.C - "*GETLINE": a stream read through the TMP's GETLINE (#467) */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "src/internal/bsam.h"

/* GETLINE hands back one line per call; @@aread cuts a longer one to
   this size, RDW included */
#define GETLINE_LRECL   1024

int
__fpget(FILE *fp)
{
    int     mode    = 0x80;     /* IOFTERM, input: GETLINE in @@aopen */
    int     recfm   = 1;        /* V */
    int     lrecl   = GETLINE_LRECL;
    int     blksize = GETLINE_LRECL;
    void    *asmbuf = 0;
    void    *dcb;

    /* read only: writing is "*PUTLINE" */
    if (!(fp->flags & _FILE_FLAG_READ) || (fp->flags & _FILE_FLAG_WRITE)) {
        errno = EINVAL;
        return 1;
    }

    /* @@aopen finds ECT and UPT through PSAAOLD -> ASCBASXB -> ASXBLWA;
       without a TMP there is no LWA and the open fails */
    dcb = __aopen("GETLINE ", &mode, &recfm, &lrecl, &blksize, &asmbuf, NULL);
    if ((int)dcb < 0) {
        errno = ((int)dcb == -12) ? ENOMEM : ENODEV;
        return 1;
    }

    fp->dcb     = dcb;
    fp->asmbuf  = asmbuf;
    fp->ungetch = -1;
    fp->flags  |= _FILE_FLAG_OPEN;
    fp->xflags |= _FILE_XFLAG_GETLINE;
    fp->recfm   = _FILE_RECFM_V;
    fp->lrecl   = GETLINE_LRECL;
    fp->blksize = GETLINE_LRECL;
    memcpy(fp->ddname, "GETLINE", 8);

    /* a line of up to LRECL - 4 bytes and the '\n' __fgetc() adds */
    fp->buf = calloc(1, GETLINE_LRECL - 4 + 8);
    if (!fp->buf) {
        errno = ENOMEM;
        return 1;               /* fclose() closes the open handle */
    }
    fp->upto   = fp->buf;
    fp->endbuf = fp->buf;       /* empty: the first read calls GETLINE */
    return 0;
}
