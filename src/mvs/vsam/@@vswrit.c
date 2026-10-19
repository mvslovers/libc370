#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include "mvs/vsam.h"

static int restore(VSFILE *vs);

/* A KSDS opened for dynamic access has OPTCD=SEQ in its RPL, so a PUT
   there was keyed-sequential: a key below the current position and a
   duplicate key both failed with feedback 12 (out of sequence), and the
   two could not be told apart.  Such a PUT goes direct (#514): a lower
   key is inserted, a duplicate fails with feedback 8.  Direct access
   has OPTCD=DIR already; sequential access is a load and stays so. */
#define PUT_DIRECT(vs)  ((vs)->type == VSTYPE_KSDS \
                         && (vs)->access == VSACCESS_DYNAM)

int
__vswrit(VSFILE *vs, void *rec, int reclen, void *key, int keylen)
{
    unsigned        pl[16]  = {0};
    volatile int    rc      = 0;

    /* modify common RPL parameters */
    __vsmdfy(vs, rec, reclen, key, keylen);

    vs->flags   |= VSFILE_FLAG_WRITE;

    if (vs->mode==VSMODE_UPD) {
        /* turn off UPD for record insertion, and go direct on a KSDS
           opened for dynamic access (#514) */
        if (PUT_DIRECT(vs))
            __asm__("MODCB RPL=(%0),OPTCD=(DIR,NUP),MF=(G,(%1))"
                : : "r"(&vs->rpl), "r"(pl) : "0", "1", "14", "15", "memory");
        else
            __asm__("MODCB RPL=(%0),OPTCD=(NUP),MF=(G,(%1))"
                : : "r"(&vs->rpl), "r"(pl) : "0", "1", "14", "15", "memory");
        /* SYNAD exit will call our restore function on error */
        vs->vsreset = restore;
    }

    /* get record from VSAM dataset */
    __asm__("LA\t2,PUTDONE\n\t"
            "PUT\tRPL=(%0)\n"
   "PUTDONE\tDS\t0H\n\t"
            "ST\t15,%1"
        : : "r"(&vs->rpl), "m"(rc) : "0", "1", "14", "15", "2", "memory");

    if (vs->mode==VSMODE_UPD && vs->vsreset) {
        /* restore UPD in RPL */
        restore(vs);
    }

quit:
    return rc;
}

static int
restore(VSFILE *vs)
{
    unsigned    pl[16]  = {0};

    if (PUT_DIRECT(vs))
        __asm__("MODCB RPL=(%0),OPTCD=(SEQ,UPD),MF=(G,(%1))"
            : : "r"(&vs->rpl), "r"(pl) : "0", "1", "14", "15", "memory");
    else
        __asm__("MODCB RPL=(%0),OPTCD=(UPD),MF=(G,(%1))"
            : : "r"(&vs->rpl), "r"(pl) : "0", "1", "14", "15", "memory");

    vs->vsreset = 0;

    return 0;
}
