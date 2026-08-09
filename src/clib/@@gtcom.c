#include "clibcib.h"
#include "clibcrt.h"

COM *
__gtcom(void)
{
    CLIBGRT     *grt    = __grtget();
    COM         *com;
    unsigned    work[3] = {0};

    if (!grt) return (COM*)0;   /* no GRT: no COM anchor (#85) */
    com = grt->grtcom;

    if (!com) {
        __asm__("EXTRACT (%0),FIELDS=COMM,MF=(E,(%1))" : :
            "r" (&grt->grtcom), "r"(work) : "1", "14", "15");
        return grt->grtcom;
    }

    return com;
}
