/* @@FWRITE.C - caller should hold lock on file handle */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <bsam.h>

#define begwrite(fp, len)   (lenwrite = (len), dptr = (fp)->asmbuf)
#define finwrite(fp)        (__awrite((fp)->dcb, &dptr, &lenwrite))

size_t
__fwrite(const void *vptr, size_t size, size_t nmemb, FILE *fp)
{
    unsigned char   *ptr    = (unsigned char *)vptr;
    size_t          i       = 0;
    int             err;
    size_t          j;
    unsigned char   *dptr;
    size_t          lenwrite;

    /* Fail fast (#149).  Once the stream has failed, nothing written from
       here on can reach the data set - but the bytes used to be accepted
       into the buffer and then dropped at @@fflush.c's reset: label.
       Measured on mvsdev JOB00252: 46 of 50 writes after an ENOSPC
       returned full length and not one of those records was on disk.
       Rejecting at the call is the whole difference.  clearerr() lifts
       it, which is what a caller who has freed space must do. */
    /* not open for writing (#189): record mode calls __awrite() directly,
       on what is an input DCB */
    if (!(fp->flags & _FILE_FLAG_WRITE)) {
        errno = EBADF;
        goto quit;
    }

    if (fp->flags & _FILE_FLAG_ERROR) {
        errno = (fp->flags & _FILE_FLAG_ENOSPC) ? ENOSPC : EIO;
        goto quit;
    }

    if (fp->flags & _FILE_FLAG_RECORD) {
        /* use record oriented i/o */
        size_t  max = fp->lrecl;

        /* C99 7.19.8.2: nothing to write, nothing written (#236) - a
           record of length 0 used to go out, blank on F */
        if (!size || !nmemb) goto quit;

        /* #236: the record must fit asmbuf (LRECL bytes) and what
           @@AWRITE takes without ABEND 002: on V the caller's RDW equal
           to the length, bytes 2-3 zero, at most LRECL-4 when spanned -
           the OPEN exit sets spanned whenever LRECL+4 does not fit the
           block.  A refused record is the caller's error, not the
           stream's: no error flag, the next record may be written. */
        if ((fp->recfm & _FILE_RECFM_TYPE) == _FILE_RECFM_V
            && (fp->recfm & _FILE_RECFM_S)) max -= 4;
        if (nmemb > max / size) {   /* size * nmemb > max, no overflow */
            errno = EINVAL;
            goto quit;
        }
        size *= nmemb;
        if ((fp->recfm & _FILE_RECFM_TYPE) == _FILE_RECFM_V
            && (size < 4 || (size_t)((ptr[0] << 8) | ptr[1]) != size
                || ptr[2] || ptr[3])) {
            errno = EINVAL;
            goto quit;
        }
        begwrite(fp, size);
        memcpy(dptr, ptr, size);
        if ((err = finwrite(fp)) != 0) {
            /* uncorrectable I/O error, recorded by the SYNAD exit
               instead of ABEND S001 (#147); 12 is the x37 exit and
               means out of space, not a device error (#176) */
            fp->flags |= _FILE_FLAG_ERROR;
            if (err == 12) fp->flags |= _FILE_FLAG_ENOSPC;
            errno = (err == 12) ? ENOSPC : EIO;
            goto quit;
        }
        fp->filepos += 1;       /* count record written */
        i = 1;
        goto quit;
    }

    /* slower but accurate implementation */
    for(i=0; i < nmemb; i++) {
        for(j=0; j < size; j++) {
            if (__fputc(*ptr++, fp)==EOF) {
                goto quit;
            }
        }
    }

quit:
    return i;
}
