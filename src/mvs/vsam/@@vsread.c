#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "mvs/vsam.h"

int
__vsread(VSFILE *vs, void *rec, int reclen, void *key, int keylen)
{
    unsigned    pl[16]  = {0};
    int         rc      = 0;
    unsigned char had;
    unsigned char got;

    if (key && keylen) {
        /* position to desired record */
        __vssteq(vs, rec, reclen, key, keylen);
    }
    else {
        /* modify common RPL parameters */
        __vsmdfy(vs, rec, reclen, key, keylen);
    }

    /* EOF and ERROR stay set for vseof()/vserror() until vsclear(), as
       feof()/ferror() do, but what this call returns must say what THIS
       GET did: an error once must not fail every later read (#411) */
    had = vs->flags & (VSFILE_FLAG_EOF | VSFILE_FLAG_ERROR);
    vs->flags &= (0xFF - VSFILE_FLAG_WRITE - VSFILE_FLAG_EOF
                  - VSFILE_FLAG_ERROR);

    /* get record from VSAM dataset.  The EODAD and SYNAD exits set
       vs->flags while the GET runs: "memory" makes the compiler read the
       flags again afterwards.  Without it, cc370 -Os tested a copy taken
       before the GET and saw the end of file one call late (#426). */
    __asm__("LA\t2,GETDONE\n\t"
            "GET\tRPL=(%1)\n"
   "GETDONE\tDS\t0H\n\t"
            "ST\t15,%0"
        : "=m"(rc) : "r"(&vs->rpl) : "0", "1", "2", "14", "15", "memory");

    got = vs->flags & (VSFILE_FLAG_EOF | VSFILE_FLAG_ERROR);
    if ((got & VSFILE_FLAG_ERROR) && vs->rsn == 4) {
        /* A GET after the end of data is a logical error to VSAM (LERAD,
           feedback X'04', measured on MVS): it is still end of file */
        got = VSFILE_FLAG_EOF;
        vs->flags = (vs->flags & (0xFF - VSFILE_FLAG_ERROR)) | VSFILE_FLAG_EOF;
    }
    vs->flags |= had;

    if (got & VSFILE_FLAG_ERROR) {
        /* SYNAD exit has indicated an error */
        errno = EVSERROR;
        reclen = -2;
        goto quit;
    }
    if (got & VSFILE_FLAG_EOF) {
        /* EODAD exit has indicated end of file */
        reclen = -1;
        goto quit;
    }

    /* get length of record just read (SHOWCB stores it into reclen) */
    __asm__("SHOWCB RPL=(%0),FIELDS=RECLEN,AREA=(%1),LENGTH=4,MF=(G,(%2))"
        : : "r"(&vs->rpl), "r"(&reclen), "r"(pl)
        : "0", "1", "14", "15", "memory");

quit:
    return reclen;
}
