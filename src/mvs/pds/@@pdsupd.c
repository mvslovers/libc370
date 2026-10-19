/* @@PDSUPD.C - one STOW on a PDS allocated by name, or by name and volume */
#include <mvs/dynalloc.h>
#include <string.h>
#include <ctype.h>
#include "mvs/osio.h"
#include "mvs/dscb.h"
#include "mvs/pds.h"
#include "src/internal/pdsupd.h"

/*
 * The core of __delmem(), __renmem() and their volume forms (#143).
 *
 * The data set is allocated DISP=SHR, the way ISPF updates a directory:
 * remove() used to route members through IDCAMS DELETE, whose dynamic
 * allocation demands the data set exclusively - and when the address
 * space already holds an allocation of that DSN (a STEPLIB, a server's
 * DD), dynamic allocation escalates the shared SYSDSN ENQ to exclusive
 * with no way back down, so the data set stays blocked until the step
 * ends (#127, mvslovers/mvsmf#342).  A directory update needs no
 * exclusive; this path never asks for one.
 *
 * With a volume the allocation names it (DALVLSER) and no unit: SVC 99
 * takes the unit from the mounted volume (measured on MVS 3.8j, 3350 and
 * 3390, where JCL insists on UNIT=), and a wrong unit type only makes it
 * fail.  It does not look into the VTOC, though: DISP=SHR of a name that
 * is not on the volume is allocated all the same.  OBTAIN answers that
 * first, so a data set that is not there is -2, as without a volume.
 *
 * The open for OUTPUT is the same RACF gate as a member write.
 */
int
__pdsupd(const char *dsn, const char *vol, void *area, int func)
{
    int     rc;
    DCB     *dcb;
    DSCB    dscb;
    char    ddname[9] = {0};
    char    name[45];
    size_t  i;

    if (vol) {
        /* OBTAIN takes the name as it stands; SVC 99 gets it in upper
           case from __dsalc() */
        for (i = 0; i < sizeof(name) - 1 && dsn[i]; i++)
            name[i] = toupper((unsigned char)dsn[i]);
        name[i] = 0;
        if (dsn[i] || __dscbdv(name, vol, &dscb)) return -2;
        rc = __dsalcf(ddname, "DSN=%s;VOLSER=%s;DISP=SHR", dsn, vol);
    }
    else {
        rc = __dsalcf(ddname, "DSN=%s;DISP=SHR", dsn);
    }
    if (rc) return -2;

    /* a BSAM/BPAM DCB for the DD */
    dcb = osbdcb(ddname, NULL);
    if (!dcb) {
        rc = -1;
        goto dealloc;
    }

    /* force partitioned organisation: opening a PDS for OUTPUT with
     * DSORG=PO updates the directory without erasing members, whereas
     * DSORG=PS OUTPUT would reset the data set */
    dcb->dcbdsrg1 = DCBDSGPO;
    dcb->dcbdsrg2 = 0;

    /* STOW requires the data set open for output/update */
    if (osbopen(dcb, 0, "write"))
        rc = -3;
    else
        rc = __stow(dcb, area, func);

    osbclose(dcb, NULL, 1, 0);  /* close and free the DCB */
dealloc:
    __dsfree(ddname);
    return rc;
}

void
__pdsmem(char *field, const char *mem)
{
    int i;

    memset(field, ' ', 8);
    for (i = 0; i < 8 && mem[i] > ' '; i++)
        field[i] = toupper((unsigned char)mem[i]);
}
