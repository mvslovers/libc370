/* @@DELMEM.C - delete a PDS member via STOW (delete) */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "osio.h"
#include "clibio.h"
#include "clibos.h"

/*
 * __delmem() - delete member 'mem' from PDS 'dsn'.
 *
 * Uses STOW delete under a DISP=SHR allocation, the way ISPF removes a
 * member.  remove() used to route members through IDCAMS DELETE, whose
 * dynamic allocation demands the data set exclusively - and when the
 * address space already holds an allocation of that DSN (a STEPLIB, an
 * httpd SYSENV DD), dynamic allocation escalates the shared SYSDSN ENQ to
 * exclusive with no way back down, so the data set stays blocked until the
 * step ends (#127, mvslovers/mvsmf#342).  A directory update needs no
 * exclusive; this path never asks for one.
 *
 * The delete also passes through OPEN for OUTPUT here, i.e. the same RACF
 * gate as a member write - IDCAMS DELETE never drove OPEN in the caller's
 * address space.
 *
 * Only the directory entry is removed; the member's tracks are reclaimed
 * by the next compress, exactly as with IDCAMS.
 *
 * Returns:
 *    0  success
 *   >0  STOW return code (register 15), typically:
 *          8  the member was not found in the directory
 *         12  permanent I/O error
 *         20  insufficient virtual storage
 *   -1  invalid parameters or out of memory
 *   -2  the data set could not be allocated (not found or in use)
 *   -3  the data set could not be opened
 */
int
__delmem(const char *dsn, const char *mem)
{
    int   rc        = -1;
    DCB   *dcb      = NULL;
    char  ddname[9] = {0};
    char  area[8];
    int   i;

    if (!dsn || !mem) goto quit;
    if (*mem <= ' ') goto quit;

    /* allocate the PDS DISP=SHR so we may update its directory */
    if (__dsalcf(ddname, "DSN=%s;DISP=SHR", dsn)) {
        rc = -2;
        goto quit;
    }

    /* build a BSAM/BPAM DCB for the DD */
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

    /* open for OUTPUT - STOW requires the data set open for output/update */
    if (osbopen(dcb, 0, "write")) {
        rc = -3;
        goto close;
    }

    /* build the 8-byte STOW delete list: the member name, upper-cased and
     * blank padded */
    memset(area, ' ', sizeof(area));
    for (i = 0; i < 8 && mem[i] > ' '; i++) {
        area[i] = toupper((unsigned char)mem[i]);
    }

    /* remove the directory entry (STOW delete) */
    rc = __stow(dcb, area, 'D');

close:
    osbclose(dcb, NULL, 1, 0);  /* close and free the DCB */
    dcb = NULL;
dealloc:
    __dsfree(ddname);
quit:
    return rc;
}
