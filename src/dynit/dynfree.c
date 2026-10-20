/* DYNFREE.C - dynfree() of <dynit.h>, z/OS's structure interface to SVC
   99 unallocation (#52) */
#include <errno.h>
#include <string.h>
#include <dynit.h>
#include "mvs/dynalloc.h"
#include "src/dynit/dynreq.h"

/* z/OS reads __ddname, __dsname, __member, __normdisp, __pathname,
   __pathndisp and __miscitems, and ignores the rest.  A path is refused:
   MVS 3.8j has no UNIX file system. */
int
dynfree(__dyn_t *d)
{
    DYNRQ   rq;

    if (!d) {
        errno = EINVAL;
        return -2;
    }
    memset(&rq, 0, sizeof(rq));
    d->__errcode = d->__infocode = 0;
    if (d->__pathname && *d->__pathname) rq.err = -2;

    __dynrqs(&rq, DUNDDNAM, d->__ddname, 8);
    __dynrqs(&rq, DUNDSNAM, d->__dsname, 44);
    __dynrqs(&rq, DUNMEMBR, d->__member, 8);
    if (d->__normdisp)
        __dynrqb(&rq, DUNOVDSP, (unsigned char)d->__normdisp, 1);
    __dynrqm(&rq, d->__miscitems);
    /* neither a DD nor a data set: nothing to name, refused (-2) */
    if (rq.count == 0 && !rq.err) rq.err = -2;
    /* also a DD that is permanently allocated, as __dsfree() does */
    __dynrqb(&rq, DUNUNALC, 0, 0);

    return __dynrqx(&rq, S99VRBUN, 0, d);
}
