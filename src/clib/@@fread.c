/* @@FREAD.C - caller should hold lock on file handle */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mvssupa.h>

size_t
__fread(void *ptr, size_t size, size_t nmemb, FILE *fp)
{
    unsigned char   *p      = (unsigned char*)ptr;
    size_t          lenread = 0;
    size_t          i       = nmemb;
    size_t          j;
    int             c;
    unsigned char   *dptr;

    if (fp->flags & _FILE_FLAG_EOF) {
        i = 0;
        goto quit;
    }

    if (fp->flags & _FILE_FLAG_RECORD) {
        int rc;
        size *= nmemb;
        rc = __aread(fp->dcb, &dptr, &lenread);
        if (rc != 0) {
            if (rc > 0) {
                /* uncorrectable I/O error, recorded by the SYNAD
                   exit instead of ABEND S001 (#147) */
                fp->flags |= _FILE_FLAG_ERROR;
                errno = EIO;
            }
            else {
                /* end of file */
                fp->flags |= _FILE_FLAG_EOF;
            }
            i = 0;
            goto quit;
        }
        /* success, return data */
        i = nmemb;
        if (size > lenread) size = lenread;
        memcpy(ptr, dptr, size);
        fp->filepos += 1;   /* count records read */
        goto quit;
    }

    /* slower but accurate implementation */
    for(i=0; i < nmemb; i++) {
        for(j=0; j < size; j++) {
            c = __fgetc(fp);
            if (c==EOF) goto quit;
            *p++ = (unsigned char)c;
        }
    }

quit:
    return i;
}
