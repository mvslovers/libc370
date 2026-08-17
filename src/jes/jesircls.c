
#include <stdio.h>
#include <stdlib.h>
#include <mvssupa.h>
#include "clibvsam.h"

int jesircls(VSFILE *vsfile)
{
    int   rc = 0;
    void *wa = NULL;

    /* Without this test the reads below fetch from the PSA - low-address
     * protection stops stores into page zero, not fetches - and the free()
     * at the end would be handed whatever word lives there.               */
    if (!vsfile) goto quit;

    /* The 80 byte RPL work area jesiropn() calloc'd and handed to VSAM
     * through MODCB AREA=.  Its only reference is rpl.rplarea, and
     * vsclose() frees the VSFILE alone, so remember the area before the
     * handle dies.  Left behind, one live 80 byte block pins its whole 4K
     * heap page for the life of the address space - one page per internal
     * reader open (#115).                                                 */
    wa = vsfile->rpl.rplarea;

    /* call ENDREQ macro */
    __asm__("ENDREQ RPL=(%0)\n\t"
            "ST  15,%1\n"
        : : "r"(&vsfile->rpl), "m"(rc) : "1", "14", "15");

    vsclose(vsfile);

    /* after the CLOSE, which has no business with the record area anymore */
    if (wa) free(wa);

quit:
    return rc;

}

