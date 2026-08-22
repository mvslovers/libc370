/* JESCLOSE.C - Close JES spool datasets */
#include <stdio.h>
#include <stdlib.h>
#include "hasphct.h"    /* JES Checkpoint Control Table, record 3 in HASPCKPT */
#include "haspjct.h"    /* JES Job Control Table                            */
#include "hasppddb.h"   /* JES PDDB Print Datasets                          */
#include "haspiot.h"    /* JES IOT                                          */
#include "clibjes2.h"   /* JES prototypes */
#include "clibary.h"    /* dynamic array                                    */

int jesclose(JES **ppjes)
{
    JES         *jes    = NULL;
    unsigned    count;
    unsigned    n;

    if (!ppjes) goto quit;

    jes = *ppjes;
    if (jes) {
        /* free whatever a jesjob() walk left anchored: after a handler
           abend, recovery has nothing but this handle, and these were the
           ~26K leaking per abend (#126) */
        if (jes->injobs) jesjobfr(&jes->injobs);
        if (jes->inbuf)  { free(jes->inbuf);  jes->inbuf  = NULL; }
        if (jes->inbuf2) { free(jes->inbuf2); jes->inbuf2 = NULL; }
        if (jes->inbuf3) { free(jes->inbuf3); jes->inbuf3 = NULL; }

        if (jes->js) {
            count = arraycount(&jes->js);
            for (n=0; n < count; n++) {
                if (!jes->js[n]) continue;
                spool_close(jes->js[n]);
                jes->js[n] = NULL;
            }
            arrayfree(&jes->js);
        }
        if (jes->cp) {
            checkpoint_close(jes->cp);
            jes->cp = NULL;
        }
        free(jes);
        *ppjes = NULL;
    }

quit:
    return 0;
}



