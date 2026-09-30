/* FOPEN.C */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "mvs/lock.h"
#include "libc370/array.h"
#include "mvs/crt.h"
#include "mvs/recovery.h"

extern char *__getpfx(void);

extern int  __fpmode(FILE *fp, const char *mode);
extern int  __fpopen(FILE *fp);
extern int  __fpstar(FILE *fp);
extern int  __fpshr(FILE *fp);
extern int  __fpold(FILE *fp);
extern int  __fpnew(FILE *fp);

static int  appmem(FILE *fp);

FILE *
fopen(const char *fn, const char *mode)
{
    CLIBGRT *grt    = __grtget();
    int     err     = 1;
    FILE    *fp     = 0;
    int     i;

    if (!fn)    goto quit;
    if (!mode)  goto quit;

    fp = calloc(1, sizeof(_FILE)); 
    if (!fp) goto quit;

    for(i=0; _FILE_EYE[i]; i++) {
        fp->eye[i] = _FILE_EYE[i];
    }
    err = __fpmode(fp, mode);
    if (err) goto quit;

    if (fp->flags & _FILE_FLAG_WRITE) {
        /* These values are used as a suggestion to __fpopen() and __aopen()
           when opening a dataset that does not have DCB values */
        fp->recfm = _FILE_RECFM_V;  /* __aopen() will try to open as variable length records */
        fp->lrecl = 136;            /* this will be the block size, the lrecl will be 4 bytes less */
    }

    if (tolower(fn[0])=='d' && tolower(fn[1])=='d' && fn[2]==':') {
        /* "DD:ddname[(member)]" */
        fn+=3;
        for(i=0; i < 8 && *fn && *fn!='('; i++,fn++) {
            fp->ddname[i] = toupper(*fn);
        }
        if (*fn=='(') {
            /* extract member name */
            for(i=0, fn++; i < 8 && *fn && *fn!=')'; i++, fn++) {
                fp->member[i] = toupper(*fn);
            }
        }
        goto doopen;
    }

    if (fn[0]=='*') {
        /* SYSOUT request "*[ddname]" */
        /* copy ddname name */
        for(i=0, fn++;i < 8 && *fn && *fn !='('; i++, fn++) {
            fp->ddname[i] = toupper(*fn);
        }
        if (fp->ddname[0]) {
            /* try to open DDNAME */
            err = __fpopen(fp);
            if (!err) goto quit;    /* success, we're done */
        }

        /* allocate SYSOUT dataset */
        err = __fpstar(fp);
        if (err) goto quit;

        /* open SYSOUT dataset */
        goto doopen;
    }

    /* fn *should* be a dataset name */
    i = 0;
    if (fn[0]=='\'') {
        /* quoted dataset name */
        fn++;       /* skip the single quote */
    }
    else {
        /* unquoted dataset name */
        if (fn[0]!='&') {
            /* not a &temp dataset name */
            if (grt->grtflag1 & GRTFLAG1_TSO) {
                /* running under TSO environment */
                char *gp = __getpfx();
                if (gp) {
                    /* prepend prefix */
                    for(;i < 8 && *gp; i++,gp++) {
                        fp->dataset[i] = toupper(*gp);
                    }
                    fp->dataset[i++]='.';
                }
            }
        }
    }

    /* copy dataset name */
    for(;i < 44 && *fn && *fn !='(' && *fn!='\''; i++, fn++) {
        fp->dataset[i] = toupper(*fn);
    }
    if (*fn=='(') {
        /* copy member name */
        fn++;
        for(i=0; i < 8 && *fn && *fn!=')'; i++, fn++) {
            fp->member[i] = toupper(*fn);
        }
    }

    if ((fp->flags & _FILE_FLAG_READ) && !(fp->flags & _FILE_FLAG_WRITE)) {
        /* READ, allocate dataset DISP=SHR */
        err = __fpshr(fp);
    }
    else {
        /* WRITE, allocate dataset.  A '+' stream can write, so it is
           allocated as a writer - DISP=OLD for a data set (#189) */
        int mustexist = (fp->flags & _FILE_FLAG_READ) && fp->mode[0] == 'r';

        if (fp->dataset[0]=='&' && !mustexist) {
            /* temp dataset being allocated, try alloc DISP=NEW on VIO */
            err = __fptmp(fp);
            if (!err) goto doopen;
        }
        if (fp->member[0]) {
            /* allocate PDS member DISP=SHR */
            err = __fpshr(fp);
            if (!err) goto doopen;
        }
        /* allocate dataset DISP=OLD */
        err = __fpold(fp);
        if (!err) goto doopen;

        /* try allocating dataset DISP=NEW - but "r+" opens what exists */
        if (!mustexist) err = __fpnew(fp);
    }
    if (err) goto quit;

doopen:
    if ((fp->flags & _FILE_FLAG_APPEND) && fp->member[0] > ' ') {
        /* "a" on a PDS member (#198) - see appmem() */
        err = appmem(fp);
        if (err) goto quit;
    }
    err = __fpopen(fp);

    if (!err && (fp->flags & _FILE_FLAG_POSEND)) {
        /* "a+" is at the end of what is there (#189).  Count it now, while
           nothing is pending: read to the end, then EXTEND.  Counting later
           would flush a half-written line first and split it. */
        if (__fpswt(fp, 0) || __fpswt(fp, 1)) err = 1;
    }

quit:
    if (err) {
        /* an error occured, return NULL.  Preserve the errno that says
           WHY across the cleanup: fclose() runs a teardown of its own,
           and there is no reason its last step should be what the caller
           reads.  #184 needs EBUSY to survive this.

           One pre-existing path changes, in the right direction: a
           _FILE_FLAG_DYNAMIC FILE reaches __fpfree()'s SVC 99, whose
           errno used to be what the caller read.  fopen() documents
           no errno, so that was never meaningful -- this replaces one
           unrelated value with an older one.  Making "NULL implies a
           meaningful errno" a real contract needs errno = 0 on entry
           and a sweep of every path above; not this change. */
        int save = errno;
        if (fp) {
            /* close the file handle */
            fclose(fp);
            fp = 0;
        }
        errno = save;
    }

    if (fp) {
        lock(&grt->grtfile,0);
        arrayadd(&grt->grtfile, fp);
        unlock(&grt->grtfile,0);
    }

    return fp;
}

/* appmem() - "a" on a PDS member (#198).

   BPAM cannot extend a member in place: a member opened for output is
   written at the end of the data set and replaces the old one only at the
   STOW that CLOSE issues.  __aopen() therefore refuses EXTEND for a member,
   and before #198 "a" never asked for EXTEND at all - it opened OUTPUT and
   replaced the member, reporting success (mvsdev JOB00490).

   So: a member that exists is refused with EOPNOTSUPP, and nothing is
   written - its records stay as they are.  A member that does not exist
   yet has nothing to append to, and "a" creates it as "w" would.
   Appending by copying the old records forward is a separate issue.

   The probe is an input open of the same DD and member: it succeeds
   exactly when FIND finds the member. */
__asm__("\n&FUNC    SETC 'appmem'");
static int
appmem(FILE *fp)
{
    char    fn[24];
    FILE    *in;
    int     dl;
    int     ml;

    /* the names may be blank padded */
    for (dl = 0; dl < 8 && fp->ddname[dl] > ' '; dl++);
    for (ml = 0; ml < 8 && fp->member[ml] > ' '; ml++);
    sprintf(fn, "DD:%.*s(%.*s)", dl, fp->ddname, ml, fp->member);
    in = fopen(fn, "r");
    if (in) {
        fclose(in);
        errno = EOPNOTSUPP;
        return 1;
    }

    /* no such member: create it, as "w".  For "a+" that also means the
       position is 0 and known */
    fp->flags &= ~(_FILE_FLAG_APPEND | _FILE_FLAG_POSEND | _FILE_FLAG_EXTEND);
    errno = 0;
    return 0;
}
