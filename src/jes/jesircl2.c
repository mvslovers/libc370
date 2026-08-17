
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mvssupa.h>
#include "clibvsam.h"

/* Close the internal reader and hand the caller the jobid JES2 assigned.
 *
 * The jobid ("JOBnnnnn") arrives in rpl.rplrbar -- an inline field of the
 * RPL embedded in the VSFILE -- as the answer to the ENDREQ below.  The
 * close that follows free()s the VSFILE, so this is the only moment the
 * feedback can be copied out legally; reading it through the handle after
 * jesircls() returns is a use-after-free, which both known consumers did
 * (#118).
 *
 * jobid may be NULL (jesircls() delegates here with NULL).  On any early
 * exit the buffer is zeroed, so a caller that ignores the return code
 * still sees an empty id rather than stack garbage.
 */
int jesircl2(VSFILE *vsfile, unsigned char jobid[8])
{
    int   rc = 0;
    void *wa = NULL;

    if (jobid) memset(jobid, 0, 8);

    /* Without this test the reads below fetch from the PSA - low-address
     * protection stops stores into page zero, not fetches - and the free()
     * at the end would be handed whatever word lives there.               */
    if (!vsfile) goto quit;

    /* The 80 byte RPL work area jesiropn() calloc'd and handed to VSAM
     * through MODCB AREA=.  Its only reference is rpl.rplarea, and
     * vsclose() frees the VSFILE alone, so remember the area before the
     * handle dies.  Left behind, one live 80 byte block pins its whole 4K
     * heap page for the life of the address space (#115).                 */
    wa = vsfile->rpl.rplarea;

    /* call ENDREQ macro */
    __asm__("ENDREQ RPL=(%0)\n\t"
            "ST  15,%1\n"
        : : "r"(&vsfile->rpl), "m"(rc) : "1", "14", "15");

    if (jobid) memcpy(jobid, vsfile->rpl.rplrbar, 8);

    vsclose(vsfile);

    /* after the CLOSE, which has no business with the record area anymore */
    if (wa) free(wa);

quit:
    return rc;
}
