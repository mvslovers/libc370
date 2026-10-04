/* @@FPOPEN.C */
#include "src/internal/fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include "src/internal/bsam.h"
#include "ibm/mvs/dcbd.h"
#include "ibm/mvs/iefjfcbn.h"
#include <mvs/dd.h>
#include "ibm/mvs/ieftiot1.h"

/* Is DD ddname a DUMMY?  Its JFCB names NULLFILE, whether the DD is a JCL
   DD DUMMY or 'NULLFILE' allocated by name.  Read through the TIOT entry's
   SWA pointer, as __listal() reads it, so it is known BEFORE the OPEN -
   __rdjfcb() needs an open DCB (#277). */
static int is_dummy(const char *ddname)
{
    static const char nullfile[9] = "NULLFILE ";
    DSAB    *dsab   = get_dsab(0, ddname);
    TIOTDD  *tiotdd;
    JFCB    *jfcb;
    int     i;

    if (!dsab || !(tiotdd = dsab->dsabtiot)) return 0;
    jfcb = (JFCB *)(((unsigned char)tiotdd->TIOEJFCB[0] << 16
                     | (unsigned char)tiotdd->TIOEJFCB[1] << 8
                     | (unsigned char)tiotdd->TIOEJFCB[2]) + 16);
    for (i = 0; i < 9; i++) {
        if (jfcb->jfcbdsnm[i] != nullfile[i]) return 0;
    }
    return 1;
}

__asm__("\n&FUNC    SETC '__fpopen'");
int
__fpopen(FILE *fp)
{
    int     err     = 1;
    int     i       = 0;
    int     mode    = 0;    /* 0==read, 1==write */
    int     want;
    int     recfm   = 0;    /* 0==fixed, 1==variable, 2==undefined */
    int     lrecl   = 0;
    int     blksize = 0;
    DCB     *dcb    = 0;
    void    *asmbuf = 0;
    char    *pmember= 0;
    JFCB    *j      = 0;
    char    ddname[8] = {0};
    char    member[8] = {0};
    JFCB    jfcb    = {0};

    if (!fp) goto quit;
    if (!fp->ddname[0]) goto quit;

    for(i=0;i<8 && fp->ddname[i]; i++) {
        ddname[i] = fp->ddname[i];
    }
    for(;i<8; i++) ddname[i] = ' ';

    /* #184: a second concurrent DCB on a spool SYSIN is refused by JES2
       with ABEND S013-C0, which kills the address space instead of the
       call.  Tell the caller instead.  __ddbusy() answers 0 for anything
       it cannot establish, so nothing that used to open stops opening. */
    if (__ddbusy(fp)) {
        errno = EBUSY;
        goto quit;
    }

    /* "a" is OPEN EXTEND, mode 3 (#198).  It used to be mode 1, OUTPUT,
       which overwrote the data set from the start: "a" was "w" except on a
       DISP=MOD DD (mvsdev JOB00490).  __aopen() takes EXTEND on DASD and
       tape and quietly makes it OUTPUT on anything else (SYSOUT, units). */
    if (fp->flags & _FILE_FLAG_DCBOUT) {
        /* EXTEND also for any output open of a '+' stream after its first:
           a direction switch must not truncate (#189) */
        mode = (fp->flags & (_FILE_FLAG_APPEND | _FILE_FLAG_EXTEND)) ? 3 : 1;
    }
    else if ((fp->flags & _FILE_FLAG_WRITE) && !(fp->flags & _FILE_FLAG_APPEND)
             && !(fp->member[0] > ' ')) {
        /* "r+"/"w+" reading: UPDAT, so that a write can overwrite the
           record just read in place (#189).  Not "a+" - it writes at the
           end only - and not a member, which BPAM cannot update */
        mode = 2;
    }
    fp->xflags &= ~(_FILE_XFLAG_UPDAT | _FILE_XFLAG_DIRTY);
    if (fp->flags & _FILE_FLAG_BSAM)  mode = mode + 8;

    if (fp->member[0] > ' ') {
        for(i=0;i<8 && fp->member[i];i++) {
            member[i] = fp->member[i];
        }
        for(;i<8;i++) {
            member[i] = ' ';
        }
        pmember = member;
    }

    switch(fp->recfm & _FILE_RECFM_TYPE) {
    case _FILE_RECFM_F:     /* ... FIXED RECORD LENGTH              */
        recfm = 0;
        break;
    case _FILE_RECFM_V:     /* ... VARIABLE RECORD LENGTH           */
        recfm = 1;
        break;
    case _FILE_RECFM_U:     /* ... UNDEFINED RECORD LENGTH          */
        recfm = 2;
        break;
    }

    lrecl = fp->lrecl;
    blksize = fp->blksize;

    /* A DUMMY read - 'NULLFILE', @@start's stdin when the step has no
       SYSIN, or a JCL DD DUMMY - has no DCB attributes, and __aopen() then
       opened it LRECL=BLKSIZE=32760 and GETMAINed a buffer of each: 64 K
       for a stream that never transfers a byte (#277, mvsdev JOB01349).
       Ask for 80-byte records; the DCB open exit takes them where the DD
       has none. */
    if (!(fp->flags & _FILE_FLAG_WRITE) && !lrecl && !blksize
        && is_dummy(fp->ddname)) {
        recfm   = 0;
        lrecl   = 80;
        blksize = 80;
    }

    /* open dataset */
    want = mode;            /* __aopen() hands the mode back, amended */
    fp->dcb = __aopen(ddname, &mode, &recfm, &lrecl,
                &blksize, &asmbuf, pmember);

    if ((want & 7) == 2) {
        if ((int)fp->dcb < 0) {
            /* UPDAT refused - a member named in the JCL, say: read only,
               and a write in the middle stays EOPNOTSUPP */
            mode = want - 2;
            fp->dcb = __aopen(ddname, &mode, &recfm, &lrecl,
                        &blksize, &asmbuf, pmember);
        }
        else {
            fp->xflags |= _FILE_XFLAG_UPDAT;
        }
    }

    if ((int)fp->dcb < 0) {
        /* -45: EXTEND refused for a PDS member, the one named in the
           JCL included - OPEN would take it and CLOSE would abend
           B14-04 (#198, mvsdev JOB00538) */
        if ((int)fp->dcb == -45) errno = EOPNOTSUPP;
        /* -12: no storage for __aopen()'s buffers (#83) - say so, the
           caller had errno 0 to go on (#254) */
        if ((int)fp->dcb == -12) errno = ENOMEM;
        goto quit;
    }

    /* success */
    fp->flags   |= _FILE_FLAG_OPEN;
    fp->asmbuf  = asmbuf;
    fp->ungetch = -1;

    /* get the recfm, lrecl, and blksize from the DCB */
    dcb = fp->dcb;

    if ((fp->flags & _FILE_FLAG_READ) && (fp->flags & _FILE_FLAG_WRITE)) {
        /* '+' needs a device it can reopen the other way and position on:
           DASD.  Not SYSOUT, not a terminal, not tape (#189) */
        if ((dcb->dcbdevt & 0xF0) != 0x20) {
            __aclose(fp->dcb);
            fp->dcb     = 0;
            fp->asmbuf  = 0;
            fp->flags  &= ~_FILE_FLAG_OPEN;
            errno       = EINVAL;
            err         = 1;
            goto quit;
        }
        /* from now on, output opens are EXTEND */
        if (fp->flags & _FILE_FLAG_DCBOUT) fp->flags |= _FILE_FLAG_EXTEND;
    }

    fp->recfm   = dcb->dcbrecfm;
    fp->lrecl   = dcb->dcblrecl;
    fp->blksize = dcb->dcbblksi;

    /* read JFCB to get the dataset name */
    err = __rdjfcb(fp->dcb, &jfcb);
    if (err) goto quit;
#if 0
	wtodumpf(&jfcb, sizeof(jfcb), "__fpopen:jscb");
#endif
    if (jfcb.jfcbdsnm[0] > ' ') {
        for(i=0; i < 44 && jfcb.jfcbdsnm[i] > ' '; i++) {
            fp->dataset[i] = jfcb.jfcbdsnm[i];
        }
        fp->dataset[i] = 0;
    }

    if (!(fp->flags & _FILE_FLAG_RECORD)) {
        /* not record oriented i/o */
        /* allocate file handle data buffer */
        switch(fp->recfm & _FILE_RECFM_TYPE) {
        case _FILE_RECFM_F: i = fp->lrecl;      break;
        case _FILE_RECFM_V: i = fp->lrecl - 4;  break;
        case _FILE_RECFM_U: i = fp->blksize;    break;
        }
        /* A DUMMY data set - DD DUMMY, or 'NULLFILE', @@start's stdin when
           the step has no SYSIN - never transfers a byte: the first
           __aread() is end of file.  Read, it needs no buffer of BLKSIZE,
           which for a DUMMY without DCB attributes is 32760: the 32 K
           calloc that ended programs before main() at a small REGION
           (#277).  The JFCB is read first for that. */
        if (!(fp->flags & _FILE_FLAG_WRITE)
            && strcmp(fp->dataset, "NULLFILE") == 0) i = 0;
        fp->buf     = calloc(1, i + 8);
        if (!fp->buf) {
            err = 1;        /* the JFCB read above set it to 0 */
            goto quit;
        }

        if (fp->flags & _FILE_FLAG_DCBOUT) {
            /* set file handle buffer pointers */
            fp->upto    = fp->buf;
            fp->endbuf  = fp->buf + i;
        }
        else {
            /* empty: the first read goes to the access method */
            fp->upto    = fp->buf;
            fp->endbuf  = fp->buf;
        }
    }

#if 0
	wtof("__fpopen: jfcb.jfcdsrg1=0x%02X dcb->dcbdsrg1=0x%02X",
		jfcb.jfcdsrg1, dcb->dcbdsrg1);
#endif

quit:
    return err;
}
