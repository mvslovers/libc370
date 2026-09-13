/* @@FWRITE.C - caller should hold lock on file handle */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mvssupa.h>

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

    if (fp->flags & _FILE_FLAG_RECORD) {
        /* use record oriented i/o */
        size *= nmemb;
        begwrite(fp, size);
        memcpy(dptr, ptr, size);
        if ((err = finwrite(fp)) != 0) {
            /* uncorrectable I/O error, recorded by the SYNAD exit
               instead of ABEND S001 (#147); 12 is the x37 exit and
               means out of space, not a device error (#176) */
            fp->flags |= _FILE_FLAG_ERROR;
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
