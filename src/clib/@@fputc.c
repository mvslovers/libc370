/* @@FPUTC.C - caller should hold lock on file handle */
#include <fileio.h>
#include <stdio.h>
#include <errno.h>

int
__fputc(int c, FILE *fp)
{
    int     rc      = c;

    /* not open for writing (#189): this used to return c - success */
    if (!(fp->flags & _FILE_FLAG_WRITE)) {
        errno = EBADF;
        rc = EOF;
        goto quit;
    }
    if (fp->flags & _FILE_FLAG_RECORD) goto quit;   /* in record mode    */

    /* Fail fast (#149).  fprintf(), fputs() and puts() all reach the
       access method through here and not through __fwrite(), so the
       guard has to be on both. */
    if (fp->flags & _FILE_FLAG_ERROR) {
        errno = (fp->flags & _FILE_FLAG_ENOSPC) ? ENOSPC : EIO;
        rc = EOF;
        goto quit;
    }

    /* a '+' stream whose DCB is reading (#189): open UPDAT, the byte is
       written over in place; otherwise the DCB turns round to the end */
    if (!(fp->flags & _FILE_FLAG_DCBOUT)) {
        if (fp->xflags & _FILE_XFLAG_UPDAT) {
            int r = __fpupc(fp, c);
            if (r < 0) {
                rc = EOF;
                goto quit;
            }
            if (r == 0) goto quit;      /* written in place */
            /* r == 1: at the end, the DCB now writes - carry on */
        }
        else if (__fpswt(fp, 1)) {
            rc = EOF;
            goto quit;
        }
    }

    /* is this a text file? */
    if (!(fp->flags & _FILE_FLAG_BINARY)) {
        /* yes, is this character a newline? */
        if (c == '\n') {
            /* yes, end the record - an empty one too (#199) */
            fp->filepos++;
            if (__fflnl(fp)) rc = EOF;
            goto quit;
        }
    }

    /* if buffer is full flush buffer to disk */
    if (fp->upto == fp->endbuf) {
        if (__fflush(fp)) {
            rc = EOF;
            goto quit;
        }
    }

    /* otherwise we copy the character to the buffer */
    *fp->upto++ = (unsigned char)c;
    fp->filepos++;

quit:
    return rc;
}
