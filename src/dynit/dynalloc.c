/* DYNALLOC.C - dynalloc() of <dynit.h>, z/OS's structure interface to
   SVC 99 allocation (#52) */
#include <errno.h>
#include <string.h>
#include <dynit.h>
#include "mvs/dynalloc.h"
#include "src/dynit/dynreq.h"

/* a string field that is set */
static int
given(const char *s)
{
    return s && *s;
}

/* what MVS 3.8j cannot do and the request would be wrong without: SMS
   data class, VSAM organization and key offset, REFDD, LIKE, a DSNTYPE
   other than PDS, a UNIX path, record level sharing.  Storage and
   management class only steer where SMS puts a data set and are ignored,
   as are the request block extension and the message list. */
static int
refused(const __dyn_t *d)
{
    return given(d->__dataclass) || d->__recorg || d->__keyoffset
        || given(d->__refdd) || given(d->__like)
        || (d->__dsntype && d->__dsntype != __DSNT_PDS)
        || given(d->__pathname) || d->__rls;
}

/* a numeric field: 0 or less is not given, more than fits is refused */
static void
number(DYNRQ *rq, unsigned key, long v, unsigned n)
{
    unsigned long   max = (n >= 4) ? 0xFFFFFFFFUL : (1UL << (8 * n)) - 1;

    if (v <= 0) return;
    if ((unsigned long)v > max) {
        rq->err = -2;
        return;
    }
    __dynrqb(rq, key, (unsigned long)v, n);
}

/* the DD name: given, generated (NULL, "" or "????????"), and where the
   generated one goes back to */
static unsigned char *
ddname(DYNRQ *rq, const __dyn_t *d, int *intoname)
{
    *intoname = d->__ddname && strcmp(d->__ddname, "????????") == 0;
    if (given(d->__ddname) && !*intoname) {
        __dynrqs(rq, DALDDNAM, d->__ddname, 8);
        return NULL;
    }
    return __dynrqr(rq, DALRTDDN, 8);
}

static void
dcb(DYNRQ *rq, const __dyn_t *d)
{
    __dynrqs(rq, DALDCBDS, d->__dcbrefds, 44);
    __dynrqs(rq, DALDCBDD, d->__dcbrefdd, 8);
    if (d->__dsorg) __dynrqb(rq, DALDSORG, (unsigned short)d->__dsorg, 2);
    number(rq, DALRECFM, d->__recfm, 1);
    if (d->__recfm < 0) rq->err = -2;
    if (d->__blksize) __dynrqb(rq, DALBLKSZ, (unsigned short)d->__blksize, 2);
    if (d->__lrecl) __dynrqb(rq, DALLRECL, d->__lrecl, 2);
    number(rq, DALKYLEN, d->__keylength, 1);
}

static void
space(DYNRQ *rq, const __dyn_t *d)
{
    if (d->__alcunit == __CYL) __dynrqb(rq, DALCYL, 0, 0);
    else if (d->__alcunit == __TRK) __dynrqb(rq, DALTRK, 0, 0);
    else if (d->__alcunit) rq->err = -2;
    number(rq, DALBLKLN, d->__avgblk, 3);
    number(rq, DALPRIME, d->__primary, 3);
    number(rq, DALSECND, d->__secondary, 3);
    number(rq, DALDIR, d->__dirblk, 3);
}

static void
flags(DYNRQ *rq, unsigned f)
{
    if (f & __CLOSE)     __dynrqb(rq, DALCLOSE, 0, 0);
    if (f & __RELEASE)   __dynrqb(rq, DALRLSE, 0, 0);
    if (f & __PERM)      __dynrqb(rq, DALPERMA, 0, 0);
    if (f & __CONTIG)    __dynrqb(rq, DALSPFRM, 0x08, 1);   /* CONTIG */
    if (f & __ROUND)     __dynrqb(rq, DALROUND, 0, 0);
    if (f & __TERM)      __dynrqb(rq, DALTERM, 0, 0);
    if (f & __DUMMY_DSN) __dynrqb(rq, DALDUMMY, 0, 0);
    if (f & __HOLDQ)     __dynrqb(rq, DALSHOLD, 0, 0);
}

static void
sysout(DYNRQ *rq, const __dyn_t *d)
{
    if (!d->__sysout) return;
    if (d->__sysout == __DEF_CLASS) __dynrqb(rq, DALSYSOU, 0, 0);
    else __dynrqb(rq, DALSYSOU, (unsigned char)d->__sysout, 1);
    __dynrqs(rq, DALSPGNM, d->__sysoutname, 8);
}

int
dynalloc(__dyn_t *d)
{
    DYNRQ           rq;
    unsigned char   *rtddn;
    unsigned char   *rtdsn  = NULL;
    int             intoname;
    int             flag1   = S99NOCNV;
    int             rc;

    if (!d) {
        errno = EINVAL;
        return -2;
    }
    memset(&rq, 0, sizeof(rq));
    d->__errcode = d->__infocode = 0;
    if (refused(d)) rq.err = -2;

    rtddn = ddname(&rq, d, &intoname);
    __dynrqs(&rq, DALDSNAM, d->__dsname, 44);
    __dynrqs(&rq, DALMEMBR, d->__member, 8);
    if (d->__status)   __dynrqb(&rq, DALSTATS, (unsigned char)d->__status, 1);
    if (d->__normdisp) __dynrqb(&rq, DALNDISP, (unsigned char)d->__normdisp, 1);
    if (d->__conddisp) __dynrqb(&rq, DALCDISP, (unsigned char)d->__conddisp, 1);
    sysout(&rq, d);
    __dynrqs(&rq, DALUNIT, d->__unit, 8);
    __dynrqs(&rq, DALVLSER, d->__volser, 6);
    __dynrqs(&rq, DALVLRDS, d->__volrefds, 44);
    dcb(&rq, d);
    space(&rq, d);
    flags(&rq, d->__misc_flags);
    __dynrqs(&rq, DALPASSW, d->__password, 8);
    /* a new data set without a name is a temporary one: ask for the name
       MVS gives it.  Not for SYSOUT, DUMMY and the terminal, which have
       none to return. */
    if (!given(d->__dsname) && !d->__sysout
        && !(d->__misc_flags & (__DUMMY_DSN | __TERM)))
        rtdsn = __dynrqr(&rq, DALRTDSN, 44);
    __dynrqm(&rq, d->__miscitems);

    /* a unit or volume that is not mounted fails at once instead of
       waiting for the operator (#181); __dyn_t cannot ask for the mount */
    if (given(d->__unit) || given(d->__volser)) flag1 |= S99NOMNT;

    rc = __dynrqx(&rq, S99VRBAL, flag1, d);
    if (rc == 0 && rtddn) {
        __dynrqc(d->__retddn, sizeof(d->__retddn), rtddn);
        if (intoname) strcpy(d->__ddname, d->__retddn);
    }
    if (rc == 0 && rtdsn)
        __dynrqc(d->__retdsn, sizeof(d->__retdsn), rtdsn);
    return rc;
}
