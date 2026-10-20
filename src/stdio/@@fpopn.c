/* @@FPOPN.C - fopen()'s core (size analysis F5)
**
** fopen() named the handler of every kind of name, so the standard
** streams, which __stdlz() opens as "*SYSPRINT", "*SYSTERM", "dd:SYSIN"
** and 'NULLFILE', linked the allocation of new and temporary data sets and
** the TSO PUTLINE/GETLINE streams as well.  This is fopen() without them:
** the caller passes what it wants beyond "dd:" names, SYSOUT ("*dd") and
** existing data sets.
**
**   tsoname  NULL, or a function for the reserved "*PUTLINE"/"*GETLINE"
**            names: -1 not one of them, otherwise the open's result
**            (0 opened, nonzero failed)
**   dsalloc  NULL, or a function that allocates a data set by name; 0 when
**            it did.  NULL allocates an existing data set only: DISP=SHR
**            to read, DISP=OLD to write, which is what 'NULLFILE' needs.
**
** fopen() passes both; __stdlz() and appmem() pass NULL.
*/
#include "src/internal/fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "mvs/lock.h"
#include "ext/array.h"
#include "mvs/crt.h"
#include "src/internal/tskey.h"

extern char *__getpfx(void);

extern int  __fpmode(FILE *fp, const char *mode);
extern int  __fpopen(FILE *fp);
extern int  __fpstar(FILE *fp);
extern int  __fpshr(FILE *fp);
extern int  __fpold(FILE *fp);

static int  appmem(FILE *fp);
static void dsname(FILE *fp, const char *fn, const CLIBGRT *grt);

FILE *
__fpopn(const char *fn, const char *mode,
        int (*tsoname)(FILE *fp, const char *fn), int (*dsalloc)(FILE *fp))
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

    if (tolower((unsigned char)fn[0])=='d' && tolower((unsigned char)fn[1])=='d' && fn[2]==':') {
        /* "DD:ddname[(member)]" */
        fn+=3;
        for(i=0; i < 8 && *fn && *fn!='('; i++,fn++) {
            fp->ddname[i] = toupper((unsigned char)*fn);
        }
        if (*fn=='(') {
            /* extract member name */
            for(i=0, fn++; i < 8 && *fn && *fn!=')'; i++, fn++) {
                fp->member[i] = toupper((unsigned char)*fn);
            }
        }
        /* the program's DCB attributes, if the mode names any, take
           precedence over the DD and the DSCB (#554) */
        fp->xflags |= _FILE_XFLAG_DDDCB;
        goto doopen;
    }

    /* "*PUTLINE" (#463) and "*GETLINE" (#467), before the SYSOUT names */
    if (tsoname) {
        i = tsoname(fp, fn);
        if (i >= 0) {
            err = i;
            goto quit;
        }
    }

    if (fn[0]=='*') {
        /* SYSOUT request "*[ddname]" */
        /* copy ddname name */
        for(i=0, fn++;i < 8 && *fn && *fn !='('; i++, fn++) {
            fp->ddname[i] = toupper((unsigned char)*fn);
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
    dsname(fp, fn, grt);

    if (dsalloc) {
        err = dsalloc(fp);
    }
    else if ((fp->flags & _FILE_FLAG_READ) && !(fp->flags & _FILE_FLAG_WRITE)) {
        /* READ, allocate dataset DISP=SHR */
        err = __fpshr(fp);
    }
    else {
        /* WRITE to a data set that exists: DISP=OLD */
        err = __fpold(fp);
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
        /* the list is created, and grows, in the task's key (#693): made
           in a caller's key-0 window it was key-0 storage, and the
           program's next fopen() in its own key ended S0C4 storing into
           it (2.15.0-2.24.0; up to 2.14.0 the start-up's fopen() of
           stdout had created it) */
        unsigned st;

        lock(&grt->grtfile,0);
        st = tskey();
        arrayadd(&grt->grtfile, fp);
        tsrestore(st);
        unlock(&grt->grtfile,0);
    }

    return fp;
}

/* dsname() - the data set name and member of fn into fp: 'quoted' as it
   is, unquoted with the TSO prefix in front when running under TSO, a
   &temp name never prefixed. */
__asm__("\n&FUNC    SETC 'dsname'");
static void
dsname(FILE *fp, const char *fn, const CLIBGRT *grt)
{
    int     i = 0;

    if (fn[0]=='\'') {
        /* quoted dataset name */
        fn++;       /* skip the single quote */
    }
    else if (fn[0]!='&' && (grt->grtflag1 & GRTFLAG1_TSO)) {
        /* unquoted, not a &temp name, running under TSO */
        char *gp = __getpfx();
        if (gp) {
            /* prepend prefix */
            for(;i < 8 && *gp; i++,gp++) {
                fp->dataset[i] = toupper((unsigned char)*gp);
            }
            fp->dataset[i++]='.';
        }
    }

    /* copy dataset name */
    for(;i < 44 && *fn && *fn !='(' && *fn!='\''; i++, fn++) {
        fp->dataset[i] = toupper((unsigned char)*fn);
    }
    if (*fn=='(') {
        /* copy member name */
        fn++;
        for(i=0; i < 8 && *fn && *fn!=')'; i++, fn++) {
            fp->member[i] = toupper((unsigned char)*fn);
        }
    }
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
   exactly when FIND finds the member.  It goes through __fpopn(), not
   fopen(): a "DD:" name needs neither of fopen()'s handlers (F5). */
__asm__("\n&FUNC    SETC 'appmem'");
static int
appmem(FILE *fp)
{
    char    fn[24];
    FILE    *in;
    int     dl;
    int     ml;
    int     i = 0;

    /* the names may be blank padded.  "DD:dd(mem)" built by hand: one
       sprintf() here linked all of printf into every program that opens
       a stream (size analysis F1) */
    fn[i++] = 'D'; fn[i++] = 'D'; fn[i++] = ':';
    for (dl = 0; dl < 8 && fp->ddname[dl] > ' '; dl++) fn[i++] = fp->ddname[dl];
    fn[i++] = '(';
    for (ml = 0; ml < 8 && fp->member[ml] > ' '; ml++) fn[i++] = fp->member[ml];
    fn[i++] = ')';
    fn[i] = 0;
    in = __fpopn(fn, "r", 0, 0);
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
