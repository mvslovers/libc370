/* @@FPMODE.C */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

int
__fpmode(FILE *fp, const char *mode)
{
    int     err     = 1;
    int     i;
    int     j;

    /* mode="(r|w|a)[b][+][b][,options]" - '+' opens for update (#189) */
    memset(fp->mode, 0, sizeof(fp->mode));
    for(i=0;i < sizeof(fp->mode) && *mode; i++, mode++) {
        fp->mode[i] = tolower(*mode);
        switch (fp->mode[i]) {
        case 'a':
            if (fp->flags & _FILE_FLAG_READ) goto quit;
            fp->flags |= _FILE_FLAG_WRITE;
            fp->flags |= _FILE_FLAG_APPEND;
            break;
        case 'b':
            fp->flags |= _FILE_FLAG_BINARY;
            break;
        case 'r':
            if (fp->flags & _FILE_FLAG_WRITE) goto quit;
            if (fp->flags & _FILE_FLAG_APPEND) goto quit;
            fp->flags |= _FILE_FLAG_READ;
            break;
        case 'w':
            if (fp->flags & _FILE_FLAG_READ) goto quit;
            fp->flags |= _FILE_FLAG_WRITE;
            break;
        case '+':
            /* r+, w+, a+: the caller may read and write.  The letter
               before it says how the data set is opened - see check: */
            if (!(fp->flags & (_FILE_FLAG_READ | _FILE_FLAG_WRITE))) goto quit;
            fp->flags |= _FILE_FLAG_READ | _FILE_FLAG_WRITE;
            break;
        case ',':
            /* copy remaining mode string to file handle mode */
            for(j=i; j < sizeof(fp->mode) && *mode; j++) {
                fp->mode[j] = tolower(*mode++);
            }
            if (strstr(&fp->mode[i],"record")) {
                /* wtof("__fpmode found record"); */
                fp->flags |= _FILE_FLAG_RECORD;
            }
            if (strstr(&fp->mode[i],"bsam")) {
                /* wtof("__fpmode found bsam"); */
                fp->flags |= _FILE_FLAG_BSAM;
            }
            if (strstr(&fp->mode[i],"rlse")) {
                /* SPACE=(,,RLSE) on the dynamic allocation (#167) */
                fp->flags |= _FILE_FLAG_RLSE;
            }
            goto check;
        }
    }

check:
    if (!(fp->flags & (_FILE_FLAG_READ | _FILE_FLAG_WRITE))) goto quit;

    if ((fp->flags & _FILE_FLAG_READ) && (fp->flags & _FILE_FLAG_WRITE)) {
        /* '+' works on the byte stream; record i/o has no position to
           switch direction at */
        if (fp->flags & _FILE_FLAG_RECORD) goto quit;
        /* "a+": the position is the end, not yet counted */
        if (fp->flags & _FILE_FLAG_APPEND) fp->flags |= _FILE_FLAG_POSEND;
        /* only the first open of "w+" may truncate: every output open of
           "r+" and "a+" is EXTEND.  Without this the first write on an
           "r+" stream opened OUTPUT and emptied the data set */
        if (fp->mode[0] != 'w') fp->flags |= _FILE_FLAG_EXTEND;
    }

    /* the direction the DCB opens in first: "w", "a", "w+" and "a+"
       write, "r" and "r+" read */
    if ((fp->flags & _FILE_FLAG_WRITE)
        && !((fp->flags & _FILE_FLAG_READ) && fp->mode[0] == 'r')) {
        fp->flags |= _FILE_FLAG_DCBOUT;
    }

    /* success */
    err = 0;

quit:
    return err;
}
