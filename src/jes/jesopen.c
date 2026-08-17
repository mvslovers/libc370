/* JESOPEN.C - Open JES spool datasets */
#include <stdio.h>
#include <stdlib.h>
#include "hasphct.h"    /* JES Checkpoint Control Table, record 3 in HASPCKPT */
#include "haspjct.h"    /* JES Job Control Table                            */
#include "hasppddb.h"   /* JES PDDB Print Datasets                          */
#include "haspiot.h"    /* JES IOT                                          */
#include "clibjes2.h"   /* JES prototypes */
#include "clibstae.h"   /* ESTAE functions */
#include "clibary.h"    /* dynamic array */
#include "clibwto.h"    /* wtof                                             */

static void try_jesopen(JES **jespp);

JES *jesopen(void)
{
    JES         *jes    = NULL;

    try(try_jesopen, &jes);

    return jes;
}

__asm__("\n&FUNC    SETC 'try_jesopen'");
static void try_jesopen(JES **jespp)
{
    JES         *jes    = NULL;
    HASPCP      *cp     = NULL;
    HASPJS      *js     = NULL;

    jes = calloc(1, sizeof(JES));
    if (!jes) {
        wtof("Unable to allocate storage for JES handle");
        goto quit;
    }

    cp = checkpoint_open("DD:HASPCKPT");
    if (!cp) {
        wtof("Unable to open checkpoint dataset DD:HASPCKPT");
        jesclose(&jes);
        goto quit;
    }

    /* Put checkpoint handle into our JES handle */
    jes->cp = cp;

    js = spool_open("DD:HASPACE1");
    if (!js) {
        wtof("Unable to open spool dataset DD:HASPACE1");
        jesclose(&jes);
        goto quit;
    }

    /* Add spool handle to array of spool handle in our JES handle.
     *
     * Unchecked, this was the one allocation on the open path that could
     * fail and still hand back a handle (#108): jes->js stays NULL while
     * the eye catcher and jes->cp say the handle is good.  jesjob() and
     * jesprint() then evaluate jes->js[0], and on MVS that load SUCCEEDS -
     * low-address protection stops stores into page zero, not fetches - so
     * they get a non-NULL value out of the PSA, walk past __jsrd4()'s own
     * NULL test and store through it.  The S0C4 the caller sees is that
     * store, not the shortage that caused it.
     *
     * jesclose() cannot release the spool handle here because it reaches it
     * through the array that does not exist, so close it directly first.
     * That is not a double close: if arrayadd() failed the array holds no
     * elements, and jesclose() only walks the ones it counts.             */
    if (arrayadd(&jes->js, js)) {
        wtof("Unable to add spool handle to JES handle");
        spool_close(js);
        jesclose(&jes);
        goto quit;
    }

quit:
    *jespp = jes;
    return;
}


