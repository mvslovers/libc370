#include <stddef.h>
#include <string.h>
#include "mvs/vsam.h"

/* the type of the open cluster: TESTCB ATRB, which sets the condition
   code (equal: the attribute is there) and R15 0.  -1 when TESTCB fails */
static int
vstype(ACB *acb)
{
    unsigned        pl[16]  = {0};
    int             r15     = 0;
    int             ksds;
    int             rrds;

    __asm__("TESTCB ACB=(%2),ATRB=KSDS,MF=(G,(%3))\n\t"
            "ST\t15,%1\n\t"
            "LA\t%0,1\n\t"
            "BE\t*+6\n\t"
            "SR\t%0,%0"
        : "=r"(ksds), "=m"(r15) : "r"(acb), "r"(pl)
        : "0", "1", "14", "15", "memory");
    if (r15) return -1;
    if (ksds) return VSTYPE_KSDS;

    memset(pl, 0, sizeof(pl));
    __asm__("TESTCB ACB=(%2),ATRB=RRDS,MF=(G,(%3))\n\t"
            "ST\t15,%1\n\t"
            "LA\t%0,1\n\t"
            "BE\t*+6\n\t"
            "SR\t%0,%0"
        : "=r"(rrds), "=m"(r15) : "r"(acb), "r"(pl)
        : "0", "1", "14", "15", "memory");
    if (r15) return -1;

    return rrds ? VSTYPE_RRDS : VSTYPE_ESDS;
}

/* __vsinfo() - the attributes of the open cluster (#623).

   KEYLEN, RKP, LRECL, CINV and the buffers come from SHOWCB ACB, the type
   from TESTCB ATRB: the cluster's own, not the type the handle was
   opened with.  SHOWCB reports the ACB's fields: BUFND, BUFNI and BUFSP
   are 0 when OPEN was given none, VSAM's defaults are not filled in
   (MVS 3.8j, measured). */
int
__vsinfo(VSFILE *vs, VSINFO *info)
{
    /* SHOWCB's list (MF=G) for n fields takes 8n+20 bytes: n+1 addresses,
       a 16-byte header and a word per field.  76 for seven; 64 bytes
       made SHOWCB return 4 (measured) */
    unsigned        pl[20]  = {0};
    unsigned        area[7];        /* KEYLEN RKP LRECL CINV BUFND BUFNI BUFSP */
    int             rc      = 0;
    int             t;

    if (!vs || !info) return -1;

    memset(info, 0, sizeof(*info));
    memset(area, 0, sizeof(area));

    __asm__("\n"
    "        SHOWCB FIELDS=(KEYLEN,RKP,LRECL,CINV,BUFND,BUFNI,BUFSP),       X\n"
    "               ACB=(%1),AREA=(%2),LENGTH=28,MF=(G,(%3))\n\t"
            "ST\t15,%0"
        : "=m"(rc) : "r"(&vs->acb), "r"(area), "r"(pl)
        : "0", "1", "14", "15", "memory");
    if (rc) return rc;

    t = vstype(&vs->acb);
    if (t < 0) return -1;

    info->type      = (VSTYPE)t;
    info->keylen    = area[0];
    info->rkp       = area[1];
    info->lrecl     = area[2];
    info->cisz      = area[3];
    info->bufnd     = area[4];
    info->bufni     = area[5];
    info->bufsp     = area[6];

    return 0;
}
