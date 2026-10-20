#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "mvs/vsam.h"
#include "src/internal/vsopen.h"

/* what prep() needs.  OPEN reads the password after prep() has returned,
   so its field lives in __vsopenx()'s frame: a length byte, then up to
   eight characters (IDA0192C takes ACBPASSW this way). */
struct vsoparg {
    const VSOPTS    *o;
    unsigned char   pw[9];
};

/* the options, into the ACB and the RPL that __vsopn() has built */
static int
prep(VSFILE *vs, void *arg)
{
    struct vsoparg  *p      = arg;
    const VSOPTS    *o      = p->o;
    ACB             *acb    = &vs->acb;
    unsigned        pl[16]  = {0};

    /* the type is not known before OPEN: keyed and addressed both, the
       RPL is set to the type OPEN finds */
    if (o->type == VSTYPE_ANY) acb->acbmacr1 |= ACBADR;

    if (o->bufnd) acb->acbbufnd = o->bufnd;
    if (o->bufni) acb->acbbufni = o->bufni;
    if (o->bufsp) acb->acbbufsp = o->bufsp;
    if (p->pw[0]) acb->acbpassw = p->pw;

    if (o->flags & VSOPT_RST) acb->acbmacr2 |= ACBRST;
    if (o->flags & VSOPT_AIX) acb->acbmacr2 |= ACBAIX;
    if (o->flags & VSOPT_SIS) acb->acbmacr3 |= ACBSIS;
    if (o->flags & VSOPT_SKP) {
        acb->acbmacr2 |= ACBSKP;
        __asm__("MODCB RPL=(%0),OPTCD=(SKP),MF=(G,(%1))"
                : : "r"(&vs->rpl), "r"(pl) : "0", "1", "14", "15", "memory");
    }

    return 0;
}

/* the RPL for the type OPEN found, when the caller gave VSTYPE_ANY */
static int
settype(VSFILE *vs, const VSOPTS *o)
{
    unsigned        pl[16]  = {0};
    VSINFO          info;

    if (__vsinfo(vs, &info)) return EVSOPEN;

    vs->type = (unsigned char)info.type;
    if (info.type != VSTYPE_ESDS) return 0;

    /* an ESDS has no key to skip by */
    if (o->flags & VSOPT_SKP) return EVSACCESS;

    __asm__("MODCB RPL=(%0),OPTCD=(ADR),MF=(G,(%1))"
            : : "r"(&vs->rpl), "r"(pl) : "0", "1", "14", "15", "memory");
    return 0;
}

/* __vsopenx() - __vsopen() with the options of a VSOPTS (#623) */
int
__vsopenx(const char *ddname, const VSOPTS *o, VSFILE **vsfile)
{
    struct vsoparg  p;
    VSFILE          *vs     = 0;
    VSTYPE          t;
    size_t          len     = 0;
    int             rc;

    if (!vsfile) {
        errno = EINVAL;
        return EINVAL;
    }
    *vsfile = 0;
    if (!o || (o->flags & ~(VSOPT_RST | VSOPT_SKP | VSOPT_SIS | VSOPT_AIX))) {
        errno = EINVAL;
        return EINVAL;
    }
    if (o->password) len = strlen(o->password);
    if (len > 8) {
        errno = EINVAL;
        return EINVAL;
    }
    /* skip sequential needs a key: no ESDS, no addressed access */
    if ((o->flags & VSOPT_SKP) &&
        (o->type == VSTYPE_ESDS || o->access == VSACCESS_ADR)) {
        errno = EVSACCESS;
        return EVSACCESS;
    }

    memset(&p, 0, sizeof(p));
    p.o     = o;
    p.pw[0] = (unsigned char)len;
    if (len) memcpy(&p.pw[1], o->password, len);

    /* VSTYPE_ANY is built as a KSDS, with the KSDS's rules for the
       access and the mode, and opened keyed and addressed (prep) */
    t = o->type == VSTYPE_ANY ? VSTYPE_KSDS : o->type;
    rc = __vsopn(ddname, t, o->access, o->mode, &vs, prep, &p);
    if (rc) return rc;

    /* the password field is gone when this function returns */
    vs->acb.acbpassw = 0;

    if (o->type == VSTYPE_ANY) {
        rc = settype(vs, o);
        if (rc) {
            __vsclos(vs);
            errno = rc;
            return rc;
        }
    }

    *vsfile = vs;
    return 0;
}
