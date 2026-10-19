/* @@DADSM.C - delete and rename a data set by volume: SCRATCH, RENAME */
#include <errno.h>
#include <string.h>
#include "mvs/dscb.h"
#include "ibm/mvs/iefucbob.h"
#include "ibm/mvs/cvt.h"

/*
 * __scratch() and __renamev() address a data set by name and volume, the
 * way IEHPROGM SCRATCH and RENAME do: through the VTOC, never through the
 * catalog.  remove() and rename() go through IDCAMS, i.e. the catalog, so
 * an uncataloged data set has no way there, and a data set of the same
 * name on another volume than the cataloged one none either (#143).
 *
 * The parameter list is CAMLST SCRATCH or CAMLST RENAME with a volume
 * list of one entry.  DADSM compares the device type in the entry with
 * the UCB of the volume (UCBTBYT3/4, IGG03002), so the entry takes the
 * UCB type of the online volume; a volume that is not online gets no SVC
 * at all, and so no mount request either.  Register 0 is zero: no UCB of
 * our own for DADSM to mount the volume on.
 *
 * SCRATCH and RENAME take the SYSDSN ENQ of the name themselves and give
 * it back when they return; neither leaves an allocation behind.
 */

#define CAMLST_SCRATCH  0x41004000U /* CAMLST SCRATCH,dsn,,vollist      */
#define CAMLST_OVRD     0x00001000U /* ... ,,OVRD: ignore the purge date */
#define CAMLST_RENAME   0xC1002000U /* CAMLST RENAME,dsn,newdsn,vollist  */

typedef struct {
    unsigned short  count;          /* number of entries, here 1        */
    unsigned char   devtype[4];     /* UCB device type                  */
    char            volser[6];      /* volume serial                    */
    unsigned char   seq;            /* spare                            */
    unsigned char   status;         /* returned: status on this volume  */
} __attribute__((packed)) VOLLST1;

typedef struct {
    unsigned        flags;          /* function and options             */
    const char      *dsn;           /* 44 bytes, blank padded           */
    const char      *newdsn;        /* RENAME: 44 bytes; SCRATCH: 0     */
    VOLLST1         *vollist;       /* volume list                      */
} CAMLST;

/* copy string s into a blank padded field of len bytes; 0, or -1 when s
   is NULL, empty or longer than the field */
static int
pad(char *field, const char *s, size_t len)
{
    size_t  n;

    if (!s) return -1;
    n = strlen(s);
    if (n == 0 || n > len) return -1;
    memset(field, ' ', len);
    memcpy(field, s, n);
    return 0;
}

/* the UCB device type of the online direct access volume vol (6 bytes),
   0 when it is found, -1 when it is not online */
static int
voltype(const char *vol, unsigned char devtype[4])
{
    UCBLIST *ucblist = CVTPTR->cvtilk2;
    UCBDASD *ucb;
    int     i;

    for (i = 0; ucblist->ucb[i] != UCBEOL; i++) {
        ucb = (UCBDASD *)(unsigned)ucblist->ucb[i];
        if (!ucb) continue;
        if (ucb->ucbtype[2] != UCBTYPEDA) continue;
        if (!(ucb->ucbstat & UCBONLI)) continue;
        if (memcmp(ucb->ucbvol, vol, 6) != 0) continue;
        memcpy(devtype, ucb->ucbtype, 4);
        return 0;
    }
    return -1;
}

/* errno for the volume status byte of return code 8; status 3 means an
   unexpired purge date to SCRATCH, a duplicate name to RENAME */
static int
statuserr(unsigned status, int rename)
{
    static const int err[8] = {
        EIO, ENOENT, EACCES, EPERM, EIO, ENODEV, ENXIO, EBUSY
    };

    if (status == 3 && rename) return EEXIST;
    if (status < 8) return err[status];
    return EIO;
}

/* issue SCRATCH (svc 29) or RENAME (svc 30) for one data set on one
   volume */
static int
dadsm(unsigned flags, const char *dsn, const char *vol, const char *newdsn)
{
    int     rc;
    int     svc30 = (newdsn != NULL);
    char    name[44];
    char    newname[44];
    VOLLST1 vl;
    CAMLST  parms;

    memset(&vl, 0, sizeof(vl));
    if (pad(name, dsn, sizeof(name)) ||
        pad(vl.volser, vol, sizeof(vl.volser)) ||
        (svc30 && pad(newname, newdsn, sizeof(newname)))) {
        errno = EINVAL;
        return -1;
    }
    if (voltype(vl.volser, vl.devtype)) {
        errno = ENODEV;
        return -1;
    }
    vl.count       = 1;
    parms.flags    = flags;
    parms.dsn      = name;
    parms.newdsn   = svc30 ? newname : NULL;
    parms.vollist  = &vl;

    if (svc30) {
        __asm__ __volatile__(
        "SR\t0,0\n\t"
        "LR\t1,%1\n\t"
        "SVC\t30\n\t"
        "LR\t%0,15"
        : "=r" (rc)
        : "r" (&parms)
        : "0", "1", "14", "15", "memory");
    }
    else {
        __asm__ __volatile__(
        "SR\t0,0\n\t"
        "LR\t1,%1\n\t"
        "SVC\t29\n\t"
        "LR\t%0,15"
        : "=r" (rc)
        : "r" (&parms)
        : "0", "1", "14", "15", "memory");
    }

    if (rc == 4) errno = ENODEV;
    else if (rc == 8) errno = statuserr(vl.status, svc30);
    else if (rc) errno = EINVAL;    /* 12: the volume list was refused */
    return rc;
}

int
__scratch(const char dsn[44], const char vol[6], int flags)
{
    unsigned f = CAMLST_SCRATCH;

    if (flags & __SCRATCH_OVRD) f |= CAMLST_OVRD;
    return dadsm(f, dsn, vol, NULL);
}

int
__renamev(const char dsn[44], const char vol[6], const char newdsn[44])
{
    if (!newdsn) {
        errno = EINVAL;
        return -1;
    }
    return dadsm(CAMLST_RENAME, dsn, vol, newdsn);
}
